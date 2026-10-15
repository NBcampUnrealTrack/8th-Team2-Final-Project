// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/NUCharacterCreatePreview.h"

#include "CharacterSelect/Data/NUAppearanceDataSettings.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

ANUCharacterCreatePreview::ANUCharacterCreatePreview()
{
	PrimaryActorTick.bCanEverTick = false;

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	RootComponent = Sprite;

	// UI 위 캐릭터가 아니라 레벨 위 캐릭터다 --- 라이팅 무시하고 원색으로 보이게
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 몸 위에 겹치는 파츠. 카메라가 -Y쪽에 있으므로 살짝 앞(-Y)으로 빼야 몸에 가리지 않는다
	HairSprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("HairSprite"));
	HairSprite->SetupAttachment(Sprite);
	HairSprite->SetRelativeLocation(FVector(0.0f, -1.0f, 0.0f));
	HairSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FaceSprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("FaceSprite"));
	FaceSprite->SetupAttachment(Sprite);
	FaceSprite->SetRelativeLocation(FVector(0.0f, -0.5f, 0.0f));
	FaceSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 눌리는 범위. 스프라이트 충돌은 스프라이트 에셋이 쥐고 있어 믿을 수 없다 --- 상자를 직접 둔다.
	// Sprite(스케일 3)에 붙으므로 여기 숫자는 3배가 되어 대략 폭 90 x 높이 186이 된다
	ClickBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ClickBox"));
	ClickBox->SetupAttachment(Sprite);
	ClickBox->SetRelativeLocation(FVector(0.0f, 0.0f, 31.0f));
	ClickBox->SetBoxExtent(FVector(15.0f, 8.0f, 31.0f));
	ClickBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ClickBox->SetCollisionObjectType(ECC_WorldDynamic);
	ClickBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	ClickBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ClickBox->SetGenerateOverlapEvents(false);
}

void ANUCharacterCreatePreview::BeginPlay()
{
	Super::BeginPlay();
	// 처음부터 자기 종족·성별의 기본 모습으로 선다 (표에 없는 조합은 기본 외형)
	ResetAppearance();

	if (ClickBox != nullptr)
	{
		ClickBox->OnClicked.AddDynamic(this, &ANUCharacterCreatePreview::HandleClicked);
		ClickBox->OnBeginCursorOver.AddDynamic(this, &ANUCharacterCreatePreview::HandleCursorOver);
		ClickBox->OnEndCursorOver.AddDynamic(this, &ANUCharacterCreatePreview::HandleCursorLeave);
	}
}

void ANUCharacterCreatePreview::HandleCursorOver(UPrimitiveComponent* /*TouchedComponent*/)
{
	if (bLocked)
	{
		return; // 미개방 종족 --- 밝아지지도, 손 커서가 되지도 않는다
	}
	bHovered = true;
	ApplyTint();
	SetCursorHand(true);
}

void ANUCharacterCreatePreview::HandleCursorLeave(UPrimitiveComponent* /*TouchedComponent*/)
{
	bHovered = false;
	ApplyTint();
	SetCursorHand(false);
}

void ANUCharacterCreatePreview::SetCursorHand(bool bHand) const
{
	if (APlayerController* PC = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		PC->CurrentMouseCursor = bHand ? EMouseCursor::Hand : EMouseCursor::Default;
	}
}

void ANUCharacterCreatePreview::HandleClicked(UPrimitiveComponent* /*TouchedComponent*/, FKey /*ButtonPressed*/)
{
	if (bLocked)
	{
		return;
	}
	OnStandClicked.ExecuteIfBound(RaceId);
}

void ANUCharacterCreatePreview::SetLocked(bool bInLocked)
{
	bLocked = bInLocked;
	if (bLocked)
	{
		bHovered = false;
	}
	ApplyTint();
}

void ANUCharacterCreatePreview::SetHighlighted(bool bInHighlighted)
{
	bHighlighted = bInHighlighted;
	ApplyTint();
}

void ANUCharacterCreatePreview::ApplyTint()
{
	// 몸은 강조색에 피부색을 곱한다. 파츠는 피부색을 타면 안 되니 강조색만.
	// 마우스를 올린 자리는 고르지 않았어도 밝게 --- 누를 수 있다는 신호다.
	// 잠긴 자리는 항상 어둡다 --- 못 고른다는 표시가 색 하나로 끝난다
	const FLinearColor Base = (!bLocked && (bHighlighted || bHovered)) ? HighlightColor : DimColor;
	if (Sprite != nullptr)
	{
		Sprite->SetSpriteColor(Base * SkinTint);
	}
	if (HairSprite != nullptr)
	{
		// 물들인 머리는 정점 색이 곧 머리 색이다 (강조색을 곱해 어두운 자리에서도 같이 어두워지게)
		HairSprite->SetSpriteColor(bHairTinted
			? FLinearColor(Base.R * HairTint.R, Base.G * HairTint.G, Base.B * HairTint.B, Base.A)
			: Base);
	}
	if (FaceSprite != nullptr)
	{
		FaceSprite->SetSpriteColor(Base);
	}
}

