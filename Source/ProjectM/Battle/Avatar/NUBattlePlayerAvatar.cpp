// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/Avatar/NUBattlePlayerAvatar.h"

#include "AnimSequences/PaperZDAnimSequence.h"
#include "AnimSequences/Skins/PaperZDAnimationSkin_Flipbook.h"
#include "Audio/NUSFXTags.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "Field/NUSpriteVisualSettings.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Field/NUSpriteVisualComponent.h"
#include "GameFramework/PlayerController.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

ANUBattlePlayerAvatar::ANUBattlePlayerAvatar()
{
	// 겹침 렌더러 구성은 AFieldPlayerCharacter·ANUCharacterPreviewActor와 같은 값이어야 함
	// Y는 겹치는 순서(깊이), Z는 파츠 그림의 몸 위치 보정임
	const auto MakeLayer = [this](const TCHAR* Name, const FVector& Location) -> UPaperFlipbookComponent*
	{
		UPaperFlipbookComponent* Component = CreateDefaultSubobject<UPaperFlipbookComponent>(Name);
		Component->SetupAttachment(Sprite);
		Component->SetRelativeLocation(Location);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetVisibility(false); // 스킨을 입히기 전에는 숨김
		return Component;
	};
	EquipSprite         = MakeLayer(TEXT("EquipSprite"),         FVector(0.0f, 2.0f, -5.0f));
	EquipSpriteGloves   = MakeLayer(TEXT("EquipSpriteGloves"),   FVector(0.0f, 4.0f, -5.0f));
	EquipSpriteBoots    = MakeLayer(TEXT("EquipSpriteBoots"),    FVector(0.0f, 1.0f, -6.0f));
	EquipSpriteLegs     = MakeLayer(TEXT("EquipSpriteLegs"),     FVector(0.0f, 3.0f, -5.0f));
	EquipSpriteShoulder = MakeLayer(TEXT("EquipSpriteShoulder"), FVector(0.0f, 5.0f, -5.0f));
	
	// 커마 파츠는 Z 0 --- 몸과 같은 캔버스·피벗이라 원점만 맞추면 됨 (장비 -5는 옛 몸 보정).
	// Y는 필드와 같은 값 --- 얼굴 1, 머리 2로 머리가 앞 (새 남캐는 머리가 얼굴을 덮어야 맞음, 9/12)
	HairSprite          = MakeLayer(TEXT("HairSprite"),          FVector(0.0f, 2.0f, 0.0f));
	FaceSprite          = MakeLayer(TEXT("FaceSprite"),          FVector(0.0f, 1.0f, 0.0f));

	// 레이어 번호는 NUSpriteVisuals::GetCharacterLayers()가 유일한 출처임
	// 액터마다 숫자를 적으면 한쪽만 고쳐져 어긋남
	const TArray<UPaperFlipbookComponent*> LayerComponents = {
		EquipSprite, EquipSpriteGloves, EquipSpriteBoots, EquipSpriteLegs, EquipSpriteShoulder,
		HairSprite, FaceSprite };
	const TArray<FNUSpriteLayerDef>& LayerDefs = NUSpriteVisuals::GetCharacterLayers();
	for (int32 i = 0; i < LayerDefs.Num() && i < LayerComponents.Num(); ++i)
	{
		SpriteVisual->RegisterLayerRenderComponent(
			LayerComponents[i], LayerDefs[i].Layer, LayerDefs[i].BounceScale, LayerDefs[i].FrameDelay);
	}
}

void ANUBattlePlayerAvatar::ApplyCombatantAppearance()
{
	if (!IsValid(SpriteVisual))
	{
		return;
	}

	// 몸 줄에 전투 그림이 있으면 그것을 입음
	// 없으면 필드와 같은 애님 BP·스킨으로 서고, 전투 방향이 Up 이라 필드 뒷모습이 됨
	// (여캐 · 전투 그림이 없는 직업이 자연스럽게 돌아오는 자리)
	bCombatAppearanceActive = ApplyCombatAppearance();
	if (bCombatAppearanceActive)
	{
		return;
	}

	// 필드와 같은 팩토리를 호출, 필드 액터를 참조하지 않음
	// 나중에 직업, 종족, 치장이 붙으면 이 팩토리 안이 바뀌고 호출부는 그대로
	SpriteVisual->SetLayerBounceEnabled(true);
	SpriteVisual->ApplyAnimBlueprintClass(NUSpriteVisuals::MakeFieldCharacter().AnimBlueprint);

	// 맨몸 애님 BP만 걸면 필드에서 입고 있던 몸·장비·머리·외모가 전부 빠진 기본 모습이 나옴
	RefreshCharacterSkins();
}

