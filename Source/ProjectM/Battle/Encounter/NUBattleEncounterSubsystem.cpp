#include "Battle/Encounter/NUBattleEncounterSubsystem.h"

#include "Battle/Encounter/NUBattleEncounter.h"
#include "Battle/Formation/NUBattleFormationAxis.h"
#include "Battle/Formation/NUBattleFormationSlot.h"
#include "Battle/NUBattleTypes.h"
#include "Camera/CameraActor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/LightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "EngineUtils.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Core/NUGameMode.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace NUArena
{
	// GameMode에 ArenaCenter가 없을 때 쓰는 값
	static const FVector DefaultCenter(0.f, 0.f, -10000.f);
}

namespace NUDebugBattleStage
{
	// 월드에 하나만 존재할 수 있는 액터를 세트에 넣었으면 알림
	// 막지는 않음, 필드 것과 충돌하거나 조용히 무시되므로 원인을 찾기 어려워서 로그로 남김
	// 세트 하나를 올릴 때 한 번만 도는 자리라 순회 비용은 문제되지 않음
	void WarnOnStageWorldSingletons(UWorld* World, const ULevel* StageLevel)
	{
		// 렌더러가 ExponentialFogs[0] 만 읽음, 필드 안개를 끄고 세트 것을 올리는 구조라
		// 세트에 둘 이상 있으면 먼저 등록된 쪽이 적용됨
		int32 StageFogCount = 0;
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			if (It->GetLevel() == StageLevel)
			{
				++StageFogCount;
			}
		}

		if (StageFogCount > 1)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("전투 배경의 ExponentialHeightFog가 %d개입니다. 하나만 적용되고 나머지는 무시됩니다"),
				StageFogCount
				);
		}

		for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
		{
			if (It->GetLevel() != StageLevel)
			{
				continue;
			}

			UE_LOG(LogTemp, Warning,
				TEXT("전투 배경의 SkyAtmosphere 월드에 하나만 있을 수 있어 필드와 충돌합니다. 스카이박스 메시를 사용하세요. Actor=%s"),
				*It->GetName()
				);
		}

		// 범위가 없는 후처리는 카메라가 어디 있든 걸려서 전투가 끝나도 필드에 남음
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			if (It->GetLevel() != StageLevel || !It->bUnbound)
			{
				continue;
			}

			UE_LOG(LogTemp, Warning,
				TEXT("전투 배경의 후처리가 Infinite Extent 라 전투가 끝난 뒤에도 필드에 남습니다. 체크를 끄고 볼륨이 전투 카메라를 감싸게 하세요. Actor=%s"),
				*It->GetName()
				);
		}
	}
}


ENUBattleEncounterStartResult UNUBattleEncounterSubsystem::TryStartEncounter(const FNUBattleEncounterRequest& Request, ANUBattleEncounter*& OutEncounter)
{
	OutEncounter = nullptr;

	ENUBattleEncounterStartResult ValidationResult = ENUBattleEncounterStartResult::InvalidRequest;
	if (!ValidateEncounterRequest(Request, ValidationResult))
	{
		return ValidationResult;
	}

	TArray<FNUBattleParticipant> Participants;
	if (!BuildParticipantsFromRequest(Request, Participants))
	{
		return ENUBattleEncounterStartResult::NoValidParticipants;
	}

	TSubclassOf<ANUBattleEncounter> EncounterClass = ResolveEncounterClass(Request);
	if (!EncounterClass)
	{
		return ENUBattleEncounterStartResult::EncounterClassMissing;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return ENUBattleEncounterStartResult::SpawnFailed;
	}

	FActorSpawnParameters SpawnParams;

	// Encounter의 네트워크 Owner는 인카운터 요청자의 Controller가 된다.
	if (ANUPlayerState* Initiator = Request.Initiator.Get())
	{
		SpawnParams.Owner = Cast<AActor>(Initiator->GetOwner());
	}

	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 필드가 아니라 아레나에서 전투
	FTransform EncounterTransform(FRotator::ZeroRotator, GetArenaLocation());

	ANUBattleEncounter* Encounter = World->SpawnActor<ANUBattleEncounter>(
			EncounterClass,
			EncounterTransform,
			SpawnParams
		);

	if (!IsValid(Encounter))
	{
		UE_LOG(LogTemp, Warning, TEXT("BattleEncounterSubsystem: Encounter Spawn 실패."));

		return ENUBattleEncounterStartResult::SpawnFailed;
	}

	// 배경 참조를 먼저 넘김, InitializeEncounter 가 암전과 함께 로딩을 시작함
	Encounter->SetBattleStageLevel(Request.BattleStageLevel);
	if (!Encounter->ConfigureBattleDefinition(Request.BattleDefinition))
	{
		Encounter->Destroy();
		return ENUBattleEncounterStartResult::InitializeFailed;
	}

	// 기존 배틀 시스템의 초기화 함수를 그대로 사용한다.
	if (!Encounter->InitializeEncounter(Participants))
	{
		UE_LOG(LogTemp, Error, TEXT("BattleEncounterSubsystem: Encounter 초기화 실패."));
		Encounter->Destroy();

		return ENUBattleEncounterStartResult::InitializeFailed;
	}

	UE_LOG(LogTemp, Log, TEXT("BattleEncounterSubsystem: 배틀 시작."));

	OutEncounter = Encounter;
	return ENUBattleEncounterStartResult::Started;
}

