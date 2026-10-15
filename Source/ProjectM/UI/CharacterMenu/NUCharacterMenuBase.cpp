#include "UI/CharacterMenu/NUCharacterMenuBase.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/NamedSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "Inventory/NUInventoryComponent.h"
#include "UI/CharacterMenu/NUCharacterMenuMotion.h"
#include "UI/NUUIManager.h"

namespace NUCharacterMenuNav
{
	// Indexed by serialized enum value, independently of the WBP's visual tab order.
	const TCHAR* Prefix(ENUCharacterMenuSection Section)
	{
		static const TCHAR* Prefixes[] = { TEXT("NavCharacter"), TEXT("NavEquipment"), TEXT("NavStats"), TEXT("NavTraits"), TEXT("NavNaia"), TEXT("NavInventory") };
		const int32 Index = static_cast<int32>(Section);
		return Index >= 0 && Index < UE_ARRAY_COUNT(Prefixes) ? Prefixes[Index] : Prefixes[0];
	}

	FVector2D ButtonPosition(const UUserWidget& Menu, ENUCharacterMenuSection Section)
	{
		const UWidget* Button = Menu.GetWidgetFromName(FName(FString(Prefix(Section)) + TEXT("Button")));
		const UCanvasPanelSlot* Slot = Button ? Cast<UCanvasPanelSlot>(Button->Slot) : nullptr;
		return Slot ? Slot->GetPosition() : FVector2D::ZeroVector;
	}

	// Optional single selection pill (NavSlider) that travels between rail items.
	void MoveSlider(UUserWidget& Menu, ENUCharacterMenuSection Section, bool bAnimate)
	{
		UWidget* Slider = Menu.GetWidgetFromName(TEXT("NavSlider"));
		const UCanvasPanelSlot* Slot = Slider ? Cast<UCanvasPanelSlot>(Slider->Slot) : nullptr;
		if (!Slot)
		{
			return;
		}
		NUCharacterMenuMotion::SlideTo(&Menu, Slider, ButtonPosition(Menu, Section) - Slot->GetPosition(), bAnimate ? 0.24f : 0.f);
	}
}

UNUCharacterMenuBase::UNUCharacterMenuBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

UNUCharacterMenuBase* UNUCharacterMenuBase::Show(APlayerController* OwningPlayer,
	TSubclassOf<UNUCharacterMenuBase> WidgetClass, ENUCharacterMenuSection InitialSection)
{
	UNUUIManager* UI = OwningPlayer ? UNUUIManager::Get(OwningPlayer) : nullptr;
	if (!UI || !WidgetClass) return nullptr;
	UNUCharacterMenuBase* Window = CreateWidget<UNUCharacterMenuBase>(OwningPlayer, WidgetClass);
	if (!Window) return nullptr;
	Window->SelectedSection = InitialSection;
	FNUUIWindowParams Params;
	Params.Layer = NUUILayer::Window;
	Params.bDimBackground = false; // The shell owns its backdrop blur and tint.
	Params.bBlockGameInput = true;
	UI->OpenWindow(Window, Params, FSimpleDelegate::CreateUObject(Window, &ThisClass::RequestClose));
	return Window;
}

void UNUCharacterMenuBase::NativePreConstruct()
{
	Super::NativePreConstruct();
	UpdateNavigation();
}

