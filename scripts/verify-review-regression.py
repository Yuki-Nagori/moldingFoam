#!/usr/bin/env python3
#******************************************************************************
# verify-review-regression.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Bounded solver regressions for review task 063.
# Run inside OpenFOAM; cases use fresh temporary inputs and 30 s timeouts.
#******************************************************************************

import math
import os
import runpy
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent
WORK = Path(tempfile.mkdtemp(prefix="molding-review-"))


def setkey(path, key, value):
    text, n = re.subn(r"(?m)^([ \t]*"+re.escape(key)+r"[ \t]+)[^;]+;",
                      lambda m: m[1]+str(value)+";", path.read_text(), count=1)
    if n != 1:
        raise ValueError(f"missing {key}: {path}")
    path.write_text(text)


def patch(path, name, entries):
    text, n = re.subn(r"\b"+name+r"\s*\{[^{}]*\}", name+" { "+entries+" }",
                      path.read_text(), count=1)
    if n != 1:
        raise ValueError(f"missing patch {name}: {path}")
    path.write_text(text)


def prepare(template, name, end="1e-8", dt="1e-8"):
    case = WORK/name
    for folder in ("0", "constant", "system"):
        shutil.copytree(ROOT/template/folder, case/folder)
    shutil.rmtree(case/"constant/polyMesh", ignore_errors=True)
    ctrl = case/"system/controlDict"
    for key, value in (("endTime", end), ("deltaT", dt), ("writeInterval", dt),
                       ("writePrecision", "14")):
        setkey(ctrl, key, value)
    if re.search(r"(?m)^adjustTimeStep\s", ctrl.read_text()):
        setkey(ctrl, "adjustTimeStep", "off")
    return case


def run(case, name="run", expected=0, mesh=True):
    commands = [["blockMesh"], ["foamRun"]] if mesh else [["foamRun"]]
    for cmd in commands:
        log = case/("log.blockMesh" if cmd[0] == "blockMesh" else "log."+name)
        with log.open("w") as stream:
            result = subprocess.run(cmd, cwd=case, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=30)
        if cmd[0] == "blockMesh" and result.returncode:
            raise ValueError(f"blockMesh failed: {log}")
    if (result.returncode == 0) != (expected == 0):
        raise ValueError(f"unexpected rc={result.returncode}: {log}")
    text = log.read_text()
    if expected == 0 and not text.rstrip().endswith("End"):
        raise ValueError(f"run did not complete: {log}")
    return text


def final(case):
    return max((p for p in case.iterdir() if p.is_dir()
                and re.fullmatch(r"\d+(?:\.\d+)?(?:e[-+]?\d+)?", p.name)),
               key=lambda p: float(p.name))


def scalar(path):
    text = path.read_text()
    m = re.search(r"internalField\s+uniform\s+([^;]+);", text)
    if m:
        result = float(m[1])
        if not math.isfinite(result):
            raise ValueError(f"nonfinite field: {path}")
        return [result]
    m = re.search(r"internalField\s+nonuniform\s+List<scalar>\s+\d+\s*\((.*?)\)\s*;", text, re.S)
    if not m:
        raise ValueError(f"unreadable scalar field: {path}")
    values = list(map(float, m[1].split()))
    if not values or not all(map(math.isfinite, values)):
        raise ValueError(f"nonfinite/empty field: {path}")
    return values


def value(text, key):
    m = re.search(r"\b"+key+r"\s+([^;]+);", text)
    if not m:
        raise ValueError(f"missing checkpoint entry {key}")
    return m[1].strip()


def archive_failure_logs(work=WORK, destination=ROOT/"build/reviewRegression"):
    """Keep bounded raw logs in a CI-uploadable location, without copying meshes."""
    for log in work.glob("*/log.*"):
        target = destination/log.relative_to(work)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(log, target)


