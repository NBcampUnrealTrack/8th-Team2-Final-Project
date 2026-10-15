// Fill out your copyright notice in the Description page of Project Settings.

#include "Field/NUSpriteVisualComponent.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Animation/NUPaperZDAnimInstance.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DecalComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "AnimSequences/Players/PaperZDAnimPlayer.h"
#include "AnimSequences/Skins/PaperZDAnimationSkin.h"
#include "AnimSequences/Players/PaperZDLayerLink.h"
#include "CharacterSelect/Data/NUAppearanceDataSettings.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "Field/NUSpriteFrameOffset.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/NUInventoryTypes.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimInstance.h"
#if WITH_EDITOR
#include "PaperSprite.h"
#include "TextureCompiler.h"
#endif

static TAutoConsoleVariable<int32> CVarEquipFrameBounce(
	TEXT("nu.EquipFrameBounce"),
	1,
	TEXT("장비 겹침 레이어가 몸통 프레임의 세로 흔들림을 따라가게 한다 (0=끔)"),
	ECVF_Default);

#if !UE_BUILD_SHIPPING
// 장비 레이어를 PIE에서 눈으로 맞추는 명령. 값이 정해지면 코드 기본값으로 옮긴다
static FAutoConsoleCommandWithWorldAndArgs GTuneEquipLayerCommand(
	TEXT("nu.Equip.Tune"),
	TEXT("장비 레이어 조정: nu.Equip.Tune <레이어 1=갑옷 2=장갑 3=신발> <X보정> <Z보정> [바운스배율=1] [프레임지연=0]"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 3 || World == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NU] 사용법: nu.Equip.Tune <레이어> <X보정> <Z보정> [바운스배율] [프레임지연]"));
			return;
		}

		const int32 LayerIndex = FCString::Atoi(*Args[0]);
		const float XOffset = FCString::Atof(*Args[1]);
		const float ZOffset = FCString::Atof(*Args[2]);
		const float BounceScale = Args.IsValidIndex(3) ? FCString::Atof(*Args[3]) : 1.0f;
		const int32 FrameDelay = Args.IsValidIndex(4) ? FCString::Atoi(*Args[4]) : 0;

		const APlayerController* PC = World->GetFirstPlayerController();
		const APawn* Pawn = PC != nullptr ? PC->GetPawn() : nullptr;
		if (Pawn == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[NU] 조종 중인 폰이 없습니다 — PIE 중에만 쓸 수 있습니다"));
			return;
		}
		if (UNUSpriteVisualComponent* Visual = Pawn->FindComponentByClass<UNUSpriteVisualComponent>())
		{
			Visual->TuneLayer(LayerIndex, XOffset, ZOffset, BounceScale, FrameDelay);
		}
	}));
#endif

namespace NUSpriteVisual
{
	constexpr float RecentlyRenderedTolerance = 0.2f;
}

UNUSpriteVisualComponent::UNUSpriteVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UNUSpriteVisualComponent::Setup(UPaperFlipbookComponent* InSprite, UPaperZDAnimationComponent* InAnimation,
	const FNUSpriteVisualSettings& InSettings)
{
	Sprite = InSprite;
	Animation = InAnimation;
	Settings = InSettings;
}

void UNUSpriteVisualComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!IsValid(Sprite))
	{
		UE_LOG(LogTemp, Error, TEXT("[NU] %s: Setup()이 호출되지 않았거나 Sprite가 없습니다"),
			*GetNameSafe(GetOwner()));
		return;
	}

#if WITH_EDITOR
	FTextureCompilingManager::Get().OnTexturePostCompileEvent().AddUObject(this, &UNUSpriteVisualComponent::HandleTexturesCompiled);
#endif

	ApplyAnimBlueprint();
	RefreshAppearance();
	ApplyLayerLinks();

	Sprite->SetUsingAbsoluteRotation(Settings.bUseAbsoluteRotation);

	if (bAutomaticBillboardUpdatesEnabled && GetNetMode() != NM_DedicatedServer)
	{
		SetComponentTickInterval(AutomaticBillboardUpdateInterval);
		SetComponentTickEnabled(true);
		UpdateBillboard();
	}
}

void UNUSpriteVisualComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 레이어를 이번 프레임의 몸 프레임에 맞춘다 --- 이 틱은 애님 컴포넌트 뒤에 돈다 (ApplyLayerLinks)
	UpdateLayerFrameOffsets();

	// 발소리: 내 캐릭터가 걷는 동안 발이 땅에 닿는 프레임(걷기 8프레임 중 0·4)에 들어설 때 한 번씩
	const UPaperFlipbook* WalkFlipbook = bLocomotionApplied && bLastMoving && IsValid(Sprite) ? Sprite->GetFlipbook() : nullptr;
	const APawn* OwnerPawn = WalkFlipbook != nullptr ? Cast<APawn>(GetOwner()) : nullptr;
	if (OwnerPawn != nullptr && OwnerPawn->IsPlayerControlled())
	{
		const int32 Frame = WalkFlipbook->GetKeyFrameIndexAtTime(Sprite->GetPlaybackPosition(), true);
		if (Frame != LastFootstepFrame)
		{
			LastFootstepFrame = Frame;
			if (Frame % 4 == 0)
			{
				UNUSFXSubsystem::Play(this, FootstepSFX.IsValid() ? FootstepSFX : FGameplayTag(NU::SFX::Field::Footstep));
			}
		}
	}
	else
	{
		LastFootstepFrame = INDEX_NONE; // 멈췄다 다시 걸으면 첫 걸음부터 난다
	}

	if (bAutomaticBillboardUpdatesEnabled && IsValid(Sprite) && Sprite->WasRecentlyRendered(NUSpriteVisual::RecentlyRenderedTolerance))
	{
		UpdateBillboard();
	}
}

