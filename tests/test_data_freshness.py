from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class DataFreshnessTest(unittest.TestCase):
    def test_age_boundaries_errors_and_rollover(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "data_freshness_test"
            subprocess.run([
                "g++", "-std=c++11", "-Wall", "-Wextra", "-Werror",
                "-I", str(ROOT / "src"), "-I", str(ROOT / "include"),
                str(ROOT / "tests/data_freshness_test.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)