/////////////////////////////////////////////////////////////////
// 아레나 배치
/////////////////////////////////////////////////////////////////

FVector UNUBattleEncounterSubsystem::GetArenaLocation() const
{
	// 아레나 위치를 변경할 수 있게 GameMode에서 읽기
	if (const ANUGameMode* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ANUGameMode>() : nullptr)
	{
		return GameMode->GetArenaCenter();
	}

	return NUArena::DefaultCenter;
}

/////////////////////////////////////////////////////////////////
// 배경 서브레벨
/////////////////////////////////////////////////////////////////

void UNUBattleEncounterSubsystem::EnsureStageLoaded(const TSoftObjectPtr<UWorld>& StageLevel)
{
	// 레벨을 지정하지 않은 전투는 배경을 건드리지 않음
	// 이전 세트는 그대로 둠
	if (StageLevel.IsNull())
	{
		return;
	}

	// 같은 레벨이면 다시 요청하지 않음
	if (IsValid(StageStreaming)
		&& LoadedStageLevel.ToSoftObjectPath() == StageLevel.ToSoftObjectPath())
	{
		return;
	}

	// 다른 레벨이 오면 이전 것을 내림
	if (IsValid(StageStreaming))
	{
		StageStreaming->SetIsRequestingUnloadAndRemoval(true);
	}

	StageStreaming = nullptr;
	LoadedStageLevel.Reset();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	bool bSuccess = false;
	StageStreaming = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
		World,
		StageLevel,
		FTransform(FRotator::ZeroRotator, GetArenaLocation()),
		bSuccess
		);

	// 배경이 없어도 전투는 돌아야 하므로 알리고 넘어감
	if (!bSuccess || !IsValid(StageStreaming))
	{
		StageStreaming = nullptr;

		UE_LOG(LogTemp, Error, TEXT("전투 배경 서브레벨을 올리지 못했습니다. Level=%s"),
			*StageLevel.ToSoftObjectPath().ToString()
			);
		return;
	}

	LoadedStageLevel = StageLevel;

	// 스트리밍 완료 델리게이트 바인딩
	StageStreaming->OnLevelShown.AddDynamic(this, &ThisClass::HandleStageLevelShown);

	UE_LOG(LogTemp, Log, TEXT("전투 배경 서브레벨 로딩 시작. Level=%s"),
		*StageLevel.ToSoftObjectPath().ToString()
		);
}

void UNUBattleEncounterSubsystem::HandleStageLevelShown()
{
	OnStageShown.Broadcast();
}

/////////////////////////////////////////////////////////////////
// 세트가 정한 진형과 카메라
/////////////////////////////////////////////////////////////////