void UNUSpriteVisualComponent::EnableAutomaticBillboardUpdates(float UpdateInterval)
{
	bAutomaticBillboardUpdatesEnabled = true;
	AutomaticBillboardUpdateInterval = FMath::Max(UpdateInterval, KINDA_SMALL_NUMBER);

	if (HasBegunPlay() && GetNetMode() != NM_DedicatedServer)
	{
		SetComponentTickInterval(AutomaticBillboardUpdateInterval);
		SetComponentTickEnabled(true);
		UpdateBillboard();
	}
}

void UNUSpriteVisualComponent::ApplyAnimBlueprint()
{
	// 기본값으로 설정
	ApplyAnimBlueprintClass(Settings.AnimBlueprint);
}

bool UNUSpriteVisualComponent::ApplyAnimBlueprintClass(const TSoftClassPtr<UPaperZDAnimInstance>& AnimBlueprint)
{
	if (AnimBlueprint.IsNull() || !IsValid(Animation))
	{
		return false;
	}

	UClass* AnimClass = AnimBlueprint.LoadSynchronous();
	if (!AnimClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[NU] %s: 애님 블루프린트를 찾을 수 없습니다 — %s"),
			*GetNameSafe(GetOwner()), *AnimBlueprint.ToSoftObjectPath().ToString());
		return false;
	}

	// 현재 AnimInstance를 확인해서 같다면 종료
	// true는 AnimBP가 적용했다는 뜻
	const UPaperZDAnimInstance* Current = Animation->GetAnimInstance();
	if (Current && Current->GetClass() == AnimClass)
	{
		return true;
	}

	Animation->SetAnimInstanceClass(AnimClass);

	// 방향을 다시 넣기 위해 이미 적용됐다는 bool 값을 초기화
	bLocomotionApplied = false;

	// 애님 BP를 갈면 플립북이 바뀌면서 머티리얼·색이 초기화되므로 다시 입힘
	// 전투 아바타의 피격 점등이 유지됨
	RefreshAppearance();

	// 애님 인스턴스가 새로 만들어지면 플레이어도 새것 — 레이어 연결을 다시 건다
	ApplyLayerLinks();

	// 쓰러짐도 새 인스턴스에 다시 넣는다, 안 넣으면 쓰러진 아군이 교체 뒤에 일어서 있다
	SetAnimDeadState(bAnimDead);
	return true;
}

void UNUSpriteVisualComponent::RegisterLayerRenderComponent(UPaperFlipbookComponent* InComponent, int32 LayerIndex,
	float BounceScale, int32 FrameDelay)
{
	if (!IsValid(InComponent) || LayerIndex <= 0)
	{
		return; // 0번은 본체 렌더 컴포넌트의 자리
	}
	const FVector BaseLocation = InComponent->GetRelativeLocation();
	LayerComponents.Add(InComponent);
	LayerIndices.Add(LayerIndex);
	LayerBaseX.Add(BaseLocation.X);
	LayerBaseZ.Add(BaseLocation.Z);
	LayerBounceScale.Add(BounceScale);
	LayerTuneX.Add(0.0f);
	LayerTuneZ.Add(0.0f);
	LayerBounceDelay.Add(FrameDelay);
	LayerAppliedZ.Add(TNumericLimits<float>::Max()); // 첫 갱신을 강제한다

	if (HasBegunPlay())
	{
		ApplyLayerLinks();
	}
}

void UNUSpriteVisualComponent::UpdateLayerFrameOffsets()
{
	if (!IsValid(Sprite) || LayerComponents.IsEmpty())
	{
		return;
	}

	// 방향·상태가 바뀌면 몸통 플립북도 바뀐다 --- 그때만 측정값을 다시 가져온다
	const UPaperFlipbook* Flipbook = Sprite->GetFlipbook();
	if (bLayerBounceEnabled && BounceFlipbook.Get() != Flipbook)
	{
		BounceFlipbook = Flipbook;
		BounceOffsets = NUSpriteFrameOffset::GetVerticalOffsets(Flipbook);
	}

	// PaperZD가 본체와 레이어에 같은 재생 시간을 넣으므로 본체의 현재 프레임이 곧 레이어의 프레임
	int32 FrameIndex = INDEX_NONE;
	if (bLayerBounceEnabled && CVarEquipFrameBounce.GetValueOnGameThread() != 0 && Flipbook != nullptr && !BounceOffsets.IsEmpty())
	{
		FrameIndex = Flipbook->GetKeyFrameIndexAtTime(Sprite->GetPlaybackPosition(), true);
	}

	for (int32 Index = 0; Index < LayerComponents.Num(); ++Index)
	{
		UPaperFlipbookComponent* Component = LayerComponents[Index];
		if (!IsValid(Component) || !LayerBaseZ.IsValidIndex(Index))
		{
			continue;
		}
		if (!Component->IsVisible())
		{
			continue; // 안 낀 부위는 계산할 것이 없다 (다시 보일 때 아래 비교가 강제로 맞춘다)
		}

		float Bounce = 0.0f;
		if (FrameIndex != INDEX_NONE)
		{
			// 지연은 순환으로 --- Idle은 루프라 앞 프레임으로 감는 것이 자연스럽다
			const int32 Delay = LayerBounceDelay.IsValidIndex(Index) ? LayerBounceDelay[Index] : 0;
			const int32 Count = BounceOffsets.Num();
			const int32 Delayed = ((FrameIndex - Delay) % Count + Count) % Count;
			const float Scale = LayerBounceScale.IsValidIndex(Index) ? LayerBounceScale[Index] : 1.0f;
			Bounce = BounceOffsets[Delayed] * Scale;
		}

		const float TuneZ = LayerTuneZ.IsValidIndex(Index) ? LayerTuneZ[Index] : 0.0f;
		const float TuneX = LayerTuneX.IsValidIndex(Index) ? LayerTuneX[Index] : 0.0f;
		const float TargetZ = LayerBaseZ[Index] + TuneZ + Bounce;
		const float TargetX = (LayerBaseX.IsValidIndex(Index) ? LayerBaseX[Index] : 0.0f) + TuneX;

		FVector Location = Component->GetRelativeLocation();
		if (LayerAppliedZ.IsValidIndex(Index)
			&& FMath::IsNearlyEqual(TargetZ, LayerAppliedZ[Index])
			&& FMath::IsNearlyEqual(TargetX, Location.X))
		{
			continue;
		}
		LayerAppliedZ[Index] = TargetZ;

		Location.X = TargetX;
		Location.Z = TargetZ;
		Component->SetRelativeLocation(Location);
	}
}

