#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Field/NUFieldTypes.h" // EFieldFacing
#include "FieldPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UNUCameraOcclusionFadeComponent;
class UNURandomEncounterComponent;
class UNUSpriteVisualComponent;
class UNUSymbolEncounterComponent;
class UNUInteractorComponent;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class USpringArmComponent;
struct FInputActionValue;

/**
 * 필드 플레이어 폰 (Paper2D 스프라이트 + 3D 지형)
 *  - SpringArm 원경 카메라, 낮은 FOV, 아래로 피치
 *  - 카메라 기준 이동
 *  - 매 프레임 Yaw만 빌보드
 *  - 4방향 idle/walk 플립북 선택
 *  - ASC는 ANUPlayerState 소유. PossessedBy에서 아바타/능력/스탯 초기화
 *  - 랜덤 인카운터는 RandomEncounterComponent가 담당
 *
 * 전투 입력(커맨드/패링)은 ANUPlayerController가 담당
 */
UCLASS(Blueprintable)
class PROJECTM_API AFieldPlayerCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AFieldPlayerCharacter();

	// --- components ---

	// 카메라 축의 기준점
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	TObjectPtr<USceneComponent> CameraAnchor;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	TObjectPtr<UCameraComponent> Camera;

	// 카메라와 캐릭터 사이를 가리는 물체를 찾아 반투명하게 페이드시킨다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Camera")
	TObjectPtr<UNUCameraOcclusionFadeComponent> CameraOcclusionFadeComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	// 장비 겹침 렌더러 — 본체 Sprite의 자식이라 빌보드·이동을 그대로 따르고,
	// PaperZD 레이어로 연결돼 본체와 같은 애님 재생을 공유한다.
	// 표시할 내용은 장착템의 EquipSkin(과 어깨는 EquipSkinOverlay)이 정한다.
	// 겹침 순서(뒤에서 앞으로): 신발 < 상의 < 하의 < 장갑 < 어깨 (뒷모습은 팔·하의가 뒤집힌다)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> EquipSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteGloves;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteBoots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteLegs;

	// 갑옷의 어깨처럼 팔보다 앞에 와야 하는 부분 (아이템의 EquipSkinOverlay)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteShoulder;

	// 생성창에서 고른 머리·외모. 장비와 같은 레이어 방식이고 번호만 6·7이다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> HairSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UPaperFlipbookComponent> FaceSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Animation")
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	// 머티리얼·그림자·애님BP·빌보드·로코모션 담당. AFieldRemoteCharacter와 같은 설정을 사용한다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
	TObjectPtr<UNURandomEncounterComponent> RandomEncounterComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Encounter")
	TObjectPtr<UNUSymbolEncounterComponent> SymbolEncounterComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Interaction")
	TObjectPtr<UNUInteractorComponent> InteractionComponent;

	// 머리 위 이름표 (스크린 스페이스). 내용은 BeginPlay가 선택 캐릭터 이름으로 채운다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|UI")
	TObjectPtr<class UWidgetComponent> NameplateWidget;

	// --- 입력 (BP 기본값에서 에셋 지정) ---
	// IMC 등록은 ANUPlayerController가 담당한다. 여기 있는 건 바인딩용 IA 참조뿐

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Movement|Camera Relative",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0",
			ToolTip = "Scales camera-relative forward and backward movement input. 1.0 uses the Character Movement max walk speed."))
	float ForwardBackwardMovementScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Movement|Camera Relative",
		meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "2.0",
			ToolTip = "Scales camera-relative left and right movement input. 1.0 uses the Character Movement max walk speed."))
	float LeftRightMovementScale = 0.7f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|State")
	EFieldFacing Facing = EFieldFacing::Down;

	/** BP hook: interact pressed (dialogue triggers, chests...). */
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Field")
	void OnInteract();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual void PossessedBy(AController* NewController) override;

protected:
	virtual void BeginPlay() override;

	// 장착 상태를 보고 장비 레이어 스킨을 걸거나 걷는다 — PlayerState 도착/장착 변경 시 호출
	virtual void OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState) override;

	void HandleMove(const FInputActionValue& Value);
	/** 이동 입력이 끝났을 때 (키를 뗌) — LastMoveInput을 지운다. 위치 동기화의 정지 감지에 필요. */
	void HandleMoveEnd(const FInputActionValue& Value);
	void HandleInteract();

	/** 입력에서 Facing을 유도하고 SpriteVisual에 넘긴다. */
	void UpdateAnimation();

	// 방향에 따라 장비 레이어 겹침 순서를 바꾼다 (뒷모습은 팔과 하의가 뒤집힌다)
	void UpdateLayerDepthForFacing();

	UFUNCTION()
	void HandleEquipmentChangedForVisual();

	// 장착 부위별 EquipSkin을 각 장비 레이어에 적용 (없으면 해제 + 숨김)
	void RefreshEquipmentSkin();

	// 생성창에서 고른 머리·외모와 직업 몸을 입힌다. 진입할 때와 직업이 바뀔 때(OnJobChanged) --- 전직하면 몸통만 바뀐다
	UFUNCTION()
	void RefreshAppearanceSkin();

	FVector2D LastMoveInput = FVector2D::ZeroVector;

	// 마지막으로 레이어 깊이를 맞춘 방향 --- 같은 방향이면 다시 계산하지 않는다
	EFieldFacing LayerDepthFacing = EFieldFacing::Down;
};
