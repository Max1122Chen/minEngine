# save_load_demo.py
import json
import uuid

# ---- Minimal object model ----
class UObject:
    def __init__(self, name, outer=None, is_asset=False):
        self.id = None  # assigned during save (if export)
        self.name = name
        self.outer = outer  # another UObject or None
        self.props = {}     # serializable properties
        self.is_asset = is_asset  # if True treat as external asset (import)
        self.cls = self.__class__.__name__
    def __repr__(self):
        return f"<{self.cls} {self.name}>"

class Actor(UObject): pass
class Component(UObject): pass
class StaticMesh(UObject): pass

# ---- Simple "asset loader" for imports ----
ASSET_STORE = {}  # path -> UObject

def register_asset(path, obj):
    ASSET_STORE[path] = obj

def load_asset(path):
    return ASSET_STORE.get(path)

# ---- Serializer ----
def collect_objects(root_objects):
    # BFS collect all reachable objects via owned subobjects and UPROPERTY-like props
    collected = set()
    queue = list(root_objects)
    while queue:
        o = queue.pop(0)
        if o in collected:
            continue
        collected.add(o)
        # traverse outer/children (subobjects)
        # children = [x for x in all_objects if x.outer == o] - here we just inspect props
        for v in o.props.values():
            if isinstance(v, UObject):
                queue.append(v)
    return collected

def save_package(root_objects, filename):
    # collect graph and assign export ids for non-asset objects that are "owned" or should be exported
    objs = collect_objects(root_objects)
    export_list = []
    import_list = {}  # path -> import index
    obj_to_export_index = {}

    # decide exports: treat object as export if not is_asset and (outer is in collected or None but root)
    for o in objs:
        if not o.is_asset:
            idx = len(export_list)
            o.id = idx
            export_list.append(o)
            obj_to_export_index[o] = idx

    # prepare serialized objects
    serialized = []
    for o in export_list:
        data = {"id": o.id, "class": o.cls, "name": o.name, "outer": None, "props": {}}
        if o.outer and o.outer in obj_to_export_index:
            data["outer"] = obj_to_export_index[o.outer]
        # serialize props: UObject -> export index or import path; non-UObject -> value
        for k,v in o.props.items():
            if isinstance(v, UObject):
                if v.is_asset:
                    # record import
                    path = f"/Asset/{v.name}"
                    if path not in import_list:
                        import_list[path] = len(import_list)
                    data["props"][k] = {"type":"import", "path": path, "import_idx": import_list[path]}
                else:
                    data["props"][k] = {"type":"export_ref", "id": obj_to_export_index.get(v)}
            else:
                data["props"][k] = {"type":"val", "value": v}
        serialized.append(data)

    package = {
        "header": {
            "export_count": len(export_list),
            "imports": list(import_list.keys()),
            "package_id": str(uuid.uuid4())
        },
        "exports": serialized
    }
    with open(filename, "w", encoding="utf-8") as f:
        json.dump(package, f, indent=2)
    print("Saved package:", filename)

def load_package(filename):
    with open(filename, "r", encoding="utf-8") as f:
        package = json.load(f)
    imports = package["header"]["imports"]
    exports = package["exports"]

    # create placeholders for exports
    placeholders = [None] * len(exports)
    for data in exports:
        idx = data["id"]
        # create empty object shell (we use UObject and set cls/name; in real impl you'd instantiate proper class)
        o = UObject(name=data["name"])
        o.cls = data["class"]
        placeholders[idx] = o

    # now fill properties
    for data in exports:
        idx = data["id"]
        o = placeholders[idx]
        for k,entry in data["props"].items():
            if entry["type"] == "val":
                o.props[k] = entry["value"]
            elif entry["type"] == "export_ref":
                refid = entry["id"]
                o.props[k] = placeholders[refid] if refid is not None else None
            elif entry["type"] == "import":
                path = entry["path"]
                asset = load_asset(path)
                if not asset:
                    print(f"Warning: missing asset {path}, leaving None")
                    o.props[k] = None
                else:
                    o.props[k] = asset
    return placeholders

# ---- Demo ----
if __name__ == "__main__":
    # create an external asset and register it
    mesh = StaticMesh("MyMesh", outer=None, is_asset=True)
    register_asset("/Asset/MyMesh", mesh)

    # create actor + component
    actor = Actor("ActorA")
    comp = Component("MeshComp", outer=actor)
    actor.props["component"] = comp
    comp.props["mesh"] = mesh  # component references external asset (StaticMesh)

    # save
    save_package([actor], "pkg.json")

    # load
    objs = load_package("pkg.json")
    print("Loaded objects:", objs)
    # inspect: actor should reference comp; comp.mesh should point to mesh object from ASSET_STORE
    loaded_actor = objs[0]  # in this simple exporter actor is first
    loaded_comp = loaded_actor.props.get("component")
    print("actor -> comp:", loaded_comp)
    if loaded_comp:
        print("comp.mesh:", loaded_comp.props.get("mesh"))
