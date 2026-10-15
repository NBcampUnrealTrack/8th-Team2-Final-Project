#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/Scene.h"
#include "NUWalkableInteriorComponent.generated.h"

class ACharacter;
class UBoxComponent;
class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class USpringArmComponent;
class APlayerController;
class UNUInteriorParticipantComponent;
class UUserWidget;
class UMaterialParameterCollection;
class ANUInteriorPresentationSettings;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNUInteriorPresentationChanged, bool, bInside);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNUInteriorTransitionRequested, UNUInteriorParticipantComponent*, Participant);

// 실내 영역의 출입을 판정하고 로컬 플레이어의 카메라와 실내 표시를 관리한다.
UCLASS(ClassGroup = (NU), meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUWalkableInteriorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUWalkableInteriorComponent();

	UFUNCTION(BlueprintPure, Category = "NU|Interior")
	bool IsLocalPlayerInside() const;
	UFUNCTION(BlueprintCallable, Category = "NU|Interior|Presentation")
	void ApplyInteriorView(bool bInside);

	bool ContainsParticipant(AActor* Actor, bool bRetention) const;
	UBoxComponent* GetInteriorBounds() const { return InteriorBounds.Get(); }
	void RefreshForegroundActor(AActor* Actor, bool bRefreshComponents = false);
	void HandleParticipantEntered(AActor* Actor);
	void HandleParticipantExited(AActor* Actor);
	// 기존 BP 연출 연결 확인용이다. 문 이동은 참가자 컴포넌트가 직접 처리한다.
	bool CanRequestTransition() const { return !bEndingPlay && OnTransitionRequested.IsBound(); }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior")
	FName ForegroundBoundsName = TEXT("ForegroundBounds");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior")
	FName InteriorBoundsName = TEXT("InteriorBounds");
	// 이 방에 대응하는 LocationTable ID. 0은 장소 미지정이며 건물 BP의 종류와 무관하다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Location", meta = (ClampMin = "0"))
	int32 LocationId = 0;
	// 이 방의 장소가 확정되었을 때 진입 알림을 표시할지 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Location")
	bool bShowLocationEntryNotice = true;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior", meta = (ClampMin = "0.0"))
	float DoorwayBoundaryMargin = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation")
	TObjectPtr<UMaterialInterface> ExteriorDimmingMaterial;
	// 이 월드의 주변 소품 머티리얼에서 사용하는 실내 표시 값이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation")
	TObjectPtr<UMaterialParameterCollection> SceneryPresentationParameters;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation")
	bool bFocusInteriorCamera = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ExteriorBrightness = 0.025f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation")
	FVector PresentationBoundsPadding = FVector(32.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation", meta = (ClampMin = "0.1"))
	float PresentationEdgeSoftness = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation")
	TArray<FName> ForegroundComponentTags;
	// 지면과 낮은 계단은 전경 가림에서 제외한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Presentation", meta = (ClampMin = "0.0", Units = "cm"))
	float ForegroundFloorClearance = 20.0f;

	// 도착 위치의 충돌 검사에 통과한 뒤 적용하는 실외 대비 크기 비율이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float InteriorCharacterScale = 1.0f;
	// 캐릭터 크기와 별도로 적용하는 실내 보행 속도 비율이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float InteriorMovementSpeedScale = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior", meta = (ClampMin = "0.1"))
	float InteriorCameraDistanceScale = 1.0f;
	UPROPERTY(BlueprintAssignable, Category = "NU|Interior")
	FNUInteriorPresentationChanged OnPresentationChanged;
	// 기존 BP 바인딩 호환용이다. 문 출입의 암전은 참가자 컴포넌트가 처리한다.
	UPROPERTY(BlueprintAssignable, Category = "NU|Interior")
	FNUInteriorTransitionRequested OnTransitionRequested;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UNUInteriorParticipantComponent;

	UFUNCTION()
	void HandleEntry(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void HandleExit(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
	UFUNCTION()
	void HandleCharacterDestroyed(AActor* DestroyedActor);

	void RegisterParticipant(AActor* Actor);

	void ObservePresentationActor(AActor* Actor);
	void HandlePresentationTransformChanged(USceneComponent* Component, EUpdateTransformFlags UpdateFlags, ETeleportType Teleport);
	UFUNCTION()
	void HandlePresentationActorEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason);

	void RefreshChangedCharacters();
	void RefreshHiddenComponents();
	void RefreshForegroundOcclusion();
	bool IsForegroundOccluder(const UStaticMeshComponent* Component) const;
	void SetForegroundOccluded(UPrimitiveComponent* Component, bool bHidden);

	void SetPresentation(bool bInside);
	void ClearCharacter();

	void RestoreExteriorView();
	void RefreshPresentationSettings();
	void UpdateInteriorFocus();
	void UpdateInteriorSurroundings();

	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> ForegroundBounds;
	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> InteriorBounds;

	TWeakObjectPtr<ACharacter> LocalCharacter;
	TWeakObjectPtr<USpringArmComponent> CameraArm;
	TArray<TWeakObjectPtr<UNUInteriorParticipantComponent>> Participants;

	TSet<TWeakObjectPtr<USceneComponent>> ObservedPresentationRoots;
	TSet<TWeakObjectPtr<AActor>> ChangedPresentationActors;
	TSet<TWeakObjectPtr<AActor>> PendingPresentationComponents;
	TMap<TWeakObjectPtr<AActor>, bool> PresentationVisibility;

	TSet<TWeakObjectPtr<AActor>> HiddenCharacters;
	TMap<TWeakObjectPtr<UUserWidget>, float> HiddenCharacterWidgets;

	FTimerHandle ChangedCharactersTimer;
	FTimerHandle HiddenComponentsTimer;
	FTimerHandle PresentationSettingsTimer;
	FDelegateHandle ActorSpawnedHandle;

	TWeakObjectPtr<UCameraComponent> PresentationCamera;
	TWeakObjectPtr<APlayerController> PresentationController;
	TWeakObjectPtr<ANUInteriorPresentationSettings> PresentationSettings;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> HiddenForegroundComponents;
	TSet<TWeakObjectPtr<UStaticMeshComponent>> ForegroundMeshes;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> HiddenOccluders;
	FTransform ForegroundViewTransform;
	FBox2D ForegroundRoomProjection = FBox2D(ForceInit);
	double ForegroundRoomDepth = 0.0;
	double ForegroundFloorHeight = 0.0;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DimmingInstance;

	float OriginalPostProcessWeight = 1.0f;
	FPostProcessSettings OriginalFocusSettings;
	float OriginalCameraDistance = 0.0f;

	bool bLocalPlayerInside = false;
	bool bOwnsSceneryPresentation = false;
	bool bEndingPlay = false;

	bool bHasSurroundingsParameters = false;
	FVector LastSurroundingsParameters = FVector::ZeroVector;
};
