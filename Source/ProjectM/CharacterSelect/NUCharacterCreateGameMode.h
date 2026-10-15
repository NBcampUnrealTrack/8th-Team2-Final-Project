// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NUCharacterCreateGameMode.generated.h"

class UNUCharacterCreateWidget;
class UNUConfirmDialog;
struct FNUCharacterCreateParams;

// 캐릭터 생성 레벨 전용 게임모드 (ANUCharacterSelectGameMode 선례).
//
// 생성을 선택창 위의 모달이 아니라 **전용 레벨**로 둔 이유 (8/25 결정):
// 종족마다 기본 외형이 갈리고 커마 항목이 늘어날 예정이라, 미리보기 캐릭터·카메라를
// 레벨에 배치해 두는 편이 낫다. 선택창의 미리보기 여러 개와 한 월드에서 섞이지도 않는다.
//
// 고르는 중인 값은 UNUCharacterManager(GameInstance 수명)가 들고 있어 레벨을 오가도 남는다.
UCLASS()
class PROJECTM_API ANUCharacterCreateGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANUCharacterCreateGameMode();

	// 선택 레벨로 돌아간다 (취소·생성 완료 공통)
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void TravelToCharacterSelect();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 생성 창이 최종 확인까지 마쳤을 때 --- 여기서 서버로 요청을 보낸다
	void HandleCreateConfirmed(const FNUCharacterCreateParams& Params);

	// 서버 응답. 성공이면 선택 레벨로 돌아가고, 이름이 거절당했으면 화면에 사유만 띄운다
	UFUNCTION()
	void HandleCreateResult(bool bSuccess, int32 ResultCode);

	// 취소로 화면이 닫혔을 때
	void HandleWidgetClosed();

	// 생성 화면 WBP. 미지정이면 C++가 만드는 최소 UI로 뜬다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Character")
	TSubclassOf<UNUCharacterCreateWidget> CharacterCreateWidgetClass;

	// 최종 확인·취소 확인에 쓰는 공용 확인 창 WBP. 미지정이면 확인 없이 진행한다 (개발 편의)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Character")
	TSubclassOf<UNUConfirmDialog> ConfirmDialogClass;

	// 돌아갈 캐릭터 선택 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Character")
	TSoftObjectPtr<UWorld> CharacterSelectLevel;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Character")
	TObjectPtr<UNUCharacterCreateWidget> CharacterCreateWidget;

private:
	// 되돌아가는 중에 응답이 또 와도 한 번만 이동한다
	bool bTravelling = false;
};
