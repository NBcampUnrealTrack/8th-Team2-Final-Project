#include "UI/CharacterMenu/NUCharacterNaiaPage.h"

#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUPassiveDefinition.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterState/NUSpiritState.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Data/NUNaiaAffection.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "Naia/Data/NUNaiaTypes.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"

namespace
{
	constexpr int32 PartySlotWidgets = 4;
	constexpr int32 StorageSlotWidgets = 12;
	// The designer named the two skill rows after the sample Naia's skills; they hold the first and second active skill.
	const TCHAR* const SkillRows[] = { TEXT("ActiveSkill2101"), TEXT("ActiveSkill2103") };

	// Same names as the starter selection screen (its helper is private there).
	FText ElementName(const FGameplayTag& ElementTag)
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
}

TArray<FNUCharacterNaiaSlotView> NUCharacterNaiaView::CollectParty(const UNUNaiaRosterComponent& Roster, int32 SlotCount)
{
	TArray<FNUCharacterNaiaSlotView> Views;
	Views.SetNum(SlotCount);
	const TArray<TObjectPtr<ANUSpiritState>>& Party = Roster.GetParty();
	for (int32 SlotNo = 0; SlotNo < SlotCount; ++SlotNo)
	{
		FNUCharacterNaiaSlotView& View = Views[SlotNo];
		View.Container = NUNaiaContainer::Party;
		View.SlotNo = SlotNo;
		const ANUSpiritState* Naia = Party.IsValidIndex(SlotNo) ? Party[SlotNo].Get() : nullptr;
		if (!IsValid(Naia))
		{
			continue;
		}
		View.bFilled = true;
		View.InstanceId = Naia->GetInstanceId();
		View.NaiaId = Naia->GetSpiritId();
		View.Level = Naia->GetCurrentLevel();
		View.Affection = Naia->GetAffection();
		View.PassiveSkillId = Naia->GetPassiveSkillId();
		View.ActiveSkillIds = Naia->GetActiveSkillIds();
		const UNUAbilitySystemComponent* ASC = Naia->GetNUASC();
		if (const UNUVitalAttributeSet* Vital = IsValid(ASC) ? ASC->GetVitalAttributeSet() : nullptr)
		{
			View.HealthRatio = Vital->GetMaxHealth() > KINDA_SMALL_NUMBER ? FMath::Clamp(Vital->GetHealth() / Vital->GetMaxHealth(), 0.f, 1.f) : 0.f;
		}
	}
	return Views;
}

TArray<FNUCharacterNaiaSlotView> NUCharacterNaiaView::CollectStorage(const TArray<FNUNaiaInstance>& Instances, int32 SlotCount)
{
	TArray<FNUCharacterNaiaSlotView> Views;
	Views.SetNum(SlotCount);
	for (int32 SlotNo = 0; SlotNo < SlotCount; ++SlotNo)
	{
		Views[SlotNo].Container = NUNaiaContainer::Storage;
		Views[SlotNo].SlotNo = SlotNo;
	}
	for (const FNUNaiaInstance& Instance : Instances)
	{
		if (!Views.IsValidIndex(Instance.SlotNo) || Instance.NaiaId <= 0)
		{
			continue;
		}
		FNUCharacterNaiaSlotView& View = Views[Instance.SlotNo];
		View.bFilled = true;
		View.InstanceId = Instance.InstanceId;
		View.NaiaId = Instance.NaiaId;
		View.Level = Instance.Level;
		View.Affection = Instance.Affection;
		View.HealthRatio = FMath::Clamp(Instance.HealthRatio, 0.f, 1.f);
		View.PassiveSkillId = Instance.PassiveSkillId;
		View.ActiveSkillIds = Instance.ActiveSkillIds;
	}
	return Views;
}

