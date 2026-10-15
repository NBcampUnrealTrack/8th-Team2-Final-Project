#include "Field/Net/FieldRemoteCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Field/Net/FieldRemoteSpirit.h"
#include "Field/NUSpriteVisualComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "PaperFlipbookComponent.h"
#include "PaperZDAnimationComponent.h"
#include "UI/NUNameplateComponent.h"
#include "UI/NUNameplateWidget.h"
#include "Util/NUCollisionChannel.h"

namespace
{
	// 내 캐릭터의 MaxWalkSpeed와 **같은 값이어야 한다** (다르면 원격이 뒤처지거나 앞선다).
	constexpr float MOVE_SPEED_CMPS = 300.0f;
	constexpr float ARRIVE_CM = 5.0f;        // 이만큼 가까우면 도착
	constexpr float HARD_SNAP_CM = 400.0f;   // 이보다 멀면 걸어가지 않고 순간이동
	constexpr float CAPSULE_HALF_HEIGHT = 48.0f; // 서버 좌표는 캡슐 중심 기준

	// 원격 캐릭터 전용 전투중 톤다운
	// 기본 색은 NUSpriteVisuals::MakeFieldCharacter()에 있다
	const FLinearColor CHARACTER_COLOR_BATTLE(0.45f, 0.45f, 0.5f, 1.0f);
}

AFieldRemoteCharacter::AFieldRemoteCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// gRPC가 전송 계층이다 — 언리얼 내장 리플리케이션은 쓰지 않는다
	bReplicates = false;
	SetReplicateMovement(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(RootComponent);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);            // 내 캐릭터를 밀면 안 된다

	// 커마 파츠 --- 위치는 AFieldPlayerCharacter와 같다 (몸과 같은 캔버스·피벗이라 Z 0,
	// Y는 겹침 순서용 1유닛 간격 --- 얼굴 1, 머리 2로 머리가 앞). 무엇을 그릴지는 SetAppearance가 정한다
	HairSprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("HairSprite"));
	HairSprite->SetupAttachment(Sprite);
	HairSprite->SetRelativeLocation(FVector(0.0f, 2.0f, 0.0f));
	HairSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HairSprite->SetVisibility(false);

	FaceSprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("FaceSprite"));
	FaceSprite->SetupAttachment(Sprite);
	FaceSprite->SetRelativeLocation(FVector(0.0f, 1.0f, 0.0f));
	FaceSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FaceSprite->SetVisibility(false);

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeFieldCharacter());
	// 레이어 정의는 NUSpriteVisuals::GetCharacterLayers()가 유일한 출처다 ---
	// 장비 레이어는 컴포넌트가 없어 건너뛰고 커마 파츠 둘만 등록한다
	for (const FNUSpriteLayerDef& Def : NUSpriteVisuals::GetCharacterLayers())
	{
		UPaperFlipbookComponent* Component =
			Def.Layer == NUSpriteLayer::Hair ? HairSprite :
			Def.Layer == NUSpriteLayer::Face ? FaceSprite : nullptr;
		if (Component != nullptr)
		{
			SpriteVisual->RegisterLayerRenderComponent(Component, Def.Layer, Def.BounceScale, Def.FrameDelay);
		}
	}

	SelectCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("SelectCollision"));
	SelectCollision->SetupAttachment(RootComponent);
	SelectCollision->InitCapsuleSize(24.f, CAPSULE_HALF_HEIGHT);
	SelectCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SelectCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	SelectCollision->SetCollisionResponseToChannel(NUCollisionChannel::PlayerSelectionTrace, ECR_Block);

	// 머리 위 이름표 --- 로컬 캐릭터(AFieldPlayerCharacter)와 같은 방식·같은 높이
	NameplateWidget = CreateDefaultSubobject<UNUNameplateComponent>(TEXT("NameplateWidget"));
	NameplateWidget->SetupAttachment(RootComponent);
	// 빌보드 세로 눌림 보정(1/cos 카메라 피치)만큼 머리 끝이 조금 올라간다 --- 지금 피치 -16에서 약 55.
	// 피치를 크게 낮추면(-30 등) 머리가 66까지 오므로 그때는 같이 올릴 것
	NameplateWidget->SetRelativeLocation(FVector(0.f, 0.f, 62.f));
	NameplateWidget->SetWidgetSpace(EWidgetSpace::Screen);
	NameplateWidget->SetDrawSize(FVector2D(160.f, 20.f));
	NameplateWidget->SetWidgetClass(UNUNameplateWidget::StaticClass());
	NameplateWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}


void AFieldRemoteCharacter::ApplyServerState(const FVector& InTarget, EFieldFacing InFacing, EFieldActivity InActivity)
{
	// 전투 중이면 위치를 갱신하지 않는다 — 남들 눈엔 그 자리에 서 있어야 한다
	if (InActivity != EFieldActivity::Battle)
	{
		TargetLocation = InTarget;
	}
	Facing = InFacing;
	SetActivity(InActivity);
}

