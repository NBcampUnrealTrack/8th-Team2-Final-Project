// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUBattleAvatar.h"
#include "NUBattlePlayerAvatar.generated.h"

class UPaperFlipbookComponent;
class UPaperZDAnimSequence;
class ANUPlayerState;

// 플레이어 캐릭터 전용 전투 아바타
// 몸·장비·커스터마이징이 여러 장으로 겹쳐 그려지므로, 그 레이어와 스킨 적용이 추가됨
UCLASS()
class PROJECTM_API ANUBattlePlayerAvatar : public ANUBattleAvatar
{
	GENERATED_BODY()

public:
	ANUBattlePlayerAvatar();

	// 넷 전투의 다른 플레이어(파티원) 외형, InitializeNetAvatar 전에 넣어야 외형을 입힐 때 쓰임
	// 넣지 않으면 로컬 플레이어의 선택 캐릭터를 따름 (내 캐릭터). 장비는 서버가 안 보내 입히지 않음
	void SetServerAppearance(int32 RaceId, int32 Gender, int32 HairId, int32 FaceId, int32 JobId);

protected:
	// 몸 줄에 전투 그림이 있으면 전투 애님 BP 와 전투 스킨, 없으면 필드와 같은 애님 BP 와 스킨
	virtual void ApplyCombatantAppearance() override;

	// 전투 그림을 입었으면 앞으로 나가는 행동에 전투 공격을 재생함
	virtual UPaperZDAnimSequence* ResolveActionAnimation(const FNUBattleActionMotionSpec& Spec) const override;

	// 전투 공격은 직업마다 길이가 다름, 입은 몸 스킨의 공격 플립북 길이를 돌려줌
	virtual float GetActionAnimationDuration(const UPaperZDAnimSequence* Sequence) const override;

	// 스킬 표 행이 없는 공격은 직업의 기본 공격 연출(UNUBattleDataSettings::JobAttackVFX)로 바꿈
	virtual void ResolveActionVFX(FNUBattleActionMotionSpec& InOutSpec) const override;

	// 외형을 입을 때 읽은 직업의 기본 공격 연출
	virtual void CollectResolvedActionVFX(TArray<TSoftObjectPtr<UNiagaraSystem>>& OutVFX) const override;

	// 직업별 공격 효과음 (준비 · 휘두름 · 닿음), 노비스는 맨손
	virtual FGameplayTag GetAttackSFX(ENUBattleAttackSFX Moment) const override;

private:
	// 필드에서 입고 있던 장비·외모를 그대로 입힘
	// 장비는 PlayerState, 외모는 캐릭터 매니저에서 읽으므로 필드 액터를 참조하지 않는다
	void RefreshCharacterSkins();

	// 전투 그림을 입혀 봄, 몸 줄에 전투 스킨이 없으면 false
	bool ApplyCombatAppearance();

	// 외형 번호 - 장비·외모와 같은 출처 (로컬 전투는 ASC 소유자, 넷 전투는 로컬 플레이어)
	// 외형 번호를 못 읽으면 false, OutState 는 그래도 채워 둠 (장비는 따로 입히므로)
	bool ResolveAppearanceIds(int32& OutRaceId, int32& OutGender, int32& OutHairId, int32& OutFaceId,
		int32& OutJobId, const ANUPlayerState*& OutState) const;

private:
	// 장비·커스터마이징 겹침 렌더러 --- 구성과 위치를 AFieldPlayerCharacter·
	// ANUCharacterPreviewActor와 똑같이 맞춰야 전투 모습이 필드와 어긋나지 않음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> EquipSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteGloves;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteBoots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteLegs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> EquipSpriteShoulder;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> HairSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> FaceSprite;
	
	// 전투 그림을 입었는지, 공격 시퀀스를 가를 때 사용함
	bool bCombatAppearanceActive = false;

	// 전투 공격 시퀀스, 전투 그림을 입을 때 한 번 읽어 둠
	UPROPERTY(Transient)
	TObjectPtr<UPaperZDAnimSequence> CombatAttackSequence;

	// 실제로 재생하는 공격 플립북의 길이, 전투 그림을 입을 때 한 번 재 둠
	float CombatAttackDuration = 0.f;

	// 외형을 입힐 때 읽은 직업, 기본 공격 연출을 고를 때 씀
	int32 AppearanceJobId = INDEX_NONE;

	// SetServerAppearance 로 받은 파티원 외형
	bool bHasServerAppearance = false;
	int32 ServerRaceId = 0;
	int32 ServerGender = 0;
	int32 ServerHairId = 0;
	int32 ServerFaceId = 0;
	int32 ServerJobId = 0;
};
