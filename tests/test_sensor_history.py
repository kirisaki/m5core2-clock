from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SensorHistoryTest(unittest.TestCase):
    def test_retention_freshness_gaps_and_timer_wrap(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "sensor_history_test"
            subprocess.run([
                "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "src"), "-I", str(ROOT / "include"),
                str(ROOT / "tests/sensor_history_test.cpp"),
                str(ROOT / "src/sensor_history.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)