void UNUCharacterMenuBase::NativeConstruct()
{
	Super::NativeConstruct();
	NavCharacterButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCharacter);
	NavEquipmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleEquipment);
	if (NavInventoryButton) NavInventoryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleInventory);
	NavStatsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleStats);
	NavTraitsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleTraits);
	NavNaiaButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleNaia);
	CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestClose);
	ProfileState = Cast<ANUPlayerState>(GetOwningPlayerState());
	if (ANUPlayerState* State = ProfileState.Get())
	{
		if (auto* Progression = State->GetProgressionComponent()) Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::RefreshProfile);
		if (auto* Job = State->GetJobComponent()) Job->OnJobChanged.AddUniqueDynamic(this, &ThisClass::RefreshProfile);
		if (auto* Inventory = State->GetInventoryComponent()) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshProfile);
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		ProfileCharacters = GI->GetSubsystem<UNUCharacterManager>();
		if (ProfileCharacters.IsValid()) ProfileCharacters->OnCharacterListChanged.AddUniqueDynamic(this, &ThisClass::RefreshProfile);
	}
	// Preserve content supplied by a designer using this WBP's exposed named slot.
	if (UWidget* Content = NamedSlot_PageContent->GetContent(); Content && !CachedPages.Contains(SelectedSection))
	{
		CachedPages.Add(SelectedSection, Content);
	}
	DisplaySelectedPage();
	UpdateNavigation();
	RefreshProfile();
	NUCharacterMenuNav::MoveSlider(*this, SelectedSection, false);
	// The dim backdrop (blur + tint) is there from the first frame; only the panels move in.
	// The WBP's Construct still starts OpenFade (a dip through black over the whole screen), so cut it.
	if (const UWidgetBlueprintGeneratedClass* Class = Cast<UWidgetBlueprintGeneratedClass>(GetClass()))
	{
		for (UWidgetAnimation* Animation : Class->Animations)
		{
			if (Animation && Animation->GetName().StartsWith(TEXT("OpenFade"))) StopAnimation(Animation);
		}
	}
	for (const TCHAR* Name : { TEXT("WorldBlur"), TEXT("WorldTint"), TEXT("ShellCanvas") })
	{
		if (UWidget* Layer = GetWidgetFromName(Name)) Layer->SetRenderOpacity(1.f);
	}
	if (UWidget* Overlay = GetWidgetFromName(TEXT("OpenFadeOverlay"))) Overlay->SetRenderOpacity(0.f);
	NUCharacterMenuMotion::OpenShell(this);
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		// Window-layer menus are outside the manager's automatic popup registration.
		UI->SetPopupButtonHover(this, true);
	}
}

void UNUCharacterMenuBase::NativeDestruct()
{
	NUCharacterMenuMotion::Stop(this);
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->SetPopupButtonHover(this, false);
		for (const TPair<ENUCharacterMenuSection, TObjectPtr<UWidget>>& Page : CachedPages)
		{
			if (Page.Value) UI->SetPopupButtonHover(Page.Value, false);
		}
	}
	if (ANUPlayerState* State = ProfileState.Get())
	{
		if (auto* Progression = State->GetProgressionComponent()) Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::RefreshProfile);
		if (auto* Job = State->GetJobComponent()) Job->OnJobChanged.RemoveDynamic(this, &ThisClass::RefreshProfile);
		if (auto* Inventory = State->GetInventoryComponent()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshProfile);
	}
	if (ProfileCharacters.IsValid()) ProfileCharacters->OnCharacterListChanged.RemoveDynamic(this, &ThisClass::RefreshProfile);
	ProfileState.Reset();
	ProfileCharacters.Reset();
	Super::NativeDestruct();
}

void UNUCharacterMenuBase::SelectSection(ENUCharacterMenuSection Section)
{
	if (static_cast<uint8>(Section) > static_cast<uint8>(ENUCharacterMenuSection::Inventory)) return;
	const bool bChanged = SelectedSection != Section;
	const ENUCharacterMenuSection Previous = SelectedSection;
	SelectedSection = Section;
	if (bChanged)
	{
		// Content and heading move in the direction of travel on the rail (down = from below).
		const float Travel = NUCharacterMenuNav::ButtonPosition(*this, Section).Y - NUCharacterMenuNav::ButtonPosition(*this, Previous).Y;
		NUCharacterMenuMotion::SwapHeading(this, GetWidgetFromName(TEXT("PageTitleText")), FMath::Sign(Travel));
		NUCharacterMenuNav::MoveSlider(*this, Section, true);
	}
	DisplaySelectedPage();
	UpdateNavigation();
	if (bChanged)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Tab);
		OnSectionChanged.Broadcast(Section);
	}
}

void UNUCharacterMenuBase::RegisterPage(ENUCharacterMenuSection Section, UWidget* Page)
{
	if (Page == this || static_cast<uint8>(Section) > static_cast<uint8>(ENUCharacterMenuSection::Inventory)) return;
	if (Page) CachedPages.Add(Section, Page);
	else CachedPages.Remove(Section);
	if (Section == SelectedSection) DisplaySelectedPage();
}

