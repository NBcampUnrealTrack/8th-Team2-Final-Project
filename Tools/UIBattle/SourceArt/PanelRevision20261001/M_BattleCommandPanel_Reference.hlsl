
// Revision 2: open command menu. Only a translucent compass at the left.
// Use logical layout coordinates to remain independent of viewport DPI.
float2 pixel=UV*float2(460,460);
float2 t=((pixel-FrameOrigin.xy)/max(FrameSize.xy,float2(1,1)))*SourceSpan.xy+SourceOrigin.xy;
float inside=step(0,t.x)*step(t.x,1)*step(0,t.y)*step(t.y,1);
float4 art=Texture2DSample(PanelTexture,PanelTextureSampler,saturate(t));
return float4(art.rgb,art.a*inside*FillOpacity);
