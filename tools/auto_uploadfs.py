"""
Ensures the LittleFS image is rebuilt and uploaded whenever the normal
firmware upload runs.
"""

Import("env")

import subprocess
import sys
from pathlib import Path


FILESYSTEM_PARTITIONS = {"spiffs", "littlefs", "fat"}


def _parse_size(value):
    if isinstance(value, int):
        return value
    value = str(value).strip()
    if value.startswith("0x"):
        return int(value, 16)
    if value[:-1].isdigit() and value[-1].upper() in ("K", "M"):
        base = 1024 if value[-1].upper() == "K" else 1024 * 1024
        return int(value[:-1]) * base
    if value.isdigit():
        return int(value)
    raise ValueError(f"Unable to parse size '{value}'")


def resolve_fs_partition(env):
    partitions_csv = env.subst("$PARTITIONS_TABLE_CSV")
    if not partitions_csv:
        print("[auto_uploadfs] No partition table configured; skipping LittleFS upload.")
        return None

    csv_path = Path(partitions_csv)
    if not csv_path.exists():
        raise RuntimeError(f"Partition table '{csv_path}' not found")

    fs_partition = None
    next_offset = 0

    with csv_path.open() as fp:
        for raw_line in fp:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            tokens = [token.strip() for token in line.split(",")]
            if len(tokens) < 5:
                continue

            part_type = tokens[1]
            align = 0x10000 if part_type in ("0", "app") else 4
            calculated_offset = (next_offset + align - 1) & ~(align - 1)
            offset = tokens[3] or calculated_offset

            partition = {
                "name": tokens[0],
                "type": part_type,
                "subtype": tokens[2],
                "offset": offset,
                "size": tokens[4],
            }

            if (
                partition["type"] == "data"
                and partition["subtype"] in FILESYSTEM_PARTITIONS
            ):
                fs_partition = partition

            next_offset = _parse_size(partition["offset"])
            next_offset += _parse_size(partition["size"])

    if not fs_partition:
        print("[auto_uploadfs] No filesystem partition found; skipping LittleFS upload.")
        return None

    fs_partition["offset"] = _parse_size(fs_partition["offset"])
    fs_partition["size"] = _parse_size(fs_partition["size"])
    return fs_partition


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