void UNUCharacterMenuBase::DisplaySelectedPage()
{
	if (!NamedSlot_PageContent) return;
	if (!CachedPages.Contains(SelectedSection))
	{
		const TSubclassOf<UUserWidget>* PageClass = PageClasses.Find(SelectedSection);
		if (PageClass && *PageClass && GetOwningPlayer())
		{
			if (UUserWidget* Page = CreateWidget<UUserWidget>(GetOwningPlayer(), *PageClass))
			{
				CachedPages.Add(SelectedSection, Page);
			}
		}
	}
	UWidget* Page = CachedPages.FindRef(SelectedSection);
	if (NamedSlot_PageContent->GetContent() != Page)
	{
		NamedSlot_PageContent->SetContent(Page);
		NUCharacterMenuMotion::EnterPage(this, Page);
		if (UNUUIManager* UI = Page ? UNUUIManager::Get(GetOwningPlayer()) : nullptr)
		{
			UI->SetPopupButtonHover(Page, true);
		}
	}
}

void UNUCharacterMenuBase::UpdateNavigation()
{
	// Indexed by serialized enum value, independently of the WBP's visual tab order.
	static const TCHAR* Prefixes[] = { TEXT("NavCharacter"), TEXT("NavEquipment"), TEXT("NavStats"), TEXT("NavTraits"), TEXT("NavNaia"), TEXT("NavInventory") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Prefixes); ++Index)
	{
		const bool bSelected = Index == static_cast<int32>(SelectedSection);
		const FLinearColor Color = bSelected ? SelectedNavigationColor : IdleNavigationColor;
		const FString Prefix(Prefixes[Index]);
		if (UWidget* Selection = GetWidgetFromName(FName(Prefix + TEXT("Selected"))))
			Selection->SetVisibility(bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UImage* Icon = Cast<UImage>(GetWidgetFromName(FName(Prefix + TEXT("Icon"))))) Icon->SetColorAndOpacity(Color);
		if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(FName(Prefix + TEXT("Label")))))
		{
			Label->SetColorAndOpacity(FSlateColor(Color));
			// Optional large page heading follows the selected tab's own label.
			if (bSelected)
				if (UTextBlock* Title = Cast<UTextBlock>(GetWidgetFromName(TEXT("PageTitleText")))) Title->SetText(Label->GetText());
		}
	}
}

