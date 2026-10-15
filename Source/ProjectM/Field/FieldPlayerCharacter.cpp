#include "Field/FieldPlayerCharacter.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "AnimSequences/Skins/PaperZDAnimationSkin.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryComponent.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimInstance.h"
#include "PaperZDAnimationComponent.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "Recovery/Component/NURecoveryComponent.h"
#include "CharacterState/NUSpiritState.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "Field/Encounter/NURandomEncounterComponent.h"
#include "Field/Encounter/NUSymbolEncounterComponent.h"
#include "Interaction/Component/NUInteractorComponent.h"
#include "Field/NUCameraOcclusionFadeComponent.h"
#include "Field/NUSpriteVisualComponent.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "Components/WidgetComponent.h"
#include "NUGAS/NUGameplayTags.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUNameplateComponent.h"
#include "UI/NUNameplateWidget.h"

AFieldPlayerCharacter::AFieldPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// bReplicates는 끄지 않음, 폰이 소유 클라에 복제되지 않으면 조작할 캐릭터가 없고,
	// 파티 전투도 불가능해짐, 남에게 안 보이게만 하면 충분
	bReplicates = true;
	SetReplicateMovement(true);
	
	// 필드 위치는 AFieldRemoteCharacter로 그려지므로 복제 설정을 소유자만 관련있게 제한함
	bOnlyRelevantToOwner = true;
	
	GetCapsuleComponent()->InitCapsuleSize(24.0f, 48.0f);

	// The capsule stays unrotated; only the sprite billboards.
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2400.0f;

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(GetCapsuleComponent());
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 장비 겹침 렌더러 — 몸과 같은 캔버스의 장비 그림을 그대로 포개는 방식.
	// Y는 카메라 쪽(빌보드 기준 +Y) 깊이라 겹치는 순서를 만든다 (8/21 아트 확인):
	//   신발(뒤) < 상의 < 하의 < 장갑(팔) < 어깨(앞).
	//   뒷모습에서는 팔과 하의만 순서가 뒤집힌다 --- UpdateLayerDepthForFacing이 처리한다.
	//   깊이 간격은 1유닛 --- 더 좁히면 레이어끼리 파고들어 보인다.
	// Z는 파츠 그림의 몸 위치를 몸통에 맞춘 값 (nu.Equip.Tune으로 다시 재면 됨).
	// 바운스는 갑옷·장갑만 몸을 따라가고 하의·신발은 0(다리는 땅에 붙어 있다).
	//   팔은 몸통보다 한 박자 늦어 장갑에만 1프레임 지연을 준다
	EquipSprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("EquipSprite"));
	EquipSprite->SetupAttachment(Sprite);
	EquipSprite->SetRelativeLocation(FVector(0.0f, 2.0f, -5.0f));
	EquipSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquipSprite->SetVisibility(false); // 장착 전에는 숨김

	EquipSpriteGloves = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("EquipSpriteGloves"));
	EquipSpriteGloves->SetupAttachment(Sprite);
	EquipSpriteGloves->SetRelativeLocation(FVector(0.0f, 4.0f, -5.0f));
	EquipSpriteGloves->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquipSpriteGloves->SetVisibility(false);

	EquipSpriteBoots = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("EquipSpriteBoots"));
	EquipSpriteBoots->SetupAttachment(Sprite);
	EquipSpriteBoots->SetRelativeLocation(FVector(0.0f, 1.0f, -6.0f));
	EquipSpriteBoots->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquipSpriteBoots->SetVisibility(false);

	EquipSpriteLegs = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("EquipSpriteLegs"));
	EquipSpriteLegs->SetupAttachment(Sprite);
	EquipSpriteLegs->SetRelativeLocation(FVector(0.0f, 3.0f, -5.0f));
	EquipSpriteLegs->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquipSpriteLegs->SetVisibility(false);

	EquipSpriteShoulder = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("EquipSpriteShoulder"));
	EquipSpriteShoulder->SetupAttachment(Sprite);
	EquipSpriteShoulder->SetRelativeLocation(FVector(0.0f, 5.0f, -5.0f));
	EquipSpriteShoulder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EquipSpriteShoulder->SetVisibility(false);

	// 커스터마이징 파츠.
	// Y는 겹치는 것(몸 0, 서로)과만 1유닛 간격을 지키고 최대한 몸에 붙인다 --- 깊이가 클수록
	// 캐릭터가 화면 중앙을 벗어날 때(이동 중 카메라 지연) 파츠가 옆으로 밀려 보인다 (시차).
	// 빌보드가 로컬 +Y를 카메라 쪽으로 돌리므로 Y가 클수록 앞이다 => 얼굴 1, 머리 2.
	// 새 남캐는 머리가 얼굴(귀·이마)을 덮어야 맞다 --- 얼굴이 앞이면 법사 머리에서 귀가 튀어나온다 (9/12).
	// 장비(1~5)와는 그림이 안 겹쳐 깊이가 같아도 무방하다.
	// Z는 0 --- 파츠가 몸과 같은 캔버스(69x96)·같은 피벗(Bottom_Center)에 그려져 있어
	// 원점만 맞추면 그림이 맞는다. 장비 레이어의 -5는 옛 몸 기준 보정이라 따라가면 안 된다
	HairSprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("HairSprite"));
	HairSprite->SetupAttachment(Sprite);
	HairSprite->SetRelativeLocation(FVector(0.0f, 2.0f, 0.0f));
	HairSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HairSprite->SetVisibility(false); // 파츠가 붙기 전에는 숨김

	FaceSprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("FaceSprite"));
	FaceSprite->SetupAttachment(Sprite);
	FaceSprite->SetRelativeLocation(FVector(0.0f, 1.0f, 0.0f));
	FaceSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FaceSprite->SetVisibility(false);

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	// 머티리얼·그림자·애님BP·빌보드·로코모션은 이 컴포넌트가 담당한다.
	// 값은 NUSpriteVisuals::MakeFieldCharacter()가 유일한 출처 —
	// AFieldRemoteCharacter가 같은 팩토리를 쓰므로 표현이 갈라지지 않는다
	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeFieldCharacter());
	// 레이어 정의는 NUSpriteVisuals::GetCharacterLayers()가 유일한 출처다 ---
	// 액터마다 숫자를 적으면 한쪽만 고쳐져 어긋난다 (실제로 그런 적이 있다)
	const TArray<UPaperFlipbookComponent*> LayerComponents = {
		EquipSprite, EquipSpriteGloves, EquipSpriteBoots, EquipSpriteLegs, EquipSpriteShoulder,
		HairSprite, FaceSprite };
	const TArray<FNUSpriteLayerDef>& LayerDefs = NUSpriteVisuals::GetCharacterLayers();
	for (int32 i = 0; i < LayerDefs.Num() && i < LayerComponents.Num(); ++i)
	{
		SpriteVisual->RegisterLayerRenderComponent(
			LayerComponents[i], LayerDefs[i].Layer, LayerDefs[i].BounceScale, LayerDefs[i].FrameDelay);
	}

	// 카메라 부착 : Capsule → CameraAnchor → SpringArm → Camera
	CameraAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("CameraAnchor"));
	CameraAnchor->SetupAttachment(RootComponent);
	CameraAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	
	// HD-2D quarter view rig (§3.1): low FOV perspective, pitched down.
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(CameraAnchor);
	SpringArm->SetRelativeLocation(FVector(0.f, 0.f, 24.f));
	SpringArm->SetRelativeRotation(FRotator(-5.0f, 0.0f, 0.0f));
	SpringArm->TargetArmLength = 1000.0f;
	// 시선 높이는 0이다 --- 레퍼런스(옥토패스)도 캐릭터가 화면 정중앙이다.
	// 배경을 더 보이게 하려면 시선을 올리는 게 아니라 카메라를 낮춘다(피치를 줄인다)
	SpringArm->bDoCollisionTest = false;
	SpringArm->bUsePawnControlRotation = false;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 5.0f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm);
	Camera->SetFieldOfView(30.0f);

	// 머리 위 이름표 --- 전투 체력바와 같은 스크린 스페이스 위젯. 모양은 프로젝트 세팅(NU UI > NameplateClass)의 WBP, 비면 코드 폴백
	NameplateWidget = CreateDefaultSubobject<UNUNameplateComponent>(TEXT("NameplateWidget"));
	NameplateWidget->SetupAttachment(GetCapsuleComponent());
	// 빌보드 세로 눌림 보정(1/cos 카메라 피치)만큼 머리 끝이 조금 올라간다 --- 지금 피치 -16에서 약 55.
	// 피치를 크게 낮추면(-30 등) 머리가 66까지 오므로 그때는 같이 올릴 것
	NameplateWidget->SetRelativeLocation(FVector(0.f, 0.f, 62.f));
	NameplateWidget->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateWidget->SetDrawSize(FVector2D(160.f, 20.f));
	NameplateWidget->SetWidgetClass(UNUNameplateWidget::StaticClass());
	NameplateWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RandomEncounterComponent = CreateDefaultSubobject<UNURandomEncounterComponent>(TEXT("RandomEncounterComponent"));
	SymbolEncounterComponent = CreateDefaultSubobject<UNUSymbolEncounterComponent>(TEXT("SymbolEncounterComponent"));
	InteractionComponent = CreateDefaultSubobject<UNUInteractorComponent>(TEXT("InteractionComponent"));
	CameraOcclusionFadeComponent = CreateDefaultSubobject<UNUCameraOcclusionFadeComponent>(TEXT("CameraOcclusionFadeComponent"));
	CreateDefaultSubobject<UNUInteriorParticipantComponent>(TEXT("InteriorParticipant"));
}

void AFieldPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 대상이 가진 InteractionVolume을 실제 범위로 사용하고, 플레이어 Capsule은 진입 여부만 제공한다.
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);
	InteractionComponent->InitializeDetectionComponent(GetCapsuleComponent());

	// 스프라이트 표현 적용은 SpriteVisual의 BeginPlay가 담당한다

	// PlayerState가 BeginPlay보다 먼저 도착하는 경로가 있어 여기서도 한 번 맞춘다
	RefreshEquipmentSkin();

	// 커마는 고른 캐릭터에 붙은 값이라 여기서 한 번. 직업 몸만 전직 때 OnJobChanged로 다시 입힌다
	RefreshAppearanceSkin();

	// 머리 위 이름표 --- 캐릭터 닉네임 (정보창과 같은 출처, 미선택이면 비워 둔다)
	if (UNUNameplateWidget* Nameplate = Cast<UNUNameplateWidget>(
		NameplateWidget != nullptr ? NameplateWidget->GetUserWidgetObject() : nullptr))
	{
		const UGameInstance* GI = GetGameInstance();
		const UNUCharacterManager* Manager = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
		FNUCharacterSummary Selected;
		if (Manager != nullptr && Manager->GetSelectedCharacter(Selected) && !Selected.Name.IsEmpty())
		{
			Nameplate->SetName(FText::FromString(Selected.Name));
		}
	}
}

