#include "Field/NPC/NUNPCBase.h"

#include "Dialogue/Component/NUDialogueProviderComponent.h"
#include "Dialogue/Data/NUDialogueDefinition.h"
#include "Field/NPC/Data/NUNPCDataSubsystem.h"
#include "Field/NPC/Data/NUNPCDefinition.h"
#include "Field/NPC/Data/NUNPCVisualSettings.h"
#include "Field/NUSpriteVisualComponent.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Interaction/Component/NUInteractionRouterComponent.h"
#include "PlayerController/NUPlayerController.h"
#include "Quest/Component/NUQuestProviderComponent.h"

#include "Components/SphereComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUNPCInteraction, Log, All);

ANUNPCBase::ANUNPCBase()
{
	PrimaryActorTick.bCanEverTick = false;

	InteractionVolume = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionVolume"));
	SetRootComponent(InteractionVolume);
	InteractionVolume->InitSphereRadius(60.0f);
	InteractionVolume->bDrawOnlyIfSelected = true;
	InteractionVolume->SetCollisionProfileName(TEXT("NU_Interactable"));
	InteractionVolume->SetGenerateOverlapEvents(true);

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(InteractionVolume);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 캐릭터 머티리얼(M_NUCharacterSprite)은 NPC 뒤 가림을 지터 없는 CustomDepth로 판정한다 --- 씬 깊이로 하면 윤곽이 일렁인다
	Sprite->SetRenderCustomDepth(true);

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeNPC());

	InteractionRouter = CreateDefaultSubobject<UNUInteractionRouterComponent>(TEXT("InteractionRouter"));
	// 기존 Blueprint 인스턴스의 컴포넌트별 대화 설정을 보존하기 위해 내부 Subobject 이름은 유지한다.
	DialogueProvider = CreateDefaultSubobject<UNUDialogueProviderComponent>(TEXT("DialogueInteraction"));
	CreateDefaultSubobject<UNUInteriorParticipantComponent>(TEXT("InteriorParticipant"));
}

void ANUNPCBase::BeginPlay()
{
	const bool bDefinitionApplied = ApplyNPCDefinition();

	Super::BeginPlay();

	// SpriteVisual의 BeginPlay가 기본 애님 BP를 먼저 적용한 뒤 NPC 정의가 최종값을 덮어쓴다.
	if (bDefinitionApplied && IsValid(SpriteVisual) && !ResolvedAnimationClass.IsNull())
	{
		SpriteVisual->ApplyAnimBlueprintClass(ResolvedAnimationClass);
	}

	SpriteVisual->EnableAutomaticBillboardUpdates(BillboardUpdateInterval);
	ApplyVisualTuning();
}

void ANUNPCBase::ApplyVisualTuning()
{
	const UNUNPCVisualSettings* Settings = GetDefault<UNUNPCVisualSettings>();
	if (Settings == nullptr || !IsValid(Sprite) || !IsValid(SpriteVisual))
	{
		return;
	}

	// 표의 키: NpcId, 아직 정의로 옮기지 않은 옛 BP 는 클래스 이름 끝의 숫자 (BP_NPC_4017_C → 4017)
	int32 Key = NpcId;
	if (Key <= 0)
	{
		FString Digits;
		for (const TCHAR Char : GetClass()->GetName())
		{
			if (FChar::IsDigit(Char))
			{
				Digits.AppendChar(Char);
			}
			else if (!Digits.IsEmpty())
			{
				break;
			}
		}
		Key = Digits.IsEmpty() ? 0 : FCString::Atoi(*Digits);
	}
	const FNUNPCVisualTuning* Found = Settings->PerNpc.Find(Key);
	const FNUNPCVisualTuning& Tuning = Found != nullptr ? *Found : Settings->Default;

	if (!FMath::IsNearlyEqual(Tuning.Scale, 1.f))
	{
		SpriteVisual->SetSizeScale(Tuning.Scale);
	}

	// 캐릭터 스프라이트 머티리얼을 쓰는 NPC 만 (옛 조명 머티리얼 NPC 는 파라미터가 달라 건드리지 않는다)
	UMaterialInterface* Current = Sprite->GetMaterial(0);
	const UMaterialInterface* Character = Settings->CharacterMaterial.LoadSynchronous();
	if (Current == nullptr || Character == nullptr || Current->GetMaterial() != Character->GetMaterial())
	{
		return;
	}
	UMaterialInstanceDynamic* Tone = UMaterialInstanceDynamic::Create(Current, this);
	Tone->SetScalarParameterValue(TEXT("ToneGamma"), Tuning.ToneGamma);
	Tone->SetScalarParameterValue(TEXT("ToneGain"), Tuning.ToneGain);
	if (Settings->PixelAA >= 0.f)
	{
		Tone->SetScalarParameterValue(TEXT("NUA_PixelAA"), Settings->PixelAA);
	}
	SpriteVisual->OverrideMaterial(Tone);
}

bool ANUNPCBase::CanInteract_Implementation(AActor* Interactor) const
{
	return bInteractionEnabled
		&& IsValid(Interactor)
		&& IsValid(InteractionRouter);
}

