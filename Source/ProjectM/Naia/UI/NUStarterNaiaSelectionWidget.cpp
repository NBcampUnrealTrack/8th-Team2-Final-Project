#include "Naia/UI/NUStarterNaiaSelectionWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Naia/UI/NUStarterNaiaCardWidget.h"
#include "Naia/UI/NUStarterNaiaSelectionEntryButton.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUPassiveDefinition.h"
#include "UI/NUFlipbookImage.h"
#include "UI/NUKoreanText.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FVector2D StarterPortraitMaxSize(136.f, 112.f);

	UPaperSprite* LoadStarterPortrait(int32 NaiaId)
	{
		const TCHAR* AssetPath = nullptr;
		switch (NaiaId)
		{
		case 1001: AssetPath = TEXT("/Game/Art/Sprites/Naia/1001_Bolto/Frames/1001_Bolto_0_aseprite.1001_Bolto_0_aseprite"); break;
		case 1004: AssetPath = TEXT("/Game/Art/Sprites/Naia/1004_Puruna_00/Frames/1004_Puruna_0_aseprite.1004_Puruna_0_aseprite"); break;
		case 1007: AssetPath = TEXT("/Game/Art/Sprites/Naia/1007_Bulcoi/Frames/1007_Bulcoi_0_aseprite.1007_Bulcoi_0_aseprite"); break;
		case 1010: AssetPath = TEXT("/Game/Art/Sprites/Naia/1010_Aqupong/Frames/1010_Aqupong_0_aseprite.1010_Aqupong_0_aseprite"); break;
		case 1013: AssetPath = TEXT("/Game/Art/Sprites/Naia/1013_Togra/Frames/1013_Togra_0_aseprite.1013_Togra_0_aseprite"); break;
		case 1016: AssetPath = TEXT("/Game/Art/Sprites/Naia/1016_Hwiry/Frames/1016_Hwiry_0_aseprite.1016_Hwiry_0_aseprite"); break;
		default: return nullptr;
		}

		UPaperSprite* Sprite = LoadObject<UPaperSprite>(nullptr, AssetPath);
		if (Sprite != nullptr)
		{
			if (UTexture2D* Texture = Sprite->GetBakedTexture())
			{
				Texture->SetForceMipLevelsToBeResident(30.f);
				Texture->WaitForPendingInitOrStreaming();
			}
		}
		return Sprite;
	}

	UTextBlock* MakeStarterNaiaText(UWidgetTree* WidgetTree, const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Result = WidgetTree->ConstructWidget<UTextBlock>();
		Result->SetText(Text);
		Result->SetFont(FCoreStyle::GetDefaultFontStyle(Size >= 24 ? TEXT("Bold") : TEXT("Regular"), Size));
		Result->SetColorAndOpacity(FSlateColor(Color));
		return Result;
	}
}

void UNUStarterNaiaSelectionWidget::InitializeSelection(UNUNaiaRosterComponent* InRosterComponent, FSimpleDelegate InCloseRequested)
{
	RosterComponent = InRosterComponent;
	SelectedNaiaId = 0;
	bCompletionHandled = false;
	SetCloseRequested(MoveTemp(InCloseRequested));
}

