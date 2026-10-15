"""Per-scanline silhouette bounds measured from the polished atlas frame pixels."""
SHAPE = r"""
float edges[81]={380,391,398,404,412,406,400,394,381,
354,361,367,373,382,375,368,362,353,
351,358,364,371,381,374,367,361,352,
352,360,365,371,382,373,365,359,350,
352,359,365,371,380,373,366,360,352,
351,358,363,369,382,375,367,361,353,
351,359,365,370,382,376,368,362,355,
351,359,365,371,382,376,368,362,355,
369,378,385,391,400,395,388,383,373};
float y=p.y;float t=clamp(y/8,0,8);int j=min((int)t,7);float f=t-j;
float right=lerp(edges[(int)row*9+j],edges[(int)row*9+j+1],f)-47+1.2;
float left=19*smoothstep(0,34,abs(y-34));
float d=min(min(p.x-left,right-p.x),min(y,67.5-y));
return saturate(d/.8);
"""
RAIL = 'float2 q=UV*float2(620,899);float row=floor((q.y-144)/68.5);if(row<0||row>8)return 0;float2 p=float2(q.x-47,q.y-144-row*68.5);'+SHAPE
ENTRY = 'float row=RowIndex;if(row<0||row>8)return 0;float2 p=UV*float2(385,68.5);'+SHAPE
