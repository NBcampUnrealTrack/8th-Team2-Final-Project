// 화면·창의 짧은 등장 연출 --- 위젯이 차례로 페이드 인하며 몇 픽셀 미끄러져 제자리에 선다 (생성·선택 화면, 확인 창).
#pragma once

#include "CoreMinimal.h"
#include "Slate/WidgetTransform.h"

class UWidget;

// 위젯의 렌더 변환·불투명도만 움직인다 (배치는 건드리지 않는다). 끝나면 위젯이 원래 값으로 돌아간다.
// 타이머나 월드를 쓰지 않는다 --- 주인 위젯의 NativeTick에서 Advance를 불러 준다
class PROJECTM_API FNUScreenMotion
{
public:
	// 시작 모습: 제자리에서 Offset(Slate 단위)만큼 떨어져 FromOpacity, FromScale로 시작해 원래 값으로 간다.
	// FromOpacity가 원래 값보다 크면 거꾸로 사라진다 (화면을 덮은 막을 걷을 때)
	void Add(UWidget* Widget, float Delay, float Duration, const FVector2D& Offset,
		float FromOpacity = 0.f, float FromScale = 1.f);

	// 한 번 반짝이고 사라진다: 원래 불투명도에서 PeakOpacity까지 올랐다가 돌아오고, 크기는 FromScale에서 1까지 컸다가 돌아온다.
	// 시작 전과 끝난 뒤에는 원래 모습이다 (평소 불투명도 0인 반짝임 그림용)
	void AddPulse(UWidget* Widget, float Delay, float Duration, float PeakOpacity = 1.f, float FromScale = 1.f);

	// 사라지는 쪽: 지금 모습에서 Offset만큼 밀려나며 ToOpacity까지 흐려진다. Add와 달리 끝 모습(흐려진 채)에 머문다 ---
	// 다 사라진 뒤 위젯을 접고 불투명도를 되돌리는 것은 부른 쪽이 한다
	void AddOut(UWidget* Widget, float Delay, float Duration, const FVector2D& Offset, float ToOpacity = 0.f);

	// 패널의 자식들을 위에서부터 Step초 간격으로 차례로 올린다
	void AddChildren(UWidget* Panel, float FirstDelay, float Step, float Duration, const FVector2D& Offset);

	// 지금 시각의 모습을 바로 그린다 (다음 틱까지 원래 모습이 한 프레임 비치지 않게). 진행 중에 보탠 뒤에도 부른다
	void Start();

	// 진행. 아직 움직이는 중이면 true
	bool Advance(float DeltaTime);

	// 지금 움직이는 것을 끝 모습으로 보낸다 (창이 닫히거나 같은 위젯을 다시 움직이기 전)
	void Finish();

	bool IsRunning() const { return bRunning; }

private:
	struct FEntry
	{
		TWeakObjectPtr<UWidget> Widget;
		float Delay = 0.f;
		float Duration = 0.f;
		FVector2D Offset = FVector2D::ZeroVector;
		float FromOpacity = 0.f;
		float FromScale = 1.f;
		bool bPulse = false;
		bool bOut = false;
		FWidgetTransform Rest;
		float RestOpacity = 1.f;
	};

	static void Apply(const FEntry& Entry, float Alpha);

	TArray<FEntry> Entries;
	float Elapsed = 0.f;
	bool bRunning = false;
};
