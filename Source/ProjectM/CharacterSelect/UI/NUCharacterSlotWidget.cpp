#include "CharacterSelect/UI/NUCharacterSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterSelect/Data/NUAppearanceDataSettings.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "PaperFlipbook.h"
#include "UI/NUFlipbookImage.h"
#include "UI/NUUISettings.h"

namespace
{
	// 직업 이름은 진영 님 DT_JobDefinition이 정본이다 (1001~1005).
	// 여기서 이름을 또 적어두면 표가 바뀔 때 갈리므로 표만 읽는다.
	// 전직은 10레벨이라 그 전까지는 JobId가 0(미전직)으로 온다
	FText GetJobDisplayName(const UObject* WorldContext, int32 JobId)
	{
		if (JobId <= 0)
		{
			return NSLOCTEXT("NUCharacter", "JobNone", "노비스");
		}

		const UGameInstance* GI = WorldContext ? WorldContext->GetWorld()->GetGameInstance() : nullptr;
		const UNUProgressionDataSubsystem* Progression = GI ? GI->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
		const FNUJobDefinitionRow* Job = Progression ? Progression->FindJob(JobId) : nullptr;
		if (Job != nullptr && !Job->DisplayName.IsEmpty())
		{
			return Job->DisplayName;
		}

		// 표에 없는 값 --- 서버가 준 숫자를 그대로 보여 준다 (조용히 "미정"으로 덮으면 원인을 못 찾는다)
		return FText::Format(NSLOCTEXT("NUCharacter", "JobUnknown", "직업 {0}"), FText::AsNumber(JobId));
	}
}

void UNUCharacterSlotWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// 한 번만 묶는다 --- 칸은 목록을 다시 그릴 때마다 새로 만들어지므로 재구독 걱정이 없다
	SlotButton->OnClicked.AddDynamic(this, &UNUCharacterSlotWidget::HandleSlotClicked);
	if (DeleteButton != nullptr)
	{
		DeleteButton->OnClicked.AddDynamic(this, &UNUCharacterSlotWidget::HandleDeleteClicked);
	}
}

