#!/usr/bin/env python3
import os, re, sys
def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    log = open(os.path.join(case_dir, "log.foamRun"), errors="replace").read()
    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time"); sys.exit(1)
    samples = [(float(a), float(b), float(c)) for a,b,c in re.findall(
        r"moldingCoolantFluid: net boundary heat = ([-+0-9.eE]+) W, outlet mdot = ([-+0-9.eE]+) kg/s, outlet bulk T = ([-+0-9.eE]+) K", log)]
    if not samples:
        print("FAIL: no coolant samples"); sys.exit(1)
    q, mdot, Tb = samples[-1]
    enthalpy = mdot*4182.0*(Tb - 300.0)
    err = abs(q - enthalpy)/max(abs(q), 1e-12)
    print("  net boundary heat = {:.4f} W, outlet bulk T = {:.4f} K".format(q, Tb))
    print("  energy balance relative error = {:.3e}".format(err))
    if err > 1e-2 or not (301.0 < Tb < 399.0):
        print("FAIL: the CHT water energy balance or temperature is off"); sys.exit(1)
    print("PASS: the multi-region water channel conserves energy")
if __name__ == "__main__":
    main()
