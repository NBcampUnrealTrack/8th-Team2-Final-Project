// Fill out your copyright notice in the Description page of Project Settings.

#include "Battle/Presentation/NUBattleVFXPreloadSubsystem.h"

#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUSkillVFXProfile.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUPlayerSkillComponent.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUVFXPreload, Log, All);

namespace
{
	// 보유 목록을 다시 보는 간격(초), 목록이 같으면 비교만 하고 끝남
	constexpr float OwnedSkillCheckInterval = 1.f;

	// 예열 컴포넌트를 띄워 두는 프레임 수, 첫 틱 · 렌더 등록이 끝날 만큼
	constexpr int32 WarmUpFrames = 3;

	// 예열 위치, 어느 맵에서도 카메라에 안 잡히는 아래쪽 (아레나는 Z -10000)
	const FVector WarmUpLocation(0.f, 0.f, -200000.f);

	TAutoConsoleVariable<bool> CVarVFXPreloadWarmUp(
		TEXT("nu.VFX.PreloadWarmUp"),
		true,
		TEXT("미리 불러 둔 전투 VFX 를 화면 밖에 한 번 띄워 첫 사용 끊김을 미리 치름"));

	void AddPath(const TSoftObjectPtr<UNiagaraSystem>& Asset, TArray<FSoftObjectPath>& OutPaths)
	{
		if (!Asset.IsNull())
		{
			OutPaths.AddUnique(Asset.ToSoftObjectPath());
		}
	}
}

bool UNUBattleVFXPreloadSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 전용 서버는 이펙트를 그리지 않음
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UNUBattleVFXPreloadSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UNUBattleDataSubsystem>();

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickOwnedSkills),
		OwnedSkillCheckInterval);

	WarmUpTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickWarmUp));

	// 스킬과 상관없는 공용 전투 VFX 는 바로 부름
	TArray<FSoftObjectPath> CommonPaths;
	for (const TSoftObjectPtr<UNiagaraSystem>& Common : GetDefault<UNUBattleDataSettings>()->CommonBattleVFX)
	{
		AddPath(Common, CommonPaths);
	}
	PreloadSystems(CommonPaths);
}

void UNUBattleVFXPreloadSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
	FTSTicker::GetCoreTicker().RemoveTicker(WarmUpTickerHandle);
	WarmUpTickerHandle.Reset();

	for (const TPair<TWeakObjectPtr<UNiagaraComponent>, int32>& Entry : WarmUpComponents)
	{
		if (UNiagaraComponent* Component = Entry.Key.Get())
		{
			Component->DestroyComponent();
		}
	}

	WarmUpComponents.Reset();
	WarmUpQueue.Reset();

	for (const TSharedPtr<FStreamableHandle>& Handle : Handles)
	{
		if (Handle.IsValid())
		{
			Handle->CancelHandle();
		}
	}

	Handles.Reset();
	Held.Reset();
	Requested.Reset();

	Super::Deinitialize();
}

