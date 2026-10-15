"""Confirmation reference authoring; artwork is sampled from the uploaded asset."""
import unreal, json, shutil
from pathlib import Path
from datetime import datetime

ROOT = Path(unreal.Paths.project_dir()).resolve()
PATH = '/Game/Developers/dbals/UI/WBP_ConfirmDialog'
ART = '/Game/Developers/MinkiCho/Art/UITheme/PopupReference'
REF = '/Game/Developers/MinkiCho/Art/UITheme/PopupReference/Textures/T_ConfirmPopupAtlas'
W, B, E, M = unreal.WidgetService, unreal.BlueprintService, unreal.EditorAssetLibrary, unreal.MaterialEditingLibrary

def material(name, code, width, height):
    path = ART + '/' + name
    mat = unreal.load_asset(path)
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, ART, unreal.Material, unreal.MaterialFactoryNew())
    mat.modify()
    M.delete_all_material_expressions(mat)
    mat.set_editor_property('material_domain', unreal.MaterialDomain.MD_UI)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = M.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -600, 0)
    tex = M.create_material_expression(mat, unreal.MaterialExpressionTextureObject, -600, 160)
    tex.set_editor_property('texture', unreal.load_asset(REF))
    custom = M.create_material_expression(mat, unreal.MaterialExpressionCustom, -200, 0)
    inputs = []
    for key in ['UV', 'Art']:
        item = unreal.CustomInput(); item.set_editor_property('input_name', key); inputs.append(item)
    custom.set_editor_property('inputs', inputs)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    custom.set_editor_property('code', code)
    M.connect_material_expressions(uv, '', custom, 'UV')
    M.connect_material_expressions(tex, '', custom, 'Art')
    for channel, prop in [('RGB', unreal.MaterialProperty.MP_EMISSIVE_COLOR), ('A', unreal.MaterialProperty.MP_OPACITY)]:
        mask = M.create_material_expression(mat, unreal.MaterialExpressionComponentMask, 100, 0 if channel == 'RGB' else 160)
        for c in ['r', 'g', 'b', 'a']: mask.set_editor_property(c, c.upper() in channel)
        assert M.connect_material_expressions(custom, '', mask, '')
        M.connect_material_property(mask, '', prop)
    errors = M.recompile_material(mat)
    assert not errors, errors
    assert E.save_loaded_asset(mat)
    return mat

def sample(p):
    return 'Texture2DSample(Art,ArtSampler,((' + p + ')-float2(502,240))/float2(668,382)).rgb'

def brush(mat, width=32, height=32):
    br = unreal.SlateBrush(); br.resource_object = mat
    br.draw_as = unreal.SlateBrushDrawType.IMAGE
    size = br.image_size.copy(); size.set_editor_property('x',width); size.set_editor_property('y',height); br.image_size = size
    br.tint_color = unreal.SlateColor(specified_color=unreal.LinearColor(1,1,1,1))
    return br

