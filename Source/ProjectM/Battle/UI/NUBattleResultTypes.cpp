// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/UI/NUBattleResultTypes.h"

#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Net/NUExperienceGrowth.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "UI/NUUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUBattleResult, Log, All);


namespace
{
	ANUPlayerState* GetResultOwnerPlayerState(const UObject* WorldContextObject)
	{
		const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
		const APlayerController* PlayerController = IsValid(World)
			? World->GetFirstPlayerController()
			: nullptr;

		return IsValid(PlayerController) ? Cast<ANUPlayerState>(PlayerController->PlayerState) : nullptr;
	}

	// 아이템 아이콘 이름 규칙, 인벤토리 창(UNUSheetWidgetBase::LoadResultItemIconTexture)과 같은 규칙
	// 한쪽만 고치면 결과창에서만 아이콘이 빠짐
	UTexture2D* LoadResultItemIcon(FName ItemRowName)
	{
		const UNUUISettings* Settings = GetDefault<UNUUISettings>();
		if (ItemRowName.IsNone() || !Settings || Settings->ItemIconFolder.IsEmpty())
		{
			return nullptr;
		}

		const FString AssetName = FString::Printf(TEXT("T_Item_%s"), *ItemRowName.ToString());
		const FSoftObjectPath IconPath(
			FString::Printf(TEXT("%s/%s.%s"), *Settings->ItemIconFolder, *AssetName, *AssetName));

		return Cast<UTexture2D>(IconPath.TryLoad());
	}

	// 같은 것을 여러 개 얻었을 때 줄을 늘리지 않고 개수만 올림
	void AddOrMergeResultEntry(TArray<FNUBattleResultEntry>& Entries, int32 NumericId, const FText& DisplayName,
		int32 Count, UTexture2D* Icon)
	{
		if (Count <= 0)
		{
			return;
		}

		FNUBattleResultEntry* Existing = Entries.FindByPredicate(
			[NumericId](const FNUBattleResultEntry& Entry)
			{
				return Entry.NumericId == NumericId;
			});

		if (Existing != nullptr)
		{
			Existing->Count += Count;
			return;
		}

		FNUBattleResultEntry& Added = Entries.AddDefaulted_GetRef();
		Added.NumericId = NumericId;
		Added.DisplayName = DisplayName;
		Added.Count = Count;
		Added.Icon = Icon;
	}
}

float FNUBattleResultMember::GetExperienceRatio() const
{
	if (!bHasExperience || RequiredExperience <= 0)
	{
		// 만렙이면 서버가 필요량을 0 으로 줌, 바는 가득 찬 것으로 그리는 것이 맞음
		return bHasExperience ? 1.f : 0.f;
	}

	return FMath::Clamp(static_cast<float>(ExperienceIntoLevel) / static_cast<float>(RequiredExperience), 0.f, 1.f);
}

int64 FNUBattleResultMember::GetRemainingExperience() const
{
	if (!bHasExperience)
	{
		return 0;
	}

	return FMath::Max<int64>(0, RequiredExperience - ExperienceIntoLevel);
}

