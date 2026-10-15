import asyncio,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'Pipeline'))
from tripo_mcp import client
async def main():
    async with client() as c:
        for kind in ['balance','usage']:
            try:
                d=await c._v3_request('GET','/v3/account/'+kind)
                Path(__file__).with_name('verification').joinpath('account_'+kind+'.json').write_text(json.dumps(d,indent=2),encoding='utf-8')
                print(kind,json.dumps(d)[:6000])
            except Exception as e:print(kind,type(e).__name__,getattr(e,'code',None),getattr(e,'message','')[:200])
asyncio.run(main())
