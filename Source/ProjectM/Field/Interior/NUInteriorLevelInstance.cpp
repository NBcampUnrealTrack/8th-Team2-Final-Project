#include "Field/Interior/NUInteriorLevelInstance.h"

#include "Field/Interior/NUWalkableDoor.h"
#include "Field/Interior/NUWalkableInteriorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "LevelInstance/LevelInstanceLevelStreaming.h"
#include "LevelInstance/LevelInstanceSubsystem.h"
#include "UObject/Script.h"

ANUInteriorLevelInstance::ANUInteriorLevelInstance()
{
	ExteriorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExteriorMesh"));
	ExteriorMesh->SetupAttachment(GetRootComponent());
	// 충돌과 안전 도착점 검사는 기존 실내 레벨의 건물 메시가 계속 담당한다.
	ExteriorMesh->SetCollisionProfileName(TEXT("NoCollision"));
	ExteriorMesh->SetGenerateOverlapEvents(false);
}

void ANUInteriorLevelInstance::OnLevelInstanceLoaded()
{
	Super::OnLevelInstanceLoaded();
#if WITH_EDITOR
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
		FWorldDelegates::LevelAddedToWorld.AddUObject(this, &ThisClass::HandleEditorLevelAdded);
	}
#endif
	BindInterior();
}

void ANUInteriorLevelInstance::BeginPlay()
{
	Super::BeginPlay();
	BindInterior();
}

#if WITH_EDITOR
void ANUInteriorLevelInstance::OnEdit()
{
	Super::OnEdit();
	ApplyEditorView();
}

void ANUInteriorLevelInstance::OnCommit(bool bChanged)
{
	Super::OnCommit(bChanged);
	BindInterior();
}

void ANUInteriorLevelInstance::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BindInterior();
}

void ANUInteriorLevelInstance::PostEditUndo()
{
	Super::PostEditUndo();
	BindInterior();
}

void ANUInteriorLevelInstance::BeginDestroy()
{
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	Super::BeginDestroy();
}

void ANUInteriorLevelInstance::HandleEditorLevelAdded(ULevel* Level, UWorld* World)
{
	if (World != GetWorld() || Level != GetLoadedLevel()) { return; }
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	BindInterior();
}

void ANUInteriorLevelInstance::ApplyEditorView()
{
	if (!GetWorld() || GetWorld()->IsGameWorld()) { return; }
	if (Interior.IsValid() && InteriorMesh.IsValid())
	{
		AActor* Building = Interior->GetOwner();
		if (IsValid(Building) && Building->Implements<UNUInteriorEditorView>())
		{
			// 실내 레벨의 가구와 NPC는 로드한 채 건물 재질만 외관 상태로 돌린다.
			FEditorScriptExecutionGuard ScriptGuard;
			INUInteriorEditorView::Execute_ApplyInteriorEditorView(Building, false);
		}
		// 두 건물 메시를 동시에 그리면 표면이 겹치므로 실내 레벨의 복제 메시만 끈다.
		InteriorMesh->SetVisibility(false);
	}
	ExteriorMesh->SetVisibility(true);
}
#endif

void ANUInteriorLevelInstance::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (InteriorStreaming.IsValid()) { InteriorStreaming->OnLevelShown.RemoveDynamic(this, &ThisClass::BindInterior); }
	InteriorStreaming.Reset();
	if (Interior.IsValid()) { Interior->OnPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged); }
	if (InteriorMesh.IsValid()) { InteriorMesh->SetVisibility(true); }
	Interior.Reset();
	InteriorMesh.Reset();
	Super::EndPlay(EndPlayReason);
}

void ANUInteriorLevelInstance::BindInterior()
{
	// OpenLevel 이후에는 로드 알림 뒤에 BP 컴포넌트가 초기화되므로, 표시 준비가 끝나면 다시 연결한다.
	ULevelInstanceSubsystem* Subsystem = GetWorld()->GetSubsystem<ULevelInstanceSubsystem>();
	ULevelStreaming* Streaming = Subsystem ? Subsystem->GetLevelInstanceLevelStreaming(this) : nullptr;
	if (InteriorStreaming.Get() != Streaming)
	{
		if (InteriorStreaming.IsValid()) { InteriorStreaming->OnLevelShown.RemoveDynamic(this, &ThisClass::BindInterior); }
		InteriorStreaming = Streaming;
	}
	if (IsValid(Streaming) && GetWorld()->IsGameWorld()) { Streaming->OnLevelShown.AddUniqueDynamic(this, &ThisClass::BindInterior); }
	ULevel* Level = GetLoadedLevel();
	if (!Level) { return; }
	if (Interior.IsValid()) { Interior->OnPresentationChanged.RemoveDynamic(this, &ThisClass::HandlePresentationChanged); }
	Interior.Reset();
	InteriorMesh.Reset();
	for (AActor* Actor : Level->Actors)
	{
		if (!IsValid(Actor)) { continue; }
		if (UNUWalkableInteriorComponent* Component = Actor->FindComponentByClass<UNUWalkableInteriorComponent>())
		{
			Interior = Component;
			Component->LocationId = LocationId;
			TInlineComponentArray<UStaticMeshComponent*> Meshes(Actor);
			for (UStaticMeshComponent* Mesh : Meshes)
			{
				if (Mesh->GetFName() == TEXT("ExteriorMesh")) { InteriorMesh = Mesh; break; }
			}
		}
		if (ANUWalkableDoor* Door = Cast<ANUWalkableDoor>(Actor)) { Door->ExteriorLocationId = ExteriorLocationId; }
	}
	if (!Interior.IsValid() || !InteriorMesh.IsValid() || !ExteriorMesh->GetStaticMesh()) { return; }
#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		// 로드 알림 시점에는 Blueprint 컴포넌트 생성 전일 수 있으므로 표시 완료를 기다린다.
		if (IsValid(Streaming) && !Streaming->IsLevelVisible()) { return; }
		ApplyEditorView();
		return;
	}
#endif
	if (GetWorld()->IsGameWorld()) { Interior->OnPresentationChanged.AddUniqueDynamic(this, &ThisClass::HandlePresentationChanged); }
	HandlePresentationChanged(Interior->IsLocalPlayerInside());
}

void ANUInteriorLevelInstance::HandlePresentationChanged(bool bInside)
{
	if (!InteriorMesh.IsValid()) { return; }
	ExteriorMesh->SetVisibility(!bInside);
	InteriorMesh->SetVisibility(bInside);
}
