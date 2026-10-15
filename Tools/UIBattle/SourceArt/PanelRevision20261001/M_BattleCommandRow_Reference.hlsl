
// Revision 2: round icon medallion joined to an open, clipped-corner label.
// Work in 60 logical vertical units; preserve round badges at any DPI.
float2 size=float2(max(PixelSize.x/max(PixelSize.y,1),2.0)*60,60);
float2 p=UV*size;
float selected=max(ForceSelected,smoothstep(.80,.94,Tint.r));
float badgeRadius=lerp(25.5,27.0,selected);
float dc=length(p-float2(32,30))-badgeRadius;
float left=32;
float right=size.x-38+selected*6;
float2 center=float2((left+right)*.5,30);
float2 halfSize=float2((right-left)*.5,23);
float2 q=abs(p-center);
float dr=max(max(q.x-halfSize.x,q.y-halfSize.y),(q.x+q.y-halfSize.x-halfSize.y+7)*.70710678);
float circle=1-smoothstep(-.65,.65,dc);
float rectangle=1-smoothstep(-.65,.65,dr);
float unionDistance=min(dc,dr);
float outline=(1-smoothstep(.5,1.35,abs(unionDistance+.45)));
// The circular seam remains visible where the badge overlaps the label.
float circleRim=(1-smoothstep(.35,1.0,abs(dc+.45)));
float glow=exp(-abs(unionDistance)*.7)*.28*selected;
float leftDiamond=1-smoothstep(-.35,.6,abs(p.x-4.5)+abs(p.y-30)-3.2);
float rightDiamond=1-smoothstep(-.35,.6,abs(p.x-right-2)+abs(p.y-30)-3.3);
float smallDiamond=1-smoothstep(-.35,.6,abs(p.x-right-16)+abs(p.y-30)-2.1);
float tail=(1-smoothstep(.25,.8,abs(p.y-30)))*step(right,p.x)*(1-smoothstep(right+22,size.x-2,p.x));
float detail=saturate(leftDiamond*.75+selected*(outline*.95+circleRim*.8+rightDiamond+smallDiamond+tail*.65));
float bodyAlpha=max(rectangle*.70,circle*.90);
float3 fill=lerp(float3(.011,.026,.034),float3(.025,.043,.042),selected);
float warmInner=selected*exp(-abs(unionDistance+3)*.42)*rectangle*.18;
float decorationAlpha=saturate(detail+glow+warmInner);
float alpha=bodyAlpha+decorationAlpha*(1-bodyAlpha);
float3 gold=float3(.96,.72,.39);
float3 premul=fill*bodyAlpha*(1-decorationAlpha)+gold*decorationAlpha;
// NU_CommonButtonWidget uses background tint as its selection signal.
return float4(premul/max(alpha,.001)/max(Tint.rgb,float3(.001,.001,.001)),alpha);
