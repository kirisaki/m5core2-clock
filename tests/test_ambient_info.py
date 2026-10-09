from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AmbientInfoTest(unittest.TestCase):
    def test_rain_sun_moon_and_creature_states(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "ambient_info_test"
            subprocess.run([
                "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "src"), "-I", str(ROOT / "include"),
                str(ROOT / "tests/ambient_info_test.cpp"),
                str(ROOT / "src/ambient_info.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)
