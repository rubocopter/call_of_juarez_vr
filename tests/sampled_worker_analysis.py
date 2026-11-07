import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("audit", Path(__file__).resolve().parents[1] / "tools/analyze_sampled_worker.py")
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


def fixture(root, *, pixel_match="true", pose=2, source_thread=17, worker_thread=18,
            input_context=3, shutdown="retired", retired=1, copied=True):
    (root / "runtime").mkdir()
    (root / "deployment").mkdir()
    stamp = ";frame_sequence=1;pose_sequence=2;owner=42;generation=1;resource_epoch=1;integrity_epoch=1;producer_device=123"
    lines = ["run_start: run_id=test-run build_manifest_id=build-test pid=123"]
    for eye in range(2):
        lines.append("native_sampled_depth: status=ready" + stamp +
                     f";eye={eye};width=64;height=64;capture_sequence={eye + 1};producer_thread={source_thread};readback_hr=0x0;temporal_admitted=false")
    if copied:
        lines.append(("native_sampled_worker: status=copied;mode=copy_audit" + stamp).replace("pose_sequence=2", f"pose_sequence={pose}") +
                     f";width=64;height=64;settings_revision=1;input_context={input_context};history_epoch=1;worker_thread={worker_thread};source_thread={source_thread}" +
                     f";pixels_checked=true;pixels_match={pixel_match};copy_ms=0.1;processing_ms=0.2;age_ms=2;neural_active=false;temporal_admitted=false")
    lines.extend([f"native_sampled_worker_shutdown: status={shutdown};submitted=1;completed=1;source_pairs_retired={retired};neural_active=false;temporal_admitted=false",
                  "native_stereo_runtime: status=stopped", "run_end: run_id=test-run"])
    (root / "runtime/cojvr.log").write_text("\n".join(lines))
    (root / "run-manifest.json").write_text(json.dumps({"runId": "test-run", "buildManifestId": "build-test"}))
    (root / "build-manifest.json").write_text(json.dumps({"manifestId": "build-test"}))
    (root / "deployment/.cojvr-d3d9-stage.json").write_text(json.dumps({"runId": "test-run", "sampledSceneDepthEnabled": True}))
    inventory = []
    for name in ["runtime/cojvr.log", "run-manifest.json", "build-manifest.json", "deployment/.cojvr-d3d9-stage.json"]:
        payload = (root / name).read_bytes()
        inventory.append({"path": name, "size": len(payload), "sha256": hashlib.sha256(payload).hexdigest()})
    (root / "evidence-manifest.json").write_text(json.dumps({
        "runId": "test-run", "buildManifestId": "build-test", "runtimeStarted": True, "runtimeEnded": True, "incomplete": False,
        "deploymentChecks": [{"matches": True, "expectedSha256": "a" * 64, "actualSha256": "a" * 64}],
        "files": inventory, "validation": {"outcome": "fail"}}))


