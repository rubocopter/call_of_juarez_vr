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


def _final_color_pairs(root, inventory, text, run_id, native, read_json):
    complete = {}
    optics_pairs = {}
    directory = root / "runtime/scene-depth"
    for row in _records(text, "native_sampled_depth_preview"):
        if row.get("final_color") != "captured":
            continue
        frame, pose, generation, eye = (
            int(row[key], 0) for key in ("frame_sequence", "pose_sequence", "generation", "eye"))
        matches = [(stamp, source) for stamp, source in native.items()
                   if (stamp[0], stamp[1], stamp[3], stamp[-1]) == (frame, pose, generation, eye)]
        if (len(matches) != 1 or row.get("status") != "written" or
                row.get("temporal_admitted") != "false" or int(row["readback_hr"], 0)):
            raise ValueError("final color has no successful matching native eye")
        stamp, source = matches[0]
        stem = f"f{frame}-p{pose}-g{generation}-e{eye}"
        filename = stem + ".json"
        if row.get("manifest") != filename or (directory / filename).resolve() not in inventory:
            raise ValueError("final color manifest is not inventoried")
        width, height = (_number(source, key) for key in ("width", "height"))
        extent = max(width, height)
        limit = min(extent, 512)
        small_width, small_height = (max(1, size * limit // extent) for size in (width, height))
        manifest = read_json(Path("runtime/scene-depth") / filename)
        expected = {"schemaVersion": 1, "runId": run_id, "frame_sequence": frame, "pose_sequence": pose,
                    "owner": stamp[2], "generation": generation, "eye": eye,
                    "capture_sequence": _number(source, "capture_sequence"),
                    "source_width": width, "source_height": height,
                    "width": small_width, "height": small_height, "source_boundary": "1B_16",
                    "sampling": "point", "format": "rgba16f_le",
                    "depth_encoding": "device_z_rgb24_integer_bytes", "temporal_admitted": False,
                    "final_color_format": "bgra8_le", "final_color_boundary": "native_eye_capture_rt0"}
        if (any(manifest.get(key) != value for key, value in expected.items()) or
                manifest.get("final_color_d3d9_format") not in (21, 22) or
                (_number(row, "width"), _number(row, "height")) != (small_width, small_height)):
            raise ValueError("final color and native preview metadata disagree")
        for field, suffix, stride in (("color_file", ".color.f16", 8), ("depth_file", ".depth.f16", 8),
                                      ("final_color_file", ".final.bgra8", 4)):
            name = stem + suffix
            path = (directory / name).resolve()
            if manifest.get(field) != name or path not in inventory or path.stat().st_size != small_width * small_height * stride:
                raise ValueError("final color preview has missing or malformed plane: " + name)
        eyes = complete.setdefault(stamp[:-1], set())
        if eye in eyes:
            raise ValueError("final color preview was replayed")
        eyes.add(eye)
        if "depth_optics" in manifest:
            optics = manifest["depth_optics"]
            near, far_limit = optics["near_metres"], optics["far_limit_metres"]
            matrix = optics["view_projection"]
            recenter = optics["recenter_sequence"]
            if (optics["space"] != "world_metres_row_vector_d3d9_clip" or
                    type(recenter) is not int or recenter <= 0 or
                    type(optics["infinite_far"]) is not bool or type(optics["final_color_match"]) is not bool or
                    type(near) not in (int, float) or type(far_limit) not in (int, float) or
                    not (math.isfinite(near) and math.isfinite(far_limit) and 0 < near < far_limit) or
                    not isinstance(matrix, list) or len(matrix) != 16 or
                    any(type(value) not in (int, float) or not math.isfinite(value) for value in matrix)):
                raise ValueError("invalid sampled eye optics")
            if "final_color_optics_valid" in optics:
                observed = optics["final_color_optics_valid"]
                if type(observed) is not bool or (optics["final_color_match"] and not observed):
                    raise ValueError("invalid or unavailable matching final color optics")
            if optics["final_color_match"]:
                optics_pairs.setdefault(stamp[:-1], {})[eye] = recenter
    matched = {stamp: eyes[0] for stamp, eyes in optics_pairs.items()
               if set(eyes) == {0, 1} and eyes[0] == eyes[1]}
    adjacent = 0
    previous = {}
    for stamp, recenter in sorted(matched.items()):
        domain = stamp[2:] + (recenter,)
        prior = previous.get(domain)
        if prior and stamp[0] == prior[0] + 1 and stamp[1] > prior[1]:
            adjacent += 1
        previous[domain] = stamp
    return sum(eyes == {0, 1} for eyes in complete.values()), len(matched), adjacent


def analyze(run: Path) -> dict:
    result = {"passed": False, "errors": [], "copied_pairs": 0, "canonical_outcome": "unknown",
              "final_color_pairs": 0, "matched_optics_pairs": 0, "consecutive_optics_pairs": 0,
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
        sequence_mode = stage.get("sampledTemporalSequenceEnabled") is True
        result["source_mode"] = "temporal_sequence" if sequence_mode else "copy_audit"
        text = (root / "runtime/cojvr.log").read_text(encoding="utf-8-sig", errors="strict")
        text = _run_interval(text, evidence["runId"], evidence["buildManifestId"])
        if not any(row.get("status") == "stopped" for row in _records(text, "native_stereo_runtime")):
            errors.append("missing runtime stop inside matching interval")
        native = {}
        sequences = set()
        for row in _records(text, "native_sampled_depth"):
            if row.get("status") != "ready":
                if row.get("status") in ("restore_pending", "retirement_pending"):
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
        result["final_color_pairs"], result["matched_optics_pairs"], result["consecutive_optics_pairs"] = _final_color_pairs(
            root, inventory, text, evidence["runId"], native, read_json)
        if sequence_mode:
            configs = _records(text, "native_sampled_depth_config")
            if not any(row.get("mode") == "temporal_sequence" and row.get("max_frames") == "8" and
                       row.get("temporal_admitted") == "false" for row in configs):
                errors.append("temporal sequence startup was not observed")
            if not result["consecutive_optics_pairs"]:
                errors.append("no consecutive stereo sources with matched optics")
            if len(native) > 16:
                errors.append("temporal sequence exceeded eight stereo frames")
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
        if not copied and not sequence_mode:
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
