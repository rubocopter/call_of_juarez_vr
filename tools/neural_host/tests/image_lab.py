"""Offline CLI safety tests; --gpu explicitly adds hardware diagnostic fixtures.

Optional --model-runtime PATH adds a real-model run, never a diagnostic fallback.
Inputs are synthetic test patterns, not game/headset/binocular evidence.
"""
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXE = Path(sys.argv.pop(1)).resolve()
GPU = "--gpu" in sys.argv
if GPU:
    sys.argv.remove("--gpu")
MODEL_RUNTIME = None
if "--model-runtime" in sys.argv:
    index = sys.argv.index("--model-runtime")
    MODEL_RUNTIME = Path(sys.argv[index + 1]).resolve()
    del sys.argv[index:index + 2]


class ImageLab(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="cojvr-image-lab-")
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name) / "imágenes 测试"
        self.root.mkdir()
        self.input = self.root / "entrada.rgba"
        self.output = self.root / "salida.rgba"
        self.width, self.height = 64, 48
        # Distinct eyes, gradients and varying source alpha expose pair/order loss.
        self.pixels = bytes(channel for eye in range(2) for y in range(48) for x in range(64)
                            for channel in (80 + eye * 30, 100 + x, 120 + y, (x + y) % 256))
        self.input.write_bytes(self.pixels)
        self.args = ["--runtime", str(self.root), "--input", str(self.input),
                     "--output", str(self.output), "--width", "64", "--height", "48",
                     "--scale100", "--warmup", "1", "--iterations", "3"]

    def run_lab(self, args=None):
        result = subprocess.run([str(EXE), *(self.args if args is None else args)],
                                capture_output=True, text=True, timeout=90)
        self.assertTrue(result.stdout.strip(), result.stderr)
        try:
            report = json.loads(result.stdout)
        except ValueError:
            self.fail("stdout must contain exactly one JSON document: " + result.stdout + result.stderr)
        return result, report

    def rejected(self, args=None, code=2, error="arguments"):
        result, report = self.run_lab(args)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        self.assertFalse(report["ok"])
        self.assertEqual(report["error"], error)
        self.assertFalse(self.output.exists())
        self.assertEqual(self.input.read_bytes(), self.pixels)

    def replaced(self, key, value):
        args = self.args.copy()
        args[args.index(key) + 1] = value
        return args

    def test_strict_required_unknown_duplicate_and_scale_arguments(self):
        self.rejected([])
        for key in ("--runtime", "--input", "--output", "--width", "--height", "--warmup", "--iterations"):
            args = self.args.copy()
            index = args.index(key)
            if key not in ("--warmup", "--iterations"):
                self.rejected(args[:index] + args[index + 2:])
            self.rejected(args + [key, args[index + 1]])
            self.rejected(args[:index + 1])
        for extra in (["--bogus", "x"], ["--scale50"], ["--scale100"], ["--scale", "100"],
                      ["--diagnostic", "model"], ["--diagnostic", "identity", "--diagnostic", "edit"]):
            self.rejected(self.args + extra)
        self.rejected([arg for arg in self.args if arg != "--scale100"])

    def test_positive_bounded_decimal_dimensions_counts_and_deadline(self):
        for key, bad_values in {
            "--width": ("0", "-1", "1281", "1.5", "1e2", "+16", " 16", "999999999999999999999"),
            "--height": ("0", "-1", "1281", "nan"),
            "--warmup": ("0", "-1", "33", "1.5"),
            "--iterations": ("0", "-1", "1001", "inf"),
        }.items():
            for value in bad_values:
                with self.subTest(key=key, value=value):
                    self.rejected(self.replaced(key, value))
        for value in ("0", "-1", "99", "120001", "NaN"):
            self.rejected(self.args + ["--timeout-ms", value])
        self.rejected(self.replaced("--runtime", "relative-runtime"))

    def test_exact_file_size_checked_before_gpu_or_missing_dependencies(self):
        for length in (0, len(self.pixels) // 2, len(self.pixels) - 1, len(self.pixels) + 1):
            self.input.write_bytes(self.pixels[:length] if length <= len(self.pixels) else self.pixels + b"x")
            result, report = self.run_lab()
            self.assertEqual(result.returncode, 3)
            self.assertEqual(report["error"], "input_size")
            self.assertFalse(self.output.exists())
        self.input.unlink()
        result, report = self.run_lab()
        self.assertEqual((result.returncode, report["error"]), (3, "input_read"))

    def test_dimension_boundaries_are_admitted_before_exact_size_check(self):
        for key in ("--width", "--height"):
            for value in ("1", "1280"):
                self.rejected(self.replaced(key, value), 3, "input_size")

    def test_reduced_eye_zero_extent_rejects_both_axes_without_work_or_output(self):
        for scale, last_zero in ((25, 7), (50, 3)):
            for axis in ("--width", "--height"):
                for extent in (1, last_zero):
                    with self.subTest(scale=scale, axis=axis, extent=extent):
                        args = self.replaced(axis, str(extent))
                        args[args.index("--scale100")] = "--scale" + str(scale)
                        width = int(args[args.index("--width") + 1])
                        height = int(args[args.index("--height") + 1])
                        self.pixels = bytes((80, 120, 160, 71)) * (width * height * 2)
                        self.input.write_bytes(self.pixels)  # Exact bytes: size rejection cannot hide this bug.
                        before = {path.name for path in self.root.iterdir()}
                        logs = set(EXE.parent.glob("neural-*.log"))
                        self.rejected(args, 2, "arguments")
                        self.assertEqual({path.name for path in self.root.iterdir()}, before)
                        self.assertEqual(set(EXE.parent.glob("neural-*.log")), logs)

    def test_first_nonzero_reduced_extent_and_scale100_one_pixel_reach_dependency_preflight(self):
        for scale, extent in ((25, 8), (50, 4), (100, 1)):
            for axis in ("--width", "--height"):
                with self.subTest(scale=scale, axis=axis, extent=extent):
                    args = self.replaced(axis, str(extent))
                    args[args.index("--scale100")] = "--scale" + str(scale)
                    width = int(args[args.index("--width") + 1])
                    height = int(args[args.index("--height") + 1])
                    self.pixels = bytes((80, 120, 160, 71)) * (width * height * 2)
                    self.input.write_bytes(self.pixels)
                    # No model dependencies exist. Valid shapes must pass argument/size
                    # checks and stop here, keeping this regression GPU-independent.
                    self.rejected(args, 4, "runtime_dependencies")

    def test_no_clobber_input_existing_output_case_alias_or_other_file(self):
        for path in (self.input, Path(str(self.input).upper()), self.root / "." / "entrada.rgba"):
            self.rejected(self.replaced("--output", str(path)), 3, "output_exists")
        self.output.write_bytes(b"existing sentinel")
        result, report = self.run_lab()
        self.assertEqual((result.returncode, report["error"]), (3, "output_exists"))
        self.assertEqual(self.output.read_bytes(), b"existing sentinel")
        self.assertEqual(self.input.read_bytes(), self.pixels)

    def test_model_default_missing_dependencies_fails_without_fake_output(self):
        self.rejected(code=4, error="runtime_dependencies")

    def test_style_preset_intensity_strict_ranges_duplicates_and_missing_values(self):
        for key, bad_values in {
            "--style": ("-1", "3", "1.0", "1e0", "nan", "+1", " 1", ""),
            "--preset": ("-1", "2", "1.0", "1e0", "nan", "+1", " 1", ""),
            "--intensity": ("-1", "101", "60.5", "1e2", "inf", "+60", " 60", "999999999999999999999", ""),
        }.items():
            for value in bad_values:
                with self.subTest(key=key, value=value):
                    self.rejected(self.args + [key, value])
            self.rejected(self.args + [key])
            self.rejected(self.args + [key, "1", key, "2"])
        for style, preset, intensity in ((0, 0, 0), (1, 1, 80), (2, 1, 100)):
            with self.subTest(style=style, preset=preset, intensity=intensity):
                self.rejected(self.args + ["--style", str(style), "--preset", str(preset), "--intensity", str(intensity)],
                              4, "runtime_dependencies")

    def test_missing_output_parent_is_not_created(self):
        self.rejected(self.replaced("--output", str(self.root / "missing" / "out.rgba")), 3, "output_path")
        self.assertFalse((self.root / "missing").exists())

    def check_success(self, mode, scale=100, runtime=None):
        args = [arg if arg != "--scale100" else "--scale" + str(scale) for arg in self.args]
        if mode != "model":
            args += ["--diagnostic", mode]
        if runtime:
            args[args.index("--runtime") + 1] = str(runtime)
        result, report = self.run_lab(args)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertTrue(report["ok"])
        self.assertEqual(report["mode"], mode)
        self.assertEqual(report["width"], self.width)
        self.assertEqual(report["height"], self.height)
        self.assertEqual(report["scale_percent"], scale)
        self.assertEqual(report["fovea_percent"], 100)
        self.assertEqual(report["viewport_percent"], 100)
        self.assertEqual(report["style"], 0)
        self.assertEqual(report["preset"], 0)
        self.assertEqual(report["passes"], 1)
        self.assertEqual(report["intensity_percent"], 60)
        self.assertTrue(report["auto_mask"])
        self.assertEqual(report["local_structure"], 1)
        self.assertEqual(report["local_tone"], 1)
        self.assertTrue(report["skin_auto"])
        self.assertEqual(report["skin_strength"], 1)
        self.assertEqual(report["warmup_iterations"], 1)
        self.assertEqual(report["measured_iterations"], 3)
        self.assertEqual(report["successful_pairs"], 4)
        self.assertEqual(len(report["roundtrip_ms"]["samples"]), 3)
        samples = report["roundtrip_ms"]["samples"]
        self.assertTrue(all(math.isfinite(value) and value > 0 for value in samples))
        self.assertEqual(report["roundtrip_ms"]["median"], sorted(samples)[1])
        self.assertEqual(report["roundtrip_ms"]["p95"], max(samples))
        self.assertFalse(report["binocular_validated"])
        self.assertFalse(report["headset_validated"])
        self.assertEqual(report["alpha_policy"], "source_preserved_after_readback")
        self.assertEqual(report["tracking"], "controlled_identity_quaternions_symmetric_fov_radians")
        self.assertEqual(self.input.read_bytes(), self.pixels)
        self.assertEqual(self.output.stat().st_size, len(self.pixels))
        output = self.output.read_bytes()
        self.assertEqual(output[3::4], self.pixels[3::4])
        self.assertNotEqual(output[:len(output)//2], output[len(output)//2:])
        self.output.unlink()
        return report, output

    @unittest.skipUnless(GPU, "explicit --gpu required")
    def test_explicit_style_preset_intensity_effective_options_are_reported_in_diagnostic_json(self):
        for style, preset, intensity in ((1, 0, 73), (2, 1, 80), (0, 1, 0)):
            with self.subTest(style=style, preset=preset, intensity=intensity):
                args = self.args + ["--diagnostic", "identity", "--style", str(style), "--preset", str(preset), "--intensity", str(intensity)]
                result, report = self.run_lab(args)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual((report["mode"], report["style"], report["preset"], report["passes"], report["intensity_percent"]),
                                 ("identity", style, preset, 1, intensity))
                self.assertTrue(report["auto_mask"])
                self.assertEqual((report["local_structure"], report["local_tone"]), (1, 1))
                self.assertTrue(report["skin_auto"])
                self.assertEqual(report["skin_strength"], 1)
                self.assertEqual(report["scale_percent"], 100)
                self.assertEqual(self.output.read_bytes(), self.pixels)
                self.output.unlink()

    @unittest.skipUnless(GPU, "explicit --gpu required")
    def test_hard_total_gpu_deadline_has_no_output(self):
        args = self.replaced("--iterations", "1000") + ["--diagnostic", "identity", "--timeout-ms", "100"]
        self.rejected(args, 5, "gpu_deadline")

    @unittest.skipUnless(GPU, "explicit --gpu required")
    def test_duplicated_mono_input_is_labeled_and_even_sample_median_excludes_warmup(self):
        self.pixels = self.pixels[:len(self.pixels)//2] * 2
        self.input.write_bytes(self.pixels)
        args = self.replaced("--iterations", "4") + ["--diagnostic", "identity"]
        result, report = self.run_lab(args)
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertTrue(report["identical_eye_inputs"])
        self.assertFalse(report["binocular_validated"])
        self.assertEqual(report["successful_pairs"], 5)
        samples = sorted(report["roundtrip_ms"]["samples"])
        self.assertEqual(len(samples), 4)
        self.assertEqual(report["roundtrip_ms"]["median"], (samples[1]+samples[2])/2)
        self.assertEqual(report["roundtrip_ms"]["p95"], samples[3])
        self.assertEqual(self.output.read_bytes(), self.pixels)

    @unittest.skipUnless(GPU, "explicit --gpu required; argument/safety tests use no GPU")
    def test_hardware_identity_edit_scales_pair_order_alpha_and_no_hidden_resizing(self):
        for scale, atlas in ((25, (32, 16)), (50, (64, 24)), (100, (128, 48))):
            report, output = self.check_success("identity", scale)
            self.assertEqual(output, self.pixels)
            self.assertEqual((report["model_atlas_width"], report["model_atlas_height"]), atlas)
            self.assertEqual(report["model_atlas_pixels"], atlas[0] * atlas[1])
        report, output = self.check_success("edit")
        self.assertGreater(report["changed_rgb_bytes"], 0)
        self.assertTrue(all(output[i] < self.pixels[i] for i in (0, 1, 2, len(output)//2)))

    @unittest.skipUnless(MODEL_RUNTIME, "explicit --model-runtime required; never uses diagnostic fallback")
    def test_real_model_supplied_runtime(self):
        self.width = self.height = 256
        self.args = self.replaced("--width", "256")
        self.args = self.replaced("--height", "256")
        self.pixels = bytes((110, 135, 160, 71)) * (256 * 256) + bytes((160, 130, 110, 193)) * (256 * 256)
        self.input.write_bytes(self.pixels)
        report, _ = self.check_success("model", runtime=MODEL_RUNTIME)
        self.assertGreater(report["changed_rgb_bytes"], 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
