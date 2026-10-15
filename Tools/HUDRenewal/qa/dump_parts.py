# -*- coding: utf-8 -*-
"""Read-only: print every row of the hair / face part tables (name, PartId, race, gender, job, sort, skin) in table
order, so part priority for female job bodies can be checked. Run headless; prints lines starting with [PARTS]."""
import json
import unreal

for path in ('/Game/Developers/dbals/Data/Appearance/DT_HairPart', '/Game/Developers/dbals/Data/Appearance/DT_FacePart'):
    dt = unreal.load_asset(path)
    rows = json.loads(dt.export_to_json_string() if hasattr(dt, 'export_to_json_string')
                      else unreal.DataTableFunctionLibrary.export_data_table_to_json_string(dt))
    for r in rows:
        unreal.log('[PARTS] %s | %s PartId=%s Race=%s Gender=%s Job=%s Sort=%s Skin=%s' % (
            path.rsplit('/', 1)[-1], r.get('Name'), r.get('PartId'), r.get('RaceId'), r.get('Gender'), r.get('JobId'),
            r.get('SortOrder'), str(r.get('FieldSkin', '')).rsplit('/', 1)[-1]))
unreal.log('[PARTS] DONE')
