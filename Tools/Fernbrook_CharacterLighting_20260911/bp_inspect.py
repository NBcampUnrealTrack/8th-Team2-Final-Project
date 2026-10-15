import unreal,json
B=unreal.BlueprintService
P='/Game/Libregrove/Level_V8/Gameplay/BP_LG8_FieldCharacter'
s,e=B.describe_graph(P,'EventGraph');assert s,e
r={'nodes':[{'id':n.node_id,'title':n.node_title,'fn':n.function_name,'x':n.pos_x,'y':n.pos_y} for n in s.nodes],'edges':[{'src':x.source_node_id,'out':x.source_pin_name,'dst':x.target_node_id,'in':x.target_pin_name} for x in s.edges], 'discover':[{'name':n.display_name,'key':n.spawner_key} for n in B.discover_nodes(P,'Set Material','',50)]}
print(json.dumps(r))
