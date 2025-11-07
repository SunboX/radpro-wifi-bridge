"""
PlatformIO post-build helper that copies generated binaries into the
GitHub Pages bundle consumed by ESP Web Tools.

The script runs after each successful build and updates
`docs/web-install/firmware/latest/` with the current bootloader, partition table,
and application image so the web installer always serves the latest
artifacts.
"""

import shutil
from pathlib import Path

Import("env")  # Provided by PlatformIO


def _copy_with_log(src: Path, dest: Path) -> None:
    """
    Copy a file while emitting a short status line so the PlatformIO
    output shows which artifacts were refreshed.
    """
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)
    print(f"[copy_firmware] Copied {src} -> {dest}")


def copy_firmware_bundle(source, target, env) -> None:
    """
    Post-action callback invoked by PlatformIO after the main firmware
    image is produced.
    """
    project_dir = Path(env["PROJECT_DIR"])
    build_dir = Path(env.subst("$BUILD_DIR"))
    dest_dir = project_dir / "docs" / "web-install" / "firmware" / "latest"

    progname = env.subst("$PROGNAME") or "firmware"

    mapping = {
        "bootloader.bin": "bootloader.bin",
        "partitions.bin": "partitions.bin",
        f"{progname}.bin": "radpro-wifi-bridge.bin",
    }

    missing = []
    for src_name, dest_name in mapping.items():
        src_path = build_dir / src_name
        if not src_path.exists():
            missing.append(src_path)
            continue
        dest_path = dest_dir / dest_name
        _copy_with_log(src_path, dest_path)

    if missing:
        for path in missing:
            print(f"[copy_firmware] WARNING: expected artifact missing: {path}")
        print("[copy_firmware] Web installer bundle is incomplete; run a full PlatformIO build.")


# Register callback after the primary firmware image is built.
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", copy_firmware_bundle)
