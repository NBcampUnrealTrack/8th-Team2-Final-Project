// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/Data/NUJobResourceDefinition.h"

struct FNUJobResourceInstance;

// 직업 자원 · 직업 상태를 화면에 그리는 보기(FNUJobResourceView)를 만드는 규칙
// 로컬 전투는 ASC 가 든 칸을 읽고, 넷 전투는 서버가 준 값을 읽어 같은 함수로 만든다
// 무엇을 띄울지가 두 경로에서 어긋나지 않도록 ASC 밖에 둠
namespace NUJobResourceView
{
	// 이 자원이 화면에 칸을 잡는지
	// 쌓이지 않는 것(속도차이 · 공용 줄기의 자원)과 직업 공통(AG · SP)은 칸이 없음
	PROJECTM_API bool IsShownResource(const FNUJobResourceDefinition& Def);

	// 자기 칸 자원(영력 · 템포 · 동조율)이 게이지로 보여야 하는지
	// 0 이어도 보여 줌 - 그 자원을 여는 노드를 가졌거나 이미 쌓인 적이 있을 때만, 적 · 나이아는 둘 다 없어 빠짐
	PROJECTM_API bool ShouldShowGauge(bool bHasStoredAmount, bool bOwnsJobNode);

	// 대상에게 붙는 자원(마크 · 약점 · 문양)이 아이콘으로 보여야 하는지, 상태이상처럼 쌓였을 때만
	PROJECTM_API bool ShouldShowBadge(int32 Amount);

	// 대상에게 붙는 자원이 사라질 때까지 남은 턴, 셀 것이 없으면 0
	// 중첩마다 따로 사라지는 자원(약점)은 가장 먼저 사라질 것까지, 더 안 쌓이면 전량 사라지는 자원(마크)은 대기 턴 - 안 쌓인 턴
	PROJECTM_API int32 ResolveRemainingTurns(const FNUJobResourceDefinition& Def, const FNUJobResourceInstance* Instance);

	PROJECTM_API FNUJobResourceView MakeResourceView(const FNUJobResourceDefinition& Def, int32 Amount,
		int32 MaxAmount, int32 RemainingTurns);

	// 상태 보기 하나, 이미 끝난 상태(끝 턴 < 지금 턴)면 false 를 주고 아무것도 안 채움
	// 대기 중인 상태(시작 턴 > 지금 턴)는 기간 전체를 남은 턴으로 보여 줌
	// Def 는 자원표에 없는 이름이면 nullptr - 번호와 키만 비고 나머지는 그대로 그려짐
	PROJECTM_API bool MakeStateView(const FNUJobResourceDefinition* Def, FName ResourceName, FName StateName,
		int32 StartTurn, int32 EndTurn, int32 Turn, FNUJobResourceView& OutView);

	// 자원표는 이름으로 찾는 맵이라 도는 순서가 정해져 있지 않음, 전투 중에 자리가 바뀌지 않게 번호로 정렬함
	// 상태는 자원 아이콘 뒤에 붙임 (InStates 도 번호순으로 정렬됨)
	PROJECTM_API void SortViews(TArray<FNUJobResourceView>& InOutGauges, TArray<FNUJobResourceView>& InOutBadges,
		TArray<FNUJobResourceView>& InOutStates);
}