void UNUSpriteVisualComponent::RefreshAppearanceSkins(int32 RaceId, int32 Gender, int32 HairId, int32 FaceId, int32 JobId)
{
	if (!IsValid(Animation))
	{
		return;
	}
	UPaperZDAnimInstance* AnimInstance = Animation->GetOrCreateAnimInstance();
	if (AnimInstance == nullptr)
	{
		return;
	}

	// 이 클래스에도 Settings 멤버가 있어 이름을 갈라 둔다 (표현 설정과 외형 표는 다른 것)
	const UNUAppearanceDataSettings* AppearanceData = GetDefault<UNUAppearanceDataSettings>();

	// 커마 베이스 몸 --- 본체(레이어 0)를 종족·성별·직업에 맞는 몸 줄의 스킨으로 덮는다. 줄이 없으면 원래 몸.
	// 스킨에 없는 시퀀스가 재생되면 원래 몸이 나오므로, 스킨 애셋이 시퀀스를 전부 덮어야 한다
	const FNUBodyRow* Body = NUAppearance::FindBody(RaceId, Gender, JobId);
	UPaperZDAnimationSkin* BodySkin = Body != nullptr ? Body->FieldSkin.LoadSynchronous() : nullptr;
	if (BodySkin != nullptr)
	{
		AnimInstance->ApplyAnimationSkinToLayer(BodySkin, 0);
	}
	else
	{
		AnimInstance->RemoveSkinsFromLayer(0);
	}

	// 발소리: 나이아 나이트(1001)는 갑옷, 남성은 남캐 발소리, 그 밖은 기본(여캐에 맞춘 소리)
	FootstepSFX = JobId == 1001 ? NU::SFX::Field::FootstepArmor
		: Gender == static_cast<int32>(ENUCharacterGender::Male) ? NU::SFX::Field::FootstepMale : NU::SFX::Field::Footstep;

	// 몸이 바뀌면 그 몸의 뒷모습 파츠 보정도 바뀐다 --- 이미 뒷모습이면 즉시 다시 넣는다
	BackViewPartTuneZ = Body != nullptr ? Body->BackViewPartTuneZ : 0.0f;
	ApplyBackViewPartTune();

	// 레이어 번호는 애님 소스(AS_Player)의 Additional Layer Data 순서 + 1.
	// 1~5는 장비가 쓰고 있어 파츠는 6부터다
	// 머리 번호에는 머리 색이 같이 실려 온다 (NUAppearance::PackHairId) --- 파츠는 번호로 찾고 색은 아래에서 레이어에 입힌다
	int32 HairPartId = HairId;
	int32 HairColorIndex = 0;
	NUAppearance::SplitHairId(HairId, HairPartId, HairColorIndex);
	const bool bWasHairTinted = bHairTinted;
	bHairTinted = NUAppearance::GetHairTint(HairColorIndex, HairTint);

	const TSoftObjectPtr<UDataTable>* Tables[] = { &AppearanceData->HairPartTable, &AppearanceData->FacePartTable };
	const int32 PartIds[] = { HairPartId, FaceId };
	const int32 Layers[] = { NUSpriteLayer::Hair, NUSpriteLayer::Face };

	for (int32 i = 0; i < 2; ++i)
	{
		const int32 Index = LayerIndices.IndexOfByKey(Layers[i]);
		UPaperFlipbookComponent* Component = LayerComponents.IsValidIndex(Index) ? LayerComponents[Index] : nullptr;
		if (!IsValid(Component))
		{
			continue; // 이 액터가 안 쓰는 레이어 (원격 캐릭터 등)
		}

		const FNUAppearancePartRow* Part = NUAppearance::FindPart(*Tables[i], PartIds[i], RaceId, Gender, JobId);
		UPaperZDAnimationSkin* Skin = Part != nullptr ? Part->FieldSkin.LoadSynchronous() : nullptr;

		// 파츠가 아직 없으면 레이어를 비워 둔다 --- 몸만 나온다
		AnimInstance->ApplyAnimationSkinToLayer(Skin, Layers[i]);
		Component->SetVisibility(Skin != nullptr);
	}

	// 머리 색이 생겼거나 없어졌으면 머리 레이어의 머티리얼과 색을 다시 맞춘다 (색을 안 쓰는 캐릭터는 건드리지 않는다)
	if (bHairTinted || bWasHairTinted)
	{
		ApplyHairTintMaterial();
		ApplyColor();
	}
}