void NUBattleResult::BuildMembers(const UObject* WorldContextObject, const FNUExperienceGrowth* Growth,
	TArray<FNUBattleResultMember>& OutMembers)
{
	OutMembers.Reset();

	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	ANUPlayerState* PlayerState = GetResultOwnerPlayerState(WorldContextObject);

	// --- 내 캐릭터 ---
	{
		FNUBattleResultMember& Member = OutMembers.AddDefaulted_GetRef();

		FNUCharacterSummary Summary;
		const UNUCharacterManager* Characters = IsValid(GameInstance)
			? GameInstance->GetSubsystem<UNUCharacterManager>()
			: nullptr;
		if (IsValid(Characters) && Characters->GetSelectedCharacter(Summary))
		{
			Member.DisplayName = FText::FromString(Summary.Name);
			Member.Level = Summary.Level;
		}
		// 고른 캐릭터가 없으면(맵을 바로 연 시험 플레이) HUD 상태 판과 같이 플레이어 이름을 잘라 씀
		if (Member.DisplayName.IsEmpty() && IsValid(PlayerState))
		{
			Member.DisplayName = FText::FromString(NUCharacterName::Shorten(PlayerState->GetPlayerName()));
		}
		Member.PreviousLevel = Member.Level;

		// 경험치는 서버가 확정한 값만 사용함
		if (Growth)
		{
			Member.Level = Growth->NewLevel;
			Member.PreviousLevel = Growth->OldLevel;
			Member.bLeveledUp = Growth->IsLevelUp();
			Member.bHasExperience = true;
			Member.ExperienceIntoLevel = Growth->ExperienceIntoLevel;
			Member.RequiredExperience = Growth->RequiredExperienceToNext;
		}
		else if (const UNUProgressionComponent* Progression =
			IsValid(PlayerState) ? PlayerState->GetProgressionComponent() : nullptr)
		{
			// 보상이 없는 종료(패배·도주·포획)라 이번 전투로 바뀐 것이 없음, 지금 상태를 그대로 보여줌
			Member.Level = Progression->GetLevel();
			Member.PreviousLevel = Member.Level;
			Member.bHasExperience = true;
			Member.ExperienceIntoLevel = Progression->GetExperienceIntoLevel();
			Member.RequiredExperience = Progression->GetRequiredExperienceToNext();
		}
	}

	// --- 파티 나이아 ---
	const UNUNaiaRosterComponent* Roster = IsValid(PlayerState)
		? PlayerState->GetNaiaRosterComponent()
		: nullptr;
	if (!IsValid(Roster))
	{
		return;
	}

	const UNUNaiaDataSubsystem* NaiaData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>()
		: nullptr;

	for (const TObjectPtr<ANUSpiritState>& Slot : Roster->GetParty())
	{
		const ANUSpiritState* Naia = Slot.Get();
		if (!IsValid(Naia))
		{
			continue;
		}

		FNUBattleResultMember& Member = OutMembers.AddDefaulted_GetRef();
		Member.Level = Naia->GetCurrentLevel();
		Member.PreviousLevel = Member.Level;

		// SpiritId는 표의 NaiaId
		const int32 NaiaId = Naia->GetSpiritId();
		Member.NaiaId = NaiaId;
		const FNUNaiaDefinition* Definition = IsValid(NaiaData)
			? NaiaData->FindDefinitionById(NaiaId)
			: nullptr;

		Member.DisplayName = Definition != nullptr && !Definition->KoreanName.IsEmpty()
			? Definition->KoreanName
			: FText::AsNumber(NaiaId);

		// 나이아 경험치는 서버가 주지 않음 - bHasExperience 를 그대로 거짓으로 두면
		// 줄 위젯이 바를 비워 그린다. 값이 생기면 여기만 채우면 됨
	}
}

void NUBattleResult::BuildItemEntries(const UObject* WorldContextObject, const TArray<int32>& ItemIds,
	const TArray<int32>& Counts, TArray<FNUBattleResultEntry>& OutEntries)
{
	OutEntries.Reset();

	if (ItemIds.IsEmpty())
	{
		return;
	}

	ANUPlayerState* PlayerState = GetResultOwnerPlayerState(WorldContextObject);
	const UNUInventoryComponent* Inventory = IsValid(PlayerState)
		? PlayerState->GetInventoryComponent()
		: nullptr;

	for (int32 Index = 0; Index < ItemIds.Num(); ++Index)
	{
		const int32 ItemId = ItemIds[Index];
		const int32 Count = Counts.IsValidIndex(Index) ? Counts[Index] : 1;

		const FNUItemDefRow* Definition = IsValid(Inventory)
			? Inventory->FindItemDefByNumericId(ItemId)
			: nullptr;
		if (!Definition)
		{
			// 표에 없는 숫자, 이름을 지어내지 않고 id 를 그대로 띄우고 로그를 남김
			UE_LOG(LogNUBattleResult, Warning,
				TEXT("획득 아이템 %d 의 정의가 표에 없음 (NumericId 확인)"), ItemId);
		}

		// 아이콘은 행 이름으로 찾는다, 표에 없는 아이템이면 이름만 뜸
		const FName RowName = IsValid(Inventory)
			? Inventory->FindRowNameByNumericId(ItemId)
			: NAME_None;

		AddOrMergeResultEntry(
			OutEntries,
			ItemId,
			Definition ? Definition->DisplayName : FText::AsNumber(ItemId),
			Count,
			LoadResultItemIcon(RowName));
	}
}

void NUBattleResult::BuildNaiaEntries(const UObject* WorldContextObject, const TArray<int32>& NaiaIds,
	TArray<FNUBattleResultEntry>& OutEntries)
{
	OutEntries.Reset();

	if (NaiaIds.IsEmpty())
	{
		return;
	}

	const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	const UNUNaiaDataSubsystem* NaiaData = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>()
		: nullptr;

	for (const int32 NaiaId : NaiaIds)
	{
		const FNUNaiaDefinition* Definition = IsValid(NaiaData)
			? NaiaData->FindDefinitionById(NaiaId)
			: nullptr;

		// 나이아는 정의가 아이콘을 직접 들고 있음
		AddOrMergeResultEntry(
			OutEntries,
			NaiaId,
			Definition && !Definition->KoreanName.IsEmpty()
				? Definition->KoreanName
				: FText::AsNumber(NaiaId),
			1,
			Definition ? Definition->IconTexture.LoadSynchronous() : nullptr);
	}
}
