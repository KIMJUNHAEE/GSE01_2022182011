"""Read-only GLB/metadata compatibility checks. Does not build or launch the C++ game."""
from pathlib import Path
import json
import math
import re
import struct

ROOT = Path(__file__).resolve().parents[1] / "SimpleGame/Resources/Art/LowPoly"
FORMATS = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}
WIDTHS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def supports_schema(version, major):
    if not isinstance(version, str) or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        return False
    parts = [int(part) for part in version.split(".")]
    return parts[0] == major and all(part <= 0xFFFFFFFF for part in parts)


def metadata(relative, major):
    data = json.loads((ROOT / relative).read_text(encoding="utf-8"))
    assert supports_schema(data.get("schema_version"), major), (
        f"Unsupported schema in {relative}: {data.get('schema_version')} (expected {major}.x)")
    return data


class Glb:
    def __init__(self, path):
        self.path = path
        raw = path.read_bytes()
        magic, version, length = struct.unpack_from("<III", raw)
        assert magic == 0x46546C67 and version == 2 and length == len(raw), path
        offset, chunks = 12, {}
        while offset < length:
            size, kind = struct.unpack_from("<II", raw, offset)
            assert size % 4 == 0 and offset + 8 + size <= length
            chunks[kind] = raw[offset+8:offset+8+size]
            offset += 8 + size
        self.data = json.loads(chunks[0x4E4F534A])
        self.binary = chunks.get(0x004E4942, b"")
        assert not self.data.get("textures"), f"Flat-color renderer does not support textures: {path}"
        assert all("uri" not in b for b in self.data.get("buffers", []))

    def accessor(self, index):
        a = self.data["accessors"][index]
        assert "sparse" not in a, f"Current art unexpectedly has sparse data: {self.path}"
        view = self.data["bufferViews"][a["bufferView"]]
        components = WIDTHS[a["type"]]
        fmt = "<" + FORMATS[a["componentType"]] * components
        size = struct.calcsize(fmt)
        stride = view.get("byteStride", size)
        base = view.get("byteOffset", 0) + a.get("byteOffset", 0)
        assert a.get("byteOffset", 0) + (a["count"]-1)*stride + size <= view["byteLength"]
        values = [struct.unpack_from(fmt, self.binary, base+i*stride) for i in range(a["count"])]
        if a.get("normalized"):
            maximum = {5120: 127, 5121: 255, 5122: 32767, 5123: 65535}[a["componentType"]]
            values = [tuple(max(-1, v/maximum) for v in row) for row in values]
        assert all(math.isfinite(v) for row in values for v in row)
        return values

    def verify(self):
        d = self.data
        for skin in d.get("skins", []):
            assert 0 < len(skin["joints"]) <= 128
            assert len({d["nodes"][j]["name"] for j in skin["joints"]}) == len(skin["joints"])
            assert len(self.accessor(skin["inverseBindMatrices"])) == len(skin["joints"])
        for node in d["nodes"]:
            if "mesh" not in node:
                continue
            for p in d["meshes"][node["mesh"]]["primitives"]:
                assert p.get("mode", 4) == 4 and "targets" not in p and "NORMAL" in p["attributes"]
                attributes = {key: self.accessor(a) for key, a in p["attributes"].items()}
                count = len(attributes["POSITION"])
                assert len(attributes["NORMAL"]) == count
                indices = self.accessor(p["indices"]) if "indices" in p else [(i,) for i in range(count)]
                assert len(indices) % 3 == 0 and all(0 <= i[0] < count for i in indices)
                if "skin" in node:
                    joint_count = len(d["skins"][node["skin"]]["joints"])
                    assert "JOINTS_0" in attributes and "WEIGHTS_0" in attributes
                    assert "JOINTS_2" not in attributes
                    for i in range(count):
                        total = 0
                        for group in (0, 1):
                            if f"JOINTS_{group}" not in attributes:
                                continue
                            for joint, weight in zip(attributes[f"JOINTS_{group}"][i], attributes[f"WEIGHTS_{group}"][i]):
                                assert 0 <= joint < joint_count and weight >= 0
                                total += weight
                        assert abs(total-1) < .0002, (self.path, total)
        for clip in d.get("animations", []):
            for channel in clip["channels"]:
                assert channel["target"]["path"] in {"translation", "rotation", "scale"}
                sampler = clip["samplers"][channel["sampler"]]
                times = self.accessor(sampler["input"])
                assert times and times[0][0] >= 0 and all(a[0] < b[0] for a, b in zip(times, times[1:]))
                interpolation = sampler.get("interpolation", "LINEAR")
                assert interpolation in {"STEP", "LINEAR", "CUBICSPLINE"}
                output = self.accessor(sampler["output"])
                assert len(output) == len(times)*(3 if interpolation == "CUBICSPLINE" else 1)

    def joint_signature(self):
        skin = self.data["skins"][0]
        matrices = self.accessor(skin["inverseBindMatrices"])
        return {self.data["nodes"][j]["name"]: matrix for j, matrix in zip(skin["joints"], matrices)}