void UNUSpriteVisualComponent::ApplyHairTintMaterial()
{
	const int32 Index = LayerIndices.IndexOfByKey(NUSpriteLayer::Hair);
	UPaperFlipbookComponent* Hair = LayerComponents.IsValidIndex(Index) ? LayerComponents[Index].Get() : nullptr;
	if (!IsValid(Hair))
	{
		return; // 머리 레이어를 안 쓰는 액터
	}

	UMaterialInterface* Material = nullptr;
	if (bHairTinted)
	{
		Material = GetDefault<UNUAppearanceDataSettings>()->HairTintMaterial.LoadSynchronous();
		if (Material == nullptr)
		{
			// 머티리얼이 없으면 정점 색이 그림에 그냥 곱해져 엉뚱한 색이 된다 --- 원래 색으로 둔다
			UE_LOG(LogTemp, Warning, TEXT("[NU] %s: 머리 색 머티리얼을 찾을 수 없어 원래 색으로 그립니다"), *GetNameSafe(GetOwner()));
			bHairTinted = false;
		}
	}
	if (Material == nullptr && !Settings.Material.IsNull())
	{
		Material = Settings.Material.LoadSynchronous(); // 다른 레이어와 같은 것 (비면 스프라이트 애셋의 머티리얼)
	}
	Hair->SetMaterial(0, Material);
}

void UNUSpriteVisualComponent::RefreshEquipmentSkins(const UNUEquipmentComponent* Equipment,
	UNUInventoryComponent* Inventory)
{
	if (!IsValid(Animation))
	{
		return;
	}
	UPaperZDAnimInstance* AnimInstance = Animation->GetOrCreateAnimInstance();
	if (AnimInstance == nullptr)
	{
		return;
	}

	// 8/25 회의: 장비 외형은 무기만 반영하고 나머지는 기본 복장 고정.
	// 새 몸(TestCHBody)에 복장이 구워져 있어 부위 겹침 스킨을 얹으면 이중으로 보인다.
	// 그래서 장비를 무시하고 레이어를 전부 비운다 --- 무기·아바타 파츠가 정해지면 되살린다
	const TArray<FNUEquippedItem>* Equipped = nullptr;

	// 부위 => 레이어. 레이어 번호는 애님 소스(AS_Player)의 Additional Layer Data 순서 + 1.
	// bOverlay는 그 부위의 보조 스킨(팔보다 앞에 그릴 어깨)을 쓰는 레이어라는 뜻.
	// 갑옷 한 부위가 레이어 둘(몸통 + 어깨)을 쓰므로 Chest가 두 번 나온다
	struct FSlotLayer { ENUEquipSlot Slot; int32 Layer; bool bOverlay; };
	static const FSlotLayer SlotLayers[] = {
		{ ENUEquipSlot::Chest,  NUSpriteLayer::EquipChest,    false },
		{ ENUEquipSlot::Gloves, NUSpriteLayer::EquipGloves,   false },
		{ ENUEquipSlot::Boots,  NUSpriteLayer::EquipBoots,    false },
		{ ENUEquipSlot::Legs,   NUSpriteLayer::EquipLegs,     false },
		{ ENUEquipSlot::Chest,  NUSpriteLayer::EquipShoulder, true  },
	};

	for (const FSlotLayer& Entry : SlotLayers)
	{
		const int32 Index = LayerIndices.IndexOfByKey(Entry.Layer);
		UPaperFlipbookComponent* Component = LayerComponents.IsValidIndex(Index) ? LayerComponents[Index] : nullptr;
		if (!IsValid(Component))
		{
			continue; // 이 액터가 안 쓰는 레이어
		}

		// 그 부위 장착템의 스킨을 찾는다 --- 미장착/스킨 없음이면 해제
		UPaperZDAnimationSkin* Skin = nullptr;
		const int32 SlotIndex = static_cast<int32>(Entry.Slot);
		if (Equipped != nullptr && Equipped->IsValidIndex(SlotIndex) && (*Equipped)[SlotIndex].InstanceId.IsValid())
		{
			if (const FNUItemDefRow* Def = Inventory->FindItemDef((*Equipped)[SlotIndex].ItemRowName))
			{
				Skin = Entry.bOverlay
					? Def->EquipSkinOverlay.LoadSynchronous()
					: Def->EquipSkin.LoadSynchronous();
			}
		}

		if (Skin != nullptr)
		{
			AnimInstance->ApplyAnimationSkinToLayer(Skin, Entry.Layer);
			Component->SetVisibility(true);
		}
		else
		{
			AnimInstance->RemoveSkinsFromLayer(Entry.Layer);
			Component->SetVisibility(false);
		}
	}
}

