"""Copy EngineDefault BasicShapes into MyMEProject with relative meta paths and preserved GUIDs."""
import json
import shutil
from pathlib import Path

SRC_ROOT = Path(__file__).resolve().parents[1] / "minEngine/Assets/EngineDefault/Meshes/BasicShapes"
DST_ROOT = Path(__file__).resolve().parents[1] / "minEngine/MyMEProject/Assets/Meshes/BasicShapes"


def main() -> None:
    DST_ROOT.mkdir(parents=True, exist_ok=True)

    for obj_path in sorted(SRC_ROOT.glob("*.obj")):
        meta_src = obj_path.with_suffix(obj_path.suffix + ".meta")
        if not meta_src.exists():
            print(f"skip (no meta): {obj_path.name}")
            continue

        data = json.loads(meta_src.read_text(encoding="utf-8"))
        rel_path = f"Meshes/BasicShapes/{obj_path.name}"

        shutil.copy2(obj_path, DST_ROOT / obj_path.name)

        out_meta = {
            "AssetName": data["AssetName"],
            "AssetPath": rel_path,
            "AssetType": data["AssetType"],
            "Guid": data["Guid"],
        }
        meta_dst = DST_ROOT / f"{obj_path.name}.meta"
        meta_dst.write_text(json.dumps(out_meta, indent=4) + "\n", encoding="utf-8")
        print(f"OK {rel_path}  Guid.High={out_meta['Guid']['High']}")


if __name__ == "__main__":
    main()
