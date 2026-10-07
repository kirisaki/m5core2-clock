"""Pass build-time settings via a header, keeping values out of compiler flags."""

import os
import math
from pathlib import Path

SETTINGS = ("CLOCK_WIFI_SSID", "CLOCK_WIFI_PASSWORD", "CLOCK_SENSOR_ENDPOINT")
COORDINATES = {"CLOCK_WEATHER_LATITUDE": 90, "CLOCK_WEATHER_LONGITUDE": 180}


def cpp_string(value):
    # Encode UTF-8 bytes explicitly, including quotes, backslashes and controls.
    # Fixed-width octal escapes cannot consume a following hex/digit character.
    return '"' + "".join(
        chr(byte) if 32 <= byte < 127 and byte not in (34, 92)
        else f"\\{byte:03o}"
        for byte in value.encode("utf-8")
    ) + '"'


def render_header(settings):
    lines = ["#pragma once", "// Generated build settings. Do not commit."]
    for name in SETTINGS:
        if name in settings:
            value = settings[name]
            if "\0" in value:
                raise ValueError(f"{name} must not contain a NUL byte")
            lines.append(f"#define {name} {cpp_string(value)}")
    for name, limit in COORDINATES.items():
        if name not in settings:
            continue
        try:
            value = float(settings[name])
        except ValueError:
            raise ValueError(f"{name} must be a number between {-limit} and {limit}") from None
        if not math.isfinite(value) or not -limit <= value <= limit:
            raise ValueError(f"{name} must be a number between {-limit} and {limit}")
        lines.append(f"#define {name} {value!r}")
    return "\n".join(lines) + "\n"


def write_header(path, settings):
    content = render_header(settings)
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        with path.open("w", encoding="utf-8") as output:
            os.chmod(path, 0o600)
            output.write(content)


if "Import" in globals():
    Import("env")
    generated = Path(env.subst("$BUILD_DIR")) / "generated"
    write_header(generated / "clock_build_config.h", os.environ)
    env.Append(CPPPATH=[str(generated.resolve())])
