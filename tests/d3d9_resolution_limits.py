"""CLI contract: python tests/d3d9_resolution_limits.py PATH_TO_HELPER [--invalid-only].

Invalid CLI cases require no D3D9 query. Hardware cases accept unavailable;
caps compatibility never requires an allocation or qualifies actual rendering.
"""

import json
from pathlib import Path
import subprocess
import sys


FORMATS = {"X8R8G8B8", "A8R8G8B8", "A16B16G16R16F", "D16", "D24S8"}


def invoke(executable, arguments):
    result = subprocess.run([executable, *arguments], capture_output=True, timeout=15)
    assert len(result.stdout) < 4096, arguments
    assert not result.stderr, (arguments, result.stderr)
    record = json.loads(result.stdout)
    assert record["schemaVersion"] == 1
    assert record["recordType"] == "cojvr-native-resolution-limits"
    assert record["adapterIndex"] == 0
    for key in ("allocationValidated", "memoryQualified", "theaterQualified"):
        assert record[key] is False, (key, record)
    for key in ("requestedWidth", "requestedHeight", "maxTextureWidth",
                "maxTextureHeight", "maxAspectRatio"):
        assert type(record[key]) is int and 0 <= record[key] <= 0xFFFFFFFF, record
    assert set(record["formatChecks"]) == FORMATS, record
    assert all(value is None or type(value) is bool
               for value in record["formatChecks"].values()), record
    assert isinstance(record["error"], str) and len(record["error"]) < 128
    assert record["status"] in {"caps_compatible", "incompatible", "unavailable"}
    assert result.returncode == (0 if record["status"] == "caps_compatible" else 2)
    return record


def main():
    executable = str(Path(sys.argv[1]).resolve())
    assert Path(executable).is_file(), "Resolution limits helper has not been built"
    cases = [[], ["--help"], ["--width"], ["--width", "641"],
             ["--height", "481"], ["--width", "641", "--width", "481"],
             ["--height", "641", "--height", "481"],
             ["--width", "641", "--height", "481", "--adapter", "0"],
             ["--unknown", "641", "--height", "481"]]
    for value in ("", "0", "-1", "+1", "true", "false", "True", "1.0", "1e2",
                  " 1", "1 ", "1x", "0x100", "2147483648", "4294967296",
                  "9" * 1000, "\u0661", "\uff11"):
        cases += [["--width", value, "--height", "481"],
                  ["--width", "641", "--height", value]]
    for arguments in cases:
        record = invoke(executable, arguments)
        assert record["status"] == "unavailable", (arguments, record)
        assert record["error"] == "invalid_arguments", (arguments, record)
        assert record["requestedWidth"] == record["requestedHeight"] == 0
        assert record["maxTextureWidth"] == record["maxTextureHeight"] == 0
        assert record["maxAspectRatio"] == 0
        assert all(value is None for value in record["formatChecks"].values())
    print(f"Passed {len(cases)} invalid CLI cases (no caps query).")
    if "--invalid-only" in sys.argv[2:]:
        return
    available = 0
    for width, height in ((641, 481), (481, 641), (1, 1),
                          (2147483647, 481), (641, 2147483647)):
        record = invoke(executable, ["--height", str(height), "--width", str(width)])
        assert (record["requestedWidth"], record["requestedHeight"]) == (width, height)
        if record["status"] == "unavailable":
            assert record["error"], record
            continue
        available += 1
        assert record["maxTextureWidth"] > 0 and record["maxTextureHeight"] > 0
        assert all(type(value) is bool for value in record["formatChecks"].values())
        if width > record["maxTextureWidth"] or height > record["maxTextureHeight"]:
            assert record["status"] == "incompatible", record
        if record["status"] == "caps_compatible":
            assert width <= record["maxTextureWidth"]
            assert height <= record["maxTextureHeight"]
            aspect = record["maxAspectRatio"]
            assert not aspect or max(width, height) <= min(width, height) * aspect
            assert all(record["formatChecks"].values()) and record["error"] == ""
        else:
            assert record["error"], record
    print(f"Queried 5 extents; {available} caps results, {5 - available} unavailable; no allocation.")


if __name__ == "__main__":
    main()
