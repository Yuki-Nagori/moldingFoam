#!/usr/bin/env python3
"""Validate a shared Cross-WLF/Tait material contract; no formula copy."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import json
import math
from pathlib import Path
import sys

REQUIRED = {
    "crossWLF": ("n", "tauStar", "D1", "D2", "D3", "A1", "A2"),
    "tait": ("b1m", "b2m", "b1s", "b2s", "b3", "b3s", "b4", "b4s",
             "b5", "b6", "C", "smoothBand"),
}


class MaterialError(ValueError):
    pass


def validate(data):
    if not isinstance(data, dict) or set(data) != {"modelVersion", "crossWLF", "tait"}:
        raise MaterialError("material: required modelVersion, crossWLF and tait objects")
    if not isinstance(data["modelVersion"], str) or not data["modelVersion"]:
        raise MaterialError("modelVersion: expected non-empty shared model identifier")
    for section, keys in REQUIRED.items():
        values = data[section]
        if not isinstance(values, dict) or set(values) != set(keys):
            raise MaterialError(f"{section}: exact required keys are {', '.join(keys)}")
        for key in keys:
            value = values[key]
            if type(value) not in (int, float) or not math.isfinite(value):
                raise MaterialError(f"{section}.{key}: expected finite number")
    if data["crossWLF"]["D1"] <= 0 or data["crossWLF"]["tauStar"] <= 0:
        raise MaterialError("crossWLF: D1 and tauStar must be positive")
    if data["tait"]["smoothBand"] < 0:
        raise MaterialError("tait.smoothBand: must be nonnegative")
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    args = parser.parse_args()
    try:
        with args.input.open(encoding="utf-8") as stream:
            data = validate(json.load(stream))
        print(json.dumps({"status": "material_contract_valid",
                          "modelVersion": data["modelVersion"],
                          "formulaSource": "shared-CrossWlf-Tait", "evaluated": False},
                         sort_keys=True))
        return 0
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"{args.input}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
