// Fill out your copyright notice in the Description page of Project Settings.


#include "NUNaiaRosterComponent.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/UI/NUActionBannerWidget.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDataSettings.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Naia/Net/NUNaiaNetSubsystem.h"
#include "Naia/Net/NUNaiaServerTypes.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Naia/UI/NUNaiaRosterWindowWidget.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "UI/NUUIManager.h"

namespace
{
	// 테스트용 기본 스킬
	// 나이아 스킬 획득이 구현되면 삭제하면 됨
	void GiveDefaultActiveSkills(const FGameplayTag& ElementTag, TArray<int32>& OutSkillIds)
	{
		// 속성별로 스킬 시작위치 지정
		static const TMap<FGameplayTag, int32> ElementBands =
		{
			{ NU::Element::Fire,      2000 },
			{ NU::Element::Water,     2100 },
			{ NU::Element::Grass,     2200 },
			{ NU::Element::Earth,     2300 },
			{ NU::Element::Lightning, 2400 },
			{ NU::Element::Wind,      2500 },
		};

		const int32* Band = ElementBands.Find(ElementTag);
		if (!Band)
		{
			return;
		}

		// 1x01 과 1x03 스킬들은 전용 구현이 필요 없어서 바로 검증 가능함
		OutSkillIds.Add(*Band + 1);
		OutSkillIds.Add(*Band + 3);
	}

	// 패시브는 포획 시 랜덤한 하나를 획득함
	// 두 후보의 확률은 시트에서 합이 항상 1이라 앞쪽 확률 하나로 가르면 됨
	int32 RollPassiveSkill(const FNUNaiaDefinition& Definition)
	{
		if (Definition.PassiveSkill1Id <= 0)
		{
			return Definition.PassiveSkill2Id;
		}
		
		if (Definition.PassiveSkill2Id <= 0)
		{
			return Definition.PassiveSkill1Id;
		}
		
		return FMath::FRand() < Definition.PassiveSkill1Chance
			? Definition.PassiveSkill1Id
			: Definition.PassiveSkill2Id;
	}
}


UNUNaiaRosterComponent::UNUNaiaRosterComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	StorageList.OwnerComponent = this;
}

void UNUNaiaRosterComponent::BeginPlay()
{
	Super::BeginPlay();

	// 로컬 나이아가 필요하면 nu.Naia.DebugAcquire 콘솔 명령으로 넣을 것

	if (IsLocalPlayerRoster())
	{
		if (UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem())
		{
			NaiaNet->OnRosterNetChanged.AddUObject(this, &ThisClass::HandleServerRosterChanged);
			NaiaNet->OnMoveRejected.AddUObject(this, &ThisClass::HandleMoveRejected);
			NaiaNet->OnStarterNaiaContractRejected.AddUObject(this, &ThisClass::HandleStarterNaiaContractRejected);

			// 레벨 전환 등으로 컴포넌트가 다시 만들어졌는데 서버 로스터는 이미 받아둔 상태일 수 있음
			if (NaiaNet->HasReceivedRoster())
			{
				HandleServerRosterChanged();
			}
		}
	}
}

void UNUNaiaRosterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem())
	{
		NaiaNet->OnRosterNetChanged.RemoveAll(this);
		NaiaNet->OnMoveRejected.RemoveAll(this);
		NaiaNet->OnStarterNaiaContractRejected.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void UNUNaiaRosterComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME_CONDITION(ThisClass, PartyNaias, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, StorageList, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, SelectedStarterNaiaId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ThisClass, StarterNaiaInstanceId, COND_OwnerOnly);
}

bool UNUNaiaRosterComponent::CanAcceptNaia() const
{
	return !IsPartyFull() || FindFreeStorageSlot() != INDEX_NONE;
}

bool UNUNaiaRosterComponent::AuthorizeStarterNaiaSelection()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || HasSelectedStarterNaia())
	{
		return false;
	}

	bStarterNaiaSelectionAuthorized = true;
	return true;
}

