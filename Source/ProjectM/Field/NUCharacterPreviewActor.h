// 캐릭터 정보창에 띄우는 정면 Idle 미리보기.
//
// UMG에서 파츠를 손으로 겹치지 않는 이유: 피벗·해상도·겹침 순서·프레임 흔들림을 전부 다시 계산해야 하는데,
// 그 계산은 이미 Paper2D가 필드에서 정확히 하고 있다. 그래서 같은 컴포넌트 구성을 그대로 세워두고
// SceneCapture로 찍어 렌더 타깃에 담는다 --- 그리는 주체가 Paper2D 자신이라 필드와 어긋날 수 없다.
//
// 레벨에 미리 배치하지 않는다. 정보창이 필요할 때 아주 먼 곳에 하나 만들고, 창이 닫히면 캡처를 끈다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NUCharacterPreviewActor.generated.h"

class UNUEquipmentComponent;
class UNUInventoryComponent;
class UNUSpriteVisualComponent;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

UCLASS()
class PROJECTM_API ANUCharacterPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	ANUCharacterPreviewActor();

	// 장착 상태를 반영한다. 정보창이 갱신될 때마다 부른다
	void RefreshEquipment(const UNUEquipmentComponent* Equipment, UNUInventoryComponent* Inventory);

	// 창이 열려 있는 동안만 찍는다. 닫히면 꺼서 비용을 0으로 만든다
	void SetCaptureEnabled(bool bEnabled);

	virtual void Tick(float DeltaSeconds) override;

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

protected:
	virtual void BeginPlay() override;

	// 위치 기준점 --- Sprite를 루트로 두면 SpritePivotZ 보정이 액터를 통째로 옮긴다
	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	// 장비 겹침 렌더러 --- 구성과 위치를 필드 캐릭터와 똑같이 맞춘다
	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> EquipSprite;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteGloves;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteBoots;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteLegs;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteShoulder;

	// 커스터마이징 겹침 --- 필드 캐릭터와 같은 레이어를 써야 초상화가 실제 모습과 맞는다
	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> HairSprite;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> FaceSprite;

	UPROPERTY(VisibleAnywhere, Category = "NU|Preview")
	TObjectPtr<USceneCaptureComponent2D> Capture;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

private:
	// 마지막으로 찍은 몸통 프레임 --- 같은 프레임이면 다시 안 찍는다
	int32 LastCapturedFrame = INDEX_NONE;

	// 마지막으로 찍은 몸통 플립북 --- 스킨이 몸을 갈아끼우면 프레임 번호가 그대로여도 그림이 바뀐다.
	// 몸이 한 장짜리(정지)면 프레임 변화가 다시는 안 와서, 이 비교가 없으면 첫 빈 캡처가 영영 남는다
	TWeakObjectPtr<const UPaperFlipbook> LastCapturedFlipbook;

	// 갱신 요청 후 몇 틱 기다렸다 찍을지 --- 스킨은 다음 애님 틱에 적용되므로
	// 요청 즉시 찍으면 적용 전 그림이 담긴다 (첫 열기 빈 초상의 원인)
	int32 PendingCaptureTicks = 0;

	// 플립북 없음 경고는 한 번만 --- 매 틱 찍으면 로그가 넘친다
	bool bLoggedMissingFlipbook = false;
	float TimeWithoutFlipbook = 0.f;
};