class AuditTests(unittest.TestCase):
    def test_final_color_pairs_require_same_native_eyes_and_inventoried_payloads(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture(root)
            scene = root / "runtime/scene-depth"
            scene.mkdir()
            manifest = root / "evidence-manifest.json"
            evidence = json.loads(manifest.read_text())
            log = root / "runtime/cojvr.log"
            final_events = []
            for eye in (0, 1):
                stem = f"f1-p2-g1-e{eye}"
                preview = {"schemaVersion": 1, "runId": "test-run", "frame_sequence": 1,
                    "pose_sequence": 2, "owner": 42, "generation": 1, "capture_sequence": eye + 1,
                    "eye": eye, "source_width": 64, "source_height": 64, "width": 64, "height": 64,
                    "source_boundary": "1B_16", "sampling": "point", "format": "rgba16f_le",
                    "depth_encoding": "device_z_rgb24_integer_bytes", "temporal_admitted": False,
                    "final_color_format": "bgra8_le", "final_color_d3d9_format": 21,
                    "final_color_boundary": "native_eye_capture_rt0",
                    "depth_optics": {"space": "world_metres_row_vector_d3d9_clip",
                        "recenter_sequence": 17, "near_metres": .25, "far_limit_metres": 500,
                        "infinite_far": True, "final_color_match": True,
                        "view_projection": [100,0,0,0, 0,100,0,0, 0,0,-100,-100, 0,0,-25,0]}}
                for field, suffix, stride in (
                    ("color_file", ".color.f16", 8), ("depth_file", ".depth.f16", 8),
                    ("final_color_file", ".final.bgra8", 4)):
                    preview[field] = stem + suffix
                    (scene / preview[field]).write_bytes(bytes(64 * 64 * stride))
                (scene / (stem + ".json")).write_text(json.dumps(preview))
                final_events.append("native_sampled_depth_preview: status=written;frame_sequence=1;pose_sequence=2"
                    f";generation=1;eye={eye};width=64;height=64;readback_hr=0x0;final_color=captured"
                    f";temporal_admitted=false;manifest={stem}.json")
            text = log.read_text()
            log.write_text(text.replace("run_end:", "\n".join(final_events) + "\nrun_end:"))

            def refresh_inventory():
                for path in [log, *scene.iterdir()]:
                    name = path.relative_to(root).as_posix()
                    evidence["files"] = [entry for entry in evidence["files"] if entry["path"] != name]
                    data = path.read_bytes()
                    evidence["files"].append({"path": name, "size": len(data),
                        "sha256": hashlib.sha256(data).hexdigest()})
                manifest.write_text(json.dumps(evidence))

            refresh_inventory()
            result = audit.analyze(root)
            self.assertTrue(result["passed"], result["errors"])
            self.assertEqual(result["final_color_pairs"], 1)
            self.assertEqual(result["matched_optics_pairs"], 1)
            right = scene / "f1-p2-g1-e1.json"
            right_meta = json.loads(right.read_text())
            right_meta["depth_optics"]["recenter_sequence"] = 18
            right.write_text(json.dumps(right_meta))
            refresh_inventory()
            result = audit.analyze(root)
            self.assertTrue(result["passed"], result["errors"])
            self.assertEqual(result["matched_optics_pairs"], 0)
            (scene / "f1-p2-g1-e1.final.bgra8").write_bytes(bytes(64 * 64 * 3))
            refresh_inventory()
            result = audit.analyze(root)
            self.assertFalse(result["passed"])
            self.assertEqual(result["final_color_pairs"], 0)

    def mutate_log(self, transform):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture(root)
            log = root / "runtime/cojvr.log"
            log.write_text(transform(log.read_text()))
            manifest = root / "evidence-manifest.json"
            data = json.loads(manifest.read_text())
            for entry in data["files"]:
                if entry["path"] == "runtime/cojvr.log":
                    payload = log.read_bytes()
                    entry.update(size=len(payload), sha256=hashlib.sha256(payload).hexdigest())
            manifest.write_text(json.dumps(data))
            return audit.analyze(root)

    def result(self, **kwargs):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture(root, **kwargs)
            return audit.analyze(root)

    def test_good_collected_transport_does_not_promote_canonical_or_reconstruction(self):
        result = self.result()
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["copied_pairs"], 1)
        self.assertEqual(result["canonical_outcome"], "fail")
        self.assertFalse(result["reconstruction_admitted"])
        self.assertEqual(result["headset_acceptance"], "not_evaluated")

    def test_unmatched_or_unretired_evidence_fails(self):
        for bad in [dict(pixel_match="false"), dict(pose=3), dict(worker_thread=17),
                    dict(input_context=0), dict(shutdown="quarantined"), dict(retired=0), dict(copied=False)]:
            with self.subTest(bad=bad):
                self.assertFalse(self.result(**bad)["passed"])

    def test_modified_runtime_log_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture(root)
            with (root / "runtime/cojvr.log").open("a") as file:
                file.write("\nnative_sampled_worker: status=copied;pixels_match=true")
            self.assertFalse(audit.analyze(root)["passed"])

    def test_foreign_manifest_path_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fixture(root)
            path = root / "evidence-manifest.json"
            data = json.loads(path.read_text())
            data["files"][0]["path"] = "../foreign.log"
            path.write_text(json.dumps(data))
            self.assertFalse(audit.analyze(root)["passed"])

    def test_other_run_proof_cannot_fill_an_empty_matching_interval(self):
        def outside(text):
            lines = text.splitlines()
            return "\n".join([lines[0], lines[-1],
                "run_start: run_id=other build_manifest_id=other-build pid=456", *lines[1:-1], "run_end: run_id=other"])
        self.assertFalse(self.mutate_log(outside)["passed"])

    def test_reversed_duplicate_or_nested_matching_boundaries_are_rejected(self):
        for transform in [lambda text: "\n".join([text.splitlines()[-1], *text.splitlines()[1:-1], text.splitlines()[0]]),
                          lambda text: text + "\n" + text,
                          lambda text: text.replace("pid=123\n", "pid=123\nrun_start: run_id=other build_manifest_id=other-build pid=456\n")]:
            with self.subTest(transform=transform):
                self.assertFalse(self.mutate_log(transform)["passed"])

    def test_runtime_build_identity_and_copy_deadline_are_enforced(self):
        for old, new in [("build_manifest_id=build-test", "build_manifest_id=foreign"), ("age_ms=2;", "age_ms=5000;")]:
            with self.subTest(new=new):
                self.assertFalse(self.mutate_log(lambda text: text.replace(old, new))["passed"])


if __name__ == "__main__":
    unittest.main()
