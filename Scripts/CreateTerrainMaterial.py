"""
Simple terrain material: Layer0Texture -> BaseColor
Run in UE5 editor: -ExecCmds="py Scripts/CreateTerrainMaterial.py"
"""
import unreal

# Delete existing
if unreal.EditorAssetLibrary.does_asset_exist("/Game/Materials/M_WowTerrain"):
    unreal.EditorAssetLibrary.delete_asset("/Game/Materials/M_WowTerrain")

factory = unreal.MaterialFactoryNew()
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_WowTerrain", "/Game/Materials", unreal.Material, factory)

# Create all 7 texture parameter samplers
param_names = [
    "Layer0Texture", "Layer1Texture", "Layer2Texture", "Layer3Texture",
    "AlphaMap1", "AlphaMap2", "AlphaMap3"
]

samplers = []
for i, name in enumerate(param_names):
    s = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionTextureSampleParameter2D, -400, i * 180)
    s.set_editor_property("parameter_name", name)
    samplers.append(s)

# Wire Layer0Texture directly to BaseColor (simplest working material)
unreal.MaterialEditingLibrary.connect_material_property(
    samplers[0], "", unreal.MaterialProperty.MP_BASE_COLOR)

# Recompile and save
unreal.MaterialEditingLibrary.recompile_material(mat)
unreal.EditorAssetLibrary.save_asset("/Game/Materials/M_WowTerrain")

unreal.log("=== M_WowTerrain created with {} params, saved ===".format(len(param_names)))
