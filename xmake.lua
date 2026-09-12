--******************************************************************************
--  moldingFoam build orchestration
--
--  moldingFoam is free software: you can redistribute it and/or modify it
--  under the terms of the GNU General Public License as published by the
--  Free Software Foundation, either version 3 of the License, or (at your
--  option) any later version. See the COPYING file for details.
--******************************************************************************

--******************************************************************************
--  moldingFoam build orchestration
--
--  The OpenFOAM-14 dependency is resolved in priority order:
--
--      1. --of_src=<path>       an already-built OpenFOAM-14 environment
--                               tree (root contains etc/bashrc), e.g. a
--                               tree built from source per openfoam.org
--      2. apt binary (default)  the official Foundation binary package
--                               `openfoam14` at /opt/openfoam14 (Ubuntu,
--                               amd64 + arm64)
--
--  libmoldingFoam.so and the model tests are ALWAYS compiled with the
--  upstream wmake against the selected environment, so flags and ABI
--  stay identical to upstream.
--
--  Usage:
--      xmake                          resolve OpenFOAM (install if needed)
--                                     + build libmoldingFoam
--      xmake run case-contract        mesh, run and verify the contract
--                                     case (serial; see below)
--      xmake run test                 run the model tests
--      xmake run bundle               assemble the self-contained
--                                     distribution archive
--
--  Options:
--      xmake f --of_src=<path>        reuse an already-built tree
--      MOLDINGFOAM_PARALLEL=<n>       number of processor subdomains for
--                                     `xmake run case-contract`
--******************************************************************************

option("of_src")
    set_default("")
    set_showmenu(true)
    set_description("Path to an already-built OpenFOAM-14 environment tree to use instead of the apt-installed openfoam14")
option_end()

local projectdir = os.projectdir()

local of_src = get_config("of_src")
if type(of_src) ~= "string" or of_src == "" then
    of_src = os.getenv("MOLDINGFOAM_OF_SRC") or ""
end

-- OpenFOAM-14 only supports Linux on a case-sensitive filesystem
-- (README section 2); the default dependency is the official Foundation
-- binary package `openfoam14` (/opt/openfoam14 on Ubuntu, amd64 + arm64).

-- Resolve the OpenFOAM-14 environment dir (the tree whose root contains
-- etc/bashrc): the configured of_src override, else the apt-installed
-- official binary.
--
-- Root-scope closures run with a restricted sandbox os (no execv/raise),
-- so this helper only returns data; the callbacks raise and execute.
local function openfoam_envdir()
    if of_src ~= "" then
        if not os.isfile(path.join(of_src, "etc", "bashrc")) then
            return nil, "of_src does not look like an OpenFOAM-14 "
                .. "environment tree: " .. of_src
        end
        return of_src, nil
    end
    local aptdir = "/opt/openfoam14"
    if os.isfile(path.join(aptdir, "etc", "bashrc")) then
        return aptdir, nil
    end
    return nil, [[
[moldingFoam] no OpenFOAM-14 environment found.
Fast path (Ubuntu 24.04, official Foundation binary, ~2 min):
  curl -fsSL https://dl.openfoam.org/gpg.key | sudo tee /etc/apt/trusted.gpg.d/openfoam.asc >/dev/null
  sudo add-apt-repository http://dl.openfoam.org/ubuntu
  sudo apt update && sudo apt install openfoam14
Alternatives: on other Linux distributions, build an OpenFOAM-14
environment tree from source per openfoam.org and point
`xmake f --of_src=<path>` at it. See README sections 2-3.]]
end

-- Run a script inside the OpenFOAM environment (etc/bashrc sourced);
-- execv bypasses the outer shell so $-expressions in the script are
-- evaluated by the inner bash only. NOTE: this helper must be defined
-- inside each callback body — root-scope closures get a sandbox os
-- without execv.

