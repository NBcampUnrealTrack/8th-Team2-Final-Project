#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterProgression/Data/NUPlayerSkillTypes.h"
#include "NUPlayerSkillComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUPlayerSkillsChanged);

// 플레이어가 보유한 직업 스킬 목록만 서버 권한으로 관리한다.
// 실행기와 패시브 효과 적용은 전투 담당 계층에서 별도로 연결한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class PROJECTM_API UNUPlayerSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUPlayerSkillComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool ApplyState(const FNUPlayerSkillState& NewState);
	bool RefreshProgressionGrants();

	UFUNCTION(BlueprintPure, Category = "NU|Skill")
	const FNUPlayerSkillState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "NU|Skill")
	TArray<int32> GetOwnedActiveSkillIds() const;

	UFUNCTION(BlueprintPure, Category = "NU|Skill")
	TArray<int32> GetOwnedPassiveSkillIds() const;

	UFUNCTION(BlueprintPure, Category = "NU|Skill")
	bool HasActiveSkill(int32 SkillId) const;

	UFUNCTION(BlueprintPure, Category = "NU|Skill")
	bool HasPassiveSkill(int32 SkillId) const;

	UPROPERTY(BlueprintAssignable, Category = "NU|Skill")
	FOnNUPlayerSkillsChanged OnSkillsChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_State();

	UFUNCTION()
	void HandleProgressionChanged();

	bool ValidateState(const FNUPlayerSkillState& CandidateState, FString& OutError) const;
	static void BuildEffectiveActiveSkillIds(const FNUPlayerSkillState& SourceState, TArray<int32>& OutSkillIds);
	static void BuildEffectivePassiveSkillIds(const FNUPlayerSkillState& SourceState, TArray<int32>& OutSkillIds);

	UPROPERTY(ReplicatedUsing = OnRep_State,
		VisibleAnywhere, BlueprintReadOnly, Category = "NU|Skill", meta = (AllowPrivateAccess = "true"))
	FNUPlayerSkillState State;
};