void UNUBattleEncounterSubsystem::RebuildStageCacheIfNeeded() const
{
	ULevel* StageLevel = IsValid(StageStreaming) ? StageStreaming->GetLoadedLevel() : nullptr;

	// 같은 레벨이면 그대로 사용함
	if (CachedStageLevel.Get() == StageLevel)
	{
		return;
	}

	CachedStageLevel = StageLevel;
	CachedAxisByRow.Reset();
	CachedSlotsByRow.Reset();
	CachedStageCamera.Reset();
	CachedStageDirectionalLights.Reset();
	CachedStageSkyLights.Reset();
	CachedStageHeightFogs.Reset();

	UWorld* World = GetWorld();
	if (!IsValid(World) || !StageLevel)
	{
		return;
	}

	/* 진형 축 캐싱 */
	
	// 세트를 교체하는 동안 이전 레벨이 아직 언로드되지 않아 두 세트의 액터가 공존할 수 있음
	// 그래서 월드 순회 결과를 지금 올라온 레벨에 있는지 확인해야 함
	for (TActorIterator<ANUBattleFormationAxis> It(World); It; ++It)
	{
		if (It->GetLevel() != StageLevel)
		{
			continue;
		}

		// 한 열에 축이 둘 이상이면 배치 실수, 먼저 찾은 것을 쓰고 알림
		if (CachedAxisByRow.Contains(It->GetRow()))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("진형 축이 한 열에 둘 이상입니다. 열=%s, 먼저 찾은 축을 사용합니다"),
				*UEnum::GetValueAsString(It->GetRow())
				);
			continue;
		}

		CachedAxisByRow.Add(It->GetRow(), *It);
	}
	
	/* 진형 슬롯 캐싱 */

	for (TActorIterator<ANUBattleFormationSlot> It(World); It; ++It)
	{
		if (It->GetLevel() != StageLevel)
		{
			continue;
		}

		// 열로 나눠 담아둠, 전투원마다 전체를 다시 걸러내지 않기 위함
		CachedSlotsByRow.FindOrAdd(It->GetRow()).Add(*It);
	}

	// TActorIterator 순서는 보장되지 않으므로 열마다 자리 번호로 정렬해야 함
	// 빠뜨리면 실행마다 보스와 잡몹의 자리가 섞인다
	for (TPair<ENUBattleFormationRow, TArray<TWeakObjectPtr<ANUBattleFormationSlot>>>& RowSlots : CachedSlotsByRow)
	{
		RowSlots.Value.Sort([](const TWeakObjectPtr<ANUBattleFormationSlot>& A,
			const TWeakObjectPtr<ANUBattleFormationSlot>& B)
			{
				int32 IndexA = A.IsValid() ? A->GetSlotIndex() : MAX_int32;
				int32 IndexB = B.IsValid() ? B->GetSlotIndex() : MAX_int32;
				return IndexA < IndexB;
			});
	}
	
	/* 세트 카메라 캐싱 */

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (It->GetLevel() == StageLevel)
		{
			CachedStageCamera = *It;
			break;
		}
	}
	
	/* 세트 조명 캐싱 */

	// 세트가 들고 있는 전역 조명, 전투 중에만 켜기 위해 명단을 만들어 둠
	// 반경이 있는 Point, Spot, Rect 는 필드까지 닿지 않으므로 X
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (It->GetLevel() == StageLevel)
		{
			CachedStageDirectionalLights.Add(*It);
		}
	}

	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (It->GetLevel() == StageLevel)
		{
			CachedStageSkyLights.Add(*It);
		}
	}

	// 안개도 전역이라 같이 들고 있어야 함, 전투 중에만 켜기 위함
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (It->GetLevel() == StageLevel)
		{
			CachedStageHeightFogs.Add(*It);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("전투 배경 진형  축=%d개, 슬롯 열=%d개 카메라=%s DirectionalLight=%d개 SkyLight=%d개 안개=%d개"),
		CachedAxisByRow.Num(),
		CachedSlotsByRow.Num(),
		CachedStageCamera.IsValid() ? TEXT("있음") : TEXT("없음"),
		CachedStageDirectionalLights.Num(),
		CachedStageSkyLights.Num(),
		CachedStageHeightFogs.Num()
		);

	// 세트를 올릴 때 규칙을 어긴 액터가 있으면 여기서 알림
	NUDebugBattleStage::WarnOnStageWorldSingletons(World, StageLevel);
}