bool UNUNaiaRosterComponent::AcquireNaia(int32 NaiaId, int32 Level, int64* OutInstanceId)
{
	if (OutInstanceId != nullptr)
	{
		*OutInstanceId = 0;
	}
	if (!IsValid(GetOwner()) || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (NaiaId <= 0 || !CanAcceptNaia())	// Id가 잘못됐거나 빈자리가 없다면 false
	{
		return false;
	}

	const FNUNaiaInstance Entry = MakeNewEntry(NaiaId, Level);

	// 파티에 빈 칸이 있으면 파티, 없으면 보관함
	// 파티는 액터가 생기고 보관함은 데이터만 들어가므로 액터 유무로 성공을 판정하면 안 됨
	const bool bToParty = !IsPartyFull();
	const bool bAcquired = bToParty
		? PutToParty(Entry, PartyNaias.Num()) != nullptr
		: PutToStorage(Entry, FindFreeStorageSlot());

	if (!bAcquired)
	{
		return false;
	}
	if (OutInstanceId != nullptr)
	{
		*OutInstanceId = Entry.InstanceId;
	}

	UE_LOG(LogTemp, Log, TEXT("나이아 획득 : NaiaId=%d, Level=%d, 패시브=%d -> %s (파티 %d / 보관함 %d)"),
		Entry.NaiaId, Entry.Level, Entry.PassiveSkillId,
		bToParty ? TEXT("파티") : TEXT("보관함"),
		PartyNaias.Num(), StorageList.Naias.Num());

	SyncActiveNaia();
	OnRosterChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
	return true;
}

bool UNUNaiaRosterComponent::CanSelectStarterNaia(int32 NaiaId, FString& OutError) const
{
	if (HasSelectedStarterNaia())
	{
		OutError = TEXT("시작 나이아를 이미 선택함");
		return false;
	}

	if (const UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem(); IsValid(NaiaNet) && NaiaNet->HasActiveServerSession())
	{
		if (!NaiaNet->HasReceivedRoster())
		{
			OutError = TEXT("나이아 로스터 동기화가 끝나지 않음");
			return false;
		}
		if (NaiaNet->IsStarterNaiaContractPending())
		{
			OutError = TEXT("스타팅 나이아 계약 요청을 처리 중임");
			return false;
		}
	}
	if (!CanAcceptNaia())
	{
		OutError = TEXT("파티와 보관함에 빈자리가 없음");
		return false;
	}

	const FNUNaiaDefinition* Definition = FindDefinition(NaiaId);
	if (Definition == nullptr || !Definition->IsStarterSelectionCandidate())
	{
		OutError = FString::Printf(TEXT("시작 나이아로 선택할 수 없는 NaiaId: %d"), NaiaId);
		return false;
	}

	OutError.Reset();
	return true;
}

bool UNUNaiaRosterComponent::SelectStarterNaia(int32 NaiaId, FString& OutError)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		OutError = TEXT("서버 권한에서만 시작 나이아를 선택할 수 있음");
		return false;
	}
	if (!CanSelectStarterNaia(NaiaId, OutError))
	{
		return false;
	}

	if (!bStarterNaiaSelectionAuthorized)
	{
		OutError = TEXT("시작 나이아 선택 단계가 허용되지 않음");
		return false;
	}

	ANUPlayerState* PlayerState = GetOwningPlayerState();
	const UNUNaiaDataSettings* Settings = GetDefault<UNUNaiaDataSettings>();
	const int32 InitialLevel = FMath::Max(1, Settings != nullptr ? Settings->StarterContractLevel : 1);
	int64 NewInstanceId = 0;
	if (!AcquireNaia(NaiaId, InitialLevel, &NewInstanceId))
	{
		OutError = TEXT("시작 나이아를 로스터에 추가하지 못함");
		return false;
	}

	SelectedStarterNaiaId = NaiaId;
	StarterNaiaInstanceId = NewInstanceId;
	bStarterNaiaSelectionAuthorized = false;
	PlayerState->SetNaiaSummoned(true);
	OnRosterChanged.Broadcast();
	OwnerActor->ForceNetUpdate();
	return true;
}