void UNUCharacterMenuBase::RefreshProfile()
{
	// Artwork and cooked texture references stay in the WBP. Match the base job's
	// stable JobKey; advancement display names must not change its class emblem.
	const auto Emblem = [this](FName JobKey)
	{
		UWidgetSwitcher* Switcher = Cast<UWidgetSwitcher>(GetWidgetFromName(TEXT("ProfileJobEmblemSwitcher")));
		if (!Switcher) return; // Older shells still use their static compass.
		UWidget* Icon = JobKey.IsNone() ? nullptr
			: GetWidgetFromName(FName(FString(TEXT("JobEmblem_")) + JobKey.ToString()));
		if (Icon && Icon->GetParent() == Switcher) Switcher->SetActiveWidget(Icon);
		else Switcher->SetActiveWidgetIndex(0); // Novice, missing state or an unknown job.
	};
	Emblem(NAME_None);
	const auto Text = [this](FName Name, const FText& Value)
	{
		if (UTextBlock* Widget = Cast<UTextBlock>(GetWidgetFromName(Name))) Widget->SetText(Value);
	};
	Text(TEXT("ProfileName"), NSLOCTEXT("NUCharacterMenu", "Adventurer", "모험가"));
	Text(TEXT("ProfileJob"), NSLOCTEXT("NUCharacterMenu", "Novice", "노비스"));
	Text(TEXT("ProfileLevel"), FText::FromString(TEXT("Lv. —")));
	Text(TEXT("ProfileExperience"), FText::FromString(TEXT("EXP —")));
	Text(TEXT("HeaderGoldText"), FText::FromString(TEXT("—")));
	UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("ProfileExperienceBar")));
	if (Bar) Bar->SetPercent(0.f);
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	FNUCharacterSummary Summary;
	const UNUCharacterManager* Characters = GI->GetSubsystem<UNUCharacterManager>();
	if (Characters && Characters->GetSelectedCharacter(Summary) && !Summary.Name.IsEmpty())
		Text(TEXT("ProfileName"), FText::FromString(Summary.Name));
	const ANUPlayerState* State = Cast<ANUPlayerState>(GetOwningPlayerState());
	if (!State) return;
	if (const UNUInventoryComponent* Inventory = State->GetInventoryComponent())
		Text(TEXT("HeaderGoldText"), FText::Format(NSLOCTEXT("NUCharacterMenu", "HeaderGold", "{0} G"), FText::AsNumber(Inventory->GetGold())));
	if (const UNUProgressionComponent* Progression = State->GetProgressionComponent())
	{
		Text(TEXT("ProfileLevel"), FText::Format(NSLOCTEXT("NUCharacterMenu", "Level", "Lv. {0}"), FText::AsNumber(Progression->GetLevel())));
		const int64 Required = Progression->GetRequiredExperienceToNext();
		Text(TEXT("ProfileExperience"), Required > 0
			? FText::Format(NSLOCTEXT("NUCharacterMenu", "Experience", "EXP {0} / {1}"), FText::AsNumber(Progression->GetExperienceIntoLevel()), FText::AsNumber(Required))
			: NSLOCTEXT("NUCharacterMenu", "MaxLevel", "최고 레벨"));
		if (Bar) Bar->SetPercent(FMath::Clamp(Progression->GetExperienceProgressRatio(), 0.f, 1.f));
	}
	const UNUProgressionDataSubsystem* Data = GI->GetSubsystem<UNUProgressionDataSubsystem>();
	const UNUJobComponent* Job = State->GetJobComponent();
	if (Data && Job)
	{
		if (const FNUJobDefinitionRow* Row = Data->FindJob(Job->GetJobId()))
		{
			Text(TEXT("ProfileJob"), Row->DisplayName);
			Emblem(Row->JobKey);
		}
		if (const FNUJobAdvancementDefinitionRow* Row = Data->FindAdvancement(Job->GetAdvancementId())) Text(TEXT("ProfileJob"), Row->DisplayName);
	}
}

void UNUCharacterMenuBase::RequestClose()
{
	if (bClosing) return; // Already fading out; repeated keys must not restart it.
	StopAllAnimations(); // An unfinished OpenFade would fight the exit over the same opacities.
	if (NUCharacterMenuMotion::DeferClose(this, FSimpleDelegate::CreateWeakLambda(this, [this]() { CloseImmediately(); })))
	{
		bClosing = true;
		SetVisibility(ESlateVisibility::HitTestInvisible);
		return;
	}
	if (!CloseFade || !IsInViewport())
	{
		CloseImmediately();
		return;
	}
	bClosing = true;
	// Clicks during the fade would reach a page that is about to disappear.
	SetVisibility(ESlateVisibility::HitTestInvisible);
	StopAllAnimations(); // An unfinished OpenFade would fight CloseFade over the same opacities.
	FWidgetAnimationDynamicEvent Finished;
	Finished.BindDynamic(this, &ThisClass::HandleCloseFadeFinished);
	UnbindAllFromAnimationFinished(CloseFade);
	BindToAnimationFinished(CloseFade, Finished);
	PlayAnimation(CloseFade);
}

void UNUCharacterMenuBase::HandleCloseFadeFinished()
{
	CloseImmediately();
}

void UNUCharacterMenuBase::CloseImmediately()
{
	if (CloseFade) UnbindAllFromAnimationFinished(CloseFade);
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()); UI && UI->IsWindowOpen(this)) UI->CloseWindow(this);
	else RemoveFromParent();
}

FReply UNUCharacterMenuBase::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (bClosing) return FReply::Handled();
	if (Event.GetKey() == EKeys::Escape || Event.GetKey() == EKeys::Gamepad_FaceButton_Right)
	{
		RequestClose();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}
