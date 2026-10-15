// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUBattleVFXPreloadSubsystem.generated.h"

struct FStreamableHandle;
class UNiagaraComponent;
class UNiagaraSystem;
class UNUBattleDataSubsystem;

// 보유한 스킬의 전투 연출 VFX 를 게임 중에 미리 비동기로 불러 들고 있음
// 처음 쓰는 스킬이 시전 시각에 동기 로드되며 끊기던 것을 없앰 (전투 시작 암전의 동기 로드는 놓친 것을 받는 폴백으로 남김)
// 대상은 로컬 플레이어의 직업 스킬 · 직업 기본 공격 · 파티 나이아 스킬 · 기본 행동 연출 · 공용 전투 VFX(피격 · 회복 · 사망),
// 보유 목록이 바뀌면 새로 생긴 것만 더 부름
// 불러온 뒤에는 화면 밖에 한 번 띄워 처음 띄울 때의 준비 비용(약 60ms)을 필드에서 미리 치름
// 이미 메모리에 있거나 요청한 에셋은 다시 부르지 않고 붙잡기만 함
UCLASS()
class PROJECTM_API UNUBattleVFXPreloadSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// 이 스킬들(DT_SkillVFX 행)과 직업 기본 공격 · 기본 행동 연출의 VFX 를 비동기로 부름, JobId 0 은 노비스, 음수면 직업 연출 생략
	void PreloadSkills(const TArray<int32>& SkillIds, int32 JobId);

	// 에셋을 직접 지정해 비동기로 부름
	void PreloadSystems(const TArray<FSoftObjectPath>& Paths);

	// 다른 곳(전투 시작 암전의 동기 로드)이 이미 불러 둔 것을 넘겨받아 붙잡고 예열함, 이미 아는 것은 건너뜀
	void AdoptLoaded(const TArray<UNiagaraSystem*>& Systems);

	// 스킬 표 행 · 직업 기본 공격 · 기본 행동 어빌리티의 VFX 경로를 모음 (빈 칸 제외, 중복 제거)
	static void CollectSkillVFX(const UNUBattleDataSubsystem* Data, const TArray<int32>& SkillIds, int32 JobId,
		TArray<FSoftObjectPath>& OutPaths);

	// 아직 불러오는 중인 요청이 있는지
	bool IsLoading() const { return !Handles.IsEmpty(); }

	// 들고 있는 VFX 수
	int32 GetHeldCount() const { return Held.Num(); }

	// 예열을 기다리는 VFX 가 있는지
	bool IsWarmingUp() const { return !WarmUpQueue.IsEmpty() || !WarmUpComponents.IsEmpty(); }

private:
	// 로컬 플레이어의 보유 스킬이 바뀌었는지 주기적으로 봄
	// 직업 스킬 · 나이아 로스터 · 직업이 각자 다른 시점에 서버에서 오므로 델리게이트를 여럿 묶는 대신 목록을 비교함
	bool TickOwnedSkills(float DeltaTime);

	void HandleBatchLoaded(TArray<FSoftObjectPath> Paths);

	// 불러 둔 VFX 를 화면 밖에 한 번 띄웠다 지움, 처음 띄울 때만 드는 준비(렌더 파이프라인 · 시뮬레이션 초기화)를 미리 치름
	// 한 프레임에 하나씩 해서 필드에서 끊김이 안 보이게 함
	bool TickWarmUp(float DeltaTime);

private:
	// 불러 둔 VFX, 게임 인스턴스가 사라질 때까지 가비지 컬렉션에 안 걸리게 붙잡음
	UPROPERTY(Transient)
	TSet<TObjectPtr<UNiagaraSystem>> Held;

	// 요청했거나 들고 있는 경로, 같은 것을 두 번 요청하지 않으려고 둠
	TSet<FSoftObjectPath> Requested;

	// 진행 중인 비동기 요청, 끝나면 버림
	TArray<TSharedPtr<FStreamableHandle>> Handles;

	FTSTicker::FDelegateHandle TickerHandle;
	FTSTicker::FDelegateHandle WarmUpTickerHandle;

	// 예열할 차례를 기다리는 VFX
	TArray<TWeakObjectPtr<UNiagaraSystem>> WarmUpQueue;

	// 예열로 띄운 컴포넌트와 남은 프레임, 다 되면 지움
	TArray<TPair<TWeakObjectPtr<UNiagaraComponent>, int32>> WarmUpComponents;

	// 마지막으로 본 보유 목록, 같으면 아무것도 안 함
	TArray<int32> LastSkillIds;
	int32 LastJobId = INDEX_NONE;
};
