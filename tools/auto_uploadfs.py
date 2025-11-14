"""
Ensures the LittleFS image is rebuilt and uploaded whenever the normal
firmware upload runs.
"""

Import("env")

import subprocess
import sys
from pathlib import Path

from fs_partition import resolve_fs_partition


def build_littlefs_image(env, fs_size):
    project_dir = Path(env["PROJECT_DIR"])
    build_dir = Path(env.subst("$BUILD_DIR"))
    image_path = build_dir / "littlefs.bin"
    build_dir.mkdir(parents=True, exist_ok=True)

    packages_dir = Path(env.subst("$PROJECT_PACKAGES_DIR"))
    tool_path = packages_dir / "tool-mklittlefs" / "mklittlefs"
    if not tool_path.exists():
        raise RuntimeError(f"LittleFS tool not found at {tool_path}")

    print(f"[auto_uploadfs] Building LittleFS image ({fs_size:#x} bytes)…")
    subprocess.check_call(
        [
            str(tool_path),
            "-b",
            "4096",
            "-p",
            "256",
            "-c",
            str(project_dir / "data"),
            "-s",
            str(fs_size),
            str(image_path),
        ]
    )
    return image_path


def upload_filesystem(source, target, env):
    fs_partition = resolve_fs_partition(env)
    if not fs_partition:
        return

    image_path = build_littlefs_image(env, fs_partition["size"])

    upload_protocol = env.subst("$UPLOAD_PROTOCOL")
    if upload_protocol != "esptool":
        print(
            f"[auto_uploadfs] Upload protocol '{upload_protocol}' not supported for automatic LittleFS upload."
        )
        return

    upload_port = env.subst("$UPLOAD_PORT").strip('"') or env.AutodetectUploadPort()
    if not upload_port:
        raise RuntimeError("[auto_uploadfs] Unable to determine upload port for LittleFS image")

    python_exe = env.subst("$PYTHONEXE").strip('"') or sys.executable
    platform = env.PioPlatform()
    esptool_dir = platform.get_package_dir("tool-esptoolpy")
    if not esptool_dir:
        raise RuntimeError("Unable to locate esptool.py package")
    esptool_path = Path(esptool_dir) / "esptool.py"

    board_config = env.BoardConfig()
    before_reset = board_config.get("upload.before_reset", "default_reset")
    after_reset = board_config.get("upload.after_reset", "hard_reset")
    chip = board_config.get("build.mcu", "esp32")
    flash_mode = env.subst("${__get_board_flash_mode(__env__)}").strip('"')
    flash_freq = env.subst("${__get_board_f_image(__env__)}").strip('"')
    upload_speed = (env.subst("$UPLOAD_SPEED") or "921600").strip('"')

    cmd = [
        python_exe,
        str(esptool_path),
        "--chip",
        chip,
        "--port",
        upload_port,
        "--baud",
        upload_speed,
        "--before",
        before_reset,
        "--after",
        after_reset,
        "write_flash",
        "-z",
        "--flash_mode",
        flash_mode,
        "--flash_freq",
        flash_freq,
        "--flash_size",
        "detect",
        hex(fs_partition["offset"]),
        str(image_path),
    ]

    print(
        f"[auto_uploadfs] Uploading LittleFS image to 0x{fs_partition['offset']:06x} via {upload_port}…"
    )
    subprocess.check_call(cmd)


env.AddPostAction("upload", upload_filesystem)