TSharedRef<SWidget> UNUStarterNaiaSelectionWidget::RebuildWidget()
{
	if (WidgetTree != nullptr && WidgetTree->RootWidget == nullptr)
	{
		const UNUUISettings* Settings = GetDefault<UNUUISettings>();
		const FLinearColor PanelColor = Settings != nullptr ? Settings->PanelColor : FLinearColor(0.10f, 0.11f, 0.16f, 1.f);
		const FLinearColor TextColor = Settings != nullptr ? Settings->TextColor : FLinearColor::White;
		const FLinearColor SecondaryTextColor = Settings != nullptr ? Settings->SubTextColor : FLinearColor(0.62f, 0.62f, 0.70f, 1.f);

		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		USizeBox* FrameSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("WindowFrame"));
		FrameSize->SetWidthOverride(1180.f);
		FrameSize->SetHeightOverride(620.f);
		WindowFrame = FrameSize;

		UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
		Panel->SetBrushColor(PanelColor);
		Panel->SetPadding(FMargin(28.f));
		FrameSize->AddChild(Panel);

		UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
		Panel->SetContent(Content);

		UTextBlock* Title = MakeStarterNaiaText(WidgetTree, NSLOCTEXT("NUStarterNaia", "Title", "함께할 나이아 선택"), 32, TextColor);
		Title->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* BoxSlot = Content->AddChildToVerticalBox(Title))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		UTextBlock* Guide = MakeStarterNaiaText(WidgetTree, NSLOCTEXT("NUStarterNaia", "Guide", "시작 나이아 한 마리를 선택합니다. 선택 후에는 변경할 수 없습니다."), 17, SecondaryTextColor);
		Guide->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* BoxSlot = Content->AddChildToVerticalBox(Guide))
		{
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		NaiaListPanel = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("NaiaListPanel"));
		if (UVerticalBoxSlot* BoxSlot = Content->AddChildToVerticalBox(NaiaListPanel))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetHorizontalAlignment(HAlign_Center);
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}

		SelectedNaiaText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SelectedNaiaText"));
		SelectedNaiaText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), 22));
		SelectedNaiaText->SetColorAndOpacity(FSlateColor(TextColor));
		SelectedNaiaText->SetJustification(ETextJustify::Center);
		if (UVerticalBoxSlot* BoxSlot = Content->AddChildToVerticalBox(SelectedNaiaText))
		{
			BoxSlot->SetPadding(FMargin(0.f, 18.f, 0.f, 4.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		DescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DescriptionText"));
		DescriptionText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 15));
		DescriptionText->SetColorAndOpacity(FSlateColor(SecondaryTextColor));
		DescriptionText->SetJustification(ETextJustify::Center);
		DescriptionText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* BoxSlot = Content->AddChildToVerticalBox(DescriptionText))
		{
			BoxSlot->SetPadding(FMargin(40.f, 0.f, 40.f, 14.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}

		UButton* ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
		ConfirmButton->SetBackgroundColor(Settings != nullptr ? Settings->PositiveColor : FLinearColor(0.18f, 0.48f, 0.26f, 1.f));
		UTextBlock* ConfirmText = MakeStarterNaiaText(WidgetTree, NSLOCTEXT("NUStarterNaia", "Confirm", "선택 확정"), 18, TextColor);
		ConfirmButton->AddChild(ConfirmText);
		if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(ConfirmText->Slot))
		{
			ButtonSlot->SetPadding(FMargin(28.f, 10.f));
			ButtonSlot->SetHorizontalAlignment(HAlign_Center);
			ButtonSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UVerticalBoxSlot* BoxSlot = Content->AddChildToVerticalBox(ConfirmButton))
		{
			BoxSlot->SetHorizontalAlignment(HAlign_Center);
		}

		UCanvasPanelSlot* FrameSlot = Root->AddChildToCanvas(FrameSize);
		FrameSlot->SetAnchors(FAnchors(0.5f));
		FrameSlot->SetAlignment(FVector2D(0.5f));
		FrameSlot->SetPosition(FVector2D::ZeroVector);
		FrameSlot->SetAutoSize(true);
		WidgetTree->RootWidget = Root;
	}

	return Super::RebuildWidget();
}

void UNUStarterNaiaSelectionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	bCompletionHandled = false;

	if (!BindConfirmButton() || NaiaListPanel == nullptr || SelectedNaiaText == nullptr || DescriptionText == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("시작 나이아 선택 위젯의 필수 위젯이 없음"));
		return;
	}

	if (RosterComponent.IsValid())
	{
		RosterComponent->OnRosterChanged.RemoveDynamic(this, &ThisClass::HandleRosterChanged);
		RosterComponent->OnRosterChanged.AddDynamic(this, &ThisClass::HandleRosterChanged);
	}

	RebuildNaiaEntries();
	if (bSelectFirstOnOpen && SelectedNaiaId == 0 && !NaiaEntries.IsEmpty())
	{
		SelectedNaiaId = NaiaEntries[0].NaiaId;
	}
	RefreshState();
	HandleRosterChanged();
}