bool UNUSpriteVisualComponent::RefreshCombatAppearanceSkins(int32 RaceId, int32 Gender, int32 HairId, int32 JobId)
{
	if (!IsValid(Animation))
	{
		return false;
	}
	UPaperZDAnimInstance* AnimInstance = Animation->GetOrCreateAnimInstance();
	if (AnimInstance == nullptr)
	{
		return false;
	}

	// 필드와 같은 몸 줄에서 찾는다 --- "이 종족·성별·직업의 몸" 판정이 두 벌이면 한쪽만 낡는다
	const FNUBodyRow* Body = NUAppearance::FindBody(RaceId, Gender, JobId);
	UPaperZDAnimationSkin* BodySkin = Body != nullptr ? Body->CombatBodySkin.LoadSynchronous() : nullptr;
	if (BodySkin == nullptr)
	{
		return false;
	}

	AnimInstance->ApplyAnimationSkinToLayer(BodySkin, 0);

	// 뒷모습 파츠 보정은 필드 뒷모습 시트의 목 라인을 맞추는 값이라 전투 그림엔 안 맞는다.
	// 안 비우면 전투 방향이 Up 이라 머리·얼굴이 한 칸 내려간다
	BackViewPartTuneZ = 0.0f;
	ApplyBackViewPartTune();

	// 머리는 필드에 그려지는 머리 파츠(번호가 없거나 표에 없으면 그 몸의 첫 파츠)의 전투 그림 --- 없으면 이 몸의 기본 전투 머리.
	// 머리 번호에 같이 실려 온 색도 필드처럼 머리 레이어에 입힌다
	int32 HairPartId = HairId;
	int32 HairColorIndex = 0;
	NUAppearance::SplitHairId(HairId, HairPartId, HairColorIndex);
	const FNUAppearancePartRow* HairPart = NUAppearance::FindPart(
		GetDefault<UNUAppearanceDataSettings>()->HairPartTable, HairPartId, RaceId, Gender, JobId);
	const TSoftObjectPtr<UPaperZDAnimationSkin>& HairSkin =
		Body->FindCombatHairSkin(HairPart != nullptr ? HairPart->PartId : HairPartId);

	const bool bWasHairTinted = bHairTinted;
	bHairTinted = NUAppearance::GetHairTint(HairColorIndex, HairTint);

	const TSoftObjectPtr<UPaperZDAnimationSkin>* Skins[] = { &HairSkin, &Body->CombatFaceSkin };
	const int32 PartLayers[] = { NUSpriteLayer::Hair, NUSpriteLayer::Face };

	for (int32 i = 0; i < 2; ++i)
	{
		const int32 Index = LayerIndices.IndexOfByKey(PartLayers[i]);
		UPaperFlipbookComponent* Component = LayerComponents.IsValidIndex(Index) ? LayerComponents[Index] : nullptr;
		if (!IsValid(Component))
		{
			continue;
		}

		// 스킨 안에서 비워 둔 칸은 그 레이어를 안 그린다 (트래커 대기 --- 머리가 몸에 구워져 있음).
		// 전투 시퀀스의 레이어 칸을 비워 뒀으므로 원본으로 샐 그림도 없다
		UPaperZDAnimationSkin* Skin = Skins[i]->LoadSynchronous();
		if (Skin != nullptr)
		{
			AnimInstance->ApplyAnimationSkinToLayer(Skin, PartLayers[i]);
		}
		else
		{
			AnimInstance->RemoveSkinsFromLayer(PartLayers[i]);
		}
		Component->SetVisibility(Skin != nullptr);
	}

	// 전투 그림엔 장비 층이 없다 --- 장비 레이어는 비우고 숨긴다
	const int32 EquipLayers[] = { NUSpriteLayer::EquipChest, NUSpriteLayer::EquipGloves,
		NUSpriteLayer::EquipBoots, NUSpriteLayer::EquipLegs, NUSpriteLayer::EquipShoulder };

	for (const int32 Layer : EquipLayers)
	{
		const int32 Index = LayerIndices.IndexOfByKey(Layer);
		UPaperFlipbookComponent* Component = LayerComponents.IsValidIndex(Index) ? LayerComponents[Index] : nullptr;
		if (!IsValid(Component))
		{
			continue;
		}

		AnimInstance->RemoveSkinsFromLayer(Layer);
		Component->SetVisibility(false);
	}

	if (bHairTinted || bWasHairTinted)
	{
		ApplyHairTintMaterial();
		ApplyColor();
	}

	return true;
}

void UNUSpriteVisualComponent::SetLayerBounceEnabled(bool bEnabled)
{
	if (bLayerBounceEnabled == bEnabled)
	{
		return;
	}

	bLayerBounceEnabled = bEnabled;

	// 다음 틱에 레이어 위치를 다시 넣게 한다, 안 하면 마지막 바운스 높이에 멈춰 있다
	for (float& Applied : LayerAppliedZ)
	{
		Applied = TNumericLimits<float>::Max();
	}
}

void UNUSpriteVisualComponent::SetAnimDeadState(bool bDead)
{
	bAnimDead = bDead;

	if (!IsValid(Animation))
	{
		return;
	}

	// 나이아·적 애님 BP 는 이 부모가 아닐 수 있다 --- 그쪽 쓰러짐은 디졸브·숨김이 맡는다
	if (UNUPaperZDAnimInstance* AnimInstance =
		Cast<UNUPaperZDAnimInstance>(Animation->GetOrCreateAnimInstance()))
	{
		AnimInstance->SetDeadState(bDead);
	}
}

void UNUSpriteVisualComponent::ApplyBackViewPartTune()
{
	const float TuneZ = LastFacing == EFieldFacing::Up ? BackViewPartTuneZ : 0.0f;
	SetLayerTuneZ(NUSpriteLayer::Hair, TuneZ);
	SetLayerTuneZ(NUSpriteLayer::Face, TuneZ);
}

void UNUSpriteVisualComponent::SetLayerTuneZ(int32 LayerIndex, float ZOffset)
{
	const int32 Index = LayerIndices.IndexOfByKey(LayerIndex);
	if (Index == INDEX_NONE || !LayerTuneZ.IsValidIndex(Index))
	{
		return;
	}
	if (FMath::IsNearlyEqual(LayerTuneZ[Index], ZOffset))
	{
		return;
	}
	LayerTuneZ[Index] = ZOffset;
	LayerAppliedZ[Index] = TNumericLimits<float>::Max(); // 다음 틱에 다시 적용되게
}

void UNUSpriteVisualComponent::TuneLayer(int32 LayerIndex, float XOffset, float ZOffset, float BounceScale, int32 FrameDelay)
{
	const int32 Index = LayerIndices.IndexOfByKey(LayerIndex);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[NU] 레이어 %d은 등록돼 있지 않습니다"), LayerIndex);
		return;
	}

	LayerTuneX[Index] = XOffset;
	LayerTuneZ[Index] = ZOffset;
	LayerBounceScale[Index] = BounceScale;
	LayerBounceDelay[Index] = FrameDelay;
	LayerAppliedZ[Index] = TNumericLimits<float>::Max(); // 다음 틱에 다시 적용되게

	UE_LOG(LogTemp, Log, TEXT("[NU] 레이어 %d 조정 — X %.2f, Z %.2f, 바운스 %.2f, 지연 %d프레임"),
		LayerIndex, XOffset, ZOffset, BounceScale, FrameDelay);
}