def apply():
    assert not W.is_pie_running()
    out = ROOT/'Saved/UIPopupReference'/datetime.now().strftime('%Y%m%d_%H%M%S')
    out.mkdir(parents=True)
    shutil.copy2(ROOT/'Content'/(PATH[6:]+'.uasset'), out/'WBP_ConfirmDialog.uasset')
    bp = unreal.load_asset(PATH); bp.modify(); tree = unreal.find_object(bp,'WidgetTree')
    (out/'before.txt').write_text('\n'.join(s.widget_name+' '+s.slot_info.export_text() for s in W.get_widget_snapshot(PATH)),encoding='utf-8')
    for src in E.list_assets(ART,True,False) if E.does_directory_exist(ART) else []:
        f=ROOT/'Content'/(src.split('.')[0][6:]+'.uasset')
        if f.exists(): shutil.copy2(f,out/f.name)
    def obj(n):
        result=unreal.find_object(tree,n);assert result,n;return result
    def rect(n,x,y,w,h,z=1):
        o=obj(n);o.modify();s=o.slot
        s.set_anchors(unreal.Anchors(unreal.Vector2D(),unreal.Vector2D()))
        s.set_alignment(unreal.Vector2D());s.set_auto_size(False)
        s.set_position(unreal.Vector2D(x,y));s.set_size(unreal.Vector2D(w,h));s.set_z_order(z)
        return o
    if not unreal.find_object(tree,'ReferenceFit'):
        for cls,n,parent in [('ScaleBox','ReferenceFit','Root'),('SizeBox','ReferenceStageSize','ReferenceFit'),('CanvasPanel','ReferenceStage','ReferenceStageSize')]:
            assert W.add_component(PATH,cls,n,parent,True).success
        panel=obj('PanelBG');panel.get_parent().remove_child(panel);obj('ReferenceStage').add_child_to_canvas(panel)
    fit=rect('ReferenceFit',0,0,0,0,1)
    fit.slot.set_anchors(unreal.Anchors(unreal.Vector2D(),unreal.Vector2D(1,1)))
    fit.slot.set_offsets(unreal.Margin())
    fit.set_editor_property('stretch',unreal.Stretch.SCALE_TO_FIT)
    fit.set_editor_property('stretch_direction',unreal.StretchDirection.BOTH)
    obj('ReferenceStageSize').set_width_override(1672);obj('ReferenceStageSize').set_height_override(941)
    # Frame, medallion, header rule and watermark retain source pixels. Only
    # baked text/interactive art is removed; live widgets own those elements.
    code = 'float2 p=float2(510,248)+UV*float2(652,366); float2 q=p;\n'
    code += 'if(p.x>590&&p.x<659&&p.y>292&&p.y<326)q.x=670;\n'
    code += 'if(p.x>717&&p.x<954&&p.y>410&&p.y<453)q.x=700;\n'
    code += 'if(p.x>1095&&p.x<1128&&p.y>295&&p.y<326)q.x=1085;\n'
    code += 'if(p.y>=507&&p.y<=568&&((p.x>=550&&p.x<=821)||(p.x>=850&&p.x<=1123)))q.y=580;\n'
    code += 'float3 c='+sample('q')+';\n'
    code += 'float2 edge=min(p-float2(514,264),float2(1158,600)-p);float d=min(min(edge.x,edge.y),edge.x+edge.y-5);float inside=saturate((d+.5)/max(.65,fwidth(d)));\n'
    code += 'float star=step(abs(p.x-835.5),21)*max(step(p.y,281),step(582,p.y));\n'
    code += 'float a= max(inside,star*smoothstep(.035,.18,c.r-c.b));\n'
    code += 'return float4(c,a);'
    panel=material('M_ReferencePanel',code,652,366)
    mats={}
    for kind,x,w in [('Primary',550,270),('Secondary',850,272)]:
        for state,factor in [('Normal',1),('Hover',1.15),('Pressed',.88)]:
            c=f'float2 p=float2({x},507)+UV*float2({w},60);float2 q=p;'
            c+=f'if(p.x>{x+100}&&p.x<{x+171}&&p.y>522&&p.y<553)q.x={x+30};'
            c+='float3 c='+sample('q')+';'
            # The native destructive tint is applied after material rendering.
            if kind=='Primary': c+='c/=float3(.55,.16,.16);'
            c+=f'c*={factor};float2 a=UV*float2({w},60);float d=min(min(a.x,{w}-a.x),min(a.y,60-a.y));'
            c+=f'float cut=min(min(a.x+a.y-10,{w}-a.x+a.y-10),min(a.x+60-a.y-10,{w}-a.x+60-a.y-10));return float4(c,saturate(min(d,cut)/max(.5,fwidth(d))));'
            mats[kind+state]=material('M_Reference'+kind+state,c,w,60)
    xcode='float2 p=float2(1099,298)+UV*float2(26,25);float3 c='+sample('p')+';return float4(c,smoothstep(.025,.20,c.r-c.b));'
    cross=material('M_ReferenceClose',xcode,26,25)
    panelobj=obj('PanelBG'); panelobj.set_padding(unreal.Margin());panelobj.set_brush(brush(panel,652,366));panelobj.set_brush_color(unreal.LinearColor(1,1,1,1))
    panelobj.set_render_transform_pivot(unreal.Vector2D(.5,.5))
    panelobj.set_render_scale(unreal.Vector2D(.75,.75))
    slot=panelobj.slot;slot.set_anchors(unreal.Anchors(unreal.Vector2D(.5,.5),unreal.Vector2D(.5,.5)))
    slot.set_alignment(unreal.Vector2D(.5,.5));slot.set_position(unreal.Vector2D(0,-39.5));slot.set_size(unreal.Vector2D(652,366))
    # Old intro animates the full margin struct, including its static width
    # and position defaults. Preserve the intro while updating that contract.
    anim=unreal.find_object(bp,'PanelIntro')
    for binding in anim.get_bindings():
        for track in binding.get_tracks():
            if str(track.get_property_name())=='Offsets':
                for section in track.get_sections():
                    for i,channel in enumerate(section.get_all_channels()):
                        channel.set_default([0,-39.5,652,366][i])
                        if i==3:
                            for key in channel.get_keys():
                                frame=key.get_time().frame_number.value;key.set_value(12 if frame==0 else 377 if frame==3 else 366)
    obj('ConfirmLayout').slot.set_padding(unreal.Margin())
    for n in ['AlertSeal','AlertHeaderRule','ConfirmFooterRule']:obj(n).set_visibility(unreal.SlateVisibility.COLLAPSED)
    font_asset=unreal.load_asset('/Game/Art/Font/MaruBuri-SemiBold_Font')
    def font(n,size,color):
        o=obj(n);f=o.get_editor_property('font').copy();f.font_object=font_asset;f.typeface_font_name='Default';f.size=size;f.outline_settings.outline_size=0;o.set_font(f)
        o.set_render_translation(unreal.Vector2D());o.set_color_and_opacity(unreal.SlateColor(specified_color=color))
    gold=unreal.LinearColor(.69,.51,.26,1);ivory=unreal.LinearColor(.85,.86,.84,1)
    rect('AlertHeading',88,50,200,38);font('AlertHeading',18,gold)
    rect('ConfirmMessageScroll',36,171,580,58);obj('ConfirmMessageScroll').set_scroll_bar_visibility(unreal.SlateVisibility.COLLAPSED)
    msg=obj('MessageText');font('MessageText',20,ivory);msg.set_editor_property('justification',unreal.TextJustify.CENTER)
    msg.slot.set_padding(unreal.Margin());msg.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    # Keep the native notice-only collapse behavior by keeping an autosized
    # centered action row; each actual Button owns all four state brushes.
    actions=obj('ConfirmActions');s=actions.slot;s.set_anchors(unreal.Anchors(unreal.Vector2D(.5,0),unreal.Vector2D(.5,0)));s.set_alignment(unreal.Vector2D(.5,0));s.set_position(unreal.Vector2D(0,259));s.set_auto_size(True)
    for n,kind,w in [('ConfirmButton','Primary',270),('CancelButton','Secondary',272)]:
        box=obj('ConfirmButtonSize' if kind=='Primary' else 'CancelButtonSize');box.set_width_override(w);box.set_height_override(60)
        o=obj(n);st=o.get_editor_property('widget_style').copy()
        for state in ['normal','hovered','pressed','disabled']:
            setattr(st,state,brush(mats[kind+({'normal':'Normal','hovered':'Hover','pressed':'Pressed','disabled':'Normal'}[state])],w,60))
        st.normal_padding=unreal.Margin();st.pressed_padding=unreal.Margin()
        for foreground in ['normal_foreground','hovered_foreground','pressed_foreground']:setattr(st,foreground,unreal.SlateColor(specified_color=unreal.LinearColor(1,1,1,1)))
        o.set_style(st)
        o.slot.set_padding(unreal.Margin(0 if kind=='Primary' else 30,0,0,0));o.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_FILL)
    for n in ['ConfirmLabelText','CancelLabelText']:
        font(n,18,ivory);o=obj(n);o.set_editor_property('justification',unreal.TextJustify.CENTER);o.slot.set_padding(unreal.Margin());o.slot.set_horizontal_alignment(unreal.HorizontalAlignment.H_ALIGN_CENTER);o.slot.set_vertical_alignment(unreal.VerticalAlignment.V_ALIGN_CENTER)
    if not unreal.find_object(tree,'ReferenceCloseButton'):
        assert W.add_component(PATH,'Button','ReferenceCloseButton','ConfirmLayout',True).success
    o=rect('ReferenceCloseButton',580,42,44,40,4)
    st=o.get_editor_property('widget_style').copy()
    for state in ['normal','hovered','pressed','disabled']:
        br=brush(cross,26,25);st.set_editor_property(state,br)
    st.normal_padding=unreal.Margin();st.pressed_padding=unreal.Margin();o.set_style(st)
    # Icon fills 26x25 while button retains a comfortable 44x40 hit area.
    if not unreal.find_object(tree,'ReferenceCloseIcon'):
        assert W.add_component(PATH,'Image','ReferenceCloseIcon','ReferenceCloseButton',True).success
    icon=obj('ReferenceCloseIcon');icon.set_brush(brush(cross,26,25));icon.slot.set_padding(unreal.Margin(9,7.5,9,7.5));icon.set_visibility(unreal.SlateVisibility.HIT_TEST_INVISIBLE)
    st=o.get_editor_property('widget_style').copy()
    for state in ['normal','hovered','pressed','disabled']:
        br=unreal.SlateBrush();br.draw_as=unreal.SlateBrushDrawType.NO_DRAW_TYPE;setattr(st,state,br)
    o.set_style(st)
    result=B.compile_blueprint(PATH);assert result.success and not result.num_errors,result.export_text()
    graph=B.describe_graph(PATH,'EventGraph')[0]
    if not any('ReferenceCloseRequested' in n.node_title for n in graph.nodes):
        event=B.add_custom_event_node(PATH,'EventGraph','ReferenceCloseRequested',850,500)
        cancel=B.add_function_call_node(PATH,'EventGraph','NUConfirmDialog','Cancel',1150,500)
        bind=B.add_delegate_bind_on_variable(PATH,'EventGraph','ReferenceCloseButton','OnClicked',850,208)
        assert event and cancel and bind
        assert B.connect_nodes(PATH,'EventGraph',event,'then',cancel,'execute')
        assert B.connect_nodes(PATH,'EventGraph',event,'OutputDelegate',bind,'Delegate')
        play=next(n for n in graph.nodes if n.function_name=='PlayAnimation')
        assert B.connect_nodes(PATH,'EventGraph',play.node_id,'then',bind,'execute')
    result=B.compile_blueprint(PATH);assert result.success and not result.num_errors,result.export_text()
    assert E.save_asset(PATH,False)
    (out/'result.json').write_text(json.dumps({'target':PATH,'reference':REF,'compile':result.export_text(),'stage':[652,366],'origin':[510,248]},indent=2),encoding='utf-8')
    print('APPLIED',out,result.export_text())

if __name__=='__main__': apply()

