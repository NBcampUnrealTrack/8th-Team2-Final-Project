"""Run outside UE to import typography assets using the supported asset tool."""
from pathlib import Path
import json
from call import call
ROOT=Path(__file__).resolve().parent
for name,*_ in json.loads((ROOT/'signs.json').read_text(encoding='utf-8')):
    path='/Game/Cindehearth/Signs/T_CH_'+name
    # Imports are only new assets; reruns do not replace source data.
    result=call('manage_asset',{'action':'search','search_term':'T_CH_'+name,'asset_type':'Texture2D'})
    if result.get('count',0):continue
    result=call('manage_asset',{'action':'import','source_file_path':str(ROOT/'signs'/('T_CH_'+name+'.png')),
                              'destination_path':'/Game/Cindehearth/Signs','asset_name':'T_CH_'+name})
    print('IMPORTED',name,result.get('success'))
