# 056 — 037 堆破坏：写入点定位（模块二分 + 内存诊断）

- 状态：**done**（2026-09-15：根因确认＝求解器库被映射两次，非我们代码的
  越界写；防线与诊断脚本已入库）
- 优先级：P1
- 依赖：046/054（并行正确性已修，038/037 报告）
- 预估规模：1–2 天（含 VM 机时）
- 来源：Kairos 侧对 037 的质询（要求栈证据与复现条件）

## 1. 结论（TL;DR）

**崩溃的原因不是任何一处越界写，而是求解器库在运行期被映射了两次。**

bundle 的 `$FOAM_LIBBIN` 与我们手工构建安装的 `$FOAM_USER_LIBBIN` 里
各有一份**实体** `libmoldingFoam.so`。case 的 `libs ("libmoldingFoam.so")`
加载前者、`solver moldingFoam` 的名字查找（`libmoldingFoamSolver.so`）
打开另一份，于是同一个库被 `dlopen` 两次：每个 runtime selection table
都收到 `Duplicate entry`（实测 152 行，涵盖 `moldingFoam`、10 个
`fvPatchField`、2 个 `fvModel`），两套静态对象在退出阶段互相踩 →
`malloc_consolidate(): unaligned fastbin chunk detected` → rc=134。

**只保留一份实体文件（另一份改为 `<名字>Solver.so` 符号链接）即恢复
rc=0。** 判据是"库被映射几次"，与求解器算什么都不相关。

## 2. 证据（同 VM、同 case、同库内容，只改布局）

| # | 配置 | 运行 | rc | Duplicate entry | glibc 损坏 |
|---|------|------|----|-----------------|-----------|
| A | `$FOAM_LIBBIN` + `$FOAM_USER_LIBBIN` 各一份实体 | np4 | **134** | 152 | 4 |
| B | 只留我们构建的一份 + 同名符号链接 | np4 | **0** | 0 | 0 |
| C | 只留 bundle 自带的一对（`.so` + 符号链接） | np4 | **0** | 0 | 0 |
| D | 两份实体 | **串行** | **0** | 0 | 0 |
| E | 清理回单份 | np4 | **0** | 0 | 0 |

- C 同时证明 **037 的符号链接修复本身有效**（bundle 自带的一对不会双载）；
  出问题的是"两处各有一份实体"这种混装；
- D 说明**串行不复现**——这也是它长期没被开发期发现的原因，与 037 报的
  "仅 np4"边界一致（机制未进一步追：np4 下才走第二次 `dlOpen`）；
- 崩溃栈与 037 一致（`free` ← `__run_exit_handlers` ← `UPstream::exit`
  ← `argList::~argList`），输出完整且与干净运行逐位一致——双载不影响物理
  结果，只影响退出。

## 3. 复现器与工具（已入库）

- `scripts/diag/repro-037.sh`：**19 步**（`endTime 0.002`）即复现，单次约
  20–30 s（原先要跑满 5,069 步约 38 min）。内置 10 个变体（基线/布局旋钮/
  各模块金丝雀），每个变体在 `mktemp` 风格的新鲜副本里跑；这套"最小复现 +
  单变量对照"的手法已提炼为常备手册 `ai-docs/diagnostics.md`；
- `scripts/diag/malloc-canary.c`：有界 LD_PRELOAD 金丝雀探针（见 §4）；
- `scripts/diag/check-lib-duplication.sh`：**本任务的防线**——列出加载路径上
  所有 `libmoldingFoam*.so` 并按 `realpath` 判重（>1 实体即 FAIL，退出码 1），
  也可扫描运行日志的 `Duplicate entry`/glibc 损坏行。

**kairos VM 收尾状态**（本轮把库布局改成单份，VM 现处于可复现"干净"的
配置）：`$FOAM_USER_LIBBIN/libmoldingFoam.so`（手工构建的当前源码）+
`libmoldingFoamSolver.so` 符号链接；bundle 自带的那份原地改名为
`$FOAM_LIBBIN/libmoldingFoam.so.disabled-bundle`（文件保留，改回名字即恢复
混装状态）。中途 VM 自行重启过一次（`/tmp` 被清空、早期日志丢失）。

## 4. 已排除的假设（勿重复）

| 假设 | 结论 |
|------|------|
| 我们代码越界写小块 | 金丝雀（`CANARY_ONLY=libmoldingFoam`）零命中 |
| libPstream / libOpenFOAM / libopen-pal / libc / libmpi 的分配被越界 | 逐个过滤零命中 |
| 质量预算路径（`massBudget`） | 关掉仍崩（且 152 行重复注册与之无关） |
| 分配器/布局：`MALLOC_ARENA_MAX=1`、`MALLOC_PERTURB_`、`btl self,tcp` | 三种都不改变崩溃 |
| bundle/OpenMPI 退出阶段的通病 | 原版教程 case（TJunction）np4 跑到 `End` 后 rc=0 |
| 长跑累积 | 19 步即崩 |
| valgrind memcheck | **ARM64 不可用**：bundle 里有它不认识的指令，直接 SIGILL |

**探针本身的两次坑（记录以免重犯）**：

1. 只对"被过滤模块"的分配登记表项 → 地址回收后被**未过滤**分配复用时表里
   残留旧尺寸，`free` 时按旧偏移读金丝雀，报出一批假 `overrun`（尺寸
   18/24/32 那一类）。修法：**所有**范围内分配都登记，且 `free` 时用
   `malloc_usable_size` 与记录值核对，不一致就不认这个表项；
2. `free` 时清空表项会在开放寻址的探测链上打洞，导致查找提前终止与表项
   计数虚高——因此**不清空**，靠"登记全部 + 尺寸核对"保证正确性。

## 5. 对 Kairos / 打包侧的要求

1. 运行期**只能有一份实体** `libmoldingFoam.so`：要么不随 bundle 发预编译
   求解器库（推荐，由构建产出），要么手工构建时**装到同一个目录**覆盖它；
   `<名字>Solver.so` 一律用**符号链接**指向该实体（037 的修法）；
2. 排查手法：`grep -c 'Duplicate entry' log.foamRun`（>0 即为双载），
   或直接跑 `scripts/diag/check-lib-duplication.sh`；
3. 037 现场若也能看到 `Duplicate entry`，则与我们这里的根因相同；若看不到，
   属**另一条**问题线，需要他们的 `mpirun --version`、`ldd`/`/proc/self/maps`
   里 `libmoldingFoam*` 的条目数与原始栈——不要与本结论互相推断。

## 6. 涉及文件

- 新增 `scripts/diag/repro-037.sh`、`scripts/diag/malloc-canary.c`、
  `scripts/diag/check-lib-duplication.sh`
- `ai-docs/report-037-heap-crash.md`（补记 2026-09-15）
- `README.md` §7「并行规则」第 3 条（库只能一份）

> 相关：054（并行的另一条线，已修）、038（Kairos 复测记录）、037（原始取证）。