bool UNUNaiaRosterComponent::MoveNaia(int32 FromContainer, int32 FromSlot, int32 ToContainer, int32 ToSlot)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}
	
	// 출발지가 파티or보관함, 목적지가 파티or보관함 이어야 함 
	bool bValidContainers = 
		(FromContainer == NUNaiaContainer::Party || FromContainer == NUNaiaContainer::Storage) 
		&& (ToContainer == NUNaiaContainer::Party || ToContainer == NUNaiaContainer::Storage);
	
	if (!bValidContainers || FromSlot < 0 || ToSlot < 0)
	{
		return false;
	}
	// 같은 장소로 옮기는데 슬롯마저 같다면 종료
	if (FromContainer == ToContainer && FromSlot == ToSlot)
	{
		return false;
	}
	
	// 전투 중엔 편성 변경 불가능, 나이아 교체만 별도로 처리
	if (IsInBattle())
	{
		UE_LOG(LogTemp, Warning, TEXT("전투 중에는 나이아 편성을 바꿀 수 없습니다!"));
		return false;
	}
	
	bool bChanged = false;
	
	// 파티 -> 파티 : 위치 교체
	if (FromContainer == NUNaiaContainer::Party && ToContainer == NUNaiaContainer::Party)
	{
		bChanged = SwapPartySlots(FromSlot, ToSlot);
	}
	// 보관함 -> 보관함 : 위치 교체
	else if (FromContainer == NUNaiaContainer::Storage && ToContainer == NUNaiaContainer::Storage)
	{
		bChanged = SwapStorageSlots(FromSlot, ToSlot);
	}
	else
	{
		if (FromContainer == NUNaiaContainer::Party && PartyNaias.Num() <= 1)
		{
			UE_LOG(LogTemp, Warning, TEXT("파티에 반드시 1마리의 나이아가 존재해야 합니다!"));
			return false;
		}
		
		// 파티라면 파티, 보관함이라면 보관함에서 꺼내기
		FNUNaiaInstance FromEntry;
		bool bTakeFrom = FromContainer == NUNaiaContainer::Party
			? TakeFromParty(FromSlot, FromEntry)
			: TakeFromStorage(FromSlot, FromEntry);
		
		if (!bTakeFrom)
		{
			return false;
		}
		
		// 목적지 칸을 비움 (있으면 교환 / 없으면 이동)
		// 여기는 From != To 목적지/출발지가 달라 반드시 ToContainer 로 판단해야 함
		FNUNaiaInstance ToEntry;
		bool bTakeTo = ToContainer == NUNaiaContainer::Party
			? TakeFromParty(ToSlot, ToEntry)
			: TakeFromStorage(ToSlot, ToEntry);
		
		// From에 있던것을 To로 이동
		if (ToContainer == NUNaiaContainer::Party)
		{
			PutToParty(FromEntry, ToSlot);
		}
		else
		{
			PutToStorage(FromEntry, ToSlot);
		}
		
		// To에서 꺼낸것이 있다면
		if (bTakeTo)
		{
			// To에 있던것을 From으로 이동
			if (FromContainer == NUNaiaContainer::Party)
			{
				PutToParty(ToEntry, FromSlot);
			}
			else
			{
				PutToStorage(ToEntry, FromSlot);
			}
		}
		bChanged = true;
	}
	
	if (!bChanged)
	{
		return false;
	}
	
	SyncActiveNaia();
	OnRosterChanged.Broadcast();
	OwnerActor->ForceNetUpdate();
	return true;
}

bool UNUNaiaRosterComponent::SwapPartyOrderForBattle(int32 SlotA, int32 SlotB)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	if (!SwapPartySlots(SlotA, SlotB))
	{
		return false;
	}

	// 활성(0번)이 바뀌었으면 기존 ActiveSpiritState 처럼 처리
	// 전투 중이라 필드 아바타는 생기지 않음
	SyncActiveNaia();
	OnRosterChanged.Broadcast();
	OwnerActor->ForceNetUpdate();
	return true;
}

bool UNUNaiaRosterComponent::ApplyServerNaiaList(int32 Container, const TArray<FNUNaiaInstance>& Entries)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority()) return false;

	if (Container == NUNaiaContainer::Storage)
	{
		StorageList.Naias = Entries;
		StorageList.MarkArrayDirty();
	}
	else if (Container == NUNaiaContainer::Party)
	{
		// HP만 바뀐 조회/회복 결과는 액터를 재생성하지 않는다. SP와 기존 효과를 유지한다.
		bool bVitalsOnly = Entries.Num() == PartyNaias.Num();
		for (int32 Slot = 0; Slot < Entries.Num() && bVitalsOnly; ++Slot)
		{
			const ANUSpiritState* Naia = PartyNaias[Slot];
			const FNUNaiaInstance& Entry = Entries[Slot];
			bVitalsOnly = IsValid(Naia) && IsValid(Naia->GetNUASC()) && Naia->GetNUASC()->GetVitalAttributeSet()
				&& Naia->GetInstanceId() == Entry.InstanceId && Naia->GetSpiritId() == Entry.NaiaId && Naia->GetCurrentLevel() == Entry.Level
				&& Naia->GetPassiveSkillId() == Entry.PassiveSkillId && Naia->GetActiveSkillIds() == Entry.ActiveSkillIds;
		}
		if (bVitalsOnly)
		{
			for (const FNUNaiaInstance& Entry : Entries) { if (!FMath::IsFinite(Entry.HealthRatio) || Entry.HealthRatio < 0.0f || Entry.HealthRatio > 1.0f) { return false; } }
			for (int32 Slot = 0; Slot < Entries.Num(); ++Slot)
			{
				UNUAbilitySystemComponent* ASC = PartyNaias[Slot]->GetNUASC();
				const float Health = ASC->GetNumericAttribute(UNUVitalAttributeSet::GetMaxHealthAttribute()) * Entries[Slot].HealthRatio;
				if (Health > 0.0f) { ASC->ReviveIfDead(Health); }
				ASC->SetNumericAttributeBase(UNUVitalAttributeSet::GetHealthAttribute(), Health);
			}
		}
		else
		{
			for (int32 Slot = PartyNaias.Num() - 1; Slot >= 0; --Slot)
			{
				FNUNaiaInstance Discarded;
				TakeFromParty(Slot, Discarded);
			}
			for (const FNUNaiaInstance& Entry : Entries) { PutToParty(Entry, PartyNaias.Num()); }
		}
	}
	else
	{
		return false;
	}

	SyncActiveNaia();
	OnRosterChanged.Broadcast();
	OwnerActor->ForceNetUpdate();
	return true;
}