bool UNUBattleEncounterSubsystem::ResolveFormationTransform(ENUBattleFormationRow Row, int32 SlotIndex,
	int32 SlotCount, FTransform& OutTransform) const
{
	RebuildStageCacheIfNeeded();

	if (SlotIndex < 0)
	{
		return false;
	}

	// 배치한 슬롯 자리를 먼저 봄, 축보다 구체적인 지정이므로 우선함
	if (const TArray<TWeakObjectPtr<ANUBattleFormationSlot>>* RowSlots = CachedSlotsByRow.Find(Row))
	{
		// 배치한 자리가 모자라면 실패 처리
		// 일부만 자리에 배치하면 남은 인원이 기본 계산 자리에 겹침
		if (RowSlots->Num() < SlotCount)
		{
			UE_LOG(LogTemp, Error,
				TEXT("진형 자리 개수가 인원보다 적어 위치를 잡는데 실패했습니다. 열=%s 자리=%d 인원=%d"),
				*UEnum::GetValueAsString(Row),
				RowSlots->Num(),
				SlotCount
				);
			return false;
		}

		const ANUBattleFormationSlot* Slot =
			RowSlots->IsValidIndex(SlotIndex) ? (*RowSlots)[SlotIndex].Get() : nullptr;

		if (!IsValid(Slot))
		{
			return false;
		}

		OutTransform = Slot->GetActorTransform();
		return true;
	}

	// 배치한 슬롯이 없으면 축을 기준으로 가운데 정렬함
	if (const TWeakObjectPtr<ANUBattleFormationAxis>* Axis = CachedAxisByRow.Find(Row))
	{
		if (const ANUBattleFormationAxis* AxisActor = Axis->Get())
		{
			OutTransform = AxisActor->GetSlotTransform(SlotIndex, SlotCount);
			return true;
		}
	}

	return false;
}

AActor* UNUBattleEncounterSubsystem::GetStageViewTarget() const
{
	RebuildStageCacheIfNeeded();

	return CachedStageCamera.Get();
}

bool UNUBattleEncounterSubsystem::IsStageReady() const
{
	// 올린 세트가 없으면 즉시 준비 완료 반환
	if (!IsValid(StageStreaming))
	{
		return true;
	}

	return StageStreaming->IsLevelVisible();
}

/////////////////////////////////////////////////////////////////
// 세트 전용 조명
/////////////////////////////////////////////////////////////////

void UNUBattleEncounterSubsystem::ApplyStageLighting()
{
	// 이미 세트 조명을 적용중이라면 조기 종료
	if (bStageLightingApplied)
	{
		return;
	}

	RebuildStageCacheIfNeeded();

	// 세트가 안 올라와 있으면 맞바꿀 대상이 없음
	if (!CachedStageLevel.IsValid())
	{
		return;
	}

	bStageLightingApplied = true;

	// 안개는 세트가 들고 있든 아니든 항상 필드 것을 끔
	// 조명은 "세트에 없으면 필드 것을 쓴다"가 성립하지만 안개는 아님, 아레나가 지면보다
	// 10000 아래라 필드 안개의 높이 밀도가 포화돼 세트가 통째로 뿌옇게 덮이기 때문
	TurnOffFieldHeightFogs();

	// 세트가 DirectionalLight를 들고 있을 때만 필드 DirectionalLight를 끔
	// SkyLight 만 들고 있는 세트는 필드 조명 그대로 사용함
	if (!CachedStageDirectionalLights.IsEmpty())
	{
		TurnoffFieldDirectionalLights();
	}

	// 필드 SkyLight 는 건드리지 않음, 나중에 등록된 것이 적용되기 때문
	// 대신 세트 것을 전투 중에만 켜야 함, 세트 레벨은 전투가 끝나도 내려가지 않음
	SetStageGlobalVisualsVisible(true);

	UE_LOG(LogTemp, Log, TEXT("전투 배경 조명으로 교체함. 세트 DirectionalLight=%d개 SkyLight=%d개 안개=%d개, "
		"끈 필드 DirectionalLight=%d개 안개=%d개"),
		CachedStageDirectionalLights.Num(),
		CachedStageSkyLights.Num(),
		CachedStageHeightFogs.Num(),
		FieldDirectionalLights.Num(),
		FieldHeightFogs.Num()
		);
}

void UNUBattleEncounterSubsystem::RestoreFieldLighting()
{
	// 세트 조명이 적용중이 아니라면 조기 종료
	if (!bStageLightingApplied)
	{
		return;
	}

	bStageLightingApplied = false;

	// 세트 전역 조명과 안개 끄기
	// 안개는 반드시 필드 것을 다시 켜기 전에 꺼야 함, 렌더러가 먼저 등록된 안개만 읽으므로
	// 순서가 뒤집히면 세트 안개가 필드에 그대로 남음
	SetStageGlobalVisualsVisible(false);

	// 꺼뒀던 조명만 다시 켬
	for (const TWeakObjectPtr<ADirectionalLight>& FieldLight : FieldDirectionalLights)
	{
		if (ADirectionalLight* Light = FieldLight.Get())
		{
			Light->SetEnabled(true);
		}
	}

	FieldDirectionalLights.Reset();

	// 꺼뒀던 안개도 다시 켬
	for (const TWeakObjectPtr<AExponentialHeightFog>& FieldFog : FieldHeightFogs)
	{
		AExponentialHeightFog* Fog = FieldFog.Get();
		UExponentialHeightFogComponent* FogComponent = IsValid(Fog) ? Fog->GetComponent() : nullptr;

		if (IsValid(FogComponent))
		{
			FogComponent->SetVisibility(true);
		}
	}

	FieldHeightFogs.Reset();

	UE_LOG(LogTemp, Log, TEXT("필드 조명으로 되돌림"));
}