void AFieldPlayerCharacter::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);

	if (const ANUPlayerState* OldState = Cast<ANUPlayerState>(OldPlayerState))
	{
		if (UNUEquipmentComponent* OldEquipment = OldState->GetEquipmentComponent())
		{
			OldEquipment->OnEquipmentChanged.RemoveDynamic(this, &AFieldPlayerCharacter::HandleEquipmentChangedForVisual);
		}
		if (UNUJobComponent* OldJob = OldState->GetJobComponent())
		{
			OldJob->OnJobChanged.RemoveDynamic(this, &AFieldPlayerCharacter::RefreshAppearanceSkin);
		}
	}
	if (const ANUPlayerState* NewState = Cast<ANUPlayerState>(NewPlayerState))
	{
		if (UNUEquipmentComponent* Equipment = NewState->GetEquipmentComponent())
		{
			Equipment->OnEquipmentChanged.RemoveDynamic(this, &AFieldPlayerCharacter::HandleEquipmentChangedForVisual);
			Equipment->OnEquipmentChanged.AddDynamic(this, &AFieldPlayerCharacter::HandleEquipmentChangedForVisual);
		}
		// 전직하면 몸통을 그 직업 몸으로 --- 장비와 같은 방식 (9/12)
		if (UNUJobComponent* Job = NewState->GetJobComponent())
		{
			Job->OnJobChanged.RemoveDynamic(this, &AFieldPlayerCharacter::RefreshAppearanceSkin);
			Job->OnJobChanged.AddDynamic(this, &AFieldPlayerCharacter::RefreshAppearanceSkin);
		}
	}
	RefreshEquipmentSkin();
	RefreshAppearanceSkin();
}

void AFieldPlayerCharacter::HandleEquipmentChangedForVisual()
{
	RefreshEquipmentSkin();
}

