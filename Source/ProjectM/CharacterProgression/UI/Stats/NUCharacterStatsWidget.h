#pragma once

#include "CoreMinimal.h"
#include "CharacterProgression/UI/NUProgressionWindowBase.h"
#include "NUCharacterStatsWidget.generated.h"

class UNUAbilitySystemComponent;
class UNUJobComponent;
class UNUProgressionComponent;
class UProgressBar;
class UTextBlock;
struct FOnAttributeChangeData;

// 플레이어의 레벨, 경험치, Primary Stat과 GAS 수치를 표시
USTRUCT(BlueprintType)
struct FNUCharacterStatsViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	FText PlayerName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	FText JobName;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Level = 1;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int64 ExperienceIntoLevel = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int64 RequiredExperienceToNext = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float ExperienceRatio = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Strength = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Dexterity = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Intelligence = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Luck = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	int32 Vitality = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float SP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float MaxSP = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float AttackPower = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float MagicAttackPower = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float Defense = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float Speed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float CriticalRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float CriticalDamage = 0.f;

	// grpc 서버가 장비까지 반영해 계산한 추가 전투 능력치
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float ParryRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float DodgeRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float DamageReduce = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float HealBonus = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipAttackRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipMagicAttackRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipMaxHealthRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	float EquipDefenseFlat = 0.f;

	// 최종 스탯 스냅샷의 사용 여부와 요청 진행 상태
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	bool bUsingServerSnapshot = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	bool bHasExperienceData = true;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	bool bRequestPending = false;
};

// WBP는 ViewData를 받아 표현하고, C++은 성장/GAS 값의 수명만 관리
UCLASS(Abstract)
class PROJECTM_API UNUCharacterStatsWidget : public UNUProgressionWindowBase
{
	GENERATED_BODY()

public:
	void InitializeStats(UNUProgressionComponent* InProgression, FSimpleDelegate InCloseRequested);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character|Stats")
	FNUCharacterStatsViewData ViewData;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> PlayerNameText;

	// WBP에서 플레이어명 바로 아래에 배치하는 직업명 텍스트
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> JobText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> ExperienceText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UProgressBar> ExperienceBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> StrengthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DexterityValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> IntelligenceValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> LuckValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> VitalityValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> HealthValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SPValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> AttackPowerValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MagicAttackPowerValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> DefenseValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> SpeedValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CriticalRateValueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> CriticalDamageValueText;

private:
	// 성장 상태와 GAS 수치 변경은 모두 같은 화면 갱신 경로를 사용
	UFUNCTION()
	void HandleProgressionChanged();

	// grpc 서버 스탯 요청 상태와 최종 능력치가 바뀌면 화면 값을 다시 만든다.
	UFUNCTION()
	void HandleCharacterStatSnapshotChanged();

	UFUNCTION()
	void HandleJobChanged();

	void HandleAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void BindAttributeDelegates();
	void UnbindAttributeDelegates();
	void RefreshValues();

	// 레벨과 Primary Stat은 Progression, 실시간 전투 수치는 ASC에서 읽는다.
	TWeakObjectPtr<UNUProgressionComponent> Progression;
	TWeakObjectPtr<UNUJobComponent> JobComponent;
	TWeakObjectPtr<UNUAbilitySystemComponent> ASC;
};
