// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "GameFramework/Actor.h"
#include "NUCharacterCreatePreview.generated.h"

class UBoxComponent;
class UMaterialInterface;
class UPaperFlipbook;
class UPaperFlipbookComponent;

// 이 자리의 캐릭터를 눌렀다 (페이로드 = 그 자리의 RaceId)
DECLARE_DELEGATE_OneParam(FOnNUPreviewStandClicked, int32 /*RaceId*/);

// 생성 레벨에 세워 두는 미리보기 캐릭터 (TOS 방식 --- UI 창이 아니라 필드에 서 있다).
//
// **종족 하나당 액터 하나**를 레벨의 다른 자리에 세운다. 고르기 전에는 전경 카메라가
// 배경 전체와 서 있는 캐릭터들을 비추고, 종족을 고르면 그 자리로 줌인한다.
// 레벨에서 할 일: 액터를 놓고 RaceId를 적고, 그 자리를 비추는 CameraActor를 StandCamera에 물린다.
//
// 몸은 프로젝트 세팅의 몸 표(종족·성별 => 정면 대기 플립북)에서 찾고, 머리·외모는 파츠 컴포넌트에 겹친다.
// 새 종족·성별 몸이 오면 표에 줄만 보태면 된다 (이 액터는 손대지 않는다).
// "미리보기 캐릭터가 걸어다니게" (민서 님 제안, 8/26)는 이 액터에 이동만 붙이면 된다
UCLASS()
class PROJECTM_API ANUCharacterCreatePreview : public AActor
{
	GENERATED_BODY()

public:
	ANUCharacterCreatePreview();

	// 고른 것에 맞춰 외형을 바꾼다. 표에 없는 종족은 기본 외형 유지.
	// 머리·외모는 몸 위에 겹쳐 그리고, 피부는 몸 색을 물들인다.
	// 에셋은 위젯이 파츠 표에서 뽑아 넘겨 준다 --- 이 액터는 표를 모른다
	void ShowAppearance(const FNUCharacterCreateParams& Params,
		UPaperFlipbook* HairBook, UPaperFlipbook* FaceBook, const FLinearColor& InSkinTint);

	// 이 자리가 어느 종족 것인지 (종족 표의 RaceId)
	int32 GetRaceId() const { return RaceId; }

	// 이 자리를 비추는 카메라. 안 물려 두면 액터 자신이 뷰 타깃이 된다
	AActor* GetStandCamera() const { return StandCamera; }

	// 고른 종족만 도드라지게 --- 나머지는 어둡게 깔아 시선을 뺏지 않는다.
	// 배경이 그림 한 장이라 카메라를 옆으로 못 옮긴다 --- 강조가 곧 "고름"의 표시다
	void SetHighlighted(bool bInHighlighted);

	// 고르지 않은 자리를 기본 모습으로 되돌린다 --- 기본 성별(Gender)의 몸, 그 몸의 첫 머리·외모, 피부색 원래대로.
	// 안 되돌리면 다른 자리로 옮겨도 이전에 고른 성별 몸과 머리·눈이 그 자리에 그대로 남는다.
	// 새 남캐 몸은 머리가 파츠 쪽에 있어 파츠까지 비우면 머리 없이 선다 (9/11)
	void ResetAppearance();

	// 미개방 종족 자리 --- 눌러도 반응하지 않고 계속 어둡게 둔다 (위젯이 종족 표를 보고 정한다)
	void SetLocked(bool bInLocked);

	// 캐릭터를 눌렀을 때. 위젯이 받아 그 종족을 고르고 폼을 연다
	FOnNUPreviewStandClicked OnStandClicked;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	// 몸 위에 겹쳐 그리는 파츠. 파츠가 오기 전에는 비어 있어 아무것도 안 그린다.
	// 필드의 NUSpriteVisualComponent와 같은 방식인데, 생성창은 정면 고정이라 훨씬 단순하다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> HairSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Preview")
	TObjectPtr<UPaperFlipbookComponent> FaceSprite;