void AFieldPlayerCharacter::RefreshAppearanceSkin()
{
	// 고른 캐릭터의 외형 번호는 매니저가 들고 있다 (GameInstance 수명이라 레벨을 넘어와도 남는다).
	// 머리·외모는 플레이 중에 안 바뀌지만 직업은 전직하면 바뀐다 --- 직업은 PlayerState의 직업 컴포넌트가 정본이고,
	// 아직 안 받았으면 목록에 있던 값(로그인 때 서버가 준 JobId)을 쓴다
	const UGameInstance* GI = GetGameInstance();
	const UNUCharacterManager* Manager = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	FNUCharacterSummary Selected;
	if (Manager == nullptr || !Manager->GetSelectedOrTestCharacter(Selected) || !IsValid(SpriteVisual))
	{
		return;
	}
	const ANUPlayerState* NUState = GetPlayerState<ANUPlayerState>();
	const UNUJobComponent* Job = NUState != nullptr ? NUState->GetJobComponent() : nullptr;
	const int32 JobId = Job != nullptr && Job->HasJob() ? Job->GetJobId() : Selected.JobId;
	SpriteVisual->RefreshAppearanceSkins(Selected.RaceId, Selected.Gender, Selected.HairId, Selected.FaceId, JobId);
}

void AFieldPlayerCharacter::RefreshEquipmentSkin()
{
	// 실제 처리는 공용 컴포넌트가 한다 --- 정보창 미리보기도 같은 함수를 쓴다
	const ANUPlayerState* NUState = GetPlayerState<ANUPlayerState>();
	if (NUState == nullptr || !IsValid(SpriteVisual))
	{
		return;
	}
	SpriteVisual->RefreshEquipmentSkins(NUState->GetEquipmentComponent(), NUState->GetInventoryComponent());
}

void AFieldPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IsValid(MoveAction))
		{
			Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFieldPlayerCharacter::HandleMove);
			// 키를 떼면 Triggered가 더 이상 발화하지 않아 LastMoveInput이 마지막 값으로 남는다.
			// 그러면 "이동이 끝났다"를 알 수 없어 위치 동기화가 정지 시점을 놓친다.
			Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AFieldPlayerCharacter::HandleMoveEnd);
			Input->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AFieldPlayerCharacter::HandleMoveEnd);
		}
		if (IsValid(InteractAction))
		{
			Input->BindAction(InteractAction, ETriggerEvent::Started, this, &AFieldPlayerCharacter::HandleInteract);
		}
	}
}

UAbilitySystemComponent* AFieldPlayerCharacter::GetAbilitySystemComponent() const
{
	ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
	if (!IsValid(NUPlayerState)) return nullptr;
	
	return NUPlayerState->GetAbilitySystemComponent();
}

// 서버에서 ASC 설정
void AFieldPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!HasAuthority()) return;
	
	ANUPlayerState* NUPlayerState = GetPlayerState<ANUPlayerState>();
	if (!IsValid(NUPlayerState)) return;
	
	// 초기화는 PlayerState의 BeginPlay로 넘어감
	// 폰이 있어야하는 것들만 남김
	NUPlayerState->SetCurrentAvatar(this);
	NUPlayerState->TryEnsureActiveSpiritFieldAvatar();
}

void AFieldPlayerCharacter::HandleMove(const FInputActionValue& Value)
{
	const UNURecoveryComponent* Recovery = GetPlayerState() != nullptr ? GetPlayerState()->FindComponentByClass<UNURecoveryComponent>() : nullptr;
	const UNUAbilitySystemComponent* RecoveryASC = Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponent());
	if (Recovery != nullptr && !Recovery->IsUsingExternalServer() && (Recovery->HasPendingDeath() || (RecoveryASC != nullptr && !RecoveryASC->IsAlive())))
	{
		LastMoveInput = FVector2D::ZeroVector;
		return;
	}

	// 전투 중에는 ANUPlayerController가 IMC_Field를 스택에서 빼므로
	// 여기까지 입력이 오지 않음
	const FVector2D Axis = Value.Get<FVector2D>();
	LastMoveInput = Axis;

	if (Axis.IsNearlyZero())
	{
		return;
	}

	// 카메라 기준 지면 평면 이동
	const float CameraYaw = Camera->GetComponentRotation().Yaw;
	const FRotator YawRot(0.0f, CameraYaw, 0.0f);
	const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Axis.Y * ForwardBackwardMovementScale);
	AddMovementInput(Right, Axis.X * LeftRightMovementScale);
}

