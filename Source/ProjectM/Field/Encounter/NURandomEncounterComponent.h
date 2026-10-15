#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NURandomEncounterComponent.generated.h"

class ANUBattleEncounter;
class ANUEncounterZone;

// 플레이어의 이동 거리를 누적하고, 무작위로 설정된 거리에 도착하면
// 랜덤 인카운터 시도하는 컴포넌트

UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNURandomEncounterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNURandomEncounterComponent();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---------------------------------------
	// 인카운터 설정 및 제어
	// ---------------------------------------
public:
	// 랜덤 인카운터 활성화 여부 설정
	UFUNCTION(BlueprintCallable, Category = "NU|Encounter")
	void SetEncounterEnabled(bool bEnabled);

	// 누적 이동 거리와 다음 인카운터 거리 초기화
	UFUNCTION(BlueprintCallable, Category = "NU|Encounter")
	void ResetEncounterProgress();

public:
	// 인카운터 활성화 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Encounter")
	bool bIsEncounterEnabled = true;

	// 인카운터 최소 이동 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Encounter", meta = (ClampMin = "0.0"))
	float MinEncounterDistance = 2000.0f;

	// 인카운터 최대 이동 거리
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Encounter", meta = (ClampMin = "0.0"))
	float MaxEncounterDistance = 5000.0f;

	// 생성할 BattleEncounter 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Encounter")
	TSoftClassPtr<ANUBattleEncounter> EncounterClass;

	// ---------------------------------------
	// 이동 거리 처리 
	// ---------------------------------------
private:
	// 플레이어가 overlap 중인 Encounter Zone 탐색
	// Zone은 겹치지 않는 것을 전제로 함
	ANUEncounterZone* FindCurrentEncounterZone() const;

	// 플레이어의 이동 거리 누적
	bool CanAccumulateDistance() const;

	// 인카운터 시도
	bool TryStartEncounter(const ANUEncounterZone& EncounterZone);

	// 다음 인카운터 목표 거리 설정
	void GenerateNextEncounterDistance();

private:
	// 이동 거리 계산 직전 위치
	FVector PreviousLocation = FVector::ZeroVector;

	// 마지막 초기화 이후 누적 이동 거리
	float MovedDistance = 0.0f;

	// 다음 랜덤 인카운터가 발생할 목표 이동거리
	float TargetDistance = 0.0f;
};
