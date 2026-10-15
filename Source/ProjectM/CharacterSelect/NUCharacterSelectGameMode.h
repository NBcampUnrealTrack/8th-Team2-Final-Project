// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NUCharacterSelectGameMode.generated.h"

class UNUCharacterSelectWidget;

// 캐릭터 선택 레벨 전용 게임모드 (NULoginGameMode 선례).
// 로그인 레벨의 FieldLevel을 이 레벨로 지정하면 로그인 => 선택창 => 필드 흐름이 된다.
UCLASS()
class PROJECTM_API ANUCharacterSelectGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANUCharacterSelectGameMode();

	// 선택 완료 후 필드로 이동 (위젯이 호출)
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void TravelToField();

	// 빈 슬롯·생성 버튼 => 생성 레벨로 이동 (위젯이 호출).
	// 생성은 전용 레벨이다 --- 종족별 기본 외형과 커마 미리보기를 레벨에 배치하려고 8/25에 그렇게 정했다
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void TravelToCharacterCreate();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 서버(또는 더미)가 선택을 확정하면 필드로 이동
	UFUNCTION()
	void HandleCharacterSelected(int64 CharacterIdx);

	// 선택창 위젯 (WBP에서 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Character")
	TSubclassOf<UNUCharacterSelectWidget> CharacterSelectWidgetClass;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "NU|Character")
	TObjectPtr<UNUCharacterSelectWidget> CharacterSelectWidget;

	// 선택 완료 후 이동할 필드 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Character")
	TSoftObjectPtr<UWorld> FieldLevel;

	// 캐릭터 생성 레벨
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "NU|Character")
	TSoftObjectPtr<UWorld> CharacterCreateLevel;
};