void UNUStarterNaiaSelectionWidget::NativeDestruct()
{
	UnbindConfirmButton();
	if (RosterComponent.IsValid())
	{
		RosterComponent->OnRosterChanged.RemoveDynamic(this, &ThisClass::HandleRosterChanged);
	}
	if (ActiveConfirmDialog != nullptr)
	{
		ActiveConfirmDialog->Dismiss();
		ActiveConfirmDialog = nullptr;
	}
	NaiaButtons.Reset();
	NaiaCards.Reset();
	NaiaEntries.Reset();
	Super::NativeDestruct();
}

bool UNUStarterNaiaSelectionWidget::CanDragWindow() const
{
	return false;
}

bool UNUStarterNaiaSelectionWidget::BindConfirmButton()
{
	UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton"));
	if (UButton* NativeButton = Cast<UButton>(ConfirmWidget))
	{
		NativeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
		NativeButton->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
		return true;
	}
	if (UNU_CommonButtonWidget* CommonButton = Cast<UNU_CommonButtonWidget>(ConfirmWidget))
	{
		CommonButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
		CommonButton->OnCommonClicked.AddDynamic(this, &ThisClass::HandleConfirmClicked);
		return true;
	}

	return false;
}

void UNUStarterNaiaSelectionWidget::UnbindConfirmButton()
{
	UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton"));
	if (UButton* NativeButton = Cast<UButton>(ConfirmWidget))
	{
		NativeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
	else if (UNU_CommonButtonWidget* CommonButton = Cast<UNU_CommonButtonWidget>(ConfirmWidget))
	{
		CommonButton->OnCommonClicked.RemoveDynamic(this, &ThisClass::HandleConfirmClicked);
	}
}

void UNUStarterNaiaSelectionWidget::SetConfirmButtonEnabled(bool bEnabled)
{
	if (UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton")))
	{
		ConfirmWidget->SetIsEnabled(bEnabled);
	}
}

void UNUStarterNaiaSelectionWidget::RebuildNaiaEntries()
{
	NaiaEntries.Reset();
	NaiaButtons.Reset();
	NaiaCards.Reset();
	NaiaListPanel->ClearChildren();

	const UGameInstance* GameInstance = GetGameInstance();
	const UNUNaiaDataSubsystem* Data = GameInstance != nullptr ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	if (Data == nullptr)
	{
		return;
	}

	for (const FNUNaiaDefinition* Definition : Data->GetStarterDefinitions())
	{
		if (Definition == nullptr)
		{
			continue;
		}

		FNUStarterNaiaSelectionViewData& Entry = NaiaEntries.AddDefaulted_GetRef();
		Entry.NaiaId = Definition->DataIndex;
		Entry.DisplayName = Definition->KoreanName;
		Entry.ElementName = GetElementDisplayName(Definition->ElementTag);
		Entry.ElementColor = GetElementColor(Definition->ElementTag);
		Entry.Description = Definition->Description;
		Entry.ElementKey = FName(*Definition->ElementTag.GetTagName().ToString().RightChop(
			Definition->ElementTag.GetTagName().ToString().Find(TEXT("."), ESearchCase::IgnoreCase, ESearchDir::FromEnd) + 1));
		Entry.Stats = { Definition->BaseHealth, Definition->BaseSP, Definition->BaseAttackPower, Definition->BaseMagicAttackPower,
			Definition->BaseDefense, Definition->BaseSpeed };
		Entry.EvolutionLevel = Definition->EvolutionLevel;
		if (const FNUNaiaDefinition* Next = Definition->NextEvolutionId > 0 ? Data->FindDefinitionById(Definition->NextEvolutionId) : nullptr)
		{
			Entry.EvolutionName = Next->KoreanName;
		}
		Entry.Flipbook = Definition->PreviewFlipbook;
		const UNUBattleDataSubsystem* Battle = GameInstance->GetSubsystem<UNUBattleDataSubsystem>();
		if (const FNUPassiveDefinition* Passive = Battle != nullptr && Definition->PassiveSkill1Id > 0 ? Battle->FindPassiveDefinition(Definition->PassiveSkill1Id) : nullptr)
		{
			Entry.PassiveName = Passive->KoreanName;
			Entry.PassiveDescription = Passive->Description;
		}

		UWidget* Card = UsesStage() ? BuildStageCard(Entry) : BuildDefaultNaiaCard(Entry);
		if (UHorizontalBoxSlot* BoxSlot = Card != nullptr ? NaiaListPanel->AddChildToHorizontalBox(Card) : nullptr)
		{
			BoxSlot->SetPadding(UsesStage() ? FMargin(0.f) : FMargin(6.f));
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

UWidget* UNUStarterNaiaSelectionWidget::BuildDefaultNaiaCard(const FNUStarterNaiaSelectionViewData& Entry)
{
	UNUStarterNaiaSelectionEntryButton* Button = WidgetTree->ConstructWidget<UNUStarterNaiaSelectionEntryButton>();
	Button->SetNaiaId(Entry.NaiaId);
	Button->OnEntryClicked.BindUObject(this, &ThisClass::SelectNaia);
	Button->SetBackgroundColor(FLinearColor(0.16f, 0.17f, 0.25f, 1.f));
	NaiaButtons.Add(Entry.NaiaId, Button);

	UVerticalBox* CardContent = WidgetTree->ConstructWidget<UVerticalBox>();
	USizeBox* PortraitArea = WidgetTree->ConstructWidget<USizeBox>();
	PortraitArea->SetWidthOverride(148.f);
	PortraitArea->SetHeightOverride(124.f);
	PortraitArea->SetClipping(EWidgetClipping::ClipToBounds);
	if (UImage* PortraitImage = BuildPortraitImage(Entry.NaiaId))
	{
		PortraitArea->AddChild(PortraitImage);
		if (USizeBoxSlot* PortraitSlot = Cast<USizeBoxSlot>(PortraitImage->Slot))
		{
			PortraitSlot->SetHorizontalAlignment(HAlign_Center);
			PortraitSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	if (UVerticalBoxSlot* PortraitAreaSlot = CardContent->AddChildToVerticalBox(PortraitArea))
	{
		PortraitAreaSlot->SetPadding(FMargin(2.f, 6.f, 2.f, 4.f));
		PortraitAreaSlot->SetHorizontalAlignment(HAlign_Center);
		PortraitAreaSlot->SetVerticalAlignment(VAlign_Center);
	}

	UTextBlock* NameText = MakeStarterNaiaText(WidgetTree, Entry.DisplayName, 21, FLinearColor(0.94f, 0.94f, 0.96f, 1.f));
	NameText->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* BoxSlot = CardContent->AddChildToVerticalBox(NameText))
	{
		BoxSlot->SetPadding(FMargin(4.f, 0.f, 4.f, 8.f));
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}

	UTextBlock* ElementLabel = MakeStarterNaiaText(WidgetTree, Entry.ElementName, 16, Entry.ElementColor);
	ElementLabel->SetJustification(ETextJustify::Center);
	if (UVerticalBoxSlot* BoxSlot = CardContent->AddChildToVerticalBox(ElementLabel))
	{
		BoxSlot->SetHorizontalAlignment(HAlign_Fill);
	}
	Button->AddChild(CardContent);

	USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
	CardSize->SetWidthOverride(168.f);
	CardSize->SetHeightOverride(220.f);
	CardSize->AddChild(Button);
	return CardSize;
}

UImage* UNUStarterNaiaSelectionWidget::BuildPortraitImage(int32 NaiaId) const
{
	UPaperSprite* Sprite = LoadStarterPortrait(NaiaId);
	if (Sprite == nullptr)
	{
		return nullptr;
	}

	UImage* PortraitImage = WidgetTree->ConstructWidget<UImage>();
	FSlateBrush Brush;
	const FVector BoundsSize = Sprite->GetRenderBounds().GetBox().GetSize();
	const FVector2D ContentSize(BoundsSize.X * Sprite->GetPixelsPerUnrealUnit(), BoundsSize.Z * Sprite->GetPixelsPerUnrealUnit());
	FVector2f MinUV(1.f, 1.f);
	FVector2f MaxUV(0.f, 0.f);
	for (const FVector4& Vertex : Sprite->BakedRenderData)
	{
		MinUV.X = FMath::Min(MinUV.X, static_cast<float>(Vertex.Z));
		MinUV.Y = FMath::Min(MinUV.Y, static_cast<float>(Vertex.W));
		MaxUV.X = FMath::Max(MaxUV.X, static_cast<float>(Vertex.Z));
		MaxUV.Y = FMath::Max(MaxUV.Y, static_cast<float>(Vertex.W));
	}

	UTexture2D* Texture = Sprite->GetBakedTexture();
	if (Texture != nullptr && MaxUV.X > MinUV.X && MaxUV.Y > MinUV.Y)
	{
		Brush.SetResourceObject(Texture);
		Brush.SetUVRegion(FBox2f(MinUV, MaxUV));
	}
	else
	{
		Brush.SetResourceObject(Sprite);
	}
	const float ScaleX = ContentSize.X > 0.f ? StarterPortraitMaxSize.X / ContentSize.X : 1.f;
	const float ScaleY = ContentSize.Y > 0.f ? StarterPortraitMaxSize.Y / ContentSize.Y : 1.f;
	Brush.ImageSize = ContentSize * FMath::Min(ScaleX, ScaleY);
	PortraitImage->SetBrush(Brush);
	return PortraitImage;
}

void UNUStarterNaiaSelectionWidget::SelectNaia(int32 NaiaId)
{
	if (!RosterComponent.IsValid() || RosterComponent->HasSelectedStarterNaia())
	{
		return;
	}
	SelectedNaiaId = NaiaId;
	RefreshState();
}

void UNUStarterNaiaSelectionWidget::RefreshState()
{
	const FNUStarterNaiaSelectionViewData* Entry = FindSelectedEntry();
	if (UsesStage())
	{
		// 무대형: 판에는 이름과 타고난 힘(패시브) 설명
		SelectedNaiaText->SetText(Entry != nullptr ? Entry->DisplayName : NSLOCTEXT("NUStarterNaia", "PromptShort", "나이아를 고르세요"));
		DescriptionText->SetText(Entry != nullptr ? Entry->PassiveDescription : FText::GetEmpty());
		for (const TPair<int32, TObjectPtr<UNUStarterNaiaCardWidget>>& Pair : NaiaCards)
		{
			if (Pair.Value != nullptr)
			{
				Pair.Value->SetSelected(Pair.Key == SelectedNaiaId);
			}
		}
		RefreshDetail(Entry);
		FString StageError;
		SetConfirmButtonEnabled(Entry != nullptr && RosterComponent.IsValid() && RosterComponent->CanSelectStarterNaia(Entry->NaiaId, StageError));
		return;
	}
	SelectedNaiaText->SetText(Entry != nullptr
		? FText::Format(NSLOCTEXT("NUStarterNaia", "Selected", "선택: {0}"), Entry->DisplayName)
		: NSLOCTEXT("NUStarterNaia", "Prompt", "함께할 나이아를 선택하세요"));
	DescriptionText->SetText(Entry != nullptr
		? (Entry->Description.IsEmpty() ? NSLOCTEXT("NUStarterNaia", "LevelGuide", "현재 캐릭터 레벨로 획득합니다.") : Entry->Description)
		: NSLOCTEXT("NUStarterNaia", "EmptyGuide", "테이블에 등록된 시작 나이아만 선택할 수 있습니다."));

	FString Error;
	SetConfirmButtonEnabled(Entry != nullptr && RosterComponent.IsValid() && RosterComponent->CanSelectStarterNaia(Entry->NaiaId, Error));
	for (const TPair<int32, TObjectPtr<UNUStarterNaiaSelectionEntryButton>>& Pair : NaiaButtons)
	{
		if (Pair.Value != nullptr)
		{
			Pair.Value->SetBackgroundColor(Pair.Key == SelectedNaiaId
				? FLinearColor(0.28f, 0.48f, 0.62f, 1.f)
				: FLinearColor(0.16f, 0.17f, 0.25f, 1.f));
		}
	}
}

void UNUStarterNaiaSelectionWidget::HandleConfirmClicked()
{
	const FNUStarterNaiaSelectionViewData* Entry = FindSelectedEntry();
	if (Entry == nullptr || !RosterComponent.IsValid())
	{
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSubclassOf<UNUConfirmDialog> DialogClass = Settings != nullptr ? Settings->ConfirmDialogClass.LoadSynchronous() : nullptr;
	ActiveConfirmDialog = UNUConfirmDialog::Show(
		GetOwningPlayer(),
		ConfirmDialogClass != nullptr ? ConfirmDialogClass : DialogClass,
		FText::Format(NSLOCTEXT("NUStarterNaia", "ConfirmSelectionParticle", "{0} 함께하시겠습니까?\n선택 후 변경할 수 없습니다."),
			FText::FromString(NUKoreanText::WaGwa(Entry->DisplayName.ToString()))),
		NSLOCTEXT("NUStarterNaia", "ConfirmAction", "선택"),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSelectionConfirmed),
		FSimpleDelegate::CreateUObject(this, &ThisClass::HandleSelectionCancelled),
		NSLOCTEXT("NUStarterNaia", "CancelAction", "취소"));
	if (ActiveConfirmDialog == nullptr)
	{
		SubmitSelection();
	}
}

void UNUStarterNaiaSelectionWidget::HandleSelectionConfirmed()
{
	ActiveConfirmDialog = nullptr;
	SubmitSelection();
}

void UNUStarterNaiaSelectionWidget::HandleSelectionCancelled()
{
	ActiveConfirmDialog = nullptr;
}

void UNUStarterNaiaSelectionWidget::SubmitSelection()
{
	if (SelectedNaiaId > 0 && RosterComponent.IsValid())
	{
		RosterComponent->ServerSelectStarterNaia(SelectedNaiaId);
	}
}

void UNUStarterNaiaSelectionWidget::HandleRosterChanged()
{
	if (bCompletionHandled || !RosterComponent.IsValid() || !RosterComponent->HasSelectedStarterNaia())
	{
		return;
	}

	bCompletionHandled = true;
	UNUSFXSubsystem::Play(this, NU::SFX::Field::NaiaContract);
	const int32 ConfirmedNaiaId = RosterComponent->GetSelectedStarterNaiaId();
	const FNUStarterNaiaSelectionViewData* ConfirmedEntry = NaiaEntries.FindByPredicate([ConfirmedNaiaId](const FNUStarterNaiaSelectionViewData& Entry)
	{
		return Entry.NaiaId == ConfirmedNaiaId;
	});
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(ConfirmedEntry != nullptr
			? FText::Format(NSLOCTEXT("NUStarterNaia", "CompleteParticle", "{0} 동행을 시작합니다"),
				FText::FromString(NUKoreanText::WaGwa(ConfirmedEntry->DisplayName.ToString())))
			: NSLOCTEXT("NUStarterNaia", "CompleteFallback", "시작 나이아 선택이 완료되었습니다"));
	}
	RequestClose();
}

const FNUStarterNaiaSelectionViewData* UNUStarterNaiaSelectionWidget::FindSelectedEntry() const
{
	return NaiaEntries.FindByPredicate([this](const FNUStarterNaiaSelectionViewData& Entry)
	{
		return Entry.NaiaId == SelectedNaiaId;
	});
}

FText UNUStarterNaiaSelectionWidget::GetElementDisplayName(const FGameplayTag& ElementTag)
{
	const FString Tag = ElementTag.ToString();
	if (Tag.EndsWith(TEXT("Lightning"))) { return NSLOCTEXT("NUStarterNaia", "Lightning", "번개"); }
	if (Tag.EndsWith(TEXT("Grass"))) { return NSLOCTEXT("NUStarterNaia", "Grass", "풀"); }
	if (Tag.EndsWith(TEXT("Fire"))) { return NSLOCTEXT("NUStarterNaia", "Fire", "불"); }
	if (Tag.EndsWith(TEXT("Water"))) { return NSLOCTEXT("NUStarterNaia", "Water", "물"); }
	if (Tag.EndsWith(TEXT("Earth"))) { return NSLOCTEXT("NUStarterNaia", "Earth", "땅"); }
	if (Tag.EndsWith(TEXT("Wind"))) { return NSLOCTEXT("NUStarterNaia", "Wind", "바람"); }
	return FText::FromName(ElementTag.GetTagName());
}

FLinearColor UNUStarterNaiaSelectionWidget::GetElementColor(const FGameplayTag& ElementTag)
{
	const FString Tag = ElementTag.ToString();
	if (Tag.EndsWith(TEXT("Lightning"))) { return FLinearColor(0.92f, 0.79f, 0.25f, 1.f); }
	if (Tag.EndsWith(TEXT("Grass"))) { return FLinearColor(0.38f, 0.82f, 0.42f, 1.f); }
	if (Tag.EndsWith(TEXT("Fire"))) { return FLinearColor(0.95f, 0.37f, 0.22f, 1.f); }
	if (Tag.EndsWith(TEXT("Water"))) { return FLinearColor(0.27f, 0.65f, 0.95f, 1.f); }
	if (Tag.EndsWith(TEXT("Earth"))) { return FLinearColor(0.71f, 0.51f, 0.29f, 1.f); }
	if (Tag.EndsWith(TEXT("Wind"))) { return FLinearColor(0.50f, 0.86f, 0.83f, 1.f); }
	return FLinearColor(0.70f, 0.72f, 0.78f, 1.f);
}

UWidget* UNUStarterNaiaSelectionWidget::BuildStageCard(const FNUStarterNaiaSelectionViewData& Entry)
{
	UNUStarterNaiaCardWidget* Card = CreateWidget<UNUStarterNaiaCardWidget>(GetOwningPlayer(), NaiaCardClass);
	if (Card == nullptr)
	{
		return nullptr;
	}
	const TSoftObjectPtr<UTexture2D>* Icon = ElementIcons.Find(Entry.ElementKey);
	Card->Setup(Entry.NaiaId, Entry.DisplayName, Entry.Flipbook.LoadSynchronous(), Icon != nullptr ? Icon->LoadSynchronous() : nullptr, Entry.ElementColor);
	Card->OnCardClicked.BindUObject(this, &ThisClass::SelectNaia);
	NaiaCards.Add(Entry.NaiaId, Card);
	return Card;
}

void UNUStarterNaiaSelectionWidget::RefreshDetail(const FNUStarterNaiaSelectionViewData* Entry)
{
	const FLinearColor Color = Entry != nullptr ? Entry->ElementColor : FLinearColor::White;
	if (DetailElementIcon != nullptr)
	{
		const TSoftObjectPtr<UTexture2D>* Icon = Entry != nullptr ? ElementIcons.Find(Entry->ElementKey) : nullptr;
		UTexture2D* Texture = Icon != nullptr ? Icon->LoadSynchronous() : nullptr;
		DetailElementIcon->SetVisibility(Texture != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
		if (Texture != nullptr)
		{
			DetailElementIcon->SetBrushResourceObject(Texture);
		}
	}
	if (AccentBar != nullptr)
	{
		AccentBar->SetColorAndOpacity(Color);
	}
	if (ElementChip != nullptr)
	{
		ElementChip->SetBrushColor(Color);
	}
	if (ElementText != nullptr)
	{
		ElementText->SetText(Entry != nullptr
			? FText::Format(NSLOCTEXT("NUStarterNaia", "ElementLabel", "{0} 속성"), Entry->ElementName)
			: FText::GetEmpty());
		ElementText->SetColorAndOpacity(FSlateColor(Color));
	}
	if (PassiveNameText != nullptr)
	{
		PassiveNameText->SetText(Entry != nullptr ? Entry->PassiveName : FText::GetEmpty());
	}

	// 능력치 --- 막대는 여섯 나이아 중 가장 높은 값을 꽉 찬 것으로, 그 값과 같으면 금색
	for (int32 Index = 0; Index < 6; ++Index)
	{
		float Best = 0.f;
		for (const FNUStarterNaiaSelectionViewData& Other : NaiaEntries)
		{
			Best = Other.Stats.IsValidIndex(Index) ? FMath::Max(Best, Other.Stats[Index]) : Best;
		}
		const float Value = Entry != nullptr && Entry->Stats.IsValidIndex(Index) ? Entry->Stats[Index] : 0.f;
		if (UTextBlock* ValueText = Cast<UTextBlock>(GetWidgetFromName(*FString::Printf(TEXT("StatValue%d"), Index))))
		{
			ValueText->SetText(FText::AsNumber(FMath::RoundToInt(Value)));
		}
		if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(*FString::Printf(TEXT("StatBar%d"), Index))))
		{
			Bar->SetPercent(Best > 0.f ? Value / Best : 0.f);
			Bar->SetFillColorAndOpacity(Best > 0.f && Value >= Best ? BestStatColor : Color);
		}
	}

	// 진화 --- 진화형 그림이 아직 없어 지금 나이아의 실루엣 + "?"
	if (EvolutionFigure != nullptr)
	{
		UPaperFlipbook* Book = Entry != nullptr ? Entry->Flipbook.LoadSynchronous() : nullptr;
		EvolutionFigure->SetFlipbookAtScale(Book, 2.f);
		EvolutionFigure->StopAtKeyFrame(0);
		EvolutionFigure->SetColorAndOpacity(SilhouetteTint);
	}
	if (EvolutionLevelText != nullptr)
	{
		EvolutionLevelText->SetText(Entry != nullptr && Entry->EvolutionLevel > 0
			? FText::Format(NSLOCTEXT("NUStarterNaia", "EvolutionLevel", "Lv.{0} 에"), FText::AsNumber(Entry->EvolutionLevel))
			: FText::GetEmpty());
	}
	if (EvolutionNameText != nullptr)
	{
		EvolutionNameText->SetText(Entry != nullptr && !Entry->EvolutionName.IsEmpty() ? Entry->EvolutionName : NSLOCTEXT("NUStarterNaia", "EvolutionUnknown", "?"));
	}
	if (ConfirmLabel != nullptr)
	{
		ConfirmLabel->SetText(Entry != nullptr
			? FText::Format(ConfirmLabelFormat, FText::FromString(NUKoreanText::WaGwa(Entry->DisplayName.ToString())))
			: NSLOCTEXT("NUStarterNaia", "ConfirmLabelEmpty", "나이아를 고르세요"));
	}
}

void UNUStarterNaiaSelectionWidget::StepSelection(int32 Step)
{
	if (NaiaEntries.IsEmpty() || (RosterComponent.IsValid() && RosterComponent->HasSelectedStarterNaia()))
	{
		return;
	}
	const int32 Current = NaiaEntries.IndexOfByPredicate([this](const FNUStarterNaiaSelectionViewData& Entry) { return Entry.NaiaId == SelectedNaiaId; });
	const int32 Next = Current == INDEX_NONE ? 0 : FMath::Clamp(Current + Step, 0, NaiaEntries.Num() - 1);
	SelectNaia(NaiaEntries[Next].NaiaId);
	if (const TObjectPtr<UNUStarterNaiaCardWidget>* Card = NaiaCards.Find(SelectedNaiaId))
	{
		if (*Card != nullptr && (*Card)->GetFocusTarget() != nullptr)
		{
			(*Card)->GetFocusTarget()->SetFocus();
		}
	}
}

FReply UNUStarterNaiaSelectionWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (UsesStage() && ActiveConfirmDialog == nullptr)
	{
		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Left || Key == EKeys::Gamepad_DPad_Left || Key == EKeys::A)
		{
			StepSelection(-1);
			return FReply::Handled();
		}
		if (Key == EKeys::Right || Key == EKeys::Gamepad_DPad_Right || Key == EKeys::D)
		{
			StepSelection(1);
			return FReply::Handled();
		}
		if (Key == EKeys::F || Key == EKeys::Gamepad_FaceButton_Left)
		{
			const UWidget* ConfirmWidget = GetWidgetFromName(TEXT("ConfirmButton"));
			if (ConfirmWidget != nullptr && ConfirmWidget->GetIsEnabled())
			{
				HandleConfirmClicked();
			}
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
