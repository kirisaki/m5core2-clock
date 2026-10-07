from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class EnvironmentDataTest(unittest.TestCase):
    def test_api_parsing_and_forecast_time_selection(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "environment_data_test"
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "src"),
                "-I", str(ROOT / ".pio/libdeps/m5stack-core2/ArduinoJson/src"),
                str(ROOT / "tests/environment_data_test.cpp"),
                str(ROOT / "src/environment_data.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)
