import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("build_config", ROOT / "scripts/build_config.py")
build_config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build_config)


class BuildConfigTest(unittest.TestCase):
    def test_environment_overrides_local_defaults(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "app_config.h").write_text((ROOT / "include/app_config.h").read_text())
            local = (ROOT / "include/secrets.example.h").read_text()
            local = local.replace('#define CLOCK_WIFI_SSID ""',
                                  '#define CLOCK_WIFI_SSID "local-default"')
            (root / "secrets.h").write_text(local)
            for settings, expected in (
                ({}, "local-default"),
                ({"CLOCK_WIFI_SSID": "environment"}, "environment"),
                ({"CLOCK_WIFI_SSID": ""}, ""),
                ({}, "local-default"),
            ):
                build_config.write_header(root / "clock_build_config.h", settings)
                source = root / "check.cpp"
                source.write_text(
                    '#include "app_config.h"\n#include <cstring>\n'
                    f'int main() {{ return std::strcmp(config::kWifiSsid, "{expected}") != 0; }}\n'
                )
                subprocess.run(["g++", str(source), "-o", str(root / "check")], check=True)
                subprocess.run([str(root / "check")], check=True)

    def test_settings_compile_as_exact_utf8_strings(self):
        # Spaces, shell syntax, Unicode and C++ punctuation must stay literal.
        value = 'a "quoted" \\ password $HOME $(cmd); 日本語\n09'
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            header = root / "clock_build_config.h"
            build_config.write_header(header, {"CLOCK_WIFI_PASSWORD": value})
            expected = ",".join(str(byte) for byte in value.encode("utf-8"))
            source = root / "check.cpp"
            source.write_text(
                '#include "clock_build_config.h"\n#include <cstring>\n'
                'int main() {\n'
                f'  const unsigned char expected[] = {{{expected},0}};\n'
                '  return std::strcmp(CLOCK_WIFI_PASSWORD, '
                'reinterpret_cast<const char*>(expected)) != 0;\n}\n'
            )
            subprocess.run(["g++", str(source), "-o", str(root / "check")], check=True)
            subprocess.run([str(root / "check")], check=True)

    def test_unset_and_explicit_empty_are_distinct(self):
        self.assertNotIn("#define CLOCK_WIFI_SSID", build_config.render_header({}))
        self.assertIn('#define CLOCK_WIFI_SSID ""',
                      build_config.render_header({"CLOCK_WIFI_SSID": ""}))

    def test_change_and_removal_update_header(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "generated/clock_build_config.h"
            build_config.write_header(path, {"CLOCK_WIFI_SSID": "first"})
            original_time = path.stat().st_mtime_ns
            build_config.write_header(path, {"CLOCK_WIFI_SSID": "first"})
            self.assertEqual(path.stat().st_mtime_ns, original_time)
            build_config.write_header(path, {"CLOCK_WIFI_SSID": "second"})
            self.assertIn('"second"', path.read_text())
            self.assertNotIn('"first"', path.read_text())
            build_config.write_header(path, {})
            self.assertNotIn("CLOCK_WIFI_SSID", path.read_text())

    def test_reject_nul_without_disclosing_value(self):
        with self.assertRaisesRegex(ValueError, "CLOCK_WIFI_PASSWORD must not contain a NUL byte"):
            build_config.render_header({"CLOCK_WIFI_PASSWORD": "private\0value"})

    def test_coordinate_boundaries_and_invalid_input(self):
        for key, limit in build_config.COORDINATES.items():
            for value in (-limit, 0, limit):
                with self.subTest(key=key, value=value):
                    header = build_config.render_header({key: str(value)})
                    self.assertIn(f"#define {key} {float(value)!r}", header)
            for value in (str(limit + 0.01), str(-limit - 0.01), "nan", "inf", "", "1;code"):
                with self.subTest(key=key, value=value):
                    with self.assertRaises(ValueError):
                        build_config.render_header({key: value})

    def test_coordinate_config_compiles_and_rejects_invalid_local_values(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "app_config.h").write_text((ROOT / "include/app_config.h").read_text())
            source = root / "check.cpp"
            source.write_text(
                '#include "app_config.h"\n'
                'static_assert(config::kWeatherLocationConfigured, "location missing");\n'
                'static_assert(config::kWeatherLatitude == 35.5, "latitude changed");\n'
                'static_assert(config::kWeatherLongitude == 140.25, "longitude changed");\n'
                'int main() {}\n'
            )
            build_config.write_header(root / "clock_build_config.h", {
                "CLOCK_WEATHER_LATITUDE": "35.5", "CLOCK_WEATHER_LONGITUDE": "140.25",
            })
            subprocess.run(["g++", str(source), "-o", str(root / "check")], check=True)
            build_config.write_header(root / "clock_build_config.h", {})
            source.write_text('#include "app_config.h"\nint main() {}\n')
            for definitions in (
                '#define CLOCK_WEATHER_LATITUDE 35.5\n',
                '#define CLOCK_WEATHER_LATITUDE 91\n#define CLOCK_WEATHER_LONGITUDE 0\n',
            ):
                (root / "secrets.h").write_text(definitions)
                result = subprocess.run(["g++", str(source), "-o", str(root / "check")],
                                        capture_output=True)
                self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
