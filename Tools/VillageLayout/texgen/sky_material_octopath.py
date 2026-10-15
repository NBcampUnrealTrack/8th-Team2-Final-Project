import unreal, json
out={}
MAT='/Game/Village/Materials/M_HD2D_Sky'; MIP='/Game/Village/Materials/MI_HD2D_Sky'
MNS=unreal.MaterialNodeService; MS=unreal.MaterialService
EAS=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LS=unreal.LandscapeService
# move the villager that snapped onto a rooftop
for a in EAS.get_all_level_actors():
    if a.get_actor_label()=='Villager_NW':
        pass
        out['villager_nw']='already placed'
unreal.SystemLibrary.execute_console_command(None, 'r.LumenScene.SurfaceCache.CardCapturesPerFrame 0')
g=json.loads(MNS.export_material_graph(MAT))
cid=[e['id'] for e in g['expressions'] if 'Custom' in e['class']][0]
HLSL = r'''
float3 d = normalize(Dir);
float h = d.z;

// pastel vertical gradient: warm horizon -> pale mid -> soft blue zenith
float t1 = saturate(h / 0.26);
float3 sky = lerp(HorizonCol, MidCol, pow(t1, 0.85));
float t2 = saturate((h - 0.26) / 0.74);
sky = lerp(sky, ZenithCol, pow(t2, 0.75));

float u = atan2(d.y, d.x) * 0.15915494 + 0.5;
float v = 0.5 - asin(clamp(d.z, -1.0, 1.0)) * 0.31830989;

float below = saturate((v - 0.5002) / 0.030);
float deep = saturate((v - 0.505) / 0.10);
sky = lerp(sky, lerp(GroundCol * 1.9, GroundCol * 0.55, deep), below);

// painted clouds first, so the horizon haze can wash them out with distance
float2 p = d.xy / max(h, 0.11);
float2 uv1 = p * Tune.y + float2(Tune.z * Time, Tune.z * Time * 0.32);
float2 uv2 = p * (Tune.y * 1.9) + float2(-Tune.z * Time * 1.6, Tune.z * Time * 0.5);
float c1 = Texture2DSample(SkyTex, SkyTexSampler, uv1).r;
float c2 = Texture2DSample(SkyTex, SkyTexSampler, uv2).g;
float fade = smoothstep(0.16, 0.78, h);
float a1 = saturate((c1 - Tune.x) * 3.0) * fade * 0.85;
float a2 = saturate((c2 - Tune.x * 1.2) * 2.4) * fade * 0.38;
// clouds pick up warm light on the side facing the sun
float sunSide = saturate(dot(normalize(float3(SunDir.xy, 0.0)), normalize(float3(d.xy, 0.0)))) * 0.5 + 0.5;
float3 litCloud = lerp(CloudCol, CloudCol * SunCol * 1.25, sunSide * 0.55);
float3 cloudShaded = lerp(CloudShadow, litCloud, saturate((c1 - Tune.x) * 2.2));
sky = lerp(sky, cloudShaded, a1);
sky = lerp(sky, litCloud, a2);

// three ranges from one crest profile, each washed further into the haze
float tFar  = Texture2DSample(SkyTex, SkyTexSampler, float2(u, 0.5)).b;
float tMid  = Texture2DSample(SkyTex, SkyTexSampler, float2(u * 1.9 + 0.31, 0.5)).b;
float tNear = Texture2DSample(SkyTex, SkyTexSampler, float2(u * 3.3 + 0.63, 0.5)).b;
float cFar  = 0.4972 - tFar  * 0.0235;
float cMid  = 0.4986 - tMid  * 0.0140;
float cNear = 0.4996 - tNear * 0.0078;
float aa = 0.0006;
float3 hazeCol = lerp(HorizonCol, MidCol, 0.35);
sky = lerp(sky, lerp(hazeCol, RidgeFar, 0.30), smoothstep(cFar - aa, cFar + aa, v));
sky = lerp(sky, lerp(RidgeFar, RidgeNear, 0.45), smoothstep(cMid - aa, cMid + aa, v));
sky = lerp(sky, RidgeNear, smoothstep(cNear - aa, cNear + aa, v));

// milky aerial-perspective band hugging the skyline, applied over everything distant
float haze = exp(-max(h, 0.0) * 42.0) * step(0.0, h);
sky = lerp(sky, hazeCol, haze * 0.30);

// sun
float sd = saturate(dot(d, normalize(SunDir)));
sky += SunCol * pow(sd, 90.0) * 1.2;
sky += SunCol * pow(sd, 5.0) * 0.16;
sky = lerp(sky, SunCol * 2.4, smoothstep(0.99930, 0.99968, sd));

return sky;
'''
out['code']=MNS.set_expression_property(MAT, cid, 'Code', HLSL)
MS.set_instance_vector_parameter(MIP,'HorizonCol',0.98,0.82,0.63,1.0)
MS.set_instance_vector_parameter(MIP,'MidCol',0.56,0.70,0.82,1.0)
MS.set_instance_vector_parameter(MIP,'ZenithCol',0.24,0.42,0.66,1.0)
MS.set_instance_vector_parameter(MIP,'CloudCol',1.10,1.05,0.98,1.0)
MS.set_instance_vector_parameter(MIP,'CloudShadow',0.62,0.66,0.76,1.0)
MS.set_instance_vector_parameter(MIP,'RidgeFar',0.55,0.63,0.75,1.0)
MS.set_instance_vector_parameter(MIP,'GroundCol',0.030,0.038,0.052,1.0)
MS.set_instance_vector_parameter(MIP,'RidgeNear',0.24,0.30,0.40,1.0)
MS.set_instance_vector_parameter(MIP,'Tune',0.36,0.46,0.0011,1.0)
out['compiled']=MS.compile_material(MAT)
dg=MNS.get_material_diagnostics(MAT)
out['diag_ok']=getattr(dg,'is_compiled_ok',None); out['errs']=[str(e) for e in getattr(dg,'compile_errors',[])][:5]
mi=unreal.load_asset(MIP); unreal.MaterialEditingLibrary.update_material_instance(mi)
unreal.EditorAssetLibrary.save_asset(MAT); unreal.EditorAssetLibrary.save_asset(MIP)
unreal.SystemLibrary.execute_console_command(None, 'r.LumenScene.SurfaceCache.CardCapturesPerFrame 3')
EAS2=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
A2={a.get_actor_label():a for a in EAS2.get_all_level_actors()}
A2['SkyLight_Fill'].get_component_by_class(unreal.SkyLightComponent).set_editor_property('intensity', 6.0)
_ppv=A2['PPV_Village']; _s=_ppv.get_editor_property('settings')
_s.set_editor_property('auto_exposure_bias', -0.8)
_ppv.set_editor_property('settings', _s)
unreal.LightingService.recapture_sky('SkyLight_Fill')
LES=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem); out['saved']=LES.save_current_level()
print(json.dumps(out, default=str))
