/* malloc-canary.c - LD_PRELOAD heap-overrun probe (task 056).
 *
 * Every small allocation gets a registration in a fixed open-addressing
 * table; allocations whose *caller* lives in a module matching
 * CANARY_ONLY additionally get a magic canary written at the end of the
 * allocator's *usable* size (not at the requested size: callers may
 * legally use the rounding slack, and only a write past the usable size
 * can clobber the next chunk header - the damage glibc reports as
 * "malloc_consolidate(): unaligned fastbin chunk detected").
 *
 * Registration is deliberately NOT restricted to the filtered module.
 * An earlier version only registered filtered blocks, so when a filtered
 * block was freed and the address was later reused by an *unfiltered*
 * allocation, the stale entry kept its old size and the next free read a
 * "canary" from the wrong offset - reporting overruns that were pure
 * artefacts. Registering everything, and re-checking that
 * malloc_usable_size() still matches the recorded size before trusting an
 * entry, removes that class of false positive.
 *
 * A hit is reported with the allocating and the freeing return address,
 * each resolved to module + offset (ready for addr2line).
 *
 * Bounded by construction: the table never grows, entries are reused in
 * place, and once TABLE_CAP/2 registrations have been seen the probe
 * disables itself (an earlier unbounded variant of this probe took a VM
 * down with OOM, see ai-docs/tasks/056).
 *
 * Build: gcc -shared -fPIC -O1 -o /tmp/malloc-canary.so malloc-canary.c -ldl
 * Use:   LD_PRELOAD=/tmp/malloc-canary.so [CANARY_ONLY=...] ./rank
 *        (hits are written to stderr, one line each)
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <stdint.h>

#define CANARY_MAX  4096                 /* canary allocations up to this */
#define CANARY      ((uint64_t) 0xCAFEF00DD15EA5E5ull)
#define TABLE_BITS  18                   /* 262144 slots ~ 6 MB */
#define TABLE_CAP   (TABLE_BITS > 0 ? (1u << TABLE_BITS) : 1u)
#define MAX_INSERTS (TABLE_CAP/2)        /* stop before the table degrades */

static void* (*r_malloc)(size_t) = NULL;
static void* (*r_calloc)(size_t, size_t) = NULL;
static void* (*r_realloc)(void*, size_t) = NULL;
static void  (*r_free)(void*) = NULL;

typedef struct { void* p; size_t n; size_t u; void* ra; int canaried; } entry;
static entry* tab = NULL;
static size_t used = 0;
static int probing = 1;
static int hits = 0;
static __thread int busy = 0;
static const char* only = NULL;

#define RA ((void*)__builtin_return_address(0))

/* Module filter: only canary allocations whose caller is in a module whose
 * path contains CANARY_ONLY (empty = all modules). Registration happens
 * regardless of the filter; only the canary itself is filtered. */
static int wanted(void* ra)
{
    Dl_info a;

    if (!only || !*only)
    {
        return 1;
    }
    if (!ra || !dladdr(ra, &a) || !a.dli_fname)
    {
        return 0;
    }

    return strstr(a.dli_fname, only) != NULL;
}

static void emit(const char* what, void* p, size_t n, void* ra, void* fra)
{
    Dl_info a, f;
    long ao = 0, fo = 0;
    const char* an = "?";
    const char* fn = "?";
    char buf[512];
    int k;

    if (ra && dladdr(ra, &a) && a.dli_fbase)
    {
        an = a.dli_fname ? a.dli_fname : "?";
        ao = (long)((char*)ra - (char*)a.dli_fbase);
    }
    if (fra && dladdr(fra, &f) && f.dli_fbase)
    {
        fn = f.dli_fname ? f.dli_fname : "?";
        fo = (long)((char*)fra - (char*)f.dli_fbase);
    }

    k = snprintf(buf, sizeof buf,
        "CANARY-HIT %s ptr=%p size=%zu alloc=%s+0x%lx free=%s+0x%lx\n",
        what, p, n, an, ao, fn, fo);
    if (k > 0)
    {
        ssize_t w = write(2, buf, (size_t)k);
        (void)w;
    }
}

static void tableInit(void)
{
    if (tab)
    {
        return;
    }

    tab = (entry*) r_malloc(TABLE_CAP*sizeof(entry));
    if (tab)
    {
        memset(tab, 0, TABLE_CAP*sizeof(entry));
    }
    else
    {
        probing = 0;
    }
}

static unsigned slotOf(void* p)
{
    return (unsigned)(((uintptr_t)p >> 4) & (TABLE_CAP - 1));
}

static entry* tableFind(void* p)
{
    unsigned h, i;

    if (!tab || !p)
    {
        return NULL;
    }

    h = slotOf(p);

    for (i = 0; i < TABLE_CAP; ++i)
    {
        entry* e = &tab[(h + i) & (TABLE_CAP - 1)];
        if (!e->p)
        {
            return NULL;
        }
        if (e->p == p)
        {
            return e;
        }
    }

    return NULL;
}

