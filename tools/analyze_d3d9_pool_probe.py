"""Summarize raw classic-D3D9 resource creation requests without inferring residency."""

import argparse
import collections
import csv
import json
from pathlib import Path

KINDS = (
    "CreateTexture", "CreateCubeTexture", "CreateVolumeTexture",
    "CreateVertexBuffer", "CreateIndexBuffer",
)
POOLS = {0: "DEFAULT", 1: "MANAGED", 2: "SYSTEMMEM", 3: "SCRATCH"}
FORMATS = {
    0: "UNKNOWN", 21: "A8R8G8B8", 22: "X8R8G8B8", 23: "R5G6B5",
    26: "A4R4G4B4", 50: "L8", 75: "D24S8", 80: "D16",
    101: "INDEX16", 102: "INDEX32",
}
USAGE = {
    0x00000001: "RENDERTARGET", 0x00000002: "DEPTHSTENCIL",
    0x00000008: "WRITEONLY", 0x00000010: "SOFTWAREPROCESSING",
    0x00000020: "DONOTCLIP", 0x00000040: "POINTS",
    0x00000080: "RTPATCHES", 0x00000100: "NPATCHES",
    0x00000200: "DYNAMIC", 0x00000400: "AUTOGENMIPMAP",
    0x00004000: "DMAP",
}


def format_name(value):
    if value in FORMATS:
        return FORMATS[value]
    raw = value.to_bytes(4, "little")
    if all(32 <= byte < 127 for byte in raw):
        return raw.decode("ascii")
    return f"0x{value:08X}"


def usage_name(value):
    names = [name for bit, name in USAGE.items() if value & bit]
    remaining = value & ~sum(USAGE)
    if remaining:
        names.append(f"UNKNOWN(0x{remaining:08X})")
    return "|".join(names) if names else "0"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture", type=Path)
    parser.add_argument("--run-manifest", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    with args.capture.open(newline="", encoding="utf-8") as stream:
        rows = list(csv.DictReader(stream))
    expected = set(KINDS) | {"FactoryHook", "DeviceHook"}
    if not rows or any(row["kind"] not in expected for row in rows):
        raise SystemExit("Capture is empty or contains an unknown record type")
    hooks = [row for row in rows if row["kind"] in {"FactoryHook", "DeviceHook"}]
    resources = [row for row in rows if row["kind"] in KINDS]
    if not hooks or not any(row["kind"] == "DeviceHook" for row in hooks):
        raise SystemExit("No successful device-hook coverage evidence")
    if any(int(row["hr"], 16) not in (0, 1) for row in hooks):
        raise SystemExit("A factory or device hook failed; resource census is incomplete")

    run_id = "unattributed"
    if args.run_manifest:
        manifest = json.loads(args.run_manifest.read_text(encoding="utf-8-sig"))
        if manifest.get("diagnosticMode") != "d3d9_pool_probe":
            raise SystemExit("Run manifest is not a pool-probe run")
        run_id = manifest["runId"]

    counts = collections.Counter((row["kind"], int(row["pool"])) for row in resources)
    managed = [row for row in resources if int(row["pool"]) == 1]
    failed = [row for row in resources if int(row["hr"], 16) & 0x80000000]
    managed_success = [row for row in managed if not int(row["hr"], 16) & 0x80000000]
    managed_signatures = collections.Counter(
        (row["kind"], format_name(int(row["format"], 16)),
         usage_name(int(row["usage"], 16))) for row in managed
    )
    managed_shapes = collections.Counter(
        (row["kind"], format_name(int(row["format"], 16)),
         f"{row['length']} bytes" if row["kind"] in {"CreateVertexBuffer", "CreateIndexBuffer"}
         else f"{row['width']}x{row['height']}x{row['depth']}, levels={row['levels']}",
         usage_name(int(row["usage"], 16))) for row in managed
    )
    lines = [
        "# Call of Juarez — D3D9 resource pool census",
        "",
        f"Run: `{run_id}`. Capture: `{args.capture.name}`.",
        f"Factory hooks: {sum(row['kind'] == 'FactoryHook' for row in hooks)}; "
        f"device hooks: {sum(row['kind'] == 'DeviceHook' for row in hooks)}.",
        "",
        "| Resource | DEFAULT | MANAGED | SYSTEMMEM | SCRATCH | Other |",
        "|---|---:|---:|---:|---:|---:|",
    ]
    for kind in KINDS:
        values = [counts[kind, pool] for pool in range(4)]
        other = sum(n for (name, pool), n in counts.items() if name == kind and pool not in range(4))
        lines.append(f"| {kind} | {' | '.join(map(str, values))} | {other} |")
    totals = [sum(counts[kind, pool] for kind in KINDS) for pool in range(4)]
    other = len(resources) - sum(totals)
    lines.extend([
        f"| **Total** | {' | '.join(map(str, totals))} | {other} |",
        "",
        f"Observed creation calls: **{len(resources)}**; MANAGED: **{len(managed)}** "
        f"({len(managed) / len(resources):.1%})" if resources else "Observed creation calls: 0.",
        f"Failed calls (HRESULT high bit): **{len(failed)}**.",
        f"Successful MANAGED creations: **{len(managed_success)}**.",
        "",
        "## Most frequent MANAGED requests",
        "",
        "| Resource | Format | Usage flags | Calls |",
        "|---|---|---|---:|",
    ])
    for (kind, fmt, usage), count in managed_signatures.most_common(20):
        lines.append(f"| {kind} | {fmt} | {usage} | {count} |")
    lines.extend([
        "",
        "## Most frequent MANAGED sizes",
        "",
        "| Resource | Format | Requested size | Usage flags | Calls |",
        "|---|---|---|---|---:|",
    ])
    for (kind, fmt, size, usage), count in managed_shapes.most_common(30):
        lines.append(f"| {kind} | {fmt} | {size} | {usage} | {count} |")
    lines.extend([
        "",
        "## Interpretation limits",
        "",
        "This census counts creation requests, including failed calls and repeats. "
        "It does not measure live allocations, peak GPU memory, upload traffic, "
        "lock/unlock behavior, reset survival or device-loss recovery. "
        "Dimensions/length, levels, raw format, usage, FVF and shared-handle presence "
        "are preserved per call in the CSV. Buffer formats are UNKNOWN where D3D9 "
        "does not accept a format parameter. No resources or call arguments were changed.",
        "",
    ])
    report = "\n".join(lines)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        print(report)


if __name__ == "__main__":
    main()