bool ANUBattlePlayerAvatar::ApplyCombatAppearance()
{
	int32 RaceId = 0;
	int32 Gender = 0;
	int32 HairId = 0;
	int32 FaceId = 0;
	int32 JobId = 0;
	const ANUPlayerState* NUState = nullptr;

	AppearanceJobId = INDEX_NONE;
	if (!ResolveAppearanceIds(RaceId, Gender, HairId, FaceId, JobId, NUState))
	{
		return false;
	}

	AppearanceJobId = JobId;
	
	// 애님 BP 를 교체하기 전에 먼저 봐야함 - 전투 그림이 없으면 필드 애님 BP 를 그대로 사용
	const FNUBodyRow* Body = NUAppearance::FindBody(RaceId, Gender, JobId);
	if (Body == nullptr || Body->CombatBodySkin.IsNull())
	{
		return false;
	}

	if (!SpriteVisual->ApplyAnimBlueprintClass(NUSpriteVisuals::GetCombatCharacterAnimBlueprint()))
	{
		return false;
	}

	// 전투 그림은 머리·얼굴이 프레임마다 그려져 있어 바운스를 끔
	SpriteVisual->SetLayerBounceEnabled(false);

	if (!SpriteVisual->RefreshCombatAppearanceSkins(RaceId, Gender, HairId, JobId))
	{
		return false;
	}

	// 직업별 공격 모션이 있으면 그것 - 길이와 Hit·Cast 표시가 그 직업 그림 기준
	// 없으면 공용(노비스), 표시도 노비스 그림 기준이 됨
	CombatAttackSequence = Body->CombatAttackSequence.LoadSynchronous();
	if (!IsValid(CombatAttackSequence))
	{
		if (!Body->CombatAttackSequence.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("[NU] %s: 몸 줄의 전투 공격 시퀀스를 읽지 못해 공용 시퀀스를 재생합니다 (%s)"),
				*GetNameSafe(this), *Body->CombatAttackSequence.ToString());
		}

		CombatAttackSequence = NUSpriteVisuals::GetCombatCharacterAttackSequence().LoadSynchronous();
	}

	if (!IsValid(CombatAttackSequence))
	{
		UE_LOG(LogTemp, Warning, TEXT("[NU] %s: 전투 공격 시퀀스를 찾지 못해 공격 모션 없이 섭니다"),
			*GetNameSafe(this));
	}

	// 실제로 재생할 플립북의 공격모션 길이를 재둠
	CombatAttackDuration = IsValid(CombatAttackSequence) ? CombatAttackSequence->GetTotalDuration() : 0.f;

	const UPaperZDAnimationSkin_Flipbook* BodySkin =
		Cast<UPaperZDAnimationSkin_Flipbook>(Body->CombatBodySkin.LoadSynchronous());
	const FPaperZDFlipbookSkinData* SkinData = IsValid(BodySkin) && IsValid(CombatAttackSequence)
		? BodySkin->GetSkinsPerAnimation().Find(TSoftObjectPtr<UPaperZDAnimSequence>(CombatAttackSequence.Get()))
		: nullptr;

	const UPaperFlipbook* AttackFlipbook = SkinData != nullptr && SkinData->AnimationDirections.IsValidIndex(0)
		? SkinData->AnimationDirections[0].Get()
		: nullptr;

	if (IsValid(AttackFlipbook))
	{
		CombatAttackDuration = AttackFlipbook->GetTotalDuration();
	}

	return true;
}

UPaperZDAnimSequence* ANUBattlePlayerAvatar::ResolveActionAnimation(const FNUBattleActionMotionSpec& Spec) const
{
	// 전투 그림을 입은 캐릭터는 어빌리티에 물린 애님 시퀀스 대신 전투 공격모션 재생
	// 제자리 행동(아이템 · 버프 · 포획 · 도주)은 공격 모션 X
	// 퀘스트 달래기 및 돌 옮기기도 공격 모션 X
	if (bCombatAppearanceActive)
	{
		return Spec.Motion != ENUBattleActionMotion::InPlace && !Spec.bSkipAttackPresentation
			? CombatAttackSequence.Get()
			: nullptr;
	}

	return Super::ResolveActionAnimation(Spec);
}

