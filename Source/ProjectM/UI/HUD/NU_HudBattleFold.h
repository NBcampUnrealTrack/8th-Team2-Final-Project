// 필드 HUD 밖에 따로 떠 있는 필드 전용 위젯(퀘스트 추적, 지역 이름 줄)을 전투 동안 접는다.
// 필드 HUD(UNU_BaseFieldWidget)와 같은 판정 --- 로컬 인카운터, 서버 전투, 서버 전투 세트가 아직 떠 있는 동안 --- 을 쓴다.
// 주인 위젯 자신은 그대로 두고 루트 위젯만 흐리게 접는다 (주인이 접히면 틱이 멈춰 전투가 끝난 것을 모른다).
#pragma once

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"

class UUserWidget;

class PROJECTM_API FNUHudBattleFold
{
public:
	// 주인 위젯의 NativeTick에서 부른다
	void Tick(UUserWidget* Owner, float DeltaTime);

	static bool IsBattleUIActive(const UUserWidget* Owner);

private:
	float Alpha = 1.f;              // 1 = 보임
	bool bCaptured = false;
	ESlateVisibility RestVisibility = ESlateVisibility::SelfHitTestInvisible;
};