void UNUSpriteVisualComponent::ApplyLayerLinks()
{
	if (!IsValid(Animation) || LayerComponents.IsEmpty())
	{
		return;
	}

	// 레이어 높이는 PaperZD가 이번 프레임의 몸 프레임을 정한 뒤에 맞춰야 한다 --- 이 컴포넌트 틱을 애님 컴포넌트 뒤에 걸고 거기서 맞춘다.
	// 액터 틱에서 맞추면 애님 컴포넌트보다 먼저 돌아(액터 PrePhysics, 컴포넌트 DuringPhysics)
	// 몸 프레임이 넘어가는 순간마다 머리·장비가 한 틱 늦게 따라온다 (10/10)
	if (GetNetMode() != NM_DedicatedServer)
	{
		PrimaryComponentTick.AddPrerequisite(Animation, Animation->PrimaryComponentTick);
		SetComponentTickEnabled(true);
	}

	UPaperZDAnimInstance* AnimInstance = Animation->GetOrCreateAnimInstance();
	UPaperZDAnimPlayer* Player = AnimInstance != nullptr ? AnimInstance->GetPlayer() : nullptr;
	if (Player == nullptr)
	{
		return;
	}

	TArray<FPaperZDLayerLinkData> Links;
	for (int32 Index = 0; Index < LayerComponents.Num(); ++Index)
	{
		UPaperFlipbookComponent* Component = LayerComponents[Index];
		if (!IsValid(Component))
		{
			continue;
		}
		FPaperZDLayerLinkData& Link = Links.AddDefaulted_GetRef();
		Link.LinkedComponentPtr = Component;
		Link.LayerIndex = LayerIndices[Index];

		// 본체와 같은 머티리얼을 컴포넌트 오버라이드로 — 플립북이 갈려도 유지된다.
		// 햇빛 그림자는 층도 본체 설정을 따른다 --- 본체만 드리우면 그림자 머리가 민머리가 되고 장비가 빠진다
		if (!Settings.Material.IsNull())
		{
			if (UMaterialInterface* Material = Settings.Material.LoadSynchronous())
			{
				Component->SetMaterial(0, Material);
			}
		}
		Component->SetCastShadow(Settings.bCastShadow);
	}
	Player->RegisterLayerLinks(Links);

	// 위에서 머리 레이어에도 본체 머티리얼을 꽂았다 --- 물들인 머리면 되돌린다
	if (bHairTinted)
	{
		ApplyHairTintMaterial();
	}
}

#if WITH_EDITOR
void UNUSpriteVisualComponent::HandleTexturesCompiled(const TArrayView<UTexture* const>& Textures)
{
	// 스프라이트가 넘기는 텍스처(그림과 노멀 같은 추가 텍스처)만 본다 --- 머티리얼이 가리키는 텍스처는 엔진이 이미 처리한다
	const auto UsesCompiledTexture = [&Textures](const UPaperFlipbook* Flipbook)
	{
		if (!IsValid(Flipbook)) return false;

		for (int32 Index = 0; Index < Flipbook->GetNumKeyFrames(); ++Index)
		{
			const UPaperSprite* FrameSprite = Flipbook->GetKeyFrameChecked(Index).Sprite;
			if (!IsValid(FrameSprite)) continue;

			if (Textures.Contains(FrameSprite->GetBakedTexture())) return true;

			FAdditionalSpriteTextureArray AdditionalTextures;
			FrameSprite->GetBakedAdditionalSourceTextures(AdditionalTextures);
			for (UTexture* AdditionalTexture : AdditionalTextures)
			{
				if (Textures.Contains(AdditionalTexture)) return true;
			}
		}
		return false;
	};

	TArray<UPaperFlipbookComponent*> Components(LayerComponents);
	Components.Add(Sprite);
	for (UPaperFlipbookComponent* Component : Components)
	{
		if (IsValid(Component) && UsesCompiledTexture(Component->GetFlipbook()))
		{
			Component->MarkRenderStateDirty();
		}
	}
}
#endif

void UNUSpriteVisualComponent::RefreshAppearance()
{
	if (!IsValid(Sprite)) return;

	// null이면 스프라이트 애셋의 머티리얼을 그대로
	// 지정하면 컴포넌트 오버라이드로 들어가므로 플립북이 바뀌어도 유지됨
	if (!Settings.Material.IsNull())
	{
		if (UMaterialInterface* Material = Settings.Material.LoadSynchronous())
		{
			Sprite->SetMaterial(0, Material);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[NU] %s: 스프라이트 머티리얼을 찾을 수 없습니다 — %s"),
				*GetNameSafe(GetOwner()), *Settings.Material.ToSoftObjectPath().ToString());
		}
	}

	// 로컬/원격이 똑같이 상대 위치 설정
	Sprite->SetRelativeLocation(FVector(0.0f, 0.0f, Settings.SpritePivotZ));

	// 빌보드 세로 늘림(UpdateBillboard)이 이미 적용돼 있으면 지우지 않고 유지한다
	FVector Scale = Settings.RelativeScale;
	Scale.Z *= AppliedBillboardStretch;
	Sprite->SetRelativeScale3D(Scale);
	Sprite->SetCastShadow(Settings.bCastShadow);
	Sprite->SetAffectDynamicIndirectLighting(false);
	ApplyColor();
	RefreshBlobShadow();
}

