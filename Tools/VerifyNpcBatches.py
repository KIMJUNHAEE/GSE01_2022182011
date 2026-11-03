"""Read-only checks of real NPC assets against the opaque shared-skeleton batch contract.

Reports feasible draw groups, not measured FPS or actual OpenGL call counts.
Run SimpleGame --verify after a user build for GPU image/order/draw-count comparisons.
"""
import json
from VerifyLowPolyAssets import Glb, ROOT


def main():
    rig = json.loads((ROOT / "Rigged/Data/rigged_asset_catalog.json").read_text(encoding="utf-8"))
    presets = json.loads((ROOT / "Rigged/Data/npc_rig_bindings.json").read_text(encoding="utf-8"))
    parts = {(r["slot"], r["part_index"]): Glb(ROOT / "Rigged" / r["file"])
             for r in rig["npc_parts"]}
    before_counts, after_counts = [], []
    for preset in presets["npc_presets"]:
        original = 0
        shading = set()
        signature = None
        for slot, index in preset["part_indices"].items():
            if index < 0:
                continue
            glb = parts[slot, index]
            data = glb.data
            scene_nodes = []

            def visit(node):
                scene_nodes.append(node)
                for child in data["nodes"][node].get("children", []):
                    visit(child)

            for node in data["scenes"][data.get("scene", 0)]["nodes"]:
                visit(node)
            for node_index in scene_nodes:
                node = data["nodes"][node_index]
                if "mesh" not in node:
                    continue
                assert "skin" in node, (preset["npc_id"], slot, "rigid part requires fallback")
                skin = data["skins"][node["skin"]]
                candidate = ([data["nodes"][n]["name"] for n in skin["joints"]],
                             glb.accessor(skin["inverseBindMatrices"]))
                if signature is None:
                    signature = candidate
                assert signature == candidate, "Joint ordering/bind matrices would change skinning"
                for primitive in data["meshes"][node["mesh"]]["primitives"]:
                    original += 1
                    material = data["materials"][primitive["material"]]
                    assert material.get("alphaMode", "OPAQUE") != "BLEND"
                    color = material.get("pbrMetallicRoughness", {}).get("baseColorFactor", [1]*4)
                    assert color[3] >= .999, "Translucent base color requires the ordered path"
                    colors = primitive["attributes"].get("COLOR_0")
                    if colors is not None:
                        assert all(len(c) == 3 or c[3] >= .999 for c in glb.accessor(colors))
                    shading.add((tuple(material.get("emissiveFactor", [0]*3)),
                                 material.get("alphaCutoff", .5) if material.get("alphaMode") == "MASK" else 0,
                                 "KHR_materials_unlit" in material.get("extensions", {})))
        assert signature and 0 < len(shading) < original, preset["npc_id"]
        before_counts.append(original)
        after_counts.append(len(shading))
    print(f"PASS {len(parts)} rigged parts / {len(before_counts)} NPC presets: "
          f"{min(before_counts)}-{max(before_counts)} primitives -> "
          f"{min(after_counts)}-{max(after_counts)} compatible opaque draw group(s) per NPC.")
    print("Ordered joints, inverse binds and vertex alpha verified. GPU/FPS was not measured.")


if __name__ == "__main__":
    main()
