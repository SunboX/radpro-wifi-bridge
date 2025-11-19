"""Create OTA ZIP bundle with manifest and binaries."""
from pathlib import Path
import zipfile
import json

Import('env')

PROJECT_DIR = Path(env['PROJECT_DIR'])
BUILD_DIR = Path(env.subst('$BUILD_DIR'))
DEST_DIR = PROJECT_DIR / 'docs' / 'web-install'
FIRMWARE_DIR = DEST_DIR / 'firmware'
MANIFEST = DEST_DIR / 'manifest.json'

_zip_written = False


def _collect_manifest_parts(manifest):
    parts = []
    seen = set()
    builds = manifest.get('builds', [])
    for build in builds:
        for part in build.get('parts', []):
            part_path = part.get('path')
            if not part_path or part_path in seen:
                continue
            seen.add(part_path)
            full_path = DEST_DIR / part_path
            parts.append((full_path, part_path))
    return parts


def create_ota_zip(source, target, env):
    global _zip_written
    if _zip_written:
        return

    if not MANIFEST.exists():
        print(f'[create_ota_zip] Manifest missing: {MANIFEST}')
        return

    try:
        data = json.loads(MANIFEST.read_text())
        version = data.get('version', 'unknown')
    except Exception as exc:
        print(f'[create_ota_zip] Failed to parse manifest: {exc}')
        version = 'unknown'
        data = {}

    part_files = _collect_manifest_parts(data)
    if not part_files:
        print('[create_ota_zip] No manifest parts described; skipping ZIP generation.')
        return

    missing = [str(src) for src, _ in part_files if not src.exists()]
    if missing:
        print('[create_ota_zip] Waiting for binaries:', ', '.join(missing))
        return

    output_zip = FIRMWARE_DIR / f'firmware_{version}.zip'
    output_zip.parent.mkdir(parents=True, exist_ok=True)

    with zipfile.ZipFile(output_zip, 'w', compression=zipfile.ZIP_DEFLATED) as zf:
        zf.write(MANIFEST, 'manifest.json')
        for src_path, arcname in part_files:
            # Store each artifact under the same relative path referenced in the manifest
            zf.write(src_path, arcname)
    print(f'[create_ota_zip] Created {output_zip}')
    _zip_written = True


env.AddPostAction('$BUILD_DIR/littlefs.bin', create_ota_zip)
env.AddPostAction('$BUILD_DIR/${PROGNAME}.bin', create_ota_zip)