bool ANUNPCBase::Interact_Implementation(AActor* Interactor)
{
	if (!INUInteractableInterface::Execute_CanInteract(this, Interactor))
	{
		return false;
	}

	if (HasAuthority())
	{
		FNUInteractionContext Context;
		Context.Interactor = Interactor;
		Context.Target = this;

		TArray<FNUInteractionOption> Options;
		const ENUInteractionRouteResult RouteResult = InteractionRouter->RouteInteraction(
			Context,
			Options,
			ENUInteractionRoutePolicy::AlwaysShowSelection);
		if (RouteResult == ENUInteractionRouteResult::Executed)
		{
			return true;
		}
		if (RouteResult == ENUInteractionRouteResult::SelectionRequired)
		{
			UNUDialogueDefinition* OpeningDialogue = IsValid(DialogueProvider)
				? DialogueProvider->GetInteractionPromptDialogueDefinition()
				: nullptr;

			const APawn* InteractorPawn = Cast<APawn>(Interactor);
			ANUPlayerController* PlayerController = InteractorPawn != nullptr ? Cast<ANUPlayerController>(InteractorPawn->GetController()) : nullptr;
			if (IsValid(OpeningDialogue))
			{
				FString ValidationError;
				if (!OpeningDialogue->ValidateDefinition(ValidationError))
				{
					UE_LOG(LogNUNPCInteraction, Warning, TEXT("유효하지 않은 상호작용 질문 대사는 제외하고 선택지만 표시합니다. Actor=%s Error=%s"), *GetNameSafe(this), *ValidationError);
					OpeningDialogue = nullptr;
				}
			}

			if (IsValid(PlayerController))
			{
				PlayerController->ClientOpenInteractionConversation(OpeningDialogue, this, Options);
				return true;
			}

			UE_LOG(LogNUNPCInteraction, Warning, TEXT("로컬 PlayerController가 없어 NPC 대화창을 열 수 없습니다. Actor=%s"), *GetNameSafe(this));
		}
	}

	return false;
}

UNUQuestProviderComponent* ANUNPCBase::GetQuestProviderComponent() const
{
	return FindComponentByClass<UNUQuestProviderComponent>();
}

UNUInteractionRouterComponent* ANUNPCBase::GetInteractionRouterComponent() const
{
	return InteractionRouter;
}

UNUDialogueProviderComponent* ANUNPCBase::GetDialogueProviderComponent() const
{
	return DialogueProvider;
}

int32 ANUNPCBase::GetNpcId() const
{
	return NpcId;
}

FText ANUNPCBase::GetNPCDisplayName() const
{
	return ResolvedDisplayName;
}

bool ANUNPCBase::ApplyNPCDefinition()
{
	ResolvedDisplayName = FText::GetEmpty();
	ResolvedAnimationClass.Reset();

	// 0은 아직 NPC 정의로 이관하지 않은 기존 Blueprint를 의미한다.
	if (NpcId <= 0)
	{
		return false;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UNUNPCDataSubsystem* NPCDataSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UNUNPCDataSubsystem>()
		: nullptr;
	if (!IsValid(NPCDataSubsystem))
	{
		UE_LOG(LogNUNPCInteraction, Warning, TEXT("NPC 데이터 서브시스템을 찾을 수 없습니다. Actor=%s NpcId=%d"),
			*GetNameSafe(this),
			NpcId);
		return false;
	}

	const FNUNPCDefinition* Definition = NPCDataSubsystem->FindDefinition(NpcId);
	if (Definition == nullptr)
	{
		UE_LOG(LogNUNPCInteraction, Warning, TEXT("NpcId에 해당하는 정의가 없습니다. Actor=%s NpcId=%d"),
			*GetNameSafe(this),
			NpcId);
		return false;
	}

	ResolvedDisplayName = Definition->DisplayName;
	ResolvedAnimationClass = Definition->AnimationClass;

	// 기존 Blueprint에 직접 지정한 대화는 보존하고, 비어 있을 때만 표의 기본 대화를 채운다.
	if (IsValid(DialogueProvider)
		&& !IsValid(DialogueProvider->DefaultDialogueDefinition)
		&& !Definition->DefaultDialogue.IsNull())
	{
		DialogueProvider->DefaultDialogueDefinition = Definition->DefaultDialogue.LoadSynchronous();
	}

	// 말을 걸 때 먼저 나오는 첫 대사도 같은 방식으로 채운다.
	if (IsValid(DialogueProvider)
		&& !IsValid(DialogueProvider->MultipleChoicePromptDialogueDefinition)
		&& !Definition->OpeningDialogue.IsNull())
	{
		DialogueProvider->MultipleChoicePromptDialogueDefinition = Definition->OpeningDialogue.LoadSynchronous();
	}

	// PIE 중 Blueprint에서 다시 적용할 때는 이미 컴포넌트 BeginPlay가 끝났으므로 즉시 교체한다.
	if (HasActorBegunPlay() && IsValid(SpriteVisual) && !ResolvedAnimationClass.IsNull())
	{
		SpriteVisual->ApplyAnimBlueprintClass(ResolvedAnimationClass);
	}

	UE_LOG(LogNUNPCInteraction, Log, TEXT("NPC 정의 적용 완료. Actor=%s NpcId=%d Name=%s"),
		*GetNameSafe(this),
		NpcId,
		*ResolvedDisplayName.ToString());
	return true;
}