void UNUSpriteVisualComponent::RefreshBlobShadow()
{
	// 스프라이트는 카메라를 향하는 빌보드라 태양 각도에 따라 실제 그림자가 사라진다.
	// 접지감은 발밑 데칼로 따로 준다 (HD-2D 게임들이 쓰는 방식).
	if (!IsValid(Sprite))
	{
		return;
	}

	const bool bWanted = Settings.bCastShadow && Settings.BlobShadowRadius > 0.0f;
	if (!bWanted)
	{
		if (IsValid(BlobShadow))
		{
			BlobShadow->SetVisibility(false);
		}
		return;
	}

	if (!IsValid(BlobShadow))
	{
		USceneComponent* Parent = Sprite->GetAttachParent();
		AActor* Owner = GetOwner();
		if (Parent == nullptr && IsValid(Owner))
		{
			Parent = Owner->GetRootComponent();
		}
		if (Parent == nullptr)
		{
			return;
		}

		// 애셋이 아직 없는 프로젝트에서도 조용히 넘어가도록 소프트 로드
		UMaterialInterface* DecalMaterial = Cast<UMaterialInterface>(
			FSoftObjectPath(TEXT("/Game/Village/Materials/MI_HD2D_BlobShadow.MI_HD2D_BlobShadow")).TryLoad());
		if (DecalMaterial == nullptr)
		{
			return;
		}

		BlobShadow = NewObject<UDecalComponent>(Parent->GetOwner(), TEXT("NUBlobShadow"));
		BlobShadow->SetDecalMaterial(DecalMaterial);
		BlobShadow->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		BlobShadow->RegisterComponent();
		// 데칼은 로컬 +X 방향으로 투영되므로 아래를 보게 눕힌다
		BlobShadow->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
		BlobShadow->SetSortOrder(-1);
	}

	const float Radius = Settings.BlobShadowRadius;
	// 데칼 박스는 컴포넌트 위치 기준으로 투영축(로컬 X) 양쪽으로 DecalSize.X 만큼 퍼진다.
	// 여기에 반경을 그대로 넣으면 박스가 발밑에서 위로도 그만큼 올라가, 스프라이트가
	// 마스크드라 뎁스를 쓰는 탓에 자기 하체에 그림자가 찍혀 다리가 통째로 어두워졌다.
	// 투영 깊이는 얕게 잡고, 박스 윗면이 지면 살짝 위에 걸치도록 아래로 내린다.
	const float ProjectionDepth = 12.0f;
	const float TopMargin = 2.0f;
	BlobShadow->DecalSize = FVector(ProjectionDepth, Radius, Radius);
	BlobShadow->SetRelativeLocation(
		FVector(0.0f, 0.0f, Settings.SpritePivotZ - ProjectionDepth + TopMargin));
	BlobShadow->SetVisibility(true);
	BlobShadow->MarkRenderStateDirty();
}

void UNUSpriteVisualComponent::SetHorizontalFlip(bool bInFlip)
{
	// 실제 반전은 UpdateBillboard가 Yaw에 180도를 더해 처리함, 여기서는 상태만 변경
	bHorizontalFlip = bInFlip;
}

bool UNUSpriteVisualComponent::ApplyNaiaAnimBlueprint(int32 NaiaId)
{
	if (!IsValid(Animation))
	{
		return false;
	}

	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UNUNaiaDataSubsystem* Subsystem = GI ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const FNUNaiaDefinition* Definition =
		IsValid(Subsystem) ? Subsystem->FindDefinitionById(NaiaId) : nullptr;

	// 정의가 없거나 값이 비면 프리셋 값을 그대로 둠 (지금은 테이블이 비어 있어 대부분 여기)
	if (!Definition || Definition->AnimationClass.IsNull())
	{
		return false;
	}

	// 색, 머티리얼 재적용은 ApplyAnimBlueprintClass 에서 처리함
	if (!ApplyAnimBlueprintClass(Definition->AnimationClass))
	{
		// 경로가 틀린 경우가 대부분, BP 클래스는 끝에 _C 가 붙어야 함
		UE_LOG(LogTemp, Error, TEXT("[NU] 나이아 애님 블루프린트 적용 실패 : NaiaId=%d"), NaiaId);
		return false;
	}
	return true;
}

UPaperFlipbookComponent* UNUSpriteVisualComponent::GetSprite() const
{
	return Sprite;
}

void UNUSpriteVisualComponent::UpdateBillboard()
{
	if (!IsValid(Sprite)) return;

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor)) return;

	// Remote는 자기 카메라가 없음, 지금 보고 있는 플레이어 카메라가 기준
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!IsValid(CameraManager)) return;

	const FRotator CameraRotation = CameraManager->GetCameraRotation();
	float Yaw = 0.0f;
	switch (Settings.BillboardMode)
	{
	case ENUBillboardMode::LookAtCamera:
		Yaw = (CameraManager->GetCameraLocation() - OwnerActor->GetActorLocation()).Rotation().Yaw;
		break;

	case ENUBillboardMode::CameraYaw:
	default:
		Yaw = CameraRotation.Yaw;
		break;
	}

	// 좌우 반전은 쿼드를 180도 돌려 뒷면을 보여주는 방식으로 처리함
	// 음수 스케일은 Paper2D 렌더에 반영되지 않아 적용이 안되는것처럼 보임
	// 머티리얼이 TwoSided라 뒷면도 그려짐
	const float FlipYaw = bHorizontalFlip ? 180.0f : 0.0f;

	Sprite->SetWorldRotation(FRotator(0.0f, Yaw + Settings.BillboardYawOffset + FlipYaw, 0.0f));

	// 판이 요만 돌아 수직으로 서 있어서, 내려다보는 카메라에서는 세로가 cos(피치)만큼 눌려 보인다.
	// 눌린 만큼 세로 스케일을 키워 도트를 그린 비율 그대로 돌려놓는다. 피벗이 발이라 발 위치는 안 변한다.
	// 극단적인 피치로 스케일이 튀지 않게 2배(피치 60도)까지만 편다
	const float Stretch = FMath::Min(1.0f / FMath::Max(FMath::Cos(FMath::DegreesToRadians(CameraRotation.Pitch)), KINDA_SMALL_NUMBER), 2.0f);
	if (!FMath::IsNearlyEqual(Stretch, AppliedBillboardStretch, 0.001f))
	{
		AppliedBillboardStretch = Stretch;
		FVector Scale = Settings.RelativeScale;
		Scale.Z *= Stretch;
		Sprite->SetRelativeScale3D(Scale);
	}
}