void ANUCharacterCreatePreview::ResetAppearance()
{
	// 그 몸의 첫 머리·외모 (없는 번호를 주면 FindPart가 첫 파츠를 돌려준다). 맞는 파츠가 없는 몸은 비워 둔다.
	// 생성 시점엔 직업이 없다 (0) --- 직업 전용 파츠는 여기 안 나온다
	const UNUAppearanceDataSettings* AppearanceData = GetDefault<UNUAppearanceDataSettings>();
	const int32 BodyGender = static_cast<int32>(Gender);
	const FNUAppearancePartRow* Hair = NUAppearance::FindPart(AppearanceData->HairPartTable, INDEX_NONE, RaceId, BodyGender, 0);
	const FNUAppearancePartRow* Face = NUAppearance::FindPart(AppearanceData->FacePartTable, INDEX_NONE, RaceId, BodyGender, 0);

	FNUCharacterCreateParams Params;
	Params.RaceId = RaceId;
	Params.Gender = Gender;
	ShowAppearance(Params,
		Hair != nullptr ? Hair->Flipbook.LoadSynchronous() : nullptr,
		Face != nullptr ? Face->Flipbook.LoadSynchronous() : nullptr,
		FLinearColor::White);
}

void ANUCharacterCreatePreview::ShowAppearance(const FNUCharacterCreateParams& Params,
	UPaperFlipbook* HairBook, UPaperFlipbook* FaceBook, const FLinearColor& InSkinTint)
{
	ApplyBody(Params.RaceId, Params.Gender);

	// 파츠가 없으면 컴포넌트를 비워 둔다 --- 그리면 이전 파츠가 남는다
	if (HairSprite != nullptr)
	{
		HairSprite->SetFlipbook(HairBook);
	}
	if (FaceSprite != nullptr)
	{
		FaceSprite->SetFlipbook(FaceBook);
	}

	// 머리 색 --- hair_id에 같이 실려 있다 (NUAppearance::PackHairId). 색은 ApplyTint가 정점 색으로 보낸다
	if (HairSprite != nullptr)
	{
		if (!bHairBaseCached)
		{
			HairBaseMaterial = HairSprite->GetNumOverrideMaterials() > 0 ? HairSprite->OverrideMaterials[0] : nullptr;
			bHairBaseCached = true;
		}
		int32 HairPartId = 0;
		int32 HairColorIndex = 0;
		NUAppearance::SplitHairId(Params.HairId, HairPartId, HairColorIndex);
		bHairTinted = NUAppearance::GetHairTint(HairColorIndex, HairTint);

		// 물들임 머티리얼은 처음 한 번만 끼우고, 그 뒤로는 켜고 끄기만 한다 (위 HairTintInstance 설명)
		if (HairTintInstance == nullptr)
		{
			if (UMaterialInterface* TintMaterial = GetDefault<UNUAppearanceDataSettings>()->HairTintStandMaterial.LoadSynchronous())
			{
				HairTintInstance = UMaterialInstanceDynamic::Create(TintMaterial, this);
				HairSprite->SetMaterial(0, HairTintInstance);
			}
		}
		if (HairTintInstance != nullptr)
		{
			HairTintInstance->SetScalarParameterValue(TEXT("HairTintOn"), bHairTinted ? 1.f : 0.f);
		}
		else
		{
			bHairTinted = false; // 물들일 머티리얼이 없다 --- 원래 색으로 (정점 색에 머리 색을 싣지 않는다)
			HairSprite->SetMaterial(0, HairBaseMaterial.Get());
		}
	}

	// SetFlipbook은 플립북이 바뀐 컴포넌트만 0초부터 다시 틀어서, 머리만 바꾸면 몸·얼굴과 재생 위치가 어긋난다.
	// 새 남캐 파츠는 프레임마다 흔들림이 그려져 있어 몇 프레임만 밀려도 머리가 따로 튄다 --- 파츠를 몸 재생 위치에 맞춘다
	// (9/12 유민 "2번 헤어랑 바운싱 싱크가 안맞아", PIE에서 머리가 0.133초 늦었다)
	if (Sprite != nullptr)
	{
		const float BodyTime = Sprite->GetPlaybackPosition();
		for (UPaperFlipbookComponent* Part : { HairSprite.Get(), FaceSprite.Get() })
		{
			if (Part != nullptr)
			{
				Part->SetPlaybackPosition(BodyTime, false);
			}
		}
	}

	SkinTint = InSkinTint;
	ApplyTint();
}

void ANUCharacterCreatePreview::ApplyBody(int32 InRaceId, ENUCharacterGender InGender)
{
	// 몸 표에 이 종족·성별의 정면 대기가 있으면 그것을, 없으면 기본 외형 (베이스 바디가 안 나온 조합은 전부 여기로 온다)
	const FNUBodyRow* Body = NUAppearance::FindBody(InRaceId, static_cast<int32>(InGender), 0); // 생성 시점엔 직업이 없다
	ApplyFlipbook(Body != nullptr && !Body->IdleFlipbook.IsNull() ? Body->IdleFlipbook : DefaultFlipbook);
}

void ANUCharacterCreatePreview::ApplyFlipbook(const TSoftObjectPtr<UPaperFlipbook>& Source)
{
	if (Sprite == nullptr || Source.IsNull())
	{
		return;
	}

	// 생성 레벨은 에셋이 몇 없어 동기 로드로 충분하다 (선택창 미리보기와 같은 판단)
	if (UPaperFlipbook* Flipbook = Source.LoadSynchronous())
	{
		Sprite->SetFlipbook(Flipbook);
	}
}
