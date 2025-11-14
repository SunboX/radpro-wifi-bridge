"""
Shared helpers for inspecting the configured filesystem partition.
"""

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
        print("[fs_partition] No partition table configured; skipping filesystem partition lookup.")
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
        print("[fs_partition] No filesystem partition found.")
        return None

    fs_partition["offset"] = _parse_size(fs_partition["offset"])
    fs_partition["size"] = _parse_size(fs_partition["size"])
    return fs_partition