bool NUCharacterNaiaView::ResolveSelection(const TArray<FNUCharacterNaiaSlotView>& Party, const TArray<FNUCharacterNaiaSlotView>& Storage,
	int64 PreviousInstanceId, int32& OutContainer, int32& OutSlotNo)
{
	const FNUCharacterNaiaSlotView* Found = nullptr;
	if (PreviousInstanceId != 0)
	{
		const auto Same = [PreviousInstanceId](const FNUCharacterNaiaSlotView& View) { return View.bFilled && View.InstanceId == PreviousInstanceId; };
		Found = Party.FindByPredicate(Same);
		Found = Found ? Found : Storage.FindByPredicate(Same);
	}
	const auto Filled = [](const FNUCharacterNaiaSlotView& View) { return View.bFilled; };
	Found = Found ? Found : Party.FindByPredicate(Filled);
	Found = Found ? Found : Storage.FindByPredicate(Filled);
	OutContainer = Found ? Found->Container : 0;
	OutSlotNo = Found ? Found->SlotNo : INDEX_NONE;
	return Found != nullptr;
}

int32 NUCharacterNaiaView::FindFreeSlot(const TArray<FNUCharacterNaiaSlotView>& Slots)
{
	const int32 Index = Slots.IndexOfByPredicate([](const FNUCharacterNaiaSlotView& View) { return !View.bFilled; });
	return Index == INDEX_NONE ? INDEX_NONE : Slots[Index].SlotNo;
}

void UNUCharacterNaiaPage::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtons(true);
	if (UNUNaiaRosterComponent* Roster = GetRoster())
	{
		Roster->OnRosterChanged.AddUniqueDynamic(this, &ThisClass::HandleRosterChanged);
		BoundRoster = Roster;
	}
	Refresh();
}

void UNUCharacterNaiaPage::NativeDestruct()
{
	if (UNUNaiaRosterComponent* Roster = BoundRoster.Get())
	{
		Roster->OnRosterChanged.RemoveDynamic(this, &ThisClass::HandleRosterChanged);
	}
	BoundRoster.Reset();
	BindButtons(false);
	Super::NativeDestruct();
}

UNUNaiaRosterComponent* UNUCharacterNaiaPage::GetRoster() const
{
	const ANUPlayerState* State = Cast<ANUPlayerState>(GetOwningPlayerState());
	return State ? State->GetNaiaRosterComponent() : nullptr;
}

void UNUCharacterNaiaPage::BindButtons(bool bBind)
{
	const auto Bind = [this, bBind](const FString& Name, const FName& HandlerName)
	{
		UButton* Button = Cast<UButton>(GetWidgetFromName(*Name));
		if (!Button)
		{
			return;
		}
		FScriptDelegate Delegate;
		Delegate.BindUFunction(this, HandlerName);
		if (bBind)
		{
			Button->OnClicked.AddUnique(Delegate);
		}
		else
		{
			Button->OnClicked.Remove(Delegate);
		}
	};
	static const FName PartyHandlers[] = { GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandlePartySlot0), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandlePartySlot1),
		GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandlePartySlot2), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandlePartySlot3) };
	static const FName StorageHandlers[] = { GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot0), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot1),
		GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot2), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot3),
		GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot4), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot5),
		GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot6), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot7),
		GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot8), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot9),
		GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot10), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleStorageSlot11) };
	for (int32 Index = 0; Index < PartySlotWidgets; ++Index)
	{
		Bind(FString::Printf(TEXT("PartySlot%dButton"), Index), PartyHandlers[Index]);
	}
	for (int32 Index = 0; Index < StorageSlotWidgets; ++Index)
	{
		Bind(FString::Printf(TEXT("StorageSlot%dButton"), Index), StorageHandlers[Index]);
	}
	Bind(TEXT("MoveToStorageButton"), GET_FUNCTION_NAME_CHECKED(UNUCharacterNaiaPage, HandleMoveClicked));
}