bool UNUNaiaRosterComponent::ApplyServerNaiaSlots(const TArray<FNUServerNaiaSlot>& Slots)
{
	TArray<FNUNaiaInstance> PartyEntries;
	TArray<FNUNaiaInstance> StorageEntries;

	for (const FNUServerNaiaSlot& Slot : Slots)
	{
		if (Slot.IsEmpty())
		{
			continue;
		}

		FNUNaiaInstance Entry;
		Entry.InstanceId = Slot.Instance.InstanceId;
		Entry.NaiaId = Slot.Instance.NaiaId;
		Entry.Level = Slot.Instance.Level;
		Entry.SlotNo = Slot.SlotNo;
		Entry.HealthRatio = Slot.Instance.HealthRatio;
		Entry.Experience = Slot.Instance.Experience;
		Entry.Affection = Slot.Instance.Affection;
		Entry.PassiveSkillId = Slot.Instance.PassiveSkillId;
		Entry.ActiveSkillIds = Slot.Instance.ActiveSkillIds;

		if (Slot.Container == NUNaiaContainer::Party)
		{
			PartyEntries.Add(MoveTemp(Entry));
		}
		else
		{
			StorageEntries.Add(MoveTemp(Entry));
		}
	}

	// 파티는 배열 인덱스가 곧 슬롯이므로 SlotNo 순서로 정렬해서 넣어야 함
	PartyEntries.Sort([](const FNUNaiaInstance& A, const FNUNaiaInstance& B)
	{
		return A.SlotNo < B.SlotNo;
	});

	const bool bPartyApplied = ApplyServerNaiaList(NUNaiaContainer::Party, PartyEntries);
	const bool bStorageApplied = ApplyServerNaiaList(NUNaiaContainer::Storage, StorageEntries);

	return bPartyApplied && bStorageApplied;
}

bool UNUNaiaRosterComponent::RestoreStorageHealth()
{
	return RecoverStorageHealth(1.0f);
}

bool UNUNaiaRosterComponent::RecoverStorageHealth(float HealthRatio)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !FMath::IsFinite(HealthRatio) || HealthRatio < 0.0f)
	{
		return false;
	}

	bool bChanged = false;
	for (FNUNaiaInstance& Entry : StorageList.Naias)
	{
		if (Entry.HealthRatio < 1.0f && HealthRatio > 0.0f)
		{
			Entry.HealthRatio = FMath::Min(1.0f, Entry.HealthRatio + HealthRatio);
			StorageList.MarkItemDirty(Entry);
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnRosterChanged.Broadcast();
		OwnerActor->ForceNetUpdate();
	}
	return true;
}

ANUSpiritState* UNUNaiaRosterComponent::GetActiveNaia() const
{
	return PartyNaias.IsValidIndex(0) ? PartyNaias[0].Get() : nullptr;
}

const TArray<TObjectPtr<ANUSpiritState>>& UNUNaiaRosterComponent::GetParty() const
{
	return PartyNaias;
}

const TArray<FNUNaiaInstance>& UNUNaiaRosterComponent::GetStorage() const
{
	
	return StorageList.Naias;
}

int32 UNUNaiaRosterComponent::GetPartyCount() const
{
	return PartyNaias.Num();
}

bool UNUNaiaRosterComponent::IsPartyFull() const
{
	return PartyNaias.Num() >= MaxPartySize;
}

int32 UNUNaiaRosterComponent::GetFreeSlotCount() const
{
	int32 FreeParty = FMath::Max(0, MaxPartySize - PartyNaias.Num());
	int32 FreeStorage = FMath::Max(0, MaxStorageSlots - StorageList.Naias.Num());

	return FreeParty + FreeStorage;
}

int32 UNUNaiaRosterComponent::GetMaxPartySize() const
{
	return MaxPartySize;
}

int32 UNUNaiaRosterComponent::GetMaxStorageSlots() const
{
	return MaxStorageSlots;
}

