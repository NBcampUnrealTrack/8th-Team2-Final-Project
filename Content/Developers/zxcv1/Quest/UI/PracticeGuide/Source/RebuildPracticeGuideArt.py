import json
from pathlib import Path
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
DEST = '/Game/Developers/zxcv1/Quest/UI/PracticeGuide'
M = unreal.MaterialEditingLibrary
E = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

def material(name, code):
    asset = unreal.load_asset(DEST + '/' + name)
    if asset is None:
        asset = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    asset.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    asset.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    M.delete_all_material_expressions(asset)
    uv = M.create_material_expression(asset, unreal.MaterialExpressionTextureCoordinate, -500, 0)
    tex = M.create_material_expression(asset, unreal.MaterialExpressionTextureObject, -500, 150)
    tex.set_editor_property('texture', atlas)
    node = M.create_material_expression(asset, unreal.MaterialExpressionCustom, -150, 0)
    node.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for name in ('UV', 'Atlas'):
        item = unreal.CustomInput()
        item.set_editor_property('input_name', name)
        inputs.append(item)
    node.set_editor_property('inputs', inputs)
    node.set_editor_property('code', code)
    assert M.connect_material_expressions(uv, '', node, 'UV')
    assert M.connect_material_expressions(tex, '', node, 'Atlas')
    color = M.create_material_expression(asset, unreal.MaterialExpressionComponentMask, 150, 0)
    for channel, enabled in [('r', True), ('g', True), ('b', True), ('a', False)]:
        color.set_editor_property(channel, enabled)
    alpha = M.create_material_expression(asset, unreal.MaterialExpressionComponentMask, 150, 120)
    for channel, enabled in [('r', False), ('g', False), ('b', False), ('a', True)]:
        alpha.set_editor_property(channel, enabled)
    assert M.connect_material_expressions(node, '', color, '')
    assert M.connect_material_expressions(node, '', alpha, '')
    assert M.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    assert M.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    M.recompile_material(asset)
    assert E.save_loaded_asset(asset)
    return asset.get_path_name()

task = unreal.AssetImportTask()
task.filename = str(ROOT / 'Content/Developers/zxcv1/Quest/UI/PracticeGuide/Source/ApprovedPracticeGuide.png')
task.destination_path = DEST
task.destination_name = 'T_PracticeGuideReference'
task.automated = True
task.replace_existing = True
task.save = False
tools.import_asset_tasks([task])
atlas = unreal.load_asset(DEST + '/T_PracticeGuideReference')
assert atlas
atlas.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
atlas.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
atlas.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
atlas.set_editor_property('srgb', True)
assert E.save_loaded_asset(atlas)

# Keep the approved ornament pixels and rebuild a continuous clean background.
# Live UMG supplies text, keycaps and pips on top of the resulting frame.
frame = material('M_PracticeGuideFrame', r'''
float2 p = UV * float2(505,620);
float2 q = p + float2(55,162);
float4 c=Texture2DSample(Atlas,AtlasSampler,q/float2(1681,936));
float2 edge=min(p,float2(505,620)-p);
float d=min(min(edge.x,edge.y),edge.x+edge.y-18);
// Preserve the original ornament pixels; use the same clean background across
// all live-text regions so no rectangular erasure patches remain visible.
float3 left=Texture2DSample(Atlas,AtlasSampler,float2(75,q.y)/float2(1681,936)).rgb;
float3 right=Texture2DSample(Atlas,AtlasSampler,float2(540,q.y)/float2(1681,936)).rgb;
float3 background=lerp(left,right,saturate(p.x/505));
bool compass=p.x>20 && p.x<94 && p.y>10 && p.y<88;
bool divider=p.x>35 && p.x<470 && p.y>148 && p.y<168;
bool footer=p.x>25 && p.x<480 && p.y>530 && p.y<536;
float2 s=p-float2(29,412);
float2 se=min(s,float2(448,104)-s);
float sd=min(min(se.x,se.y),(se.x+se.y-10)*.70710678);
bool summaryBorder=sd>=-2 && sd<=2;
if(d>4 && !compass && !divider && !footer && !summaryBorder) c.rgb=background;
if(sd>2) c.rgb=Texture2DSample(Atlas,AtlasSampler,float2(521,q.y)/float2(1681,936)).rgb;
c.a=saturate((edge.x+edge.y-17)/max(fwidth(edge.x+edge.y),.65));
return c;
''')
dodge = material('M_PracticeGuideDodgeExample', r'''
float2 q=float2(1174,585)+UV*float2(400,46);
if(q.x>1220 && q.x<1518 && q.y>597 && q.y<622) q.x=1525;
return float4(Texture2DSample(Atlas,AtlasSampler,q/float2(1681,936)).rgb,1);
''')
out = {'atlas': atlas.get_path_name(), 'materials': [frame, dodge], 'source': task.filename}
report = ROOT/'Saved/Codex/PracticeGuide_20261015/art.json'
report.parent.mkdir(parents=True, exist_ok=True)
report.write_text(json.dumps(out,indent=2),encoding='utf-8')
unreal.log('PRACTICE_GUIDE_ART_DONE ' + json.dumps(out))