void UNUBattleEncounterSubsystem::TurnoffFieldDirectionalLights()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const ULevel* StageLevel = CachedStageLevel.Get();

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		// 세트가 들고 있는 조명은 건너뜀
		if (It->GetLevel() == StageLevel)
		{
			continue;
		}

		const ULightComponent* LightComponent = It->GetLightComponent();

		// 켜져있는 조명만 명단에 담음
		// 원래 꺼져있던것까지 담으면 기존 조명과 달라짐
		if (!IsValid(LightComponent) || !LightComponent->IsVisible())
		{
			continue;
		}

		It->SetEnabled(false);
		FieldDirectionalLights.Add(*It);
	}
}

void UNUBattleEncounterSubsystem::TurnOffFieldHeightFogs()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const ULevel* StageLevel = CachedStageLevel.Get();

	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		// 세트가 들고 있는 안개는 건너뜀
		if (It->GetLevel() == StageLevel)
		{
			continue;
		}

		UExponentialHeightFogComponent* FogComponent = It->GetComponent();

		// 켜져있는 안개만 명단에 담음
		// 원래 꺼져있던 것까지 담으면 복구했을 때 기존 필드와 달라짐
		if (!IsValid(FogComponent) || !FogComponent->IsVisible())
		{
			continue;
		}

		// AExponentialHeightFog 는 ALight 가 아니라 AInfo 라서 SetEnabled 이 없음
		// 가시성을 끄면 렌더 상태가 다시 만들어지면서 씬의 안개 목록에서 빠짐
		FogComponent->SetVisibility(false);
		FieldHeightFogs.Add(*It);
	}
}

void UNUBattleEncounterSubsystem::SetStageGlobalVisualsVisible(bool bVisible)
{
	// 세트 조명 설정
	for (const TWeakObjectPtr<ADirectionalLight>& StageLight : CachedStageDirectionalLights)
	{
		if (ADirectionalLight* Light = StageLight.Get())
		{
			Light->SetEnabled(bVisible);
		}
	}

	// SkyLight 는 ALight 가 아니라 AInfo 라서 SetEnabled 이 없음
	// 가시성을 끄고 키는 방식으로 처리해야 함
	for (const TWeakObjectPtr<ASkyLight>& StageSkyLight : CachedStageSkyLights)
	{
		const ASkyLight* SkyLight = StageSkyLight.Get();
		USkyLightComponent* SkyLightComponent =
			IsValid(SkyLight) ? SkyLight->GetLightComponent() : nullptr;

		if (IsValid(SkyLightComponent))
		{
			SkyLightComponent->SetVisibility(bVisible);
		}
	}

	// 세트 안개도 같은 이유로 전투 중에만 켬
	// 끄지 않고 두면 다음에 필드 안개를 켤 때 세트 것이 먼저 등록된 상태가 되어 필드까지 덮음
	for (const TWeakObjectPtr<AExponentialHeightFog>& StageFog : CachedStageHeightFogs)
	{
		const AExponentialHeightFog* Fog = StageFog.Get();
		UExponentialHeightFogComponent* FogComponent =
			IsValid(Fog) ? Fog->GetComponent() : nullptr;

		if (IsValid(FogComponent))
		{
			FogComponent->SetVisibility(bVisible);
		}
	}
}

/////////////////////////////////////////////////////////////////
// 전투 참가자 처리
/////////////////////////////////////////////////////////////////