void AFieldRemoteCharacter::TeleportToServerState(const FVector& InLocation, EFieldFacing InFacing, EFieldActivity InActivity)
{
	TargetLocation = InLocation;
	Facing = InFacing;
	SetActivity(InActivity);
	SetActorLocation(InLocation);

	// 애니메이션 상태는 SpriteVisual이 들고 있고 Tick에서 매번 넘기므로
	// 여기서 "이미 적용됨"으로 표시할 필요가 없다
}

void AFieldRemoteCharacter::SetActivity(EFieldActivity InActivity)
{
	if (Activity == InActivity)
	{
		return;
	}
	Activity = InActivity;

	if (Activity == EFieldActivity::Battle)
	{
		SpriteVisual->SetColorOverride(CHARACTER_COLOR_BATTLE);
	}
	else
	{
		SpriteVisual->ClearColorOverride();
	}

	// 정령도 주인과 같이 톤다운
	if (IsValid(Spirit))
	{
		Spirit->SetActivity(Activity);
	}
}

void AFieldRemoteCharacter::SetSpirit(int32 InSpiritId)
{
	if (SpiritId == InSpiritId)
	{
		return;
	}
	SpiritId = InSpiritId;

	// 0 = 정령 없음
	if (SpiritId == 0)
	{
		if (IsValid(Spirit))
		{
			Spirit->Destroy();
		}
		Spirit = nullptr;
		return;
	}

	if (!IsValid(Spirit))
	{
		UWorld* World = GetWorld();
		if (World == nullptr) return;

		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Spirit = World->SpawnActor<AFieldRemoteSpirit>(
			AFieldRemoteSpirit::StaticClass(), GetActorLocation(), FRotator::ZeroRotator, Params);

		if (!IsValid(Spirit)) return;

		Spirit->SetFollowTarget(this);
		Spirit->SetActivity(Activity);	// 새로 생길땐 플레이어 Activity를 따라가게
	}

	// 이미 있으면 종류만 바뀐 것 (정령 교체)
	Spirit->SetSpiritId(SpiritId);
}

void AFieldRemoteCharacter::SetAppearance(int32 InRaceId, int32 InGender, int32 InHairId, int32 InFaceId, int32 InJobId)
{
	if (RaceId == InRaceId && Gender == InGender && HairId == InHairId && FaceId == InFaceId && JobId == InJobId)
	{
		return;
	}
	RaceId = InRaceId;
	Gender = InGender;
	HairId = InHairId;
	FaceId = InFaceId;
	JobId = InJobId;

	// 커마 베이스 몸 스킨(레이어 0)도 이 안에서 같이 입혀진다. 직업은 SVR_Field_Appeared job_id(9/14부터)
	SpriteVisual->RefreshAppearanceSkins(RaceId, Gender, HairId, FaceId, JobId);
}

FText AFieldRemoteCharacter::GetDisplayName() const
{
	// 캐릭터를 선택하지 않으면 서버가 빈 문자열을 보냄
	// 이 경우엔 PlayerId가 보이도록 처리
	if (CharacterName.IsEmpty())
	{
		return FText::FromString(FString::Printf(TEXT("Player %lld"), PlayerId));
	}

	return FText::FromString(CharacterName);
}

void AFieldRemoteCharacter::SetCharacterName(const FString& InCharacterName)
{
	CharacterName = InCharacterName;

	// 이름표 갱신 --- 이름이 빈 대상은 GetDisplayName이 "Player N"으로 채워 준다
	if (UNUNameplateWidget* Nameplate = Cast<UNUNameplateWidget>(
		NameplateWidget != nullptr ? NameplateWidget->GetUserWidgetObject() : nullptr))
	{
		Nameplate->SetName(GetDisplayName());
	}
}

void AFieldRemoteCharacter::Destroyed()
{
	// 주인이 사라지면 정령도 같이 사라진다
	if (IsValid(Spirit))
	{
		Spirit->Destroy();
	}
	Spirit = nullptr;

	Super::Destroyed();
}

void AFieldRemoteCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Current = GetActorLocation();
	const FVector ToTarget = TargetLocation - Current;
	const float Distance = ToTarget.Size();

	bool bMoving = false;

	if (Distance > HARD_SNAP_CM)
	{
		SetActorLocation(TargetLocation); // 너무 멀다 — 걸어가면 부자연스럽다
	}
	else if (Distance > ARRIVE_CM)
	{
		// 타겟 추격: 목표까지 내 캐릭터와 같은 속도로 걸어간다
		const float Step = MOVE_SPEED_CMPS * DeltaSeconds;
		SetActorLocation(Step >= Distance ? TargetLocation : Current + ToTarget / Distance * Step);
		bMoving = true;
	}

	SpriteVisual->UpdateBillboard();

	// 방향은 서버가 준 값을 그대로 사용
	SpriteVisual->SetLocomotion(Facing, bMoving);
}