target("moldingFoam")
    set_kind("phony")
    set_default(true)
    on_build(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print(string.format(
            "[moldingFoam] building libmoldingFoam.so with wmake (%s)",
            envdir))
        local ok = in_of_env(envdir, string.format([[
set -e
mkdir -p "$FOAM_USER_LIBBIN" "$FOAM_USER_APPBIN"
export WM_NCOMPPROCS=$(nproc)

# Compiler shim: the wmake Arm64 rules emit -mcpu=native, which bakes
# the BUILD machine's CPU features (e.g. SVE auto-vectorisation on CI
# Arm runners) into the objects and crashes with SIGILL on recipient
# machines without them. Strip the native flags so the wmake-built
# artifacts stay baseline ARMv8-A portable (x86 builds never see these
# flags and pass through unchanged).
mkdir -p "$HOME/.moldingFoam-bin"
cat > "$HOME/.moldingFoam-bin/g++" <<'SHIM'
#!/bin/bash
args=()
for a in "$@"
do
    case "$a" in
        -mcpu=native|-march=native|-mtune=native) ;;
        *) args+=("$a") ;;
    esac
done
exec /usr/bin/g++ "${args[@]}"
SHIM
chmod +x "$HOME/.moldingFoam-bin/g++"
export PATH="$HOME/.moldingFoam-bin:$PATH"

cd %s
wmake libso src
wmake tests
ln -sf libmoldingFoam.so "$FOAM_USER_LIBBIN/libmoldingFoamSolver.so"
]], projectdir))
        if ok ~= 0 then
            os.raise("building libmoldingFoam.so failed; see the log above")
        end
    end)
target_end()

target("test")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && modelTests")
        if ok ~= 0 then
            os.raise("model tests failed")
        end
    end)
target_end()

target("test-solver")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the fast solver feature cases")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-solver-tests.sh")
            .. " tests/cases")
        if ok ~= 0 then
            os.raise("solver feature cases failed")
        end
    end)
target_end()

target("case-contract")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        -- Parallel subdomains: MOLDINGFOAM_PARALLEL=<n> env, or the first
        -- positional argument after `--`
        local nprocs = os.getenv("MOLDINGFOAM_PARALLEL") or "1"
        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print(string.format(
            "[moldingFoam] running the contract case with %s subdomain(s)",
            nprocs))
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-case.sh")
            .. " case-contract " .. nprocs)
        if ok ~= 0 then
            os.raise("contract case run failed; see case-contract/log.foamRun")
        end
    end)
target_end()

target("highPressure")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the 40 MPa packing case")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-case.sh")
            .. " validation/highPressure 1")
        if ok ~= 0 then
            os.raise("high-pressure case failed; see"
                .. " validation/highPressure/log.foamRun")
        end
    end)
target_end()

target("couette")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the Couette shear-heating validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-validation.sh")
            .. " validation/couette")
        if ok ~= 0 then
            os.raise("Couette validation failed; see "
                .. "validation/couette/log.foamRun")
        end
    end)
target_end()

target("couetteSlip")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the Couette Navier-slip validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-validation.sh")
            .. " validation/couetteSlip")
        if ok ~= 0 then
            os.raise("Couette slip validation failed; see "
                .. "validation/couetteSlip/log.foamRun")
        end
    end)
target_end()

target("coolantMold")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the mould cooling-channel validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-moldcht.sh")
            .. " validation/coolantMold")
        if ok ~= 0 then
            os.raise("coolantMold validation failed; see "
                .. "validation/coolantMold/log.foamRun")
        end
    end)
target_end()

target("moldCHT")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        for _, case in ipairs({"moldCHT", "moldCHT-fill", "moldCHT-cycle", "moldCHT-cooled", "moldCHT-lumped", "coolantWaterMold"}) do
            print("[moldingFoam] running the two-region CHT validation: "
                .. case)
            local ok = in_of_env(envdir,
                "cd " .. projectdir .. " && "
                .. path.join(projectdir, "scripts", "run-moldcht.sh")
                .. " validation/" .. case)
            if ok ~= 0 then
                os.raise("CHT validation failed; see validation/" .. case
                    .. "/log.foamRun")
            end
        end
    end)
target_end()