float ANUBattlePlayerAvatar::GetActionAnimationDuration(const UPaperZDAnimSequence* Sequence) const
{
	if (bCombatAppearanceActive && Sequence == CombatAttackSequence && CombatAttackDuration > KINDA_SMALL_NUMBER)
	{
		return CombatAttackDuration;
	}

	return Super::GetActionAnimationDuration(Sequence);
}

void ANUBattlePlayerAvatar::ResolveActionVFX(FNUBattleActionMotionSpec& InOutSpec) const
{
	// 스킬 표가 정한 이펙트 · 제자리 행동(아이템 · 버프 · 포획 · 도주)은 그대로
	// 직업 연출은 공격 모션에 맞춘 것이라 제자리 행동에 붙으면 버프에 카드가 날아감
	// 공격이 아닌 전진 행동도 같음
	if (InOutSpec.bHasSkillVFXProfile || InOutSpec.Motion == ENUBattleActionMotion::InPlace
		|| InOutSpec.bSkipAttackPresentation)
	{
		return;
	}

	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	const FNUJobAttackVFX* JobVFX = IsValid(Settings) ? Settings->JobAttackVFX.Find(AppearanceJobId) : nullptr;
	if (!JobVFX)
	{
		return;
	}

	// 행이 있으면 통째로 덮음, 빈 칸은 안 나감
	InOutSpec.CastVFX = JobVFX->CastVFX;
	InOutSpec.ProjectileVFX = JobVFX->ProjectileVFX;
	InOutSpec.ImpactVFX = JobVFX->ImpactVFX;
	InOutSpec.EffectColor = JobVFX->EffectColor;

	// 빈 행이면 화면엔 아무것도 안 떠서, 설정이 먹었는지 여기서만 확인됨
	UE_LOG(LogTemp, Log, TEXT("[NU] %s: 직업 %d 의 기본 공격 연출로 바꿈 (시전=%s 탄환=%s)"),
		*GetNameSafe(this), AppearanceJobId,
		InOutSpec.CastVFX.IsNull() ? TEXT("없음") : *InOutSpec.CastVFX.GetAssetName(),
		InOutSpec.ProjectileVFX.IsNull() ? TEXT("없음") : *InOutSpec.ProjectileVFX.GetAssetName());
}

void ANUBattlePlayerAvatar::CollectResolvedActionVFX(TArray<TSoftObjectPtr<UNiagaraSystem>>& OutVFX) const
{
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	const FNUJobAttackVFX* JobVFX = IsValid(Settings) ? Settings->JobAttackVFX.Find(AppearanceJobId) : nullptr;
	if (!JobVFX)
	{
		return;
	}

	OutVFX.AddUnique(JobVFX->CastVFX);
	OutVFX.AddUnique(JobVFX->ProjectileVFX);
	OutVFX.AddUnique(JobVFX->ImpactVFX);
}

FGameplayTag ANUBattlePlayerAvatar::GetAttackSFX(ENUBattleAttackSFX Moment) const
{
	// 직업 번호는 JobAttackVFX 의 키와 같음. 이펙트에 걸린 소리가 먼저라 휘두름 · 닿음은 이펙트가 없는 칸이나
	// 직업끼리 같이 쓰는 이펙트(베기 자국은 검과 곰 발톱이 같이 씀)일 때 쓰임
	// 준비 소리는 공격 모션 그림을 따라감: 검 치켜들기 · 활 당기기 · 마력 모으기 · 카드 뽑기 · 곰 변신
	struct FJobSFX
	{
		int32 JobId;
		FGameplayTag Windup;
		FGameplayTag Swing;
		FGameplayTag Impact;
	};

	static const FJobSFX Jobs[] =
	{
		{ 1001, NU::SFX::Battle::Windup::Sword, NU::SFX::Battle::Attack::Slash, NU::SFX::Battle::Impact::Slash },
		{ 1002, NU::SFX::Battle::Windup::Bow, NU::SFX::Battle::Attack::Arrow, NU::SFX::Battle::Impact::Pierce },
		{ 1003, NU::SFX::Battle::Windup::Magic, NU::SFX::Battle::Attack::Magic, NU::SFX::Battle::Impact::Magic },
		{ 1004, NU::SFX::Battle::Windup::Card, NU::SFX::Battle::Attack::Card, NU::SFX::Battle::Impact::Card },
		{ 1005, NU::SFX::Battle::Windup::Beast, NU::SFX::Battle::Attack::Beast, NU::SFX::Battle::Impact::Claw },
		// 노비스 · 직업을 모르는 캐릭터는 맨손 주먹
		{ INDEX_NONE, FGameplayTag(), NU::SFX::Battle::Attack::Punch, NU::SFX::Battle::Impact::Punch },
	};

	for (const FJobSFX& Job : Jobs)
	{
		if (Job.JobId == AppearanceJobId || Job.JobId == INDEX_NONE)
		{
			return Moment == ENUBattleAttackSFX::Windup ? Job.Windup : Moment == ENUBattleAttackSFX::Swing ? Job.Swing : Job.Impact;
		}
	}

	return FGameplayTag();
}