void UNUBattleVFXPreloadSubsystem::CollectSkillVFX(const UNUBattleDataSubsystem* Data, const TArray<int32>& SkillIds,
	int32 JobId, TArray<FSoftObjectPath>& OutPaths)
{
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();

	// 스킬 표 행이 없는 행동이 쓰는 기본 연출 (검기 등), 넷 전투와 같은 기본 행동 어빌리티
	if (const UClass* DefaultAbilityClass = Settings->NetDefaultActionAbility.LoadSynchronous())
	{
		if (const UNUBattleGameplayAbility* DefaultAbility = DefaultAbilityClass->GetDefaultObject<UNUBattleGameplayAbility>())
		{
			FNUBattleActionMotionSpec Defaults;
			DefaultAbility->ApplyPresentationDefaults(Defaults);
			AddPath(Defaults.CastVFX, OutPaths);
			AddPath(Defaults.ProjectileVFX, OutPaths);
			AddPath(Defaults.ImpactVFX, OutPaths);
		}
	}

	// 자기 임팩트가 없는 공격의 대체 임팩트 (NUBattlePresentation::ApplyFallbackImpact)
	AddPath(Settings->FallbackImpactVFX, OutPaths);
	AddPath(Settings->FallbackProjectileVFX, OutPaths);

	// 나이아 기본 공격 연출 (ANUBattleAvatar::ResolveActionVFX), 파티 · 적 나이아의 속성을 여기서 모르므로 속성마다 다 부름
	for (const TPair<FGameplayTag, FNUJobAttackVFX>& Pair : Settings->NaiaElementAttackVFX)
	{
		AddPath(Pair.Value.CastVFX, OutPaths);
		AddPath(Pair.Value.ProjectileVFX, OutPaths);
		AddPath(Pair.Value.ImpactVFX, OutPaths);
	}

	// 직업 기본 공격 연출 (ANUBattlePlayerAvatar::ResolveActionVFX 가 바꿔 넣는 것), 노비스는 0
	if (JobId >= 0)
	{
		if (const FNUJobAttackVFX* JobVFX = Settings->JobAttackVFX.Find(JobId))
		{
			AddPath(JobVFX->CastVFX, OutPaths);
			AddPath(JobVFX->ProjectileVFX, OutPaths);
			AddPath(JobVFX->ImpactVFX, OutPaths);
		}
	}

	if (!IsValid(Data))
	{
		return;
	}

	for (const int32 SkillId : SkillIds)
	{
		if (const FNUSkillVFXProfile* Profile = Data->FindSkillVFXProfile(SkillId))
		{
			AddPath(Profile->CastVFX, OutPaths);
			AddPath(Profile->ProjectileVFX, OutPaths);
			AddPath(Profile->ImpactVFX, OutPaths);
		}
	}
}

void UNUBattleVFXPreloadSubsystem::PreloadSkills(const TArray<int32>& SkillIds, int32 JobId)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	TArray<FSoftObjectPath> Paths;
	CollectSkillVFX(Data, SkillIds, JobId, Paths);
	PreloadSystems(Paths);
}

void UNUBattleVFXPreloadSubsystem::PreloadSystems(const TArray<FSoftObjectPath>& Paths)
{
	TArray<FSoftObjectPath> ToLoad;
	int32 AlreadyLoaded = 0;

	for (const FSoftObjectPath& Path : Paths)
	{
		if (!Path.IsValid() || Requested.Contains(Path))
		{
			continue;
		}

		Requested.Add(Path);

		// 이미 메모리에 있으면 다시 부르지 않고 붙잡기만 함 (한 번도 안 띄웠을 수 있어 예열은 함)
		if (UNiagaraSystem* Loaded = Cast<UNiagaraSystem>(Path.ResolveObject()))
		{
			Held.Add(Loaded);
			WarmUpQueue.Add(Loaded);
			++AlreadyLoaded;
			continue;
		}

		ToLoad.Add(Path);
	}

	if (AlreadyLoaded > 0)
	{
		UE_LOG(LogNUVFXPreload, Log, TEXT("이미 메모리에 있는 전투 VFX %d개를 붙잡음"), AlreadyLoaded);
	}

	if (ToLoad.IsEmpty())
	{
		return;
	}

	UE_LOG(LogNUVFXPreload, Log, TEXT("전투 VFX %d개 비동기 로드 시작"), ToLoad.Num());

	// 다른 로드를 앞지르지 않게 기본 우선순위로 뒤에서 부름
	TSharedPtr<FStreamableHandle> Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		ToLoad,
		FStreamableDelegate::CreateUObject(this, &ThisClass::HandleBatchLoaded, ToLoad),
		FStreamableManager::DefaultAsyncLoadPriority);

	if (Handle.IsValid() && !Handle->HasLoadCompleted())
	{
		Handles.Add(Handle);
	}
}

void UNUBattleVFXPreloadSubsystem::AdoptLoaded(const TArray<UNiagaraSystem*>& Systems)
{
	for (UNiagaraSystem* System : Systems)
	{
		if (!IsValid(System))
		{
			continue;
		}

		const FSoftObjectPath Path(System);
		if (Requested.Contains(Path))
		{
			continue;
		}

		Requested.Add(Path);
		Held.Add(System);
		WarmUpQueue.Add(System);
	}
}

