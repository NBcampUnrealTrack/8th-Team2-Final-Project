import asyncio,json,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT.parents[3]/'Tools/Cindehearth/Pipeline'))
from tripo_mcp import client
async def main():
    async with client() as c:
        for kind in ['balance','usage']:
            d=await c._v3_request('GET','/v3/account/'+kind)
            (ROOT/'verification'/('account_'+kind+'.json')).write_text(json.dumps(d,indent=2),encoding='utf-8')
            if kind=='balance':print(json.dumps(d))
asyncio.run(main())
