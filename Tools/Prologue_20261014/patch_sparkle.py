# 일회성 패치: 도트 입자를 3픽셀 점에서 '+' 모양 도트 반짝이로 바꾼다(가로·세로 막대 두 개).
p = r'C:/Users/Ndus_072/RevDevTool/RevStudio-ProjectM/Source/ProjectM/Cinematics/NUStorySlideshowWidget.cpp'
s = open(p, encoding='utf-8').read()

def rep(a, b):
    global s
    assert s.count(a) == 1, a[:60]
    s = s.replace(a, b)

rep('''	constexpr int32 MoteCount = 56;''', '''	constexpr int32 MoteCount = 64;''')
rep('''	for (int32 Index = 0; Index < NUStoryMotion::MoteCount; ++Index)
	{
		UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
		Box->SetBrushColor(NUStoryMotion::MoteColors[Index % UE_ARRAY_COUNT(NUStoryMotion::MoteColors)]);
		Box->SetRenderOpacity(0.0f);
		MoteLayer->AddChildToCanvas(Box);
		MoteBoxes.Add(Box);
		ResetMote(Index, true);
	}''', '''	for (int32 Index = 0; Index < NUStoryMotion::MoteCount; ++Index)
	{
		// 입자 하나 = 가로 막대 + 세로 막대. 반짝일 때 팔이 뻗어 '+' 모양 도트 반짝이가 된다.
		for (int32 Arm = 0; Arm < 2; ++Arm)
		{
			UBorder* Box = WidgetTree->ConstructWidget<UBorder>();
			Box->SetBrushColor(NUStoryMotion::MoteColors[Index % UE_ARRAY_COUNT(NUStoryMotion::MoteColors)]);
			Box->SetRenderOpacity(0.0f);
			MoteLayer->AddChildToCanvas(Box);
			MoteBoxes.Add(Box);
		}
		ResetMote(Index, true);
	}''')
rep('''	Mote.SizeTexels = FMath::FRand() < 0.25f ? 2 : 1;''', '''	Mote.SizeTexels = FMath::FRand() < 0.3f ? 2 : 1;''')
rep('''		const float LifeT = Mote.Age / Mote.Life;
		const float LifeFade = FMath::SmoothStep(0.0f, 0.2f, LifeT) * (1.0f - FMath::SmoothStep(0.7f, 1.0f, LifeT));
		const float Twinkle = 0.6f + 0.4f * FMath::Sin(Mote.Phase * 3.0f + Mote.Age * 4.5f);
		const double X = (Mote.Position.X + FMath::Sin(Mote.Phase + Mote.Age * 1.3f) * Mote.Sway) * LocalSize.X;
		const double Y = Mote.Position.Y * LocalSize.Y;
		UBorder* Box = MoteBoxes[Index];
		if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(Box->Slot))
		{
			BoxSlot->SetPosition(FVector2D(FMath::RoundToDouble(X / Texel) * Texel, FMath::RoundToDouble(Y / Texel) * Texel));
			BoxSlot->SetSize(FVector2D(Texel * Mote.SizeTexels, Texel * Mote.SizeTexels));
		}
		Box->SetRenderOpacity(Intensity * LifeFade * Twinkle);''', '''		const float LifeT = Mote.Age / Mote.Life;
		const float LifeFade = FMath::SmoothStep(0.0f, 0.2f, LifeT) * (1.0f - FMath::SmoothStep(0.7f, 1.0f, LifeT));
		const float Twinkle = 0.5f + 0.5f * FMath::Sin(Mote.Phase * 3.0f + Mote.Age * 4.0f);
		const double X = (Mote.Position.X + FMath::Sin(Mote.Phase + Mote.Age * 1.3f) * Mote.Sway) * LocalSize.X;
		const double Y = Mote.Position.Y * LocalSize.Y;
		// 격자에 맞춘 중심. 반짝일수록 팔이 한 칸씩 뻗는다(작은 것 1~3칸, 큰 것 2~6칸).
		const double Core = Texel * Mote.SizeTexels;
		const int32 ArmTexels = Twinkle > 0.75f ? 2 : (Twinkle > 0.4f ? 1 : 0);
		const double Arm = Core * ArmTexels;
		const double CenterX = FMath::RoundToDouble(X / Texel) * Texel;
		const double CenterY = FMath::RoundToDouble(Y / Texel) * Texel;
		const float Opacity = Intensity * LifeFade * (0.7f + 0.3f * Twinkle);
		for (int32 Part = 0; Part < 2; ++Part)
		{
			UBorder* Box = MoteBoxes[Index * 2 + Part];
			const bool bHorizontal = Part == 0;
			if (UCanvasPanelSlot* BoxSlot = Cast<UCanvasPanelSlot>(Box->Slot))
			{
				BoxSlot->SetPosition(bHorizontal ? FVector2D(CenterX - Arm, CenterY) : FVector2D(CenterX, CenterY - Arm));
				BoxSlot->SetSize(bHorizontal ? FVector2D(Core + Arm * 2.0, Core) : FVector2D(Core, Core + Arm * 2.0));
			}
			// 팔 끝은 가운데보다 조금 옅게 보이도록 세로 막대를 살짝 낮춘다.
			Box->SetRenderOpacity(Opacity * (bHorizontal ? 1.0f : 0.85f));
		}''')
open(p, 'w', encoding='utf-8').write(s)
print('ok')