void UNUSpriteVisualComponent::SetLocomotion(EFieldFacing Facing, bool bMoving)
{
	if (bLocomotionApplied && LastFacing == Facing && bLastMoving == bMoving)
	{
		return;
	}

	bLocomotionApplied = true;
	LastFacing = Facing;
	bLastMoving = bMoving;
	ApplyBackViewPartTune();

	if (!IsValid(Animation)) return;

	if (UNUPaperZDAnimInstance* AnimInstance =
		Cast<UNUPaperZDAnimInstance>(Animation->GetOrCreateAnimInstance()))
	{
		AnimInstance->SetLocomotionState(bMoving, FacingToDirection(Facing));
	}
}

void UNUSpriteVisualComponent::SetColorOverride(const FLinearColor& InColor)
{
	ColorOverride = InColor;
	ApplyColor();
}

void UNUSpriteVisualComponent::ClearColorOverride()
{
	ColorOverride.Reset();
	ApplyColor();
}

void UNUSpriteVisualComponent::SetDissolveAmount(float InAmount)
{
	DissolveAmount = FMath::Clamp(InAmount, 0.f, 1.f);

	ApplyColor();
}

void UNUSpriteVisualComponent::SetOpacity(float InOpacity)
{
	Opacity = FMath::Clamp(InOpacity, 0.f, 1.f);

	ApplyColor();
}

void UNUSpriteVisualComponent::SetSizeScale(float InScale)
{
	Settings.RelativeScale = FVector(FMath::Max(InScale, 0.01f));
	if (IsValid(Sprite))
	{
		FVector Scale = Settings.RelativeScale;
		Scale.Z *= AppliedBillboardStretch;
		Sprite->SetRelativeScale3D(Scale);
	}
	RefreshBlobShadow();
}

void UNUSpriteVisualComponent::OverrideMaterial(UMaterialInterface* InMaterial)
{
	if (!InMaterial)
	{
		// 덮어쓴 것을 지워 스프라이트 애셋 것으로 돌린 뒤, 설정 머티리얼이 있으면 RefreshAppearance 가 다시 넣음
		// RefreshAppearance 는 설정 머티리얼이 있을 때만 머티리얼을 건드려서, 지우지 않으면 설정이 빈 쪽은 덮어쓴 것이 남음
		if (IsValid(Sprite))
		{
			Sprite->SetMaterial(0, nullptr);
		}

		for (const TObjectPtr<UPaperFlipbookComponent>& Layer : LayerComponents)
		{
			if (UPaperFlipbookComponent* Component = Layer.Get())
			{
				Component->SetMaterial(0, nullptr);
			}
		}

		RefreshAppearance();
		if (bHairTinted)
		{
			ApplyHairTintMaterial(); // 덮어쓴 것을 걷으면서 머리 레이어의 머티리얼도 지워졌다
		}
		return;
	}

	if (IsValid(Sprite))
	{
		Sprite->SetMaterial(0, InMaterial);
	}

	// 레이어도 같이 덮어야 함, 레이어 등록부가 본체 머티리얼을 레이어까지 꽂는 것과 같은 이유
	for (const TObjectPtr<UPaperFlipbookComponent>& Layer : LayerComponents)
	{
		if (UPaperFlipbookComponent* Component = Layer.Get())
		{
			Component->SetMaterial(0, InMaterial);
		}
	}
}

void UNUSpriteVisualComponent::ApplyColor()
{
	if (!IsValid(Sprite))
	{
		return;
	}

	FLinearColor Color = ColorOverride.Get(Settings.Color);

	// 알파는 디졸브와 불투명도(깜빡임) 축의 곱이다
	// 지금 쓰이는 색은 전부 알파 1이라 디졸브가 0 · 불투명도가 1 이면 변화 X
	Color.A = (1.f - DissolveAmount) * Opacity;

	Sprite->SetSpriteColor(Color);

	// 레이어에도 같이 걸어야 함
	// 장비까지 같이 처리해야하기 때문
	// 물들인 머리는 정점 색이 곧 머리 색이라 그 레이어에만 색을 곱해 보낸다 (알파는 같다)
	const int32 HairLayer = bHairTinted ? LayerIndices.IndexOfByKey(NUSpriteLayer::Hair) : INDEX_NONE;
	for (int32 Index = 0; Index < LayerComponents.Num(); ++Index)
	{
		if (UPaperFlipbookComponent* Component = LayerComponents[Index].Get())
		{
			Component->SetSpriteColor(Index == HairLayer
				? FLinearColor(Color.R * HairTint.R, Color.G * HairTint.G, Color.B * HairTint.B, Color.A)
				: Color);
		}
	}
}

FVector2D UNUSpriteVisualComponent::FacingToDirection(EFieldFacing Facing)
{
	switch (Facing)
	{
	case EFieldFacing::Up:    
		return FVector2D(0.0f, 1.0f);
		
	case EFieldFacing::Right:
		return FVector2D(1.0f, 0.0f);
		
	case EFieldFacing::Left: 
		return FVector2D(-1.0f, 0.0f);
		
	case EFieldFacing::Down:
	default:                  
		return FVector2D(0.0f, -1.0f);
	}
}
