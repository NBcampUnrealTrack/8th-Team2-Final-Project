#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "NUWorldLocationComponent.generated.h"

class APawn;
class APlayerState;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUWorldLocationChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNULocationOverlapsChanged);

// PlayerState의 Pawn을 따라 장소를 추적한다. 로컬 관측값이며 서버 확정 위치가 아니다.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUWorldLocationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUWorldLocationComponent();
	// 현재 확정된 실내 또는 선택된 장소 트리거의 ID. 미지정이면 0이다.
	UFUNCTION(BlueprintPure, Category = "NU|WorldLocation")
	int32 GetCurrentLocationId() const;
	UFUNCTION(BlueprintPure, Category = "NU|WorldLocation")
	int32 GetLastKnownAreaId() const;

	bool ShouldShowEntryNotice() const;
	bool HasTrackedPawn() const;
	// 퀘스트 도착 판정에 제공하는 현재 장소 목록이다.
	const TArray<FName>& GetOverlappingLocationIds() const;
	void RefreshLocation();
	// 같은 프레임의 이동, 겹침, 실내 확정 요청을 한 번의 갱신으로 모은다.
	void ScheduleLocationRefresh();

	// 장소와 소속 지역의 퀘스트 해금 조건. 물리적인 문/포탈의 실행은 호출자가 담당한다.
	UFUNCTION(BlueprintPure, Category = "NU|WorldLocation")
	bool IsLocationUnlocked(int32 LocationId) const;

	UPROPERTY(BlueprintAssignable, Category = "NU|WorldLocation")
	FOnNUWorldLocationChanged OnLocationChanged;

	UPROPERTY(BlueprintAssignable, Category = "NU|WorldLocation")
	FOnNULocationOverlapsChanged OnLocationOverlapsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void BindPawn(APawn* Pawn);
	void HandlePawnTransformUpdated(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	UFUNCTION()
	void HandlePawnSet(APlayerState* PlayerState, APawn* NewPawn, APawn* OldPawn);

	UFUNCTION()
	void HandlePawnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandlePawnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);

	TWeakObjectPtr<APawn> BoundPawn;
	TWeakObjectPtr<UPrimitiveComponent> BoundPawnRoot;
	FTimerHandle LocationRefreshTimer;
	TArray<FName> OverlappingLocationIds;
	int32 CurrentLocationId = 0;
	int32 LastKnownAreaId = 0;
	bool bShowEntryNotice = false;
	bool bLocationRefreshPending = false;
};