bool UNUBattleEncounterSubsystem::ValidateEncounterRequest(const FNUBattleEncounterRequest& Request, ENUBattleEncounterStartResult& OutResult) const
{
	OutResult = ENUBattleEncounterStartResult::InvalidRequest;

	const UWorld* World = GetWorld();

	// Subsystem은 클라이언트 월드에도 존재할 수 있지만
	// 실제 전투 생성은 서버에서만 수행한다.
	if (!IsValid(World) || World->GetNetMode() == NM_Client)
	{
		OutResult = ENUBattleEncounterStartResult::NotAuthority;
		return false;
	}

	if (!Request.IsValid())
	{
		OutResult = ENUBattleEncounterStartResult::InvalidRequest;
		return false;
	}

	ANUPlayerState* Initiator = Request.Initiator.Get();

	if (!IsValid(Initiator))
	{
		OutResult = ENUBattleEncounterStartResult::InvalidRequest;
		return false;
	}

	// 이미 전투에 참가 중이면 새로운 Encounter를 시작하지 않는다.
	if (IsValid(Initiator->GetBattleEncounter()))
	{
		OutResult = ENUBattleEncounterStartResult::AlreadyInBattle;
		return false;
	}

	// 필드 Avatar가 없으면 전투 Avatar 전환과 복귀를 수행할 수 없다.
	if (!IsValid(Initiator->GetCurrentAvatar()))
	{
		OutResult = ENUBattleEncounterStartResult::NoValidParticipants;
		return false;
	}

	OutResult = ENUBattleEncounterStartResult::Started;
	return true;
}

bool UNUBattleEncounterSubsystem::BuildParticipantsFromRequest(const FNUBattleEncounterRequest& Request, TArray<FNUBattleParticipant>& OutParticipants) const
{
	OutParticipants.Reset();

	ANUPlayerState* Initiator = Request.Initiator.Get();
	if (!IsValid(Initiator))
	{
		return false;
	}

	AActor* PlayerAvatar = Initiator->GetCurrentAvatar();
	if (!IsValid(PlayerAvatar))
	{
		return false;
	}

	// 플레이어 Participant
	{
		FNUBattleParticipant PlayerParticipant;

		PlayerParticipant.Team = ENUBattleTeam::Ally;
		PlayerParticipant.ASCOwner = Initiator;
		PlayerParticipant.AvatarActor = PlayerAvatar;
		PlayerParticipant.OwningPlayerState = Initiator;

		OutParticipants.Add(MoveTemp(PlayerParticipant));
	}

	// 활성 정령 Participant
	if (ANUSpiritState* SpiritState = Initiator->GetActiveSpiritState())
	{
		FNUBattleParticipant SpiritParticipant;

		SpiritParticipant.Team = ENUBattleTeam::Ally;
		SpiritParticipant.ASCOwner = SpiritState;

		// 나이아를 꺼내지 않았으면 필드 Avatar가 없어 nullptr이 들어감
		// 전투 Avatar는 인카운터가 새로 만들기 때문에 없어도 참가할 수 있어야 함
		SpiritParticipant.AvatarActor = SpiritState->GetCurrentAvatar();
		SpiritParticipant.OwningPlayerState = Initiator;

		OutParticipants.Add(MoveTemp(SpiritParticipant));
	}

	// 적 Participant
	for (const FNUEnemySpawnSpec& EnemySpec : Request.Enemies)
	{
		if (!EnemySpec.IsValid())
		{
			OutParticipants.Reset();
			return false;
		}

		FNUBattleParticipant EnemyParticipant;

		EnemyParticipant.Team = ENUBattleTeam::Enemy;

		// 실제 Enemy Avatar와 ASC는
		// ANUBattleEncounter::InitializeEncounter()에서 생성한다.
		EnemyParticipant.ASCOwner = nullptr;
		EnemyParticipant.AvatarActor = nullptr;
		EnemyParticipant.OwningPlayerState = nullptr;

		// 필드에서 전달된 EnemyId와 Level 보존
		EnemyParticipant.EnemySpec = EnemySpec;

		OutParticipants.Add(MoveTemp(EnemyParticipant));
	}

	return !OutParticipants.IsEmpty();
}

TSubclassOf<ANUBattleEncounter> UNUBattleEncounterSubsystem::ResolveEncounterClass(const FNUBattleEncounterRequest& Request) const
{
	if (Request.EncounterClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedClass = Request.EncounterClass.LoadSynchronous();

	if (!IsValid(LoadedClass) || !LoadedClass->IsChildOf(ANUBattleEncounter::StaticClass()))
	{
		return nullptr;
	}

	return LoadedClass;
}