// 나중에 UIManager로 관리 필요
void UNUNaiaRosterComponent::ToggleRosterUI()
{
	const ANUPlayerState* PS = GetOwningPlayerState();
	APlayerController* PC = PS ? Cast<APlayerController>(PS->GetOwner()) : nullptr;

	// 로컬 클라의 창만 띄움, 남의 PlayerState에도 이 컴포넌트가 붙어 있음
	if (!IsValid(PC) || !PC->IsLocalController())
	{
		return;
	}

	if (!RosterWindowWidgetClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("RosterWindowWidgetClass가 설정되지 않았습니다! PlayerState BP의 NaiaRosterComponent를 확인하세요"));
		return;
	}

	UNUUIManager* UI = UNUUIManager::Get(PC);

	if (IsValid(RosterWindowWidget) && RosterWindowWidget->IsInViewport())
	{
		if (UI)
		{
			UI->CloseWindow(RosterWindowWidget);
		}
		else
		{
			RosterWindowWidget->RemoveFromParent();
		}
		return;
	}

	if (!IsValid(RosterWindowWidget))
	{
		RosterWindowWidget = CreateWidget<UNUSheetWidgetBase>(PC, RosterWindowWidgetClass);
		if (!IsValid(RosterWindowWidget))
		{
			return;
		}
	}

	if (UI)
	{
		FNUUIWindowParams Params;
		Params.bCloseOnSessionEnd = true;
		UI->OpenWindow(RosterWindowWidget, Params);
	}
	else
	{
		RosterWindowWidget->AddToViewport(NUUILayer::Window);
	}
}

void UNUNaiaRosterComponent::ServerMoveNaia_Implementation(int32 FromContainer, int32 FromSlot, int32 ToContainer,
	int32 ToSlot)
{
	// gRPC 서버 로스터를 한 번이라도 받았으면 결과(SVR_Naia_Updated)를 기다림
	// 아직 못 받았으면 기존 로컬 처리로 그대로 둠
	if (IsLocalPlayerRoster())
	{
		UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem();
		if (IsValid(NaiaNet) && NaiaNet->HasReceivedRoster())
		{
			// 서버가 어차피 거절하므로 로컬에서 먼저 걸러둠
			if (IsInBattle())
			{
				UE_LOG(LogTemp, Warning, TEXT("전투 중에는 나이아 편성을 바꿀 수 없습니다!"));
				return;
			}

			if (FromContainer == NUNaiaContainer::Party
				&& ToContainer == NUNaiaContainer::Storage
				&& NaiaNet->GetPartySlots().Num() <= 1)
			{
				UE_LOG(LogTemp, Warning, TEXT("파티의 마지막 나이아는 보관함으로 내릴 수 없습니다!"));
				return;
			}

			NaiaNet->RequestNaiaMove(FromContainer, FromSlot, ToContainer, ToSlot);
			return;
		}
	}

	MoveNaia(FromContainer, FromSlot, ToContainer, ToSlot);
}

void UNUNaiaRosterComponent::ServerDebugAcquireNaia_Implementation(int32 NaiaId, int32 Level)
{
#if !UE_BUILD_SHIPPING
	AcquireNaia(NaiaId, Level);
#endif	
}

void UNUNaiaRosterComponent::ServerSelectStarterNaia_Implementation(int32 NaiaId)
{
	FString Error;
	if (!CanSelectStarterNaia(NaiaId, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("스타팅 나이아 계약 거부: NaiaId=%d 이유=%s"), NaiaId, *Error);
		ClientStarterNaiaSelectionRejected(Error);
		return;
	}

	if (!bStarterNaiaSelectionAuthorized)
	{
		Error = TEXT("스타팅 나이아 선택 단계가 허용되지 않음");
		UE_LOG(LogTemp, Warning, TEXT("스타팅 나이아 계약 거부: NaiaId=%d 이유=%s"), NaiaId, *Error);
		ClientStarterNaiaSelectionRejected(Error);
		return;
	}

	if (IsLocalPlayerRoster())
	{
		UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem();
		if (IsValid(NaiaNet) && NaiaNet->HasActiveServerSession())
		{
			FText RequestError;
			if (NaiaNet->RequestStarterNaiaContract(NaiaId, RequestError))
			{
				UE_LOG(LogTemp, Log, TEXT("스타팅 나이아 계약 서버 응답 대기: NaiaId=%d"), NaiaId);
				OnRosterChanged.Broadcast();
				return;
			}

			Error = RequestError.ToString();
			UE_LOG(LogTemp, Warning, TEXT("스타팅 나이아 계약 요청 실패: NaiaId=%d 이유=%s"), NaiaId, *Error);
			ClientStarterNaiaSelectionRejected(Error);
			return;
		}
	}

	// 서버 세션이 없는 에디터/오프라인 테스트만 기존 로컬 계약 경로를 사용함
	if (SelectStarterNaia(NaiaId, Error))
	{
		UE_LOG(LogTemp, Log, TEXT("스타팅 나이아 로컬 계약 완료: NaiaId=%d InstanceId=%lld"), NaiaId, StarterNaiaInstanceId);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("스타팅 나이아 로컬 계약 거부: NaiaId=%d 이유=%s"), NaiaId, *Error);
	ClientStarterNaiaSelectionRejected(Error);
}

