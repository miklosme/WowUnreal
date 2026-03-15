"""
Create/update the WoW terrain preview material (M_WowTerrainPreview).
Run in UE editor: Tools -> Execute Python Script
Or via commandlet: UnrealEditor project.uproject -run=pythonscript -script=Scripts/CreateTerrainPreviewMaterial.py

4-layer splatmap material:
  - UV1 = tiled UVs for texture sampling
  - UV0 = per-chunk [0,1] for alpha/splatmap lookup
  - Layer0Texture..Layer3Texture = diffuse textures
  - AlphaMap1..AlphaMap3 = blend maps
  - Roughness=1.0, Specular=0.0
"""
import unreal

ASSET_PATH = "/Game/Wow/Materials/M_WowTerrainPreview"
PACKAGE_PATH = "/Game/Wow/Materials"
ASSET_NAME = "M_WowTerrainPreview"

editing = unreal.MaterialEditingLibrary

# Delete and recreate for clean state
if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
    unreal.EditorAssetLibrary.delete_asset(ASSET_PATH)

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
factory = unreal.MaterialFactoryNew()
material = asset_tools.create_asset(ASSET_NAME, PACKAGE_PATH, unreal.Material, factory)
if material is None:
    raise RuntimeError(f"Failed to create {ASSET_PATH}")

material.set_editor_property("two_sided", True)

# UV1 for tiled texture sampling
uv_tiled = editing.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -800, 0)
uv_tiled.set_editor_property("coordinate_index", 1)

# UV0 for splatmap/alpha lookup [0,1] per chunk
uv_alpha = editing.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -800, 500)
uv_alpha.set_editor_property("coordinate_index", 0)

# 4 layer texture samplers on UV1 (tiled)
layers = []
for i in range(4):
    s = editing.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, i * 220)
    s.set_editor_property("parameter_name", f"Layer{i}Texture")
    editing.connect_material_expressions(uv_tiled, "", s, "UVs")
    layers.append(s)

# 3 alpha map samplers on UV0 (splatmap)
alphas = []
for i in range(3):
    s = editing.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 950 + i * 220)
    s.set_editor_property("parameter_name", f"AlphaMap{i+1}")
    editing.connect_material_expressions(uv_alpha, "", s, "UVs")
    alphas.append(s)

# Blend: lerp chain  result = lerp(lerp(lerp(L0, L1, A1), L2, A2), L3, A3)
result = layers[0]
for i in range(3):
    lerp = editing.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, 0 + i * 250, 400)
    editing.connect_material_expressions(result, "", lerp, "A")
    editing.connect_material_expressions(layers[i+1], "", lerp, "B")
    editing.connect_material_expressions(alphas[i], "", lerp, "Alpha")
    result = lerp

# Roughness=1, Specular=0
rough = editing.create_material_expression(material, unreal.MaterialExpressionConstant, 600, 200)
rough.set_editor_property("r", 1.0)
spec = editing.create_material_expression(material, unreal.MaterialExpressionConstant, 600, 300)
spec.set_editor_property("r", 0.0)

# Connect outputs
editing.connect_material_property(result, "", unreal.MaterialProperty.MP_BASE_COLOR)
editing.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
editing.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)

editing.layout_material_expressions(material)
editing.recompile_material(material)
unreal.EditorAssetLibrary.save_asset(ASSET_PATH)
unreal.log(f"SUCCESS: Created {ASSET_PATH} with UV1 tiled textures, UV0 splatmaps")