void ANUBattlePlayerAvatar::SetServerAppearance(int32 RaceId, int32 Gender, int32 HairId, int32 FaceId, int32 JobId)
{
	bHasServerAppearance = true;
	ServerRaceId = RaceId;
	ServerGender = Gender;
	ServerHairId = HairId;
	ServerFaceId = FaceId;
	ServerJobId = JobId;
}

bool ANUBattlePlayerAvatar::ResolveAppearanceIds(int32& OutRaceId, int32& OutGender, int32& OutHairId,
	int32& OutFaceId, int32& OutJobId, const ANUPlayerState*& OutState) const
{
	// 넷 전투의 파티원은 서버가 준 외형, 그 사람의 PlayerState 가 이 월드에 없어 장비는 못 입힘
	if (bHasServerAppearance)
	{
		OutState = nullptr;
		OutRaceId = ServerRaceId;
		OutGender = ServerGender;
		OutHairId = ServerHairId;
		OutFaceId = ServerFaceId;
		OutJobId = ServerJobId;

		return true;
	}

	// 로컬 전투는 AbilitySystemOwner에서, 넷 전투는 ASC가 없어 로컬 플레이어에서 가져옴
	OutState = Cast<ANUPlayerState>(GetAbilitySystemOwner());
	if (!IsValid(OutState))
	{
		const UWorld* World = GetWorld();
		const APlayerController* PC = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
		OutState = IsValid(PC) ? PC->GetPlayerState<ANUPlayerState>() : nullptr;
	}

	// 외형 번호는 캐릭터 매니저가 들고 있음, GameInstance 수명이라 전투에 들어와도 남아 있음
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUCharacterManager* Manager =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUCharacterManager>() : nullptr;

	FNUCharacterSummary Selected;
	if (!IsValid(Manager) || !Manager->GetSelectedOrTestCharacter(Selected))
	{
		return false;
	}

	// 직업 몸은 직업 컴포넌트로 구분, 아직 안 받았으면 목록 값을 씀 - 필드와 같음
	const UNUJobComponent* Job = IsValid(OutState) ? OutState->GetJobComponent() : nullptr;

	OutRaceId = Selected.RaceId;
	OutGender = Selected.Gender;
	OutHairId = Selected.HairId;
	OutFaceId = Selected.FaceId;
	OutJobId = IsValid(Job) && Job->HasJob() ? Job->GetJobId() : Selected.JobId;

	return true;
}

void ANUBattlePlayerAvatar::RefreshCharacterSkins()
{
	if (GetNetMode() == NM_DedicatedServer || !IsValid(SpriteVisual))
	{
		return;
	}

	int32 RaceId = 0;
	int32 Gender = 0;
	int32 HairId = 0;
	int32 FaceId = 0;
	int32 JobId = 0;
	const ANUPlayerState* NUState = nullptr;
	const bool bHasAppearance = ResolveAppearanceIds(RaceId, Gender, HairId, FaceId, JobId, NUState);

	// 장비는 필드와 같은 출처를 씀 - AFieldPlayerCharacter::RefreshEquipmentSkin과 같은 호출임
	if (IsValid(NUState))
	{
		SpriteVisual->RefreshEquipmentSkins(
			NUState->GetEquipmentComponent(), NUState->GetInventoryComponent());
	}

	if (bHasAppearance)
	{
		SpriteVisual->RefreshAppearanceSkins(RaceId, Gender, HairId, FaceId, JobId);
	}
}
