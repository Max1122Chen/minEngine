#!/usr/bin/env python3
"""Migrate Transform.Rotation from Euler [x,y,z] degrees to Quaternion {W,X,Y,Z}."""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path
from typing import Any


def quat_mul(a: tuple[float, float, float, float], b: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    aw, ax, ay, az = a
    bw, bx, by, bz = b
    return (
        aw * bw - ax * bx - ay * by - az * bz,
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
    )


def normalize_quat(q: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    mag = math.sqrt(sum(c * c for c in q))
    if mag <= 1e-8:
        return (1.0, 0.0, 0.0, 0.0)
    return tuple(c / mag for c in q)


def angle_axis_degrees(angle_deg: float, axis: tuple[float, float, float]) -> tuple[float, float, float, float]:
    angle_rad = math.radians(angle_deg)
    half = angle_rad * 0.5
    s = math.sin(half)
    w = math.cos(half)
    x, y, z = axis
    return normalize_quat((w, x * s, y * s, z * s))


def from_euler_degrees_xyz(euler: list[float]) -> dict[str, float]:
    if len(euler) != 3:
        raise ValueError(f"Expected Euler array of length 3, got {euler}")
    rx, ry, rz = (float(euler[0]), float(euler[1]), float(euler[2]))
    qx = angle_axis_degrees(rx, (1.0, 0.0, 0.0))
    qy = angle_axis_degrees(ry, (0.0, 1.0, 0.0))
    qz = angle_axis_degrees(rz, (0.0, 0.0, 1.0))
    w, x, y, z = normalize_quat(quat_mul(quat_mul(qx, qy), qz))
    return {"W": w, "X": x, "Y": y, "Z": z}


def migrate_node(node: Any) -> int:
    changed = 0
    if isinstance(node, dict):
        rotation = node.get("Rotation")
        if isinstance(rotation, list) and len(rotation) == 3 and all(isinstance(v, (int, float)) for v in rotation):
            node["Rotation"] = from_euler_degrees_xyz(rotation)
            changed += 1
        for value in node.values():
            changed += migrate_node(value)
    elif isinstance(node, list):
        for item in node:
            changed += migrate_node(item)
    return changed


def migrate_file(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    data = json.loads(text)
    count = migrate_node(data)
    if count == 0:
        return 0
    path.write_text(json.dumps(data, indent=4) + "\n", encoding="utf-8")
    return count


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("Usage: migrate_transform_rotation_to_quat.py <file-or-dir> [...]", file=sys.stderr)
        return 1

    total_files = 0
    total_rotations = 0
    for arg in argv[1:]:
        target = Path(arg)
        paths = [target] if target.is_file() else sorted(target.rglob("*.mescene"))
        for path in paths:
            if not path.is_file():
                continue
            count = migrate_file(path)
            if count > 0:
                total_files += 1
                total_rotations += count
                print(f"migrated {count} Rotation field(s) in {path}")

    print(f"done: {total_rotations} rotation(s) in {total_files} file(s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
