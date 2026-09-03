#!/usr/bin/env python3
"""Generate a minimal skinned GLB for ANIM-F01 visual validation.

Why .glb:
  AssetTypeRegistry maps .glb → SkeletalMesh only (unlike .fbx/.gltf which default to StaticMesh).
  Assimp + SkeletalMeshLoader can load JOINTS_0 / WEIGHTS_0 / skins from this file.

Asset:
  Two-bone chain (Hip → Chest) + a vertical stick mesh.
  Bottom verts → Hip; top verts → Chest; middle blended.
  Twisting Chest in the Editor should deform the upper stick.

Usage (repo root or any cwd):
  python scripts/animation/generate_min_skinned_glb.py
  python scripts/animation/generate_min_skinned_glb.py --out path/to/MinSkinnedStick.glb
  python scripts/animation/generate_min_skinned_glb.py --install   # copy into MyMEProject

No third-party packages required (stdlib only).
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import uuid
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO_ROOT / "minEngine" / "MyMEProject" / "Assets" / "Animations" / "MinSkinnedStick.glb"

GLB_MAGIC = 0x46546C67
GLB_VERSION = 2
CHUNK_JSON = 0x4E4F534A
CHUNK_BIN = 0x004E4942


def _align4(n: int) -> int:
    return (n + 3) & ~3


def _pad4(data: bytes, pad_byte: int) -> bytes:
    pad = _align4(len(data)) - len(data)
    return data + bytes([pad_byte]) * pad


def _mat4_identity() -> list[float]:
    return [
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        0.0, 0.0, 0.0, 1.0,
    ]


def _mat4_translate(x: float, y: float, z: float) -> list[float]:
    m = _mat4_identity()
    m[12], m[13], m[14] = x, y, z
    return m


def _pack_f32(values: list[float]) -> bytes:
    return struct.pack(f"<{len(values)}f", *values)


def _pack_u16(values: list[int]) -> bytes:
    return struct.pack(f"<{len(values)}H", *values)


def build_stick_geometry(
    height: float = 2.0,
    half_width: float = 0.2,
    segments: int = 8,
) -> tuple[list[float], list[float], list[float], list[int], list[int], list[float]]:
    """Build a vertical box stick along +Y.

    Returns positions, normals, uvs, indices, joints(u16*4), weights(f32*4).
    """
    positions: list[float] = []
    normals: list[float] = []
    uvs: list[float] = []
    joints: list[int] = []
    weights: list[float] = []

    # Cross-section corners (local XZ), extruded along Y.
    corners = [
        (-half_width, -half_width),
        (half_width, -half_width),
        (half_width, half_width),
        (-half_width, half_width),
    ]

    def bone_blend(y: float) -> tuple[int, int, float, float]:
        # Soft blend around mid-height so twisting Chest is obvious.
        t = max(0.0, min(1.0, y / height))
        # Smoothstep toward Chest.
        s = t * t * (3.0 - 2.0 * t)
        return 0, 1, 1.0 - s, s

    # rings: segments+1
    for ring in range(segments + 1):
        y = height * (ring / segments)
        j0, j1, w0, w1 = bone_blend(y)
        v = ring / segments
        for ci, (x, z) in enumerate(corners):
            positions.extend([x, y, z])
            # Approximate outward normal in XZ.
            length = math.sqrt(x * x + z * z) or 1.0
            normals.extend([x / length, 0.0, z / length])
            uvs.extend([ci / 3.0, v])
            joints.extend([j0, j1, 0, 0])
            weights.extend([w0, w1, 0.0, 0.0])

    indices: list[int] = []
    for ring in range(segments):
        base = ring * 4
        nxt = (ring + 1) * 4
        for c in range(4):
            a = base + c
            b = base + ((c + 1) % 4)
            c0 = nxt + c
            d = nxt + ((c + 1) % 4)
            indices.extend([a, c0, b, b, c0, d])

    # Caps (flat top/bottom) for a solid look.
    bottom = [0, 1, 2, 0, 2, 3]
    top_base = segments * 4
    top = [top_base + 0, top_base + 2, top_base + 1, top_base + 0, top_base + 3, top_base + 2]
    indices.extend(bottom)
    indices.extend(top)

    # Cap normals override: rebuild normals for cap verts is optional; Assimp GenSmoothNormals
    # may overwrite. We already provide normals for sides.

    return positions, normals, uvs, indices, joints, weights


def build_glb_bytes() -> bytes:
    positions, normals, uvs, indices, joints, weights = build_stick_geometry()

    # Bind pose: Hip at origin, Chest at y=1 (mid stick). IBM = inverse(global bind).
    ibm_hip = _mat4_identity()
    ibm_chest = _mat4_translate(0.0, -1.0, 0.0)

    bin_parts: list[bytes] = []
    views: list[dict] = []
    accessors: list[dict] = []

    def add_blob(blob: bytes, target: int | None = None) -> int:
        offset = sum(len(p) for p in bin_parts)
        # Keep each bufferView 4-byte aligned.
        if offset % 4 != 0:
            pad = 4 - (offset % 4)
            bin_parts.append(b"\x00" * pad)
            offset += pad
        bin_parts.append(blob)
        view_index = len(views)
        view: dict = {"buffer": 0, "byteOffset": offset, "byteLength": len(blob)}
        if target is not None:
            view["target"] = target
        views.append(view)
        return view_index

    def add_accessor(
        view_index: int,
        component_type: int,
        count: int,
        type_name: str,
        *,
        max_v: list[float] | None = None,
        min_v: list[float] | None = None,
        byte_offset: int = 0,
    ) -> int:
        acc: dict = {
            "bufferView": view_index,
            "byteOffset": byte_offset,
            "componentType": component_type,
            "count": count,
            "type": type_name,
        }
        if max_v is not None:
            acc["max"] = max_v
        if min_v is not None:
            acc["min"] = min_v
        accessors.append(acc)
        return len(accessors) - 1

    # ELEMENT_ARRAY_BUFFER = 34963, ARRAY_BUFFER = 34962
    idx_blob = _pack_u16(indices)
    pos_blob = _pack_f32(positions)
    nrm_blob = _pack_f32(normals)
    uv_blob = _pack_f32(uvs)
    joint_blob = _pack_u16(joints)
    weight_blob = _pack_f32(weights)
    ibm_blob = _pack_f32(ibm_hip + ibm_chest)

    idx_view = add_blob(idx_blob, 34963)
    pos_view = add_blob(pos_blob, 34962)
    nrm_view = add_blob(nrm_blob, 34962)
    uv_view = add_blob(uv_blob, 34962)
    joint_view = add_blob(joint_blob, 34962)
    weight_view = add_blob(weight_blob, 34962)
    ibm_view = add_blob(ibm_blob)

    vert_count = len(positions) // 3
    idx_count = len(indices)

    xs = positions[0::3]
    ys = positions[1::3]
    zs = positions[2::3]

    acc_idx = add_accessor(idx_view, 5123, idx_count, "SCALAR", max_v=[float(max(indices))], min_v=[0.0])
    acc_pos = add_accessor(
        pos_view,
        5126,
        vert_count,
        "VEC3",
        max_v=[max(xs), max(ys), max(zs)],
        min_v=[min(xs), min(ys), min(zs)],
    )
    acc_nrm = add_accessor(nrm_view, 5126, vert_count, "VEC3")
    acc_uv = add_accessor(uv_view, 5126, vert_count, "VEC2")
    acc_joints = add_accessor(joint_view, 5123, vert_count, "VEC4")
    acc_weights = add_accessor(weight_view, 5126, vert_count, "VEC4")
    acc_ibm = add_accessor(ibm_view, 5126, 2, "MAT4")

    bin_blob = _pad4(b"".join(bin_parts), 0)

    gltf = {
        "asset": {"version": "2.0", "generator": "minEngine generate_min_skinned_glb.py"},
        "buffers": [{"byteLength": len(bin_blob)}],
        "bufferViews": views,
        "accessors": accessors,
        "meshes": [
            {
                "name": "StickMesh",
                "primitives": [
                    {
                        "attributes": {
                            "POSITION": acc_pos,
                            "NORMAL": acc_nrm,
                            "TEXCOORD_0": acc_uv,
                            "JOINTS_0": acc_joints,
                            "WEIGHTS_0": acc_weights,
                        },
                        "indices": acc_idx,
                        "mode": 4,
                    }
                ],
            }
        ],
        "nodes": [
            {
                "name": "Hip",
                "children": [1],
                "translation": [0.0, 0.0, 0.0],
            },
            {
                "name": "Chest",
                "translation": [0.0, 1.0, 0.0],
            },
            {
                "name": "StickMesh",
                "mesh": 0,
                "skin": 0,
            },
            {
                "name": "Root",
                "children": [0, 2],
            },
        ],
        "skins": [
            {
                "name": "StickSkin",
                "joints": [0, 1],
                "inverseBindMatrices": acc_ibm,
                "skeleton": 0,
            }
        ],
        "scenes": [{"name": "Scene", "nodes": [3]}],
        "scene": 0,
    }

    json_blob = _pad4(
        json.dumps(gltf, separators=(",", ":")).encode("utf-8"),
        0x20,  # space padding per glTF 2.0
    )

    total_length = 12 + 8 + len(json_blob) + 8 + len(bin_blob)
    header = struct.pack("<III", GLB_MAGIC, GLB_VERSION, total_length)
    json_chunk = struct.pack("<II", len(json_blob), CHUNK_JSON) + json_blob
    bin_chunk = struct.pack("<II", len(bin_blob), CHUNK_BIN) + bin_blob
    return header + json_chunk + bin_chunk


def write_sidecar_meta(glb_path: Path, asset_rel: str) -> Path:
    """Optional meta so Content Browser shows SkeletalMesh even before rescan quirks."""
    # Engine GUID is two int64s; generate stable-ish randoms for a fresh asset.
    u = uuid.uuid4()
    high = int.from_bytes(u.bytes[0:8], "little", signed=True)
    low = int.from_bytes(u.bytes[8:16], "little", signed=True)
    meta = {
        "AssetName": glb_path.stem,
        "AssetPath": asset_rel.replace("\\", "/"),
        "AssetType": "SkeletalMesh",
        "Guid": {"High": high, "Low": low},
    }
    meta_path = Path(str(glb_path) + ".meta")
    meta_path.write_text(json.dumps(meta, indent=4) + "\n", encoding="utf-8")
    return meta_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--out",
        type=Path,
        default=DEFAULT_OUT,
        help=f"Output .glb path (default: {DEFAULT_OUT})",
    )
    parser.add_argument(
        "--install",
        action="store_true",
        help="Write into MyMEProject Animations and emit .meta (same as default --out).",
    )
    parser.add_argument(
        "--no-meta",
        action="store_true",
        help="Do not write .meta (engine can still InferAssetType from .glb → SkeletalMesh).",
    )
    args = parser.parse_args()

    out_path: Path = args.out
    if args.install:
        out_path = DEFAULT_OUT

    out_path.parent.mkdir(parents=True, exist_ok=True)
    data = build_glb_bytes()
    out_path.write_bytes(data)
    print(f"Wrote {out_path} ({len(data)} bytes)")

    if not args.no_meta:
        try:
            rel = out_path.relative_to(REPO_ROOT / "minEngine" / "MyMEProject" / "Assets")
            asset_rel = rel.as_posix()
        except ValueError:
            asset_rel = out_path.name
        meta_path = write_sidecar_meta(out_path, asset_rel)
        print(f"Wrote {meta_path} (AssetType=SkeletalMesh, AssetPath={asset_rel})")

    print(
        "Next: open Editor → Content Browser → Animations/MinSkinnedStick.glb → "
        "assign to SkeletalMeshComponent → ResetToBindPose / twist Chest."
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
