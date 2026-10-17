"""CLI-only fixture: python tests/openvr_resolution_query.py PATH_TO_HELPER.

Never supplies a valid query command, so these cases must not initialize OpenVR.
The parent can additionally test runtime errors/escaping with a controlled DLL.
"""

import datetime
import json
import subprocess
import sys


def main():
    executable = sys.argv[1]
    cases = [
        ([], ""),
        (["--preparation-id"], ""),
        (["--unknown"], ""),
        (["--help", "--unknown"], ""),
        (["--preparation-id", ""], ""),
        (["--preparation-id", "a" * 81], ""),
        (["--preparation-id", 'quote"\\\n\t'], ""),
        (["--preparation-id", "non-ascii-\u00e9"], ""),
        (["--preparation-id", "one", "--preparation-id", "two"], ""),
        (["--preparation-id", "A.z_0-9", "--unknown"], ""),
    ]
    for arguments, preparation_id in cases:
        result = subprocess.run([executable, *arguments], capture_output=True, timeout=10)
        assert result.returncode == 2, (arguments, result)
        assert len(result.stdout) < 32768, arguments
        record = json.loads(result.stdout)
        assert record["schemaVersion"] == 1
        assert record["recordType"] == "cojvr-resolution-observation"
        assert record["status"] == "unavailable"
        assert record["preparationId"] == preparation_id
        assert record["identityQualified"] is False
        assert record["applicationKeyStatus"] == "unavailable"
        assert record["recommendedWidth"] == record["recommendedHeight"] == 0
        assert record["error"] == "invalid_arguments"
        assert record["observedUtc"].endswith("Z")
        datetime.datetime.fromisoformat(record["observedUtc"].replace("Z", "+00:00"))
    result = subprocess.run([executable, "--help"], capture_output=True, timeout=10)
    assert result.returncode == 0, result
    assert b"--preparation-id" in result.stdout
    print(f"Passed {len(cases)} invalid CLI cases and help (no runtime query).")


if __name__ == "__main__":
    main()