def main():
    assert all(supports_schema(v, 2) for v in ("2.0.0", "2.1.0", "2.1.7", "2.10.0"))
    assert not any(supports_schema(v, 2) for v in (
        "1.0.0", "3.0.0", "2", "2.1", "2.1.0.0", "2.x.0", "2.1.0x", "2.-1.0",
        "2.1.999999999999999999999"))
    catalog = metadata("Data/asset_catalog.json", 1)
    presets = metadata("Rigged/Data/npc_rig_bindings.json", 2)["npc_presets"]
    palettes = metadata("Data/palettes.json", 1)["palettes"]
    rigs = metadata("Rigged/Data/rigged_asset_catalog.json", 2)
    clips = metadata("Rigged/Data/animation_clips.json", 2)["clips"]
    assert catalog["units"] == "meters" and catalog["glb_axes"] == {"up": "+Y", "front": "+Z"}
    assert len(rigs["npc_parts"]) == 26 and len(rigs["mobs"]) == 7
    rig_ids = {row["rig_id"] for row in rigs["npc_parts"]} | {row["rig_name"] for row in rigs["mobs"]}
    for row in rigs["npc_parts"]:
        assert isinstance(row["asset_id"], str) and isinstance(row["rig_id"], str)
    for row in rigs["mobs"]:
        assert isinstance(row["mesh_name"], str) and isinstance(row["rig_name"], str)
    for row in clips:
        assert row["rig_id"] in rig_ids and isinstance(row["clip_id"], str)
        assert isinstance(row["loop"], bool)
        assert math.isfinite(row.get("suggested_speed_mps", 0)) and row.get("suggested_speed_mps", 0) >= 0
        assert math.isfinite(row.get("heading_delta_degrees", 0))
    glbs = {}
    for path in sorted(ROOT.rglob("*.glb")):
        glb = Glb(path)
        glb.verify()
        glbs[path] = glb
    for row in catalog["assets"]:
        assert (ROOT / row["file"]).is_file(), row["asset_id"]
    slots = {key: {row["index"]: row["asset_id"] for row in entries} for key, entries in catalog["slots"].items()}
    canonical = glbs[ROOT / "Rigged/Meshes/NPC/SM_NPC_BODY_000_Male.glb"].joint_signature()
    shared_parts = 0
    for path, glb in glbs.items():
        if path.parent != ROOT / "Rigged/Meshes/NPC" or not path.name.startswith("SM_NPC_"):
            continue
        signature = glb.joint_signature()
        assert signature.keys() == canonical.keys(), path
        assert all(max(abs(a-b) for a, b in zip(signature[name], canonical[name])) < .0001 for name in canonical), path
        assert {clip["name"] for clip in glb.data["animations"]} >= {"Idle", "Walk", "Run", "Jump"}
        shared_parts += 1
    for preset in presets:
        parts = preset["part_indices"]
        assert parts["headgear"] < 0 or parts["hair"] == -1
        for slot, index in parts.items():
            assert index in slots[slot] or (index == -1 and slot in {"hair", "accessory", "headgear"})
        for role, index in preset["palette_indices"].items():
            assert index in {row["index"] for row in palettes[role]}
    binding_data = json.loads((ROOT.parents[1] / "LowPolyBindings.json").read_text(encoding="utf-8"))
    assert binding_data["schema_version"] == 2
    bindings = binding_data["bindings"]
    known_ids = {p.stem for p in glbs}
    preset_ids = {p["npc_id"] for p in presets}
    used = set()
    for key, binding in bindings.items():
        assert binding["assets"] and all(value in known_ids or value in preset_ids for value in binding["assets"]), key
        scale = binding.get("scale", 1)
        assert math.isfinite(scale) and scale > 0, key
        assert not binding.get("fit_footprint", False) or scale <= 1, key
        assert all((value in preset_ids) == key.startswith("NPC.") for value in binding["assets"]), key
        used.update(binding["assets"])
    mobs = {p.stem for p in glbs if p.parent.name == "Mobs"}
    assert len(mobs) == 7 and mobs <= used, "Every completed mob must have a runtime binding"
    assert preset_ids <= used, "Every completed NPC preset must have a runtime binding"
    scenery_source = (ROOT.parents[3] / "SimpleGame/PlanetGeneration.cpp").read_text(encoding="utf-8")
    for key in re.findall(r'"((?:Environment|Prop)\.[^"]+)"', scenery_source):
        assert key in bindings, f"Unbound village scenery: {key}"
    for key in ("Prop.Lamp", "Prop.Relay", "NPC.Story", "NPC.Captain"):
        assert not bindings[key].get("fit_footprint", False), f"Narrow collision boxes shrink {key}"
    assert all(row["loop"] == (row["clip_id"] in {"Idle", "Walk", "Run"}) for row in clips)
    print(f"PASS {len(glbs)} GLBs: geometry, normals, indices, weights, inverse binds, animation accessors")
    print(f"PASS {len(presets)} NPC presets, palettes, runtime bindings, {shared_parts} parts sharing one named skeleton")
    print(f"PASS {len(clips)} rig clip metadata entries; read-only asset compatibility check only")
    print(f"PASS {len(bindings)} runtime visual profiles: all 7 mobs, 16 NPC presets, village scenery")
    print(f"PASS schema compatibility: rig catalog {rigs['schema_version']}, required loader metadata fields, invalid/major-version rejection")
    print("NOTE: C++ loader / shaders / GPU rendering were not compiled or run.")


if __name__ == "__main__":
    main()
