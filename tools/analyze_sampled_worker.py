"""Read-only verification of a collected source/worker COPY audit, never DLSS admission."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def _records(text, prefix):
    records = []
    for line in text.splitlines():
        if not line.startswith(prefix + ": "):
            continue
        record = {}
        for item in line[len(prefix) + 2:].split(";"):
            key, sep, value = item.partition("=")
            if sep:
                if key in record:
                    raise ValueError("duplicate telemetry field: " + key)
                record[key] = value
        records.append(record)
    return records


def _number(record, key):
    value = int(record[key], 0)
    if value <= 0:
        raise ValueError("nonpositive " + key)
    return value


def _stamp(record):
    return tuple(_number(record, key) for key in (
        "frame_sequence", "pose_sequence", "owner", "generation", "resource_epoch", "integrity_epoch", "producer_device"))


def _run_interval(text, run_id, build_id):
    """Native run_start is space-delimited; the audit rows use semicolons."""
    lines = text.splitlines()
    markers, starts, ends = [], [], []
    for index, line in enumerate(lines):
        name, sep, body = line.partition(": ")
        if not sep or name not in ("run_start", "run_end"):
            continue
        fields = {}
        for token in body.replace(";", " ").split():
            key, equals, value = token.partition("=")
            if not equals or key in fields:
                raise ValueError("malformed run boundary")
            fields[key] = value
        markers.append(index)
        if fields.get("run_id") != run_id:
            continue
        if name == "run_start":
            if fields.get("build_manifest_id") != build_id or int(fields.get("pid", "0")) <= 0:
                raise ValueError("runtime startup build/process identity mismatch")
            starts.append(index)
        else:
            ends.append(index)
    if len(starts) != 1 or len(ends) != 1 or starts[0] >= ends[0]:
        raise ValueError("missing, duplicated or unordered matching run interval")
    begin, end = starts[0], ends[0]
    if any(begin < index < end for index in markers):
        raise ValueError("nested or foreign run boundary inside matching interval")
    return "\n".join(lines[begin + 1:end])


def analyze(run: Path) -> dict:
    result = {"passed": False, "errors": [], "copied_pairs": 0, "canonical_outcome": "unknown",
              "reconstruction_admitted": False, "headset_acceptance": "not_evaluated",
              "timing_basis": "worker_cpu_wall_clock"}
    errors = result["errors"]
    try:
        root = run.resolve(strict=True)
        def read_json(name):
            return json.loads((root / name).read_text(encoding="utf-8-sig"))
        evidence = read_json("evidence-manifest.json")
        result["run_id"] = evidence["runId"]
        result["canonical_outcome"] = evidence.get("validation", {}).get("outcome", "unknown")
        if not evidence.get("runtimeStarted") or not evidence.get("runtimeEnded") or evidence.get("incomplete"):
            errors.append("runtime evidence is incomplete")
        inventory = set()
        for entry in evidence["files"]:
            path = (root / entry["path"]).resolve(strict=True)
            if not path.is_relative_to(root) or path in inventory:
                raise ValueError("foreign or duplicate evidence path")
            inventory.add(path)
            payload = path.read_bytes()
            if len(payload) != entry["size"] or hashlib.sha256(payload).hexdigest().lower() != entry["sha256"].lower():
                raise ValueError("evidence size/hash mismatch: " + entry["path"])
        required = ["runtime/cojvr.log", "run-manifest.json", "build-manifest.json", "deployment/.cojvr-d3d9-stage.json"]
        if any((root / name).resolve() not in inventory for name in required):
            raise ValueError("required runtime/deployment provenance missing from inventory")
        deployment = evidence.get("deploymentChecks", [])
        if not deployment or any(not item.get("matches") or len(item["expectedSha256"]) != 64 or
                                len(item["actualSha256"]) != 64 or
                                item["expectedSha256"].lower() != item["actualSha256"].lower() for item in deployment):
            errors.append("deployment hashes do not match")
        manifest, build, stage = (read_json(name) for name in required[1:])
        if manifest["runId"] != evidence["runId"] or stage["runId"] != evidence["runId"] or \
                manifest["buildManifestId"] != evidence["buildManifestId"] or build["manifestId"] != evidence["buildManifestId"]:
            errors.append("run/build/deployment identity mismatch")
        if stage.get("sampledSceneDepthEnabled") is not True:
            errors.append("source diagnostic was not enabled in the staged candidate")
        text = (root / "runtime/cojvr.log").read_text(encoding="utf-8-sig", errors="strict")
        text = _run_interval(text, evidence["runId"], evidence["buildManifestId"])
        if not any(row.get("status") == "stopped" for row in _records(text, "native_stereo_runtime")):
            errors.append("missing runtime stop inside matching interval")
        native = {}
        sequences = set()
        for row in _records(text, "native_sampled_depth"):
            if row.get("status") != "ready":
                if row.get("status") == "restore_pending":
                    errors.append("native source restoration remained pending")
                continue
            stamp = _stamp(row)
            eye = int(row["eye"])
            if eye not in (0, 1) or row.get("temporal_admitted") != "false" or int(row["readback_hr"], 0) != 0:
                raise ValueError("invalid native ready evidence")
            key = stamp + (eye,)
            sequence = (stamp[3], stamp[4], stamp[6], _number(row, "capture_sequence"))
            if key in native or sequence in sequences:
                raise ValueError("duplicated native eye/capture identity")
            native[key] = row
            sequences.add(sequence)
        copied, seen, worker_threads, source_threads = [], set(), set(), set()
        events = _records(text, "native_sampled_worker")
        for row in events:
            if row.get("status") in ("failed", "source_rejected", "unexpected_reconstruction"):
                errors.append("worker failure: " + row.get("reason", row["status"]))
            if row.get("status") != "copied":
                continue
            stamp = _stamp(row)
            if stamp in seen:
                raise ValueError("worker completion was replayed")
            seen.add(stamp)
            if any(row.get(key) != value for key, value in {
                    "mode": "copy_audit", "pixels_checked": "true", "pixels_match": "true",
                    "neural_active": "false", "temporal_admitted": "false"}.items()):
                errors.append("worker pixel proof/admission flags rejected")
            width, height = _number(row, "width"), _number(row, "height")
            if width > 4096 or height > 4096 or width * height * 36 > 256 * 1024 * 1024:
                errors.append("worker extent exceeds admitted capture bounds")
            worker_thread, source_thread = _number(row, "worker_thread"), _number(row, "source_thread")
            if worker_thread == source_thread:
                errors.append("worker ran on the native producer thread")
            worker_threads.add(worker_thread)
            source_threads.add(source_thread)
            for field in ("settings_revision", "input_context", "history_epoch"):
                _number(row, field)
            for eye in (0, 1):
                source = native.get(stamp + (eye,))
                if source is None:
                    errors.append("worker pair has no exact matching native eye")
                elif (_number(source, "width"), _number(source, "height"), _number(source, "producer_thread")) != \
                        (width, height, source_thread):
                    errors.append("native/worker extent or producer thread mismatch")
            timing = {key: float(row[key]) for key in ("copy_ms", "processing_ms", "age_ms")}
            if any(not math.isfinite(value) or value < 0 for value in timing.values()):
                errors.append("invalid worker wall-clock timing")
            if timing["age_ms"] > 2000:  # CoJSampledWorkerAudit's fixed diagnostic deadline.
                errors.append("expired copy-audit result")
            copied.append({"frame": stamp[0], "pose": stamp[1], "width": width, "height": height, **timing})
        if not copied:
            errors.append("no completed copy-audit pair")
        if len(worker_threads) > 1 or len(source_threads) > 1:
            errors.append("persistent worker/producer thread identity changed")
        shutdowns = _records(text, "native_sampled_worker_shutdown")
        if not shutdowns:
            errors.append("missing worker shutdown proof")
        else:
            shutdown = shutdowns[-1]
            submitted = int(shutdown["submitted"])
            completed = int(shutdown["completed"])
            retired = int(shutdown["source_pairs_retired"])
            if shutdown.get("status") != "retired" or not (0 <= completed <= submitted and retired == submitted):
                errors.append("worker source pairs did not retire at shutdown")
            if len(copied) > completed:
                errors.append("more reported copies than completed jobs")
        result.update(copied_pairs=len(copied), pairs=copied,
                      cancelled_pairs=sum(row.get("status") == "cancelled" for row in events),
                      expired_pairs=sum(row.get("status") == "expired" for row in events))
    except (OSError, ValueError, KeyError, TypeError, OverflowError, AttributeError) as error:
        errors.append(str(error))
    result["passed"] = not errors
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path, help="Collected, inventoried run directory")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = analyze(args.run)
    rendered = json.dumps(result, indent=2, ensure_ascii=False)
    if args.output:
        args.output.write_text(rendered + "\n", encoding="utf-8")
    print(rendered)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
