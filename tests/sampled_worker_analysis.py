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