void UNUNaiaRosterComponent::ClientStarterNaiaSelectionRejected_Implementation(const FString& Reason)
{
	const ANUPlayerState* PlayerState = GetOwningPlayerState();
	APlayerController* PlayerController = PlayerState != nullptr ? Cast<APlayerController>(PlayerState->GetOwner()) : nullptr;
	if (UNUUIManager* UI = UNUUIManager::Get(PlayerController))
	{
		UI->ShowNotice(FText::FromString(Reason));
	}
}

void UNUNaiaRosterComponent::HandleStorageReplicated()
{
	OnRosterChanged.Broadcast();
}

void UNUNaiaRosterComponent::OnRep_PartyNaias()
{
	OnRosterChanged.Broadcast();
}

void UNUNaiaRosterComponent::OnRep_StarterNaiaSelection()
{
	OnRosterChanged.Broadcast();
}

void UNUNaiaRosterComponent::SyncActiveNaia()
{
	ANUPlayerState* PS = GetOwningPlayerState();
	if (!IsValid(PS))
	{
		return;
	}
	
	ANUSpiritState* NewActive = GetActiveNaia();
	ANUSpiritState* Previous = PS->GetActiveSpiritState();
	if (Previous == NewActive)
	{
		return;
	}
	
	// 이전 활성 나이아의 필드 아바타 치우기,
	if (IsValid(Previous))
	{
		Previous->DestroyFieldAvatar();
	}
	
	// 새로운 정령으로 변경
	PS->SetActiveSpiritState(NewActive);
}

bool UNUNaiaRosterComponent::TakeFromParty(int32 Slot, FNUNaiaInstance& OutEntry)
{
	if (!PartyNaias.IsValidIndex(Slot))
	{
		return false;
	}
	
	ANUSpiritState* Naia = PartyNaias[Slot].Get();
	if (!IsValid(Naia))
	{
		return false;
	}
	
	OutEntry = MakeSnapshot(Naia);
	
	// 필드 아바타 먼저 치우기
	Naia->DestroyFieldAvatar();
	Naia->Destroy();
	
	PartyNaias.RemoveAt(Slot);
	return true;
}

bool UNUNaiaRosterComponent::TakeFromStorage(int32 Slot, FNUNaiaInstance& OutEntry)
{
	int32 Index = StorageList.Naias.IndexOfByPredicate(
		[Slot](const FNUNaiaInstance& Entry)
		{
			return Entry.SlotNo == Slot;
		}
		);
	
	if (Index == INDEX_NONE)
	{
		return false;
	}
	
	OutEntry = StorageList.Naias[Index];
	StorageList.Naias.RemoveAt(Index);
	StorageList.MarkArrayDirty();
	return true;
}

ANUSpiritState* UNUNaiaRosterComponent::PutToParty(const FNUNaiaInstance& Entry, int32 Slot)
{
	if (IsPartyFull())
	{
		UE_LOG(LogTemp, Warning, TEXT("파티에 빈 자리가 없어 나이아를 넣을 수 없습니다 : NaiaId=%d"), Entry.NaiaId);
		return nullptr;
	}

	const TSubclassOf<ANUSpiritState> ResolvedNaiaStateClass = ResolveNaiaStateClass();
	if (!ResolvedNaiaStateClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("NaiaStateClass가 설정되지 않았습니다! NU Naia Data 또는 PlayerState의 NaiaRosterComponent를 확인하세요"));
		return nullptr;
	}

	ANUPlayerState* PS = GetOwningPlayerState();
	if (!IsValid(PS))
	{
		UE_LOG(LogTemp, Error, TEXT("로스터의 소유자가 PlayerState가 아닙니다 : %s"), *GetNameSafe(GetOwner()));
		return nullptr;
	}
	
	FActorSpawnParameters Params;
	Params.Owner = PS->GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	ANUSpiritState* Naia = GetWorld()->SpawnActor<ANUSpiritState>(
		ResolvedNaiaStateClass,
		FTransform::Identity,
		Params
		);
	
	if (!IsValid(Naia))
	{
		UE_LOG(LogTemp, Error, TEXT("나이아 스폰 실패 : NaiaId = %d"), Entry.NaiaId);
		return nullptr;
	}
	
	Naia->InitializeSpirit(PS, Entry);
	
	int32 InsertAt = FMath::Clamp(Slot, 0, PartyNaias.Num());
	PartyNaias.Insert(Naia, InsertAt);
	return Naia;
}

bool UNUNaiaRosterComponent::PutToStorage(FNUNaiaInstance Entry, int32 Slot)
{
	if (Slot < 0 || Slot >= MaxStorageSlots)
	{
		return false;
	}
	if (FindMutableStorageBySlot(Slot))
	{
		// 이미 차있는 칸이라면 false
		return false;
	}
	
	Entry.SlotNo = Slot;
	StorageList.Naias.Add(Entry);
	StorageList.MarkArrayDirty();
	return true;
}