void AFieldPlayerCharacter::HandleMoveEnd(const FInputActionValue& Value)
{
	// 이동 입력 종료 — 마지막 입력값을 지운다.
	// (Facing은 유지된다: 멈춘 캐릭터는 마지막으로 보던 방향을 그대로 본다)
	LastMoveInput = FVector2D::ZeroVector;
}

void AFieldPlayerCharacter::HandleInteract()
{
	const UNURecoveryComponent* Recovery = GetPlayerState() != nullptr ? GetPlayerState()->FindComponentByClass<UNURecoveryComponent>() : nullptr;
	const UNUAbilitySystemComponent* RecoveryASC = Cast<UNUAbilitySystemComponent>(GetAbilitySystemComponent());
	if (Recovery != nullptr && !Recovery->IsUsingExternalServer() && (Recovery->HasPendingDeath() || (RecoveryASC != nullptr && !RecoveryASC->IsAlive())))
	{
		LastMoveInput = FVector2D::ZeroVector;
		return;
	}

	const ANUPlayerController* PlayerController = Cast<ANUPlayerController>(GetController());
	if (IsValid(PlayerController) && PlayerController->IsGameInputBlocked())
	{
		return;
	}

	// Overlap 진입 시점의 대상을 그대로 믿지 않고 F 입력 순간 실제 거리를 다시 확인한다.
	if (!IsValid(InteractionComponent) || !IsValid(InteractionComponent->UpdateCurrentInteractable()))
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!IsValid(ASC)) 
	{
		return;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(NU::Ability::Field::Interact);

	// 대화·문·포탈·창 열림처럼 제 소리가 있는 대상이면 그 소리만 난다 (공용음은 뒤에 불린 소리에 자리를 내준다)
	UNUSFXSubsystem::PlayCommon(this, NU::SFX::Field::Interact);
	ASC->TryActivateAbilitiesByTag(AbilityTags);
}

void AFieldPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	SpriteVisual->UpdateBillboard();
	UpdateAnimation();
	UpdateLayerDepthForFacing();
}

void AFieldPlayerCharacter::UpdateLayerDepthForFacing()
{
	if (LayerDepthFacing == Facing)
	{
		return; // 방향이 그대로면 건드릴 것이 없다
	}
	LayerDepthFacing = Facing;

	// 뒤에서 보면 팔이 몸 뒤로 가고 하의가 앞으로 온다 --- 나머지 순서는 그대로 (8/21 아트 확인).
	// Y만 바꾼다: X와 Z는 UpdateLayerFrameOffsets가 매 프레임 맞추고 있어 겹치지 않는다
	const bool bBackView = Facing == EFieldFacing::Up;
	const auto SetDepth = [](UPaperFlipbookComponent* Component, float Depth)
	{
		if (IsValid(Component))
		{
			FVector Location = Component->GetRelativeLocation();
			Location.Y = Depth;
			Component->SetRelativeLocation(Location);
		}
	};
	SetDepth(EquipSpriteLegs, bBackView ? 4.0f : 3.0f);
	SetDepth(EquipSpriteGloves, bBackView ? 3.0f : 4.0f);

	// 뒷모습 하의는 그림이 조금 높이 붙어 있어 살짝 내린다 (레이어 4 = 하의)
	if (IsValid(SpriteVisual))
	{
		SpriteVisual->SetLayerTuneZ(4, bBackView ? -0.4f : 0.0f);
	}
}

void AFieldPlayerCharacter::UpdateAnimation()
{
	const bool bMoving = GetVelocity().SizeSquared2D() > 25.0f;

	if (bMoving && !LastMoveInput.IsNearlyZero())
	{
		// Screen-space input -> 4-direction facing.
		if (FMath::Abs(LastMoveInput.X) >= FMath::Abs(LastMoveInput.Y))
		{
			Facing = LastMoveInput.X >= 0.0f ? EFieldFacing::Right : EFieldFacing::Left;
		}
		else
		{
			Facing = LastMoveInput.Y >= 0.0f ? EFieldFacing::Up : EFieldFacing::Down;
		}
	}

	// 값이 바뀔 때만 실제로 적용된다
	SpriteVisual->SetLocomotion(Facing, bMoving);
}