void UNUBattleVFXPreloadSubsystem::HandleBatchLoaded(TArray<FSoftObjectPath> Paths)
{
	int32 Loaded = 0;
	for (const FSoftObjectPath& Path : Paths)
	{
		if (UNiagaraSystem* System = Cast<UNiagaraSystem>(Path.ResolveObject()))
		{
			Held.Add(System);
			WarmUpQueue.Add(System);
			++Loaded;
		}
		else
		{
			UE_LOG(LogNUVFXPreload, Warning, TEXT("전투 VFX 를 불러오지 못함: %s"), *Path.ToString());
		}
	}

	// 끝난 요청은 버림, 에셋은 Held 가 붙잡고 있음
	Handles.RemoveAll([](const TSharedPtr<FStreamableHandle>& Handle)
	{
		return !Handle.IsValid() || Handle->HasLoadCompleted() || Handle->WasCanceled();
	});

	UE_LOG(LogNUVFXPreload, Log, TEXT("전투 VFX %d개 비동기 로드 끝 (들고 있는 것 %d개)"), Loaded, Held.Num());
}

bool UNUBattleVFXPreloadSubsystem::TickWarmUp(float DeltaTime)
{
	// 다 띄운 것은 지움
	for (int32 Index = WarmUpComponents.Num() - 1; Index >= 0; --Index)
	{
		TPair<TWeakObjectPtr<UNiagaraComponent>, int32>& Entry = WarmUpComponents[Index];
		UNiagaraComponent* Component = Entry.Key.Get();
		if (!Component || --Entry.Value <= 0)
		{
			if (Component)
			{
				Component->DestroyComponent();
			}
			WarmUpComponents.RemoveAtSwap(Index);
		}
	}

	if (WarmUpQueue.IsEmpty())
	{
		return true;
	}

	if (!CVarVFXPreloadWarmUp.GetValueOnGameThread())
	{
		WarmUpQueue.Reset();
		return true;
	}

	// 게임 월드가 없으면(로딩 · 메뉴 전환 중) 다음 틱에
	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = IsValid(GameInstance) ? GameInstance->GetWorld() : nullptr;
	if (!IsValid(World) || !World->IsGameWorld())
	{
		return true;
	}

	// 한 프레임에 하나
	UNiagaraSystem* System = WarmUpQueue[0].Get();
	WarmUpQueue.RemoveAt(0);
	if (!System)
	{
		return true;
	}

	UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, System, WarmUpLocation, FRotator::ZeroRotator, FVector::OneVector,
		false,	// bAutoDestroy, 직접 지움
		true,	// bAutoActivate
		ENCPoolMethod::None,
		false	// bPreCullCheck, 거리 컬링 검사를 하면 화면 밖이라 안 띄워짐
		);

	if (Component)
	{
		WarmUpComponents.Emplace(Component, WarmUpFrames);
	}

	return true;
}

bool UNUBattleVFXPreloadSubsystem::TickOwnedSkills(float DeltaTime)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const APlayerController* PC = IsValid(GameInstance) ? GameInstance->GetFirstLocalPlayerController() : nullptr;
	const ANUPlayerState* PlayerState = IsValid(PC) ? PC->GetPlayerState<ANUPlayerState>() : nullptr;
	if (!IsValid(PlayerState))
	{
		return true;
	}

	// 직업 스킬
	TArray<int32> SkillIds;
	if (const UNUPlayerSkillComponent* Skills = PlayerState->GetPlayerSkillComponent())
	{
		SkillIds = Skills->GetOwnedActiveSkillIds();
	}

	// 파티 나이아 스킬, 전투에 데려가는 것은 파티뿐
	if (const UNUNaiaRosterComponent* Roster = PlayerState->GetNaiaRosterComponent())
	{
		for (const ANUSpiritState* Naia : Roster->GetParty())
		{
			if (IsValid(Naia))
			{
				for (const int32 SkillId : Naia->GetActiveSkillIds())
				{
					SkillIds.AddUnique(SkillId);
				}
			}
		}
	}

	const UNUJobComponent* Job = PlayerState->GetJobComponent();
	// 직업이 없으면 노비스(0) - 전투 아바타의 외형 직업 번호와 같음
	const int32 JobId = IsValid(Job) && Job->HasJob() ? Job->GetJobId() : 0;

	SkillIds.Sort();
	if (SkillIds == LastSkillIds && JobId == LastJobId)
	{
		return true;
	}

	LastSkillIds = SkillIds;
	LastJobId = JobId;

	// 이미 요청한 것은 PreloadSystems 가 건너뜀, 새로 생긴 스킬의 것만 부름
	PreloadSkills(SkillIds, JobId);
	return true;
}