bool UNUNaiaRosterComponent::SwapPartySlots(int32 SlotA, int32 SlotB)
{
	if (!PartyNaias.IsValidIndex(SlotA) || !PartyNaias.IsValidIndex(SlotB))
	{
		return false;
	}
	if (SlotA == SlotB)
	{
		return false;
	}
	
	// 위치만 변경
	PartyNaias.Swap(SlotA, SlotB);
	return true;
}

bool UNUNaiaRosterComponent::SwapStorageSlots(int32 SlotA, int32 SlotB)
{
	if (SlotA < 0 || SlotB < 0 || SlotA >= MaxStorageSlots || SlotB >= MaxStorageSlots)
	{
		return false;
	}
	if (SlotA == SlotB)
	{
		return false;
	}
	
	FNUNaiaInstance* From = FindMutableStorageBySlot(SlotA);
	if (!From)
	{
		return false;
	}
	
	if (FNUNaiaInstance* To = FindMutableStorageBySlot(SlotB))
	{
		To->SlotNo = SlotA;
		StorageList.MarkItemDirty(*To);
	}
	From->SlotNo = SlotB;
	StorageList.MarkItemDirty(*From);
	return true;
}

FNUNaiaInstance UNUNaiaRosterComponent::MakeSnapshot(const ANUSpiritState* Naia) const
{
	FNUNaiaInstance Entry;
	if (!IsValid(Naia))
	{
		UE_LOG(LogTemp, Error, TEXT("나이아 스냅샷 대상이 유효하지 않습니다, 빈 항목이 보관함에 들어갑니다"));
		return Entry;
	}

	Entry.InstanceId = Naia->GetInstanceId();
	Entry.NaiaId = Naia->GetSpiritId();
	Entry.Level = Naia->GetCurrentLevel();
	
	// 경험치·애정도도 반드시 되담아야 함, 스킬을 아래에서 담는 것과 같은 이유
	Entry.Experience = Naia->GetExperience();
	Entry.Affection = Naia->GetAffection();

	const UNUAbilitySystemComponent* ASC = Naia->GetNUASC();

	// 배운 패시브, 액티브 스킬을 반드시 인스턴스에 저장해야 함
	// 빠뜨리면 보관함에 넣었다 꺼낼 때 클라이언트 쪽에서 스킬이 사라짐
	// 체력을 못 읽는 경우에도 스킬은 남아야 하므로 아래 검사보다 먼저 처리함
	// 현재 패시브는 획득한 이후로 고정임, ASC 목록에서 첫 칸을 꺼내지 않고 여기서 바로 읽음
	Entry.PassiveSkillId = Naia->GetPassiveSkillId();
	
	if (IsValid(ASC))
	{
		Entry.ActiveSkillIds = ASC->GetActiveSkillIds();
	}

	const UNUVitalAttributeSet* Vital = IsValid(ASC) ? ASC->GetVitalAttributeSet() : nullptr;
	if (!IsValid(Vital))
	{
		UE_LOG(LogTemp, Error,
			TEXT("나이아 체력을 읽지 못했습니다, 보관함에 사망으로 기록됩니다 : NaiaId=%d, Lv=%d, %s"),
			Entry.NaiaId, Entry.Level, IsValid(ASC) ? TEXT("VitalAttributeSet 없음") : TEXT("ASC 없음"));
		
		Entry.HealthRatio = 0.f;
		return Entry;
	}

	// 절대값이 아니라 비율로 남김, 꺼낼 때 최대체력이 달라져 있어도 비율이 따라감
	const float MaxHealth = Vital->GetMaxHealth();
	Entry.HealthRatio = MaxHealth > KINDA_SMALL_NUMBER
		? FMath::Clamp(Vital->GetHealth() / MaxHealth, 0.f, 1.f)
		: 0.f;
	
	// 현재는 전투 종료시 죽은 나이아 전부 체력 1로 부활함
	// 누군가 0이라면 오류가 있는 것
	if (FMath::IsNearlyZero(Entry.HealthRatio))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("죽은 나이아가 보관함에 들어갑니다, 전투 종료 부활이 누락된 경로가 있는지 확인하세요 : NaiaId=%d, Lv=%d"),
			Entry.NaiaId, Entry.Level);
	}
	return Entry;
}

FNUNaiaInstance UNUNaiaRosterComponent::MakeNewEntry(int32 NaiaId, int32 Level) const
{
	FNUNaiaInstance Entry;
	Entry.InstanceId = NextLocalInstanceId--;
	Entry.NaiaId = NaiaId;
	
	const FNUNaiaDefinition* Definition = FindDefinition(NaiaId);
	
	// Level이 0이면 Definition 에 있는 SpawnLevel을 따라감
	Entry.Level = Level > 0
		? Level
		: (Definition ? FMath::Max(1, Definition->SpawnLevel) : 1);
	
	// 새로 얻은 개체는 만피, 실제 숫자는 최대치가 집계되는 InitializeSpirit 에서 나옴
	Entry.HealthRatio = 1.f;
	
	if (Definition)
	{
		GiveDefaultActiveSkills(Definition->ElementTag, Entry.ActiveSkillIds);
		
		// 나이아를 획득하고 Entry를 만들 때 패시브를 여기서 한 번만 확정함
		Entry.PassiveSkillId = RollPassiveSkill(*Definition);
	}

	return Entry;
}