def main():
    # Independent two-vent seal state; the dry top must stay pressure-open.
    vents = prepare("tests/cases/cycleReset", "vents")
    setkey(vents/"constant/moldingDict", "nCycles", 1)
    setkey(vents/"constant/moldingDict", "ejectionTemperature", 50)
    for name, alpha in (("bottom", 1), ("top", 0)):
        patch(vents/"0/U", name, "type moldingVentVelocity; value uniform (0 0 0);")
        patch(vents/"0/p_rgh", name, "type moldingVentPressure; p0 1e5; value uniform 1e5;")
        patch(vents/"0/alpha.melt", name, f"type fixedValue; value uniform {alpha};")
    run(vents)
    def checkvents():
        history = (final(vents)/"moldingCycleState").read_text()
        sealed = value(history, "sealedVents")
        assert "bottom" in sealed and "top" not in sealed, sealed
        pressure = (final(vents)/"p_rgh").read_text()
        for name, expected in (("bottom", 0), ("top", 1)):
            block = re.search(r"\b"+name+r"\s*\{([^{}]*)\}", pressure)[1]
            assert float(value(block, "valueFraction").split()[-1]) == expected
    checkvents()
    setkey(vents/"system/controlDict", "startFrom", "latestTime")
    setkey(vents/"system/controlDict", "endTime", "2e-8")
    run(vents, "restart", mesh=False)
    checkvents()
    print("PASS: independent wet/dry vents survive restart")

    # Snapshot origin and cycle count must be unchanged by a mid-cycle restart.
    full = prepare("tests/cases/cycleReset", "cycle-full", "0.0005", "0.0001")
    split = prepare("tests/cases/cycleReset", "cycle-split", "0.0001", "0.0001")
    for case in (full, split):
        with (case/"constant/moldingDict").open("a") as f:
            f.write("\nmassBudget true;\nmassBudgetInterval 1;\n")
        run(case)
    before = {p.name: p.read_bytes() for p in (split/"constant/moldingInitial").iterdir()}
    setkey(split/"system/controlDict", "startFrom", "latestTime")
    setkey(split/"system/controlDict", "endTime", "0.0005")
    run(split, "restart", mesh=False)
    assert before == {p.name: p.read_bytes() for p in (split/"constant/moldingInitial").iterdir()}
    assert float(final(full).name) == float(final(split).name)
    for case in (full, split):
        assert value((final(case)/"moldingCycleState").read_text(), "cycle") == "2"
    for name in ("T", "p", "alpha.melt"):
        a, b = scalar(final(full)/name), scalar(final(split)/name)
        assert len(a) == len(b)
        assert max(abs(x-y) for x,y in zip(a,b)) < 1e-6*max(1, max(map(abs,a))), name
    for key in ("initialMass", "boundaryMass"):
        a = float(value((final(full)/"moldingMassBudget").read_text(), key))
        b = float(value((final(split)/"moldingMassBudget").read_text(), key))
        assert abs(a-b) < 1e-10, key
    print("PASS: cycle restart preserves original inputs, cycle count, fields and budget")

    # A two-step 1000-cell serial/np4 pair, not the full parallel matrix.
    verify_budget = runpy.run_path(str(ROOT/"scripts/verify-parallel-budget.py"))["verify"]
    for parallel in (False, True):
        budget = prepare("tests/cases/parallelMassBudget", f"budget-{parallel}", "2e-5", "1e-5")
        if not parallel:
            run(budget)
            verify_budget(budget/"log.run")
        else:
            env = dict(os.environ, OMPI_MCA_rmaps_base_oversubscribe="1")
            for cmd in (["blockMesh"], ["decomposePar", "-force"],
                        ["mpirun", "-np", "4", "foamRun", "-parallel"]):
                log = budget/("log."+cmd[0])
                with log.open("w") as out:
                    subprocess.run(cmd, cwd=budget, env=env, stdout=out,
                                   stderr=subprocess.STDOUT, check=True, timeout=30)
            verify_budget(budget/"log.mpirun")
    print("PASS: bounded serial/np4 mass diagnostic agreement")

    # A pre-existing closed-volume reference must not be recaptured on restart.
    void = prepare("tests/cases/voidCavitation", "void")
    setkey(void/"0/alpha.melt", "internalField", "uniform 0.99")
    ufile = void/"0/U"
    ufile.write_text(ufile.read_text().replace("totalFlowRate", "volumetricFlowRateProfile { type constant; value 1e-7; }\n        totalFlowRate", 1))
    field = (void/"0/p").read_text().replace("object      p;", "object      moldingVoidRhoRef;")
    field = field.replace("[1 -1 -2 0 0 0 0]", "[1 -3 0 0 0 0 0]")
    (void/"0/moldingVoidRhoRef").write_text(field)
    setkey(void/"0/moldingVoidRhoRef", "internalField", "uniform 500")
    run(void)
    assert all(abs(x-500) < 1e-10 for x in scalar(final(void)/"moldingVoidRhoRef"))
    setkey(void/"system/controlDict", "startFrom", "latestTime")
    setkey(void/"system/controlDict", "endTime", "2e-8")
    run(void, "restart", mesh=False)
    assert all(abs(x-500) < 1e-10 for x in scalar(final(void)/"moldingVoidRhoRef"))
    assert "volumetricFlowRateProfile" in (final(void)/"U").read_text()
    assert "runner" in (final(void)/"U").read_text()
    print("PASS: cavitation reference density and runner/profile configuration survive restart")

    invalid = prepare("validation/anisoConduction", "invalid-conductivity")
    setkey(invalid/"constant/moldingDict", "conductivityAnisotropy", 4)
    log = run(invalid, expected=1)
    assert "conductivityAnisotropy must lie" in log
    print("PASS: non-positive conductivity rejected at construction")

    # Uniform Couette shear: independent fourfold material enhancements.
    rises = []
    for mode in ("base", "crystal", "fiber"):
        exponent = 2 if mode == "crystal" else 0
        heat = prepare("validation/couette", f"heat-{mode}", "1e-5", "1e-5")
        setkey(heat/"constant/moldingDict", "viscousDissipation", "true")
        # The crystallisation source is the sole latent-heat representation
        # in this carrier; disable the hMelt apparent-Cp latent peak.
        meltProps = heat/"constant/physicalProperties.melt"
        meltProps.write_text(
            re.sub(r"(latentHeat\s+)2e5", r"\g<1>0", meltProps.read_text(), count=1)
        )
        with (heat/"constant/moldingDict").open("a") as f:
            f.write("\ncrystallization { avramiExponent 2; rateConstant 1e-30; peakTemperature 400; windowWidth 40; latentHeat 0; rho 800; }\n")
        chi = (heat/"0/T").read_text()
        chi = re.sub(r"object\s+T;", "object chi;", chi)
        chi = chi.replace("[0 0 0 1 0 0 0]", "[0 0 0 0 0 0 0]")
        (heat/"0/chi").write_text(chi)
        setkey(heat/"0/chi", "internalField", "uniform 0.5")
        schemes = heat/"system/fvSchemes"
        schemes.write_text(re.sub(r"divSchemes\s*\{", "divSchemes {\n div(phi,chi) Gauss upwind;", schemes.read_text(), count=1))
        solution = heat/"system/fvSolution"
        solution.write_text(re.sub(r"solvers\s*\{", "solvers {\n \"(chi|chiFinal)\" { solver smoothSolver; smoother symGaussSeidel; tolerance 1e-12; relTol 0; }", solution.read_text(), count=1))
        props = heat/"constant/momentumTransport"
        props.write_text(re.sub(r"CrossWlfCoeffs\s*\{", "CrossWlfCoeffs {\n crystallinity { chiInfinity 1; exponent "+str(exponent)+"; }", props.read_text(), count=1))
        for key, val in ( ("D1",100), ("A1",0),
                         ("tauStar","1e20"), ("etaMin",99.9), ("etaMax",100.1)):
            setkey(props, key, val)
        if mode == "fiber":
            with (heat/"constant/moldingDict").open("a") as f:
                f.write("\nfiberOrientation { aspectRatio 1; interactionCoefficient 0; closure hybrid; conductivityAnisotropy 0; }\n")
            afield = chi.replace("object chi;", "object a;").replace("class       volScalarField", "class       volSymmTensorField")
            (heat/"0/a").write_text(afield)
            setkey(heat/"0/a", "internalField", "uniform (0.5 0.5 0 0.5 0 0)")
            schemes.write_text(re.sub(r"divSchemes\s*\{", "divSchemes {\n div(phi,a) Gauss upwind;", schemes.read_text(), count=1))
            solution.write_text(re.sub(r"solvers\s*\{", 'solvers {\n "(a|aFinal)" { solver smoothSolver; smoother symGaussSeidel; tolerance 1e-12; relTol 0; }', solution.read_text(), count=1))
            props.write_text(re.sub(r"CrossWlfCoeffs\s*\{", "CrossWlfCoeffs {\n lipscombRatio 5;", props.read_text(), count=1))
        run(heat)
        temperature = scalar(final(heat)/"T")
        rises.append(sum(temperature)/len(temperature)-480)
    assert rises[0] > 1e-5, rises
    for name, rise in zip(("crystallinity", "orientation"), rises[1:]):
        ratio = rise/rises[0]
        assert abs(ratio-4) < .08, (rises, ratio)
        print(f"PASS: {name} multiplier enters shear heating, ratio={ratio:.6g}")
    shutil.rmtree(WORK)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        try:
            archive_failure_logs()
        except OSError as error:
            print(f"Could not archive logs: {error}")
        print(f"FAIL: retained regression inputs and logs at {WORK}")
        raise
