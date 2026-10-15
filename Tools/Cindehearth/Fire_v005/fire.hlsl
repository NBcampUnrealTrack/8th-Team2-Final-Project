// Persistent, rooted fire with five independent tapered tongues.
float2 q=(floor(UV*float2(64.0,80.0))+.5)/float2(64.0,80.0);
float t=floor(Time*16.0)/16.0+Phase*9.0;
float h=q.y;
float field=-1.0;
float heat=0.0;
[unroll] for(int i=0;i<5;i++){
    float seed=float(i)*2.371;
    float peak=.55+.19*sin(seed+1.2)+.07*sin(t*2.1+seed);
    float u=max(0.0,(h-.045)/peak);
    float bend=(sin(u*7.0-t*2.6+seed)*.047+sin(u*15.0-t*3.7+seed)*.019)*u;
    float center=.24+float(i)*.13+bend;
    float width=(.115+.022*sin(seed))*pow(saturate(1.0-u),.82);
    width*=.85+.15*sin(u*20.0-t*5.0+seed);
    float density=(width-abs(q.x-center))/max(width,.012);
    density=min(density,(1.0-u)*8.0);
    field=max(field,density);
    heat=max(heat,saturate(density)*saturate((.56-h)*2.6));
}
float root=smoothstep(.035,.065,h);
float a=smoothstep(0.0,.16,field)*root;
float bands=floor(saturate(heat)*6.0)/6.0;
float3 orange=lerp(float3(1.4,.095,.003),float3(2.7,.62,.018),saturate(field*1.5));
float3 col=lerp(orange,float3(3.9,2.25,.43),bands);
a*=lerp(.68,.96,saturate(field));
// A few motes separate from the tip and fade out before their loop reset.
[unroll] for(int j=0;j<3;j++){
    float age=frac(t*(.19+.018*j)+float(j)*.317);
    float2 pos=float2(.34+.16*j+.035*sin(age*5.0+j),.43+age*.51);
    float2 delta=(q-pos)/float2(.011,.018);
    float spark=saturate(1.0-dot(delta,delta))*smoothstep(.02,.16,age)*(1.0-smoothstep(.62,.98,age));
    col=lerp(col,float3(2.8,1.14,.09),step(a,spark));
    a=max(a,spark*.78);
}
return float4(col*Intensity,a);