TSubclassOf<ANUSpiritState> UNUNaiaRosterComponent::ResolveNaiaStateClass() const
{
	if (NaiaStateClass)
	{
		return NaiaStateClass;
	}

	const UNUNaiaDataSettings* Settings = GetDefault<UNUNaiaDataSettings>();
	return Settings != nullptr ? Settings->NaiaStateClass.LoadSynchronous() : nullptr;
}

int32 UNUNaiaRosterComponent::FindFreeStorageSlot() const
{
	for (int32 Slot = 0; Slot < MaxStorageSlots; ++Slot)
	{
		bool bUsed = StorageList.Naias.ContainsByPredicate(
			[Slot](const FNUNaiaInstance& Entry)
			{
				return Entry.SlotNo == Slot;
			}
			);

		if (!bUsed)
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

FNUNaiaInstance* UNUNaiaRosterComponent::FindMutableStorageBySlot(int32 SlotNo)
{
	return StorageList.Naias.FindByPredicate(
		[SlotNo](const FNUNaiaInstance& Entry)
		{
			return Entry.SlotNo == SlotNo;
		}
	);
}

ANUPlayerState* UNUNaiaRosterComponent::GetOwningPlayerState() const
{
	return Cast<ANUPlayerState>(GetOwner());
}

const FNUNaiaDefinition* UNUNaiaRosterComponent::FindDefinition(int32 NaiaId) const
{
	const UGameInstance* GameInstance = IsValid(GetOwner()) ? GetOwner()->GetGameInstance() : nullptr;
	const UNUNaiaDataSubsystem* Subsystem = 
		GameInstance ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	
	return IsValid(Subsystem) ? Subsystem->FindDefinitionById(NaiaId) : nullptr;
}

bool UNUNaiaRosterComponent::IsInBattle() const
{
	const ANUPlayerState* PS = GetOwningPlayerState();
	const UNUAbilitySystemComponent* ASC = IsValid(PS) ? PS->GetNUASC() : nullptr;

	return IsValid(ASC) && IsValid(ASC->GetBattleEncounter());
}

bool UNUNaiaRosterComponent::IsLocalPlayerRoster() const
{
	const ANUPlayerState* PS = GetOwningPlayerState();
	const APlayerController* PC = IsValid(PS) ? Cast<APlayerController>(PS->GetOwner()) : nullptr;

	// 로컬 클라 자신의 것만 서버 로스터를 반영해야 함
	return IsValid(PC) && PC->IsLocalController();
}

UNUNaiaNetSubsystem* UNUNaiaRosterComponent::GetNaiaNetSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;

	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUNaiaNetSubsystem>() : nullptr;
}

void UNUNaiaRosterComponent::HandleServerRosterChanged()
{
	const UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem();
	if (!IsValid(NaiaNet))
	{
		return;
	}
	const bool bHadStarterContract = HasSelectedStarterNaia();

	TArray<FNUServerNaiaSlot> AllSlots;
	AllSlots.Append(NaiaNet->GetPartySlots());
	AllSlots.Append(NaiaNet->GetStorageSlots());

	ApplyServerNaiaSlots(AllSlots);

	SelectedStarterNaiaId = NaiaNet->GetStarterNaiaId();
	StarterNaiaInstanceId = NaiaNet->GetStarterNaiaInstanceId();
	if (HasSelectedStarterNaia())
	{
		bStarterNaiaSelectionAuthorized = false;
		if (!bHadStarterContract)
		{
			if (ANUPlayerState* PlayerState = GetOwningPlayerState(); IsValid(PlayerState) && IsValid(GetActiveNaia()))
			{
				PlayerState->SetNaiaSummoned(true);
			}
		}
	}

	OnRosterChanged.Broadcast();
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
}

void UNUNaiaRosterComponent::HandleMoveRejected(FText Reason)
{
	const ANUPlayerState* PS = GetOwningPlayerState();
	APlayerController* PC = IsValid(PS) ? Cast<APlayerController>(PS->GetOwner()) : nullptr;

	if (UNUUIManager* UI = UNUUIManager::Get(PC))
	{
		UI->ShowNotice(Reason);
	}
}

void UNUNaiaRosterComponent::HandleStarterNaiaContractRejected(FText Reason)
{
	HandleMoveRejected(Reason);
	OnRosterChanged.Broadcast();
}
