from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AppStateTest(unittest.TestCase):
    def test_data_ownership_and_history_without_displays(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "app_state_test"
            subprocess.run([
                "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "src"), "-I", str(ROOT / "include"),
                str(ROOT / "tests/app_state_test.cpp"),
                str(ROOT / "src/sensor_history.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)