	// 눌리는 범위. 스프라이트에는 클릭용 충돌이 없어서 따로 둔다.
	// Sprite에 붙어 있으므로 캐릭터를 키우면 이 상자도 같이 커진다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Preview")
	TObjectPtr<UBoxComponent> ClickBox;

	// 이 자리가 맡을 종족 (종족 표의 RaceId --- 인간1 엘프2 드워프3 수인4)
	UPROPERTY(EditAnywhere, Category = "NU|Preview")
	int32 RaceId = 0;

	// 이 자리를 비추는 레벨의 CameraActor. 비워 두면 액터 위치에서 본다
	UPROPERTY(EditInstanceOnly, Category = "NU|Preview")
	TObjectPtr<AActor> StandCamera;

	// 고르지 않은 동안 이 자리가 서 있을 성별. 기본 남성 --- 고른 자리만 생성창의 선택값(여성 등)을 따르고,
	// 다른 자리로 옮기면 이 값으로 되돌아간다 (9/4 유민 결정)
	UPROPERTY(EditAnywhere, Category = "NU|Preview")
	ENUCharacterGender Gender = ENUCharacterGender::Male;

	// 몸 표에 이 종족·성별 줄이 없을 때 쓰는 외형 (선택창 미리보기와 같은 것을 지정)
	UPROPERTY(EditAnywhere, Category = "NU|Preview")
	TSoftObjectPtr<UPaperFlipbook> DefaultFlipbook;

	// 고른 자리의 색 (원래 색 그대로)
	UPROPERTY(EditAnywhere, Category = "NU|Preview")
	FLinearColor HighlightColor = FLinearColor::White;

	// 고르지 않은 자리의 색. 너무 어두우면 배경에 묻히니 형체는 남긴다
	UPROPERTY(EditAnywhere, Category = "NU|Preview")
	FLinearColor DimColor = FLinearColor(0.34f, 0.34f, 0.42f, 1.0f);

private:
	UFUNCTION()
	void HandleClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

	// 마우스를 올리면 밝아지고 커서가 손 모양이 된다 --- 누를 수 있다는 것을 알려 줘야
	// 캐릭터를 클릭해 종족을 고른다는 것이 전달된다
	UFUNCTION()
	void HandleCursorOver(UPrimitiveComponent* TouchedComponent);

	UFUNCTION()
	void HandleCursorLeave(UPrimitiveComponent* TouchedComponent);

	void SetCursorHand(bool bHand) const;

	void ApplyFlipbook(const TSoftObjectPtr<UPaperFlipbook>& Source);

	// 이 종족·성별의 몸을 입힌다 --- 몸 표에 정면 대기가 있으면 그것을, 없으면 기본 외형
	void ApplyBody(int32 InRaceId, ENUCharacterGender InGender);

	// 강조와 피부색을 함께 반영한다 (둘 다 몸 색을 건드리므로 한 곳에서 곱한다)
	void ApplyTint();

	bool bHighlighted = true;
	bool bHovered = false;
	bool bLocked = false;
	FLinearColor SkinTint = FLinearColor::White;

	// 머리 색 --- ShowAppearance가 hair_id에서 풀어 둔다. 머리 컴포넌트는 처음부터 물들임 머티리얼(의 인스턴스)을 계속 쓰고
	// HairTintOn으로 켜고 끈다. 머티리얼을 갈아 끼우면 엔진이 새 셰이더가 준비될 때까지 그 부품을 그리지 않아 머리가 몇 프레임 사라졌다
	bool bHairTinted = false;
	FLinearColor HairTint = FLinearColor::White;
	bool bHairBaseCached = false;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HairBaseMaterial;      // 물들임 머티리얼이 없을 때 쓰는, 레벨에 놓인 그대로
	UPROPERTY(Transient)
	TObjectPtr<class UMaterialInstanceDynamic> HairTintInstance;
};