void UNUCharacterNaiaPage::Refresh()
{
	const UNUNaiaRosterComponent* Roster = GetRoster();
	const int32 PartyCount = Roster ? FMath::Min(Roster->GetMaxPartySize(), PartySlotWidgets) : 0;
	const int32 StorageCount = Roster ? FMath::Min(Roster->GetMaxStorageSlots(), StorageSlotWidgets) : 0;
	PartyViews = Roster ? NUCharacterNaiaView::CollectParty(*Roster, PartyCount) : TArray<FNUCharacterNaiaSlotView>();
	StorageViews = Roster ? NUCharacterNaiaView::CollectStorage(Roster->GetStorage(), StorageCount) : TArray<FNUCharacterNaiaSlotView>();
	NUCharacterNaiaView::ResolveSelection(PartyViews, StorageViews, SelectedInstanceId, SelectedContainer, SelectedSlotNo);
	const FNUCharacterNaiaSlotView* Selected = FindView(SelectedContainer, SelectedSlotNo);
	SelectedInstanceId = Selected ? Selected->InstanceId : 0;
	FillSlots();
	FillDetail();
}

void UNUCharacterNaiaPage::SelectSlot(int32 Container, int32 SlotNo)
{
	const FNUCharacterNaiaSlotView* View = FindView(Container, SlotNo);
	if (!View || !View->bFilled)
	{
		return; // empty slots keep the current detail
	}
	SelectedContainer = Container;
	SelectedSlotNo = SlotNo;
	SelectedInstanceId = View->InstanceId;
	FillSlots();
	FillDetail();
}

void UNUCharacterNaiaPage::HandleRosterChanged()
{
	Refresh();
}

void UNUCharacterNaiaPage::HandleMoveClicked()
{
	UNUNaiaRosterComponent* Roster = GetRoster();
	const FNUCharacterNaiaSlotView* View = FindView(SelectedContainer, SelectedSlotNo);
	if (!Roster || !View || !View->bFilled)
	{
		return;
	}
	const bool bToStorage = View->Container == NUNaiaContainer::Party;
	const int32 Target = NUCharacterNaiaView::FindFreeSlot(bToStorage ? StorageViews : PartyViews);
	if (Target == INDEX_NONE)
	{
		return; // destination full; the label already says so
	}
	// The selection follows the Naia by instance id when the roster answers.
	Roster->ServerMoveNaia(View->Container, View->SlotNo, bToStorage ? NUNaiaContainer::Storage : NUNaiaContainer::Party, Target);
}

const FNUCharacterNaiaSlotView* UNUCharacterNaiaPage::FindView(int32 Container, int32 SlotNo) const
{
	const TArray<FNUCharacterNaiaSlotView>& Views = Container == NUNaiaContainer::Party ? PartyViews : StorageViews;
	return Container > 0 && Views.IsValidIndex(SlotNo) ? &Views[SlotNo] : nullptr;
}

void UNUCharacterNaiaPage::SetText(const TCHAR* Name, const FText& Text) const
{
	if (UTextBlock* Block = Cast<UTextBlock>(GetWidgetFromName(Name)))
	{
		Block->SetText(Text);
	}
}