/* Register the current occupant of an address. Always called for in-range
 * allocations, so an entry can never keep a size from a previous life of
 * that address unless the address is now held by an out-of-range block -
 * and that case is rejected at free time via the usable-size check. */
static void tablePut(void* p, size_t n, size_t u, void* ra, int canary)
{
    unsigned h, i;

    if (!tab || !p)
    {
        return;
    }
    if (used >= MAX_INSERTS)
    {
        /* Bounded: report once and stop probing rather than degrade */
        if (probing && used == MAX_INSERTS)
        {
            emit("table-full-stopping", NULL, 0, NULL, NULL);
        }
        probing = 0;
        return;
    }

    h = slotOf(p);

    for (i = 0; i < TABLE_CAP; ++i)
    {
        entry* e = &tab[(h + i) & (TABLE_CAP - 1)];
        if (!e->p || e->p == p)
        {
            if (!e->p)
            {
                ++used;
            }
            e->p = p;
            e->n = n;
            e->u = u;
            e->ra = ra;
            e->canaried = canary;
            return;
        }
    }

    probing = 0;
}

/* The canary guards the end of the usable size - the last bytes before the
 * next chunk's header. */
static size_t canaryOffset(void* p, size_t n)
{
    const size_t u = malloc_usable_size(p);

    return (u > n + 8) ? u - 8 : n;
}

static void canaryWrite(void* p, size_t n)
{
    memcpy((char*)p + canaryOffset(p, n), &(uint64_t){CANARY},
        sizeof(uint64_t));
}

/* Validate an entry against the block actually being freed: a size change
 * (or a block that was never canaried) means this address is not the block
 * the entry describes, so there is nothing to check. */
static void canaryCheck(entry* e, void* q, void* fra)
{
    uint64_t v;

    if (!e->canaried || !e->p)
    {
        return;
    }
    if (malloc_usable_size(q) != e->u)
    {
        return;
    }

    memcpy(&v, (char*)q + canaryOffset(q, e->n), sizeof(uint64_t));

    if (v != CANARY)
    {
        ++hits;
        emit("overrun", q, e->n, e->ra, fra);
    }
}

static void init(void)
{
    if (r_malloc)
    {
        return;
    }

    busy = 1;
    r_malloc  = dlsym(RTLD_NEXT, "malloc");
    r_calloc  = dlsym(RTLD_NEXT, "calloc");
    r_realloc = dlsym(RTLD_NEXT, "realloc");
    r_free    = dlsym(RTLD_NEXT, "free");
    only = getenv("CANARY_ONLY");
    tableInit();
    busy = 0;
}

void* malloc(size_t n)
{
    void* p;

    if (!r_malloc)
    {
        init();
    }

    if (probing && n <= CANARY_MAX)
    {
        const int c = wanted(RA);

        p = r_malloc(n + 16);
        if (p && !busy)
        {
            if (c)
            {
                canaryWrite(p, n);
            }
            tablePut(p, n, malloc_usable_size(p), RA, c);
        }
        return p;
    }

    return r_malloc(n);
}

void* calloc(size_t nm, size_t sz)
{
    void* p;
    const size_t n = nm*sz;

    if (!r_calloc)
    {
        init();
    }

    if (probing && n <= CANARY_MAX)
    {
        const int c = wanted(RA);

        p = r_calloc(n + 16, 1);
        if (p && !busy)
        {
            if (c)
            {
                canaryWrite(p, n);
            }
            tablePut(p, n, malloc_usable_size(p), RA, c);
        }
        return p;
    }

    return r_calloc(nm, sz);
}

void* realloc(void* q, size_t n)
{
    void* p;

    if (!r_realloc)
    {
        init();
    }

    if (q && !busy)
    {
        entry* e = tableFind(q);
        if (e)
        {
            canaryCheck(e, q, RA);
        }
        /* the entry is left in place: clearing it would punch a hole in the
         * open-addressing probe chain, and the usable-size check already
         * rejects it if the address is reused by an untracked block */
    }

    if (probing && n <= CANARY_MAX)
    {
        const int c = wanted(RA);

        p = r_realloc(q, n + 16);
        if (p && !busy)
        {
            if (c)
            {
                canaryWrite(p, n);
            }
            tablePut(p, n, malloc_usable_size(p), RA, c);
        }
        return p;
    }

    return r_realloc(q, n);
}

void free(void* q)
{
    if (!r_free)
    {
        init();
    }

    if (q && !busy && tab)
    {
        entry* e = tableFind(q);

        if (e)
        {
            canaryCheck(e, q, RA);
        }
    }

    r_free(q);
}
