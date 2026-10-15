// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUBGMTypes.h"
#include "NUBGMZone.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class APawn;

/*
레벨/전투 세트 전체 또는 그 안의 한 구역에 재생할 BGM을 지정하는 배치용 액터.

Scope로 세 가지 중 하나를 선택, 박스 크기는 Area일 때만 지정
  WholeLevel  : 지금 있는 영구 레벨의 Level 레이어 BGM (레벨마다 하나)
  BattleStage : 이 액터가 들어있는 전투 배경 서브레벨(세트)의 Battle 레이어 BGM (전투 세트마다 하나)
  Area        : 박스 오버랩으로 갈아끼우는 Area 레이어 BGM (한 레벨 안에서 구역별로 갈릴 때만 배치, 건물 안이 예시)
*/
UCLASS(Blueprintable)
class PROJECTM_API ANUBGMZone : public AActor
{
	GENERATED_BODY()

public:
	ANUBGMZone();

	FName GetBGMId() const;
	ENUBGMZoneScope GetScope() const;
	int32 GetPriority() const;

	// 로컬 플레이어 폰이 지금 이 박스 안에 실제로 있는지 다시 확인함
	// 전투 진입/종료가 필드 폰의 콜리전을 껐다 켜서 오버랩 이벤트가 변경될 수 있어,
	// 그 시점에 실제 오버랩을 다시 확인하는 용도로 사용함
	bool IsLocalPawnInside() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UFUNCTION()
	void HandleZoneBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleZoneEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	// 오버랩한 것이 로컬 폰의 루트 컴포넌트인지 확인 - 캡슐/메시 등 보조 콜라이더로
	// 두 번 발화하는 것을 막음 (NUQuestLocationTrigger와 같은 필터)
	static const APawn* ResolveOverlappingLocalPawn(const AActor* OtherActor, const UPrimitiveComponent* OtherComponent);

	void NotifyOverlapChanged() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Audio")
	TObjectPtr<UBoxComponent> ZoneBounds;

	// 재생할 BGM - UNUBGMConfig(NUAudioSettings::BGMConfig)의 키
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Audio")
	FName BGMId = NAME_None;

	// WholeLevel/BattleStage/Area 중 어느 범위로 등록할지 - 클래스 주석 참고
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Audio")
	ENUBGMZoneScope Scope = ENUBGMZoneScope::WholeLevel;

	// Area 스코프에서 여러 박스가 겹칠 때 큰 값이 이김, 같다면 나중에 등록된 쪽이 이김.
	// 다른 스코프에서는 읽지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Audio")
	int32 Priority = 0;
};