target("coolantWater")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the 3D coolant-flow validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-validation.sh")
            .. " validation/coolantWater")
        if ok ~= 0 then
            os.raise("coolantWater validation failed; see "
                .. "validation/coolantWater/log.foamRun")
        end
    end)
target_end()

target("warpageAniso")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the anisotropic shrinkage warpage "
            .. "validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-validation.sh")
            .. " validation/warpageAniso")
        if ok ~= 0 then
            os.raise("warpageAniso validation failed; see "
                .. "validation/warpageAniso/log.foamRun")
        end
    end)
target_end()

target("warpagePlate")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the shrinkage warpage plate validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-validation.sh")
            .. " validation/warpagePlate")
        if ok ~= 0 then
            os.raise("warpagePlate validation failed; see "
                .. "validation/warpagePlate/log.foamRun")
        end
    end)
target_end()

target("thermoelastic")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the thermoelastic cantilever validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-validation.sh")
            .. " validation/thermoelastic")
        if ok ~= 0 then
            os.raise("thermoelastic validation failed; see "
                .. "validation/thermoelastic/log.foamRun")
        end
    end)
target_end()

target("stefan")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        print("[moldingFoam] running the Stefan solidification validation")
        local ok = in_of_env(envdir,
            "cd " .. projectdir .. " && "
            .. path.join(projectdir, "scripts", "run-validation.sh")
            .. " validation/stefan")
        if ok ~= 0 then
            os.raise("Stefan validation failed; see "
                .. "validation/stefan/log.foamRun")
        end
    end)
target_end()

