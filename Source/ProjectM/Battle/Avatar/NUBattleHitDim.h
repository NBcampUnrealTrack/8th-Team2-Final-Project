// 피격 순간의 짧은 화면 암전 (HD-2D 전투 연출): 피격 리액션마다 카메라 페이드를 잠깐 올렸다 내린다.
// 콘솔 변수 nu.VFX.HitDim.Enabled / Alpha / Seconds 로 조절, nu.VFX.HitDim 명령으로 단독 재생해 볼 수 있다.
// 전환 페이드(인카운터 진입·실내 전환)가 진행 중이면 겹치지 않도록 건너뛴다.
#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace NUBattleHitDim
{
	/** 피격 지점에서 호출: 로컬 플레이어 카메라를 Alpha 만큼 어둡게 했다가 Seconds 안에 되돌린다. */
	PROJECTM_API void Play(UWorld* World);
}