void UNUCharacterSlotWidget::SetEmpty()
{
	CharacterIdx = INDEX_NONE;
	bDeletePending = false;

	CharacterBox->SetVisibility(ESlateVisibility::Collapsed);
	EmptyBox->SetVisibility(ESlateVisibility::HitTestInvisible); // 클릭은 SlotButton이 받는다
	PortraitOverlay->ClearChildren();
	if (SelectionFrame != nullptr)
	{
		SelectionFrame->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UNUCharacterSlotWidget::SetCharacter(const FNUCharacterSummary& Character, bool bSelected)
{
	CharacterIdx = Character.CharacterIdx;
	bDeletePending = Character.bDeletePending;

	// 묶음 자체는 클릭을 안 막고, 안의 삭제 버튼은 눌린다 (중첩 버튼: 안쪽이 클릭을 소비)
	CharacterBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	EmptyBox->SetVisibility(ESlateVisibility::Collapsed);
	if (SelectionFrame != nullptr)
	{
		SelectionFrame->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// 외형 --- 인게임과 같은 애셋(스프라이트가 곧 캐릭터 모습). 골라 둔 칸은 대기 대신 걷기 (얼굴 칸은 끈다).
	// 직업 문장 자리가 있는 칸은 얼굴 대신 문장만 띄운다
	if (ApplyJobIcon(Character.JobId))
	{
		PortraitOverlay->ClearChildren();
		PortraitOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		BuildPortrait(this, PortraitOverlay, Character, bSelected && bWalkWhenSelected, PortraitHeight, PortraitPixelScale,
			bPortraitAlignTop, DefaultIdleFlipbook.LoadSynchronous(), DefaultWalkFlipbook.LoadSynchronous());
	}

	NameText->SetText(FText::FromString(Character.Name));

	// 기획서 2.3: 슬롯에는 이름/레벨/직업/종족 (문구는 DescribeCharacter)
	if (SubText != nullptr)
	{
		SubText->SetText(DescribeCharacter(this, Character, bSingleLineSub));
	}

	// 삭제 예약 배지 --- 예약 중에도 목록에 남고 슬롯을 차지한다 (선택은 서버가 1212로 거절).
	// 예약이 아닌 칸도 같은 자리를 비워 둔다(Hidden) --- 배지 유무로 아래 버튼이 밀리면 칸마다 어긋난다
	if (PendingText != nullptr)
	{
		if (bDeletePending)
		{
			const int32 RemainHours = FMath::Max(1, FMath::DivideAndRoundUp(Character.DeleteRemainSec, 3600));
			PendingText->SetText(FText::Format(
				NSLOCTEXT("NUCharacter", "DeletePendingBadge", "삭제 예정 · {0}시간 남음"), FText::AsNumber(RemainHours)));
		}
		PendingText->SetVisibility(bDeletePending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}

	// 예약 중이면 삭제 대신 [삭제 취소] --- 141로 24시간 안에 되돌릴 수 있다. 색은 팔레트(파괴/긍정)
	if (DeleteButton != nullptr)
	{
		const UNUUISettings* Palette = GetDefault<UNUUISettings>();
		DeleteButton->SetBackgroundColor(bDeletePending ? Palette->PositiveColor : Palette->DestructiveColor);
		if (bDeleteOnlyWhenSelected)
		{
			DeleteButton->SetVisibility(bSelected || bDeletePending ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
	if (DeleteText != nullptr)
	{
		DeleteText->SetText(bDeletePending
			? NSLOCTEXT("NUCharacter", "DeleteCancel", "삭제 취소")
			: NSLOCTEXT("NUCharacter", "Delete", "삭제"));
	}
}

bool UNUCharacterSlotWidget::ApplyJobIcon(int32 JobId)
{
	if (JobIconImage == nullptr)
	{
		return false;
	}

	FName JobKey = TEXT("Novice");
	if (JobId > 0)
	{
		const UGameInstance* GI = GetWorld() != nullptr ? GetWorld()->GetGameInstance() : nullptr;
		const UNUProgressionDataSubsystem* Progression = GI != nullptr ? GI->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
		const FNUJobDefinitionRow* Job = Progression != nullptr ? Progression->FindJob(JobId) : nullptr;
		if (Job != nullptr && !Job->JobKey.IsNone() && JobIcons.Contains(Job->JobKey))
		{
			JobKey = Job->JobKey;
		}
	}

	const TSoftObjectPtr<UTexture2D>* Icon = JobIcons.Find(JobKey);
	UTexture2D* Texture = Icon != nullptr ? Icon->LoadSynchronous() : nullptr;
	JobIconImage->SetVisibility(Texture != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (Texture != nullptr)
	{
		JobIconImage->SetBrushResourceObject(Texture);   // 크기·색은 WBP 브러시 그대로
	}
	return true;
}

void UNUCharacterSlotWidget::BuildPortrait(UUserWidget* Owner, UOverlay* Into, const FNUCharacterSummary& Character,
	bool bWalk, float Height, float PixelScale, bool bAlignTop, UPaperFlipbook* DefaultIdle, UPaperFlipbook* DefaultWalk)
{
	if (Owner == nullptr || Owner->WidgetTree == nullptr || Into == nullptr)
	{
		return;
	}
	Into->ClearChildren();

	// 플립북 한 장을 얹는다 (nullptr이면 건너뛴다). BounceBody를 주면 그 몸 그림의 흔들림을 따라간다 (머리·얼굴 층)
	const auto AddLayer = [Owner, Into, Height, PixelScale, bAlignTop](UPaperFlipbook* Flipbook, const UPaperFlipbook* BounceBody, float PartTuneZ) -> UNUFlipbookImage*
	{
		if (Flipbook == nullptr)
		{
			return nullptr;
		}
		UNUFlipbookImage* Layer = Owner->WidgetTree->ConstructWidget<UNUFlipbookImage>();
		Layer->FollowBodyBounce(BounceBody, PartTuneZ);
		if (PixelScale > 0.f)
		{
			Layer->SetFlipbookAtScale(Flipbook, PixelScale);
		}
		else
		{
			Layer->SetFlipbook(Flipbook, Height);
		}

		// 캔버스가 다른 그림이 섞여도(여캐 몸 52px + 남캐 머리 69px) 발을 맞춘 채 가운데에 오게 정렬한다.
		// Overlay 기본값(왼쪽 위)이면 좁은 쪽이 왼쪽에 붙는다
		if (UOverlaySlot* LayerSlot = Into->AddChildToOverlay(Layer))
		{
			LayerSlot->SetHorizontalAlignment(HAlign_Center);
			LayerSlot->SetVerticalAlignment(bAlignTop ? VAlign_Top : VAlign_Bottom);
		}
		return Layer;
	};

	// 몸 표에 이 종족·성별·직업의 플립북이 있으면 그것을, 없으면 넘겨받은 공용 외형
	UPaperFlipbook* Idle = DefaultIdle;
	UPaperFlipbook* Walk = DefaultWalk;
	float PartTuneZ = 0.f;
	if (const FNUBodyRow* Body = NUAppearance::FindBody(Character.RaceId, Character.Gender, Character.JobId))
	{
		PartTuneZ = Body->PortraitPartTuneZ;
		if (UPaperFlipbook* BodyIdle = Body->IdleFlipbook.LoadSynchronous())
		{
			Idle = BodyIdle;
			UPaperFlipbook* BodyWalk = Body->WalkFlipbook.LoadSynchronous();
			Walk = BodyWalk != nullptr ? BodyWalk : BodyIdle;
		}
	}
	UPaperFlipbook* const BodyBook = bWalk && Walk != nullptr ? Walk : Idle;
	AddLayer(BodyBook, nullptr, 0.f);

	// 커마 겹치기 --- 몸 위에 외모 => 머리 순(파츠 표의 플립북, 생성창과 같은 그림). 필드처럼 머리가 맨 앞이다 ---
	// 새 남캐는 머리가 얼굴(귀·이마)을 덮어야 맞다 (9/12).
	// 몸이 걷기 그림이면 파츠도 걷기 그림을 쓴다 --- 대기 그림을 얹으면 캔버스·프레임이 달라 어긋난다.
	// 맞는 파츠가 없는 몸(외형 필드가 없던 시절 캐릭터 등)이면 몸만 나온다
	const bool bBodyWalks = BodyBook != Idle;
	const UNUAppearanceDataSettings* Appearance = GetDefault<UNUAppearanceDataSettings>();
	const TSoftObjectPtr<UDataTable>* PartTables[] = { &Appearance->FacePartTable, &Appearance->HairPartTable };
	// 머리 번호에는 머리 색이 같이 실려 온다 (NUAppearance::PackHairId)
	int32 HairPartId = Character.HairId;
	int32 HairColorIndex = 0;
	NUAppearance::SplitHairId(Character.HairId, HairPartId, HairColorIndex);
	const int32 PartIds[] = { Character.FaceId, HairPartId };
	for (int32 i = 0; i < 2; ++i)
	{
		const FNUAppearancePartRow* Part = NUAppearance::FindPart(*PartTables[i], PartIds[i], Character.RaceId, Character.Gender, Character.JobId);
		if (Part == nullptr)
		{
			continue;
		}
		UPaperFlipbook* PartBook = bBodyWalks ? Part->WalkFlipbook.LoadSynchronous() : nullptr;
		// 필드처럼 몸 그림의 프레임별 흔들림 표를 따라간다 --- 안 따라가면 남캐 직업 몸에서 머리가 몸과 1칸씩 어긋난다
		UNUFlipbookImage* Layer = AddLayer(PartBook != nullptr ? PartBook : Part->Flipbook.LoadSynchronous(), BodyBook, PartTuneZ);
		FLinearColor HairTint;
		if (i == 1 && Layer != nullptr && NUAppearance::GetHairTint(HairColorIndex, HairTint))
		{
			Layer->SetTintMaterial(Appearance->HairTintUIMaterial.LoadSynchronous(), HairTint);
		}
	}
}

FText UNUCharacterSlotWidget::DescribeCharacter(const UObject* WorldContext, const FNUCharacterSummary& Character, bool bSingleLine)
{
	// 종족은 외형 필드가 생기기 전에 만든 캐릭터면 비어 있어서, 그때는 줄에서 뺀다.
	// 카드 칸은 직업 이름이 길 수 있어 줄을 나눈다 --- "Lv.15  엘리멘탈리스트 · 인간"은 한 줄로 203px이라 칸(180px) 밖으로 나갔다 (9/12 유민)
	const FText RaceName = NURace::GetDisplayName(Character.RaceId);
	const FText JobName = GetJobDisplayName(WorldContext, Character.JobId);
	const FText Level = FText::AsNumber(Character.Level);
	if (bSingleLine)
	{
		return RaceName.IsEmpty()
			? FText::Format(NSLOCTEXT("NUCharacter", "CharacterLine", "Lv.{0} · {1}"), Level, JobName)
			: FText::Format(NSLOCTEXT("NUCharacter", "CharacterLineRace", "Lv.{0} · {1} · {2}"), Level, JobName, RaceName);
	}
	return RaceName.IsEmpty()
		? FText::Format(NSLOCTEXT("NUCharacter", "CharacterSub", "Lv.{0}\n{1}"), Level, JobName)
		: FText::Format(NSLOCTEXT("NUCharacter", "CharacterSubRace", "Lv.{0} · {2}\n{1}"), Level, JobName, RaceName);
}

UPaperFlipbook* UNUCharacterSlotWidget::LoadDefaultIdleFlipbook() const
{
	return DefaultIdleFlipbook.LoadSynchronous();
}

UPaperFlipbook* UNUCharacterSlotWidget::LoadDefaultWalkFlipbook() const
{
	return DefaultWalkFlipbook.LoadSynchronous();
}

void UNUCharacterSlotWidget::HandleSlotClicked()
{
	OnSlotClicked.ExecuteIfBound(CharacterIdx);
}

void UNUCharacterSlotWidget::HandleDeleteClicked()
{
	if (CharacterIdx != INDEX_NONE)
	{
		OnDeleteClicked.ExecuteIfBound(CharacterIdx, bDeletePending);
	}
}