-- Assemble a self-contained distribution bundle: a complete OpenFOAM-14
-- environment tree with the moldingFoam products merged into its platform
-- dirs, compressed into build/moldingFoam-openfoam14-<WM_OPTIONS>-<date>.tar.xz.
-- Recipients extract it, source OpenFOAM-14/etc/bashrc and run - no apt,
-- no download. See MOLDINGFOAM-BUNDLE.md inside the archive for the
-- usage and GPL-3.0 source pointers. Run with `xmake run bundle`.
target("bundle")
    set_kind("phony")
    add_deps("moldingFoam")
    on_run(function (target)
        local function in_of_env(envdir, script)
            return os.execv("/bin/bash", {"-c",
                "source " .. path.join(envdir, "etc", "bashrc")
                .. " && " .. script})
        end
        local function in_of_env_out(envdir, script)
            local outfile = path.join(projectdir, ".xmake", "bundle-env.txt")
            in_of_env(envdir, "mkdir -p '" .. path.join(projectdir, ".xmake")
                .. "'; " .. script .. " > '" .. outfile .. "' 2>&1")
            local out = io.readfile(outfile)
            return (out and out:gsub("%s+$", "")) or ""
        end

        local envdir, err = openfoam_envdir()
        if envdir == nil then
            os.raise(err)
        end
        local wmo = in_of_env_out(envdir, "echo -n $WM_OPTIONS")
        if wmo == "" then
            os.raise("cannot resolve WM_OPTIONS from " .. envdir)
        end
        local userlib = in_of_env_out(envdir, "echo -n $FOAM_USER_LIBBIN")
        local userbin = in_of_env_out(envdir, "echo -n $FOAM_USER_APPBIN")

        -- stage a full copy of the environment tree (cp -a keeps the
        -- lnInclude/lib symlink farms intact)
        local stagedir = path.join(projectdir, "build", "bundle")
        os.rm(stagedir)
        os.exec(string.format("mkdir -p '%s'", stagedir))
        print("[moldingFoam] staging the OpenFOAM-14 environment tree ...")
        os.exec(string.format("cp -a '%s' '%s'",
            envdir, path.join(stagedir, "openfoam14")))
        local treedir = path.join(stagedir, "openfoam14")

        -- the deb hard-codes FOAM_INST_DIR=/opt; restore the upstream
        -- location-independent derivation so the tree works anywhere
        local bashrc = path.join(treedir, "etc", "bashrc")
        local bashrc_content = io.readfile(bashrc)
        io.writefile(bashrc, (bashrc_content:gsub(
            "export FOAM_INST_DIR=/opt",
            "export FOAM_INST_DIR=$(cd $(dirname $bashrcFile)/../.. && pwd -P)")))

        -- merge the moldingFoam products into the tree: FOAM_LIBBIN and
        -- the platform bin dir are on LD_LIBRARY_PATH/PATH via bashrc.
        --
        -- Defensive: the staged environment tree may carry stale
        -- libmoldingFoam*.so artifacts from an earlier installation (the
        -- release v0.2.0 did, and the stale solver-name copy both broke
        -- the run - a different build with unsupported instructions - and
        -- made foamRun load the module twice, corrupting the heap at
        -- exit). Remove every stale copy, install the freshly built
        -- libmoldingFoam.so and provide libmoldingFoamSolver.so as a
        -- symlink (the module-discovery name of the local build), so
        -- exactly one inode is ever loaded
        local libdir = string.format("%s/platforms/%s/lib/", treedir, wmo)
        os.exec(string.format(
            "rm -f '%s'libmoldingFoam*.so", libdir))
        os.cp(string.format("%s/libmoldingFoam.so", userlib), libdir)
        os.exec(string.format(
            "ln -sf libmoldingFoam.so '%s'libmoldingFoamSolver.so", libdir))

        -- The two names must share one inode: packaging bug guard
        local ino1 = in_of_env_out(envdir, string.format(
            "stat -c %%i '%s'libmoldingFoam.so", libdir))
        local ino2 = in_of_env_out(envdir, string.format(
            "stat -c %%i '%s'libmoldingFoamSolver.so", libdir))
        if ino1 ~= ino2 or ino1 == "" then
            os.raise("bundle lib check failed: libmoldingFoam*.so are not "
                .. "the same file (inodes " .. ino1 .. " vs " .. ino2 .. ")")
        end

        os.cp(string.format("%s/modelTests", userbin),
            string.format("%s/platforms/%s/bin/", treedir, wmo))

        io.writefile(path.join(treedir, "MOLDINGFOAM-BUNDLE.md"), [=[
# moldingFoam bundle

A self-contained injection-molding simulation environment:
the solver module `moldingFoam` (this project, GPL-3.0) merged into a
complete OpenFOAM-14 runtime environment (OpenFOAM Foundation
`openfoam14` package contents, GPL-3.0).

## Usage

    tar -xJf moldingFoam-openfoam14-*.tar.xz
    . openfoam14/etc/bashrc
    modelTests                       # self-check

Run a case (system/controlDict):

    application     foamRun;
    solver          moldingFoam;
    libs            ("libmoldingFoam.so");

`libmoldingFoam.so` already sits in the tree's platform lib dir, so no
extra paths are needed.

## Requirements

- Linux on the same architecture the bundle was built for (see the
  WM_OPTIONS suffix of the archive name, e.g. linuxArm64GccDPInt32Opt);
- the OpenMPI runtime library: `sudo apt install -y libopenmpi3`
  (already present on most systems).

## License / source

OpenFOAM-14 (c) OpenFOAM Foundation, GPL-3.0, unmodified upstream build
except one line in etc/bashrc restoring the upstream location-independent
FOAM_INST_DIR derivation. Source: https://openfoam.org/version-14/
moldingFoam (c) the moldingFoam authors, GPL-3.0. Source: the moldingFoam
project repository.
]=])

        local name = "moldingFoam-openfoam14-" .. wmo .. "-"
            .. os.date("%Y%m%d")
        local tarball = path.join(projectdir, "build", name .. ".tar.xz")
        os.rm(tarball)
        print("[moldingFoam] compressing the bundle (xz, a few minutes) ...")
        if in_of_env(envdir, "export XZ_OPT='-T0'; tar -cJf '" .. tarball
            .. "' -C '" .. stagedir .. "' openfoam14") ~= 0 then
            os.raise("compressing the bundle failed")
        end
        os.exec(string.format("du -sh '%s'", tarball))
        print("[moldingFoam] bundle ready: " .. tarball)
    end)
target_end()
