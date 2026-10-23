# PlatformIO pre-script: bakes the firmware version and board id into the build (see src/version.h).
# FW_VERSION comes from the environment (the release workflow sets it from the tag); local builds are "dev".
Import("env")
import os

version = os.environ.get("FW_VERSION", "dev").lstrip("v")
env.Append(CPPDEFINES=[
    ("FW_VERSION", env.StringifyMacro(version)),
    ("BOARD_ID", env.StringifyMacro(env["PIOENV"])),
])