void UNUCharacterNaiaPage::SetShown(const TCHAR* Name, bool bShown) const
{
	if (UWidget* Widget = GetWidgetFromName(Name))
	{
		// Hidden keeps the slot's layout; the designer sized every slot already.
		Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UNUCharacterNaiaPage::SetPortrait(const TCHAR* Name, int32 NaiaId, float TargetHeight) const
{
	UImage* Image = Cast<UImage>(GetWidgetFromName(Name));
	if (!Image)
	{
		return;
	}
	const UGameInstance* GI = GetGameInstance();
	const UNUNaiaDataSubsystem* Data = GI ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const FNUNaiaDefinition* Definition = Data && NaiaId > 0 ? Data->FindDefinitionById(NaiaId) : nullptr;
	UObject* Resource = nullptr;
	if (UPaperFlipbook* Flipbook = Definition ? Definition->PreviewFlipbook.LoadSynchronous() : nullptr)
	{
		Resource = Flipbook->GetSpriteAtFrame(0); // first frame keeps the pixel art; the page does not animate
	}
	if (!Resource && Definition)
	{
		Resource = Definition->IconTexture.LoadSynchronous();
	}
	if (Resource)
	{
		// The designer left the portrait wells as NoDrawType placeholders; draw the art at its own size.
		FSlateBrush Brush = Image->GetBrush();
		Brush.SetResourceObject(Resource);
		Brush.DrawAs = ESlateBrushDrawType::Image;
		if (const UPaperSprite* Sprite = Cast<UPaperSprite>(Resource))
		{
			Brush.ImageSize = Sprite->GetSlateAtlasData().GetSourceDimensions();
		}
		else if (const UTexture2D* Texture = Cast<UTexture2D>(Resource))
		{
			Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
		}
		if (TargetHeight > 0.f && Brush.ImageSize.Y > 0.f)
		{
			Brush.ImageSize *= FMath::Max(1.f, FMath::FloorToFloat(TargetHeight / Brush.ImageSize.Y));
		}
		Image->SetBrush(Brush);
	}
	Image->SetVisibility(Resource ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
}

void UNUCharacterNaiaPage::FillSlots()
{
	const UGameInstance* GI = GetGameInstance();
	const UNUNaiaDataSubsystem* Data = GI ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const auto Level = [](int32 InLevel) { return FText::Format(NSLOCTEXT("NUCharacterMenu", "NaiaSlotLevel", "Lv.{0}"), FText::AsNumber(InLevel)); };
	const auto Count = [](const TArray<FNUCharacterNaiaSlotView>& Views)
	{
		const int32 Used = Views.FilterByPredicate([](const FNUCharacterNaiaSlotView& View) { return View.bFilled; }).Num();
		return FText::Format(NSLOCTEXT("NUCharacterMenu", "NaiaSlotCount", "{0} / {1}"), FText::AsNumber(Used), FText::AsNumber(Views.Num()));
	};
	SetText(TEXT("PartyHeaderCount"), Count(PartyViews));
	SetText(TEXT("StorageHeaderCount"), Count(StorageViews));

	for (int32 Index = 0; Index < PartySlotWidgets; ++Index)
	{
		const FNUCharacterNaiaSlotView* View = PartyViews.IsValidIndex(Index) ? &PartyViews[Index] : nullptr;
		const bool bFilled = View && View->bFilled;
		const FString Prefix = FString::Printf(TEXT("PartySlot%d"), Index);
		SetShown(*(Prefix + TEXT("Level")), bFilled);
		SetShown(*(Prefix + TEXT("HealthTrack")), bFilled);
		SetShown(*(Prefix + TEXT("HealthFill")), bFilled);
		SetShown(*(Prefix + TEXT("Empty")), !bFilled);
		SetText(*(Prefix + TEXT("Number")), FText::AsNumber(Index + 1));
		if (bFilled)
		{
			SetText(*(Prefix + TEXT("Level")), Level(View->Level));
			// The designer drew the bar as two images: the fill shrinks from its left edge.
			if (UWidget* Fill = GetWidgetFromName(*(Prefix + TEXT("HealthFill"))))
			{
				if (UProgressBar* Bar = Cast<UProgressBar>(Fill))
				{
					Bar->SetPercent(View->HealthRatio);
				}
				else
				{
					Fill->SetRenderTransformPivot(FVector2D(0.f, .5f));
					Fill->SetRenderScale(FVector2D(FMath::Max(View->HealthRatio, 0.001f), 1.f));
				}
			}
		}
		SetPortrait(*(Prefix + TEXT("Portrait")), bFilled ? View->NaiaId : 0);
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(*(Prefix + TEXT("Button")))))
		{
			Button->SetIsEnabled(View != nullptr);
			Button->SetRenderOpacity(bFilled && SelectedContainer == NUNaiaContainer::Party && SelectedSlotNo == Index ? 1.f : (bFilled ? .82f : .55f));
		}
	}

	for (int32 Index = 0; Index < StorageSlotWidgets; ++Index)
	{
		const FNUCharacterNaiaSlotView* View = StorageViews.IsValidIndex(Index) ? &StorageViews[Index] : nullptr;
		const bool bFilled = View && View->bFilled;
		const FString Prefix = FString::Printf(TEXT("StorageSlot%d"), Index);
		const FNUNaiaDefinition* Definition = bFilled && Data ? Data->FindDefinitionById(View->NaiaId) : nullptr;
		SetShown(*(Prefix + TEXT("Name")), bFilled);
		SetShown(*(Prefix + TEXT("Level")), bFilled);
		SetShown(*(Prefix + TEXT("Rarity")), bFilled);
		SetShown(*(Prefix + TEXT("EmptyMark")), !bFilled);
		if (bFilled)
		{
			SetText(*(Prefix + TEXT("Name")), Definition && !Definition->KoreanName.IsEmpty() ? Definition->KoreanName : FText::AsNumber(View->NaiaId));
			SetText(*(Prefix + TEXT("Level")), Level(View->Level));
		}
		SetPortrait(*(Prefix + TEXT("Portrait")), bFilled ? View->NaiaId : 0);
		if (UButton* Button = Cast<UButton>(GetWidgetFromName(*(Prefix + TEXT("Button")))))
		{
			Button->SetIsEnabled(View != nullptr);
			Button->SetRenderOpacity(bFilled && SelectedContainer == NUNaiaContainer::Storage && SelectedSlotNo == Index ? 1.f : (bFilled ? .82f : .55f));
		}
	}
}

void UNUCharacterNaiaPage::FillDetail()
{
	const FNUCharacterNaiaSlotView* View = FindView(SelectedContainer, SelectedSlotNo);
	const bool bHas = View && View->bFilled;
	const UGameInstance* GI = GetGameInstance();
	const UNUNaiaDataSubsystem* Data = GI ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const UNUBattleDataSubsystem* Battle = GI ? GI->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;
	const FNUNaiaDefinition* Definition = bHas && Data ? Data->FindDefinitionById(View->NaiaId) : nullptr;
	const FText Missing = FText::FromString(TEXT("—"));

	SetText(TEXT("SelectedNaiaName"), bHas
		? (Definition && !Definition->KoreanName.IsEmpty() ? Definition->KoreanName : FText::AsNumber(View->NaiaId))
		: NSLOCTEXT("NUCharacterMenu", "NaiaNone", "선택 없음"));
	SetText(TEXT("SelectedNaiaLevel"), bHas ? FText::Format(NSLOCTEXT("NUCharacterMenu", "NaiaLevel", "Lv.{0}"), FText::AsNumber(View->Level)) : FText::GetEmpty());
	SetText(TEXT("SelectedNaiaClassification"), Definition
		? FText::Format(NSLOCTEXT("NUCharacterMenu", "NaiaElement", "{0} 속성"), ElementName(Definition->ElementTag))
		: FText::GetEmpty());
	SetPortrait(TEXT("SelectedNaiaPortrait"), bHas ? View->NaiaId : 0, 150.f);
	SetText(TEXT("SelectedLocationLabel"), !bHas ? FText::GetEmpty() : View->Container == NUNaiaContainer::Party
		? (View->SlotNo == 0 ? NSLOCTEXT("NUCharacterMenu", "NaiaActive", "전투 참가 중") : FText::Format(NSLOCTEXT("NUCharacterMenu", "NaiaPartySlot", "대기 목록 {0}번"), FText::AsNumber(View->SlotNo + 1)))
		: NSLOCTEXT("NUCharacterMenu", "NaiaInStorage", "보관함"));

	// Stats from the definition at this level and bond tier --- the same rule the server uses for every slot.
	const int32 Tier = bHas ? NUNaiaAffection::ResolveTier(View->Affection) : 0;
	const FNUNaiaStats Stats = Definition ? Definition->ResolveStats(View->Level, Tier) : FNUNaiaStats();
	const auto Number = [&](float Value) { return Definition ? FText::AsNumber(FMath::RoundToInt(Value)) : Missing; };
	SetText(TEXT("StatMaxHealthValue"), Number(Stats.MaxHealth));
	SetText(TEXT("StatMaxSPValue"), Number(Stats.MaxSP));
	SetText(TEXT("StatAttackPowerValue"), Number(Stats.AttackPower));
	SetText(TEXT("StatMagicAttackPowerValue"), Number(Stats.MagicAttackPower));
	SetText(TEXT("StatDefenseValue"), Number(Stats.Defense));
	SetText(TEXT("StatSpeedValue"), Number(Stats.Speed));
	SetText(TEXT("StatCriticalRateValue"), Definition ? FText::FromString(FString::Printf(TEXT("%.0f%%"), Stats.CriticalRate * 100.f)) : Missing);
	SetText(TEXT("StatCriticalDamageValue"), Definition ? FText::FromString(FString::Printf(TEXT("%.0f%%"), Stats.CriticalDamage * 100.f)) : Missing);

	for (int32 Row = 0; Row < UE_ARRAY_COUNT(SkillRows); ++Row)
	{
		const int32 SkillId = bHas && View->ActiveSkillIds.IsValidIndex(Row) ? View->ActiveSkillIds[Row] : 0;
		const FNUActiveSkillDefinition* Skill = Battle && SkillId > 0 ? Battle->FindActiveSkillDefinition(SkillId) : nullptr;
		const FString Prefix = SkillRows[Row];
		SetShown(*(Prefix + TEXT("Name")), Skill != nullptr);
		SetShown(*(Prefix + TEXT("Cost")), Skill != nullptr);
		SetShown(*(Prefix + TEXT("Mark")), Skill != nullptr);
		if (Skill)
		{
			SetText(*(Prefix + TEXT("Name")), Skill->KoreanName);
			SetText(*(Prefix + TEXT("Cost")), FText::Format(NSLOCTEXT("NUCharacterMenu", "NaiaSkillCost", "SP {0}"), FText::AsNumber(Skill->SPCost)));
		}
	}

	const FNUPassiveDefinition* Passive = Battle && bHas && View->PassiveSkillId > 0 ? Battle->FindPassiveDefinition(View->PassiveSkillId) : nullptr;
	SetShown(TEXT("PassiveName"), Passive != nullptr);
	SetShown(TEXT("PassiveKind"), Passive != nullptr);
	SetShown(TEXT("PassiveDescription"), Passive != nullptr);
	SetShown(TEXT("PassiveEmptyMark"), Passive == nullptr);
	if (Passive)
	{
		SetText(TEXT("PassiveName"), Passive->KoreanName);
		SetText(TEXT("PassiveKind"), NSLOCTEXT("NUCharacterMenu", "NaiaPassiveKind", "패시브"));
		SetText(TEXT("PassiveDescription"), Passive->Description);
	}

	// Move button: party -> storage or back; disabled when nothing is selected or the destination is full.
	const bool bToStorage = bHas && View->Container == NUNaiaContainer::Party;
	const bool bRoom = bHas && NUCharacterNaiaView::FindFreeSlot(bToStorage ? StorageViews : PartyViews) != INDEX_NONE;
	SetText(TEXT("MoveToStorageLabel"), !bHas ? NSLOCTEXT("NUCharacterMenu", "NaiaMoveNone", "보관함으로 보내기")
		: bToStorage ? (bRoom ? NSLOCTEXT("NUCharacterMenu", "NaiaMoveToStorage", "보관함으로 보내기") : NSLOCTEXT("NUCharacterMenu", "NaiaStorageFull", "보관함이 가득 찼어요"))
		: (bRoom ? NSLOCTEXT("NUCharacterMenu", "NaiaMoveToParty", "대기 목록에 넣기") : NSLOCTEXT("NUCharacterMenu", "NaiaPartyFull", "대기 목록이 가득 찼어요")));
	if (UButton* Move = Cast<UButton>(GetWidgetFromName(TEXT("MoveToStorageButton"))))
	{
		Move->SetIsEnabled(bRoom);
		Move->SetRenderOpacity(bRoom ? 1.f : .45f); // the designer dimmed the placeholder button
	}
}
