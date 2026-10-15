#include "UI/CharacterMenu/NUCharacterInfoPage.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUSpiritState.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Field/NUCharacterPreviewActor.h"
#include "Field/NUCharacterPreviewSubsystem.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUEquipmentTypes.h"
#include "Inventory/NUInventoryComponent.h"
#include "Inventory/UI/NUItemUI.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Naia/Component/NUNaiaRosterComponent.h"
#include "Naia/Data/NUNaiaAffection.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"
#include "UI/CharacterMenu/NUCharacterMenuBase.h"
#include "UI/CharacterMenu/NUCharacterMenuMotion.h"
#include "UI/NUFlipbookImage.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"

namespace NUCharacterInfoCards
{
	// Attributes whose changes repaint the vitals and stats cards.
	TArray<FGameplayAttribute, TInlineAllocator<9>> WatchedAttributes()
	{
		return { UNUVitalAttributeSet::GetHealthAttribute(), UNUVitalAttributeSet::GetMaxHealthAttribute(),
			UNUVitalAttributeSet::GetSPAttribute(), UNUVitalAttributeSet::GetMaxSPAttribute(),
			UNUCombatAttributeSet::GetAttackPowerAttribute(), UNUCombatAttributeSet::GetDefenseAttribute(),
			UNUCombatAttributeSet::GetMagicAttackPowerAttribute(), UNUCombatAttributeSet::GetSpeedAttribute(),
			UNUCombatAttributeSet::GetCriticalRateAttribute() };
	}
}

UNUCharacterInfoPage::UNUCharacterInfoPage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PortraitMaterial(FSoftObjectPath(TEXT("/Game/Developers/dbals/UI/M_NUCharacterPreview.M_NUCharacterPreview")))
{
}

void UNUCharacterInfoPage::NativeConstruct()
{
	Super::NativeConstruct();
	UnbindData();
	BoundState = Cast<ANUPlayerState>(GetOwningPlayerState());
	if (ANUPlayerState* State = BoundState.Get())
	{
		if (auto* Progression = State->GetProgressionComponent()) Progression->OnProgressionChanged.AddUniqueDynamic(this, &ThisClass::RefreshPage);
		if (auto* Job = State->GetJobComponent()) Job->OnJobChanged.AddUniqueDynamic(this, &ThisClass::RefreshPage);
		if (auto* Equipment = State->GetEquipmentComponent()) Equipment->OnEquipmentChanged.AddUniqueDynamic(this, &ThisClass::RefreshPage);
		if (auto* Inventory = State->GetInventoryComponent()) Inventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::RefreshPage);
		if (auto* Location = State->FindComponentByClass<UNUWorldLocationComponent>()) Location->OnLocationChanged.AddUniqueDynamic(this, &ThisClass::RefreshPage);
		if (UNUNaiaRosterComponent* Roster = State->GetNaiaRosterComponent()) Roster->OnRosterChanged.AddUniqueDynamic(this, &ThisClass::RefreshPage);
		if (UNUAbilitySystemComponent* ASC = State->GetNUASC())
		{
			for (const FGameplayAttribute& Attribute : NUCharacterInfoCards::WatchedAttributes())
				ASC->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &ThisClass::HandleAttributeChanged);
		}
	}
	// "나이아 보기" reuses the shell's own Naia tab handler. The page sits in the shell's
	// named slot, whose outer chain reaches the shell.
	if (UButton* NaiaView = Cast<UButton>(GetWidgetFromName(TEXT("NaiaViewButton"))))
	{
		if (UNUCharacterMenuBase* Menu = GetParent() ? GetParent()->GetTypedOuter<UNUCharacterMenuBase>() : nullptr)
		{
			FScriptDelegate ShowNaia;
			ShowNaia.BindUFunction(Menu, TEXT("HandleNaia"));
			NaiaView->OnClicked.AddUnique(ShowNaia);
		}
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		BoundCharacters = GI->GetSubsystem<UNUCharacterManager>();
		if (BoundCharacters.IsValid()) BoundCharacters->OnCharacterListChanged.AddUniqueDynamic(this, &ThisClass::RefreshPage);
	}
	RefreshPage();
}

void UNUCharacterInfoPage::NativeDestruct()
{
	UnbindData();
	ReleasePortrait();
	Super::NativeDestruct();
}

void UNUCharacterInfoPage::UnbindData()
{
	if (ANUPlayerState* State = BoundState.Get())
	{
		if (auto* Progression = State->GetProgressionComponent()) Progression->OnProgressionChanged.RemoveDynamic(this, &ThisClass::RefreshPage);
		if (auto* Job = State->GetJobComponent()) Job->OnJobChanged.RemoveDynamic(this, &ThisClass::RefreshPage);
		if (auto* Equipment = State->GetEquipmentComponent()) Equipment->OnEquipmentChanged.RemoveDynamic(this, &ThisClass::RefreshPage);
		if (auto* Inventory = State->GetInventoryComponent()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::RefreshPage);
		if (auto* Location = State->FindComponentByClass<UNUWorldLocationComponent>()) Location->OnLocationChanged.RemoveDynamic(this, &ThisClass::RefreshPage);
		if (UNUNaiaRosterComponent* Roster = State->GetNaiaRosterComponent()) Roster->OnRosterChanged.RemoveDynamic(this, &ThisClass::RefreshPage);
		if (UNUAbilitySystemComponent* ASC = State->GetNUASC())
		{
			for (const FGameplayAttribute& Attribute : NUCharacterInfoCards::WatchedAttributes())
				ASC->GetGameplayAttributeValueChangeDelegate(Attribute).RemoveAll(this);
		}
	}
	if (UButton* NaiaView = Cast<UButton>(GetWidgetFromName(TEXT("NaiaViewButton")))) NaiaView->OnClicked.Clear();
	if (BoundCharacters.IsValid()) BoundCharacters->OnCharacterListChanged.RemoveDynamic(this, &ThisClass::RefreshPage);
	BoundState.Reset();
	BoundCharacters.Reset();
}

void UNUCharacterInfoPage::RefreshPage()
{
	const auto Text = [this](FName Name, const FText& Value)
	{
		if (UTextBlock* Widget = Cast<UTextBlock>(GetWidgetFromName(Name))) Widget->SetText(Value);
	};
	const FText Missing = FText::FromString(TEXT("—"));
	Text(TEXT("NameText"), NSLOCTEXT("NUCharacterMenu", "Adventurer", "모험가"));
	Text(TEXT("JobText"), NSLOCTEXT("NUCharacterMenu", "Novice", "노비스"));
	for (const FName Name : { FName(TEXT("CharacterIdText")), FName(TEXT("LocationText")), FName(TEXT("GoldText")),
		FName(TEXT("LevelText")), FName(TEXT("ExperienceText")), FName(TEXT("ExperiencePercentText")) }) Text(Name, Missing);
	Text(TEXT("RaceText"), FText::GetEmpty()); // No placeholder dash under the job line when no character is selected.
	UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("ExperienceBar")));
	if (Bar) Bar->SetPercent(0.f);
	UGameInstance* GI = GetGameInstance();
	if (!GI) return;
	FNUCharacterSummary Selected;
	const UNUCharacterManager* Characters = GI->GetSubsystem<UNUCharacterManager>();
	if (Characters && Characters->GetSelectedCharacter(Selected))
	{
		if (!Selected.Name.IsEmpty()) Text(TEXT("NameText"), FText::FromString(Selected.Name));
		if (Selected.CharacterIdx > 0) Text(TEXT("CharacterIdText"), FText::FromString(LexToString(Selected.CharacterIdx)));
		Text(TEXT("RaceText"), NURace::GetDisplayName(Selected.RaceId));
	}
	ANUPlayerState* State = Cast<ANUPlayerState>(GetOwningPlayerState());
	if (!State) return;
	if (const UNUProgressionComponent* Progression = State->GetProgressionComponent())
	{
		Text(TEXT("LevelText"), FText::Format(NSLOCTEXT("NUCharacterMenu", "Level", "Lv. {0}"), FText::AsNumber(Progression->GetLevel())));
		const int64 Required = Progression->GetRequiredExperienceToNext();
		Text(TEXT("ExperienceText"), Required > 0
			? FText::Format(NSLOCTEXT("NUCharacterMenu", "Experience", "EXP {0} / {1}"), FText::AsNumber(Progression->GetExperienceIntoLevel()), FText::AsNumber(Required))
			: NSLOCTEXT("NUCharacterMenu", "MaxLevel", "최고 레벨"));
		Text(TEXT("ExperiencePercentText"), Required > 0
			? FText::Format(NSLOCTEXT("NUCharacterMenu", "ExperiencePercent", "{0}%"), FText::AsNumber(FMath::FloorToInt(FMath::Clamp(Progression->GetExperienceProgressRatio(), 0.f, 1.f) * 100.f)))
			: FText::GetEmpty());
		if (Bar) Bar->SetPercent(FMath::Clamp(Progression->GetExperienceProgressRatio(), 0.f, 1.f));
	}
	const UNUProgressionDataSubsystem* Data = GI->GetSubsystem<UNUProgressionDataSubsystem>();
	const UNUJobComponent* Job = State->GetJobComponent();
	if (Data && Job)
	{
		if (const FNUJobDefinitionRow* Row = Data->FindJob(Job->GetJobId())) Text(TEXT("JobText"), Row->DisplayName);
		if (const FNUJobAdvancementDefinitionRow* Row = Data->FindAdvancement(Job->GetAdvancementId())) Text(TEXT("JobText"), Row->DisplayName);
	}
	if (const UNUInventoryComponent* Inventory = State->GetInventoryComponent()) Text(TEXT("GoldText"), FText::AsNumber(Inventory->GetGold()));
	const UNUWorldLocationComponent* Location = State->FindComponentByClass<UNUWorldLocationComponent>();
	const UNUWorldDataSubsystem* WorldData = GI->GetSubsystem<UNUWorldDataSubsystem>();
	if (Location && WorldData)
	{
		if (const FNULocationDefinition* Row = WorldData->FindLocationDefinition(Location->GetCurrentLocationId())) Text(TEXT("LocationText"), Row->DisplayName);
		else if (const FNUAreaDefinition* Area = WorldData->FindAreaDefinition(Location->GetLastKnownAreaId())) Text(TEXT("LocationText"), Area->DisplayName);
	}
	RefreshVitalsAndStats(State);
	RefreshEquipment(State);
	RefreshNaiaPartner(State);
	// Manual refresh is also callable offscreen; only a constructed page may hold a pooled preview.
	if (BoundState.IsValid()) RefreshPortrait(State);
}

void UNUCharacterInfoPage::RefreshVitalsAndStats(ANUPlayerState* State)
{
	const UNUAbilitySystemComponent* ASC = State ? State->GetNUASC() : nullptr;
	const auto Value = [ASC](const FGameplayAttribute& Attribute) { return ASC ? ASC->GetNumericAttribute(Attribute) : 0.f; };
	const auto Text = [this](FName Name, const FText& InText)
	{
		if (UTextBlock* Widget = Cast<UTextBlock>(GetWidgetFromName(Name))) Widget->SetText(InText);
	};
	const FText Missing = FText::FromString(TEXT("—"));
	const auto Vital = [&](FName TextName, FName BarName, const FGameplayAttribute& Current, const FGameplayAttribute& Max)
	{
		const float CurrentValue = Value(Current), MaxValue = Value(Max);
		Text(TextName, ASC ? FText::Format(NSLOCTEXT("NUCharacterMenu", "VitalValue", "{0} / {1}"),
			FText::AsNumber(FMath::RoundToInt(CurrentValue)), FText::AsNumber(FMath::RoundToInt(MaxValue))) : Missing);
		if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(BarName)))
			Bar->SetPercent(MaxValue > KINDA_SMALL_NUMBER ? FMath::Clamp(CurrentValue / MaxValue, 0.f, 1.f) : 0.f);
		// Equipment changes move the maximum; flash only on that, not on every hit point.
		if (ASC) NUCharacterMenuMotion::FlashOnChange(this, Cast<UTextBlock>(GetWidgetFromName(TextName)), MaxValue);
	};
	Vital(TEXT("HealthText"), TEXT("HealthBar"), UNUVitalAttributeSet::GetHealthAttribute(), UNUVitalAttributeSet::GetMaxHealthAttribute());
	Vital(TEXT("SPText"), TEXT("SPBar"), UNUVitalAttributeSet::GetSPAttribute(), UNUVitalAttributeSet::GetMaxSPAttribute());

	const float Attack = Value(UNUCombatAttributeSet::GetAttackPowerAttribute());
	const float Defense = Value(UNUCombatAttributeSet::GetDefenseAttribute());
	const float Magic = Value(UNUCombatAttributeSet::GetMagicAttackPowerAttribute());
	const float Speed = Value(UNUCombatAttributeSet::GetSpeedAttribute());
	const float Critical = Value(UNUCombatAttributeSet::GetCriticalRateAttribute());
	const auto Number = [ASC, &Missing](float InValue) { return ASC ? FText::AsNumber(FMath::RoundToInt(InValue)) : Missing; };
	Text(TEXT("StatAttackValue"), Number(Attack));
	Text(TEXT("StatDefenseValue"), Number(Defense));
	Text(TEXT("StatMagicValue"), Number(Magic));
	Text(TEXT("StatSpeedValue"), Number(Speed));
	Text(TEXT("StatCriticalValue"), ASC ? FText::FromString(FString::Printf(TEXT("%.0f%%"), Critical * 100.f)) : Missing);
	if (ASC)
	{
		for (const TPair<const TCHAR*, float>& Stat : { TPair<const TCHAR*, float>(TEXT("StatAttackValue"), Attack),
			TPair<const TCHAR*, float>(TEXT("StatDefenseValue"), Defense), TPair<const TCHAR*, float>(TEXT("StatMagicValue"), Magic),
			TPair<const TCHAR*, float>(TEXT("StatSpeedValue"), Speed), TPair<const TCHAR*, float>(TEXT("StatCriticalValue"), Critical) })
		{
			NUCharacterMenuMotion::FlashOnChange(this, Cast<UTextBlock>(GetWidgetFromName(Stat.Key)), Stat.Value);
		}
	}

	// The radar is a shape cue only: the four flat stats share one scale (the largest of them)
	// and critical rate is drawn against a 50% ceiling. Exact numbers stay in the list beside it.
	if (UImage* Radar = Cast<UImage>(GetWidgetFromName(TEXT("StatRadarShape"))))
	{
		if (UMaterialInstanceDynamic* RadarInstance = Radar->GetDynamicMaterial())
		{
			const float Scale = FMath::Max(FMath::Max(Attack, Defense), FMath::Max(Magic, Speed));
			const auto Ratio = [Scale](float InValue) { return Scale > KINDA_SMALL_NUMBER ? FMath::Clamp(InValue / Scale, 0.08f, 1.f) : 0.08f; };
			// Vertex order clockwise from the top: attack, defense, magic, speed, critical.
			RadarInstance->SetScalarParameterValue(TEXT("V0"), Ratio(Attack));
			RadarInstance->SetScalarParameterValue(TEXT("V1"), Ratio(Defense));
			RadarInstance->SetScalarParameterValue(TEXT("V2"), Ratio(Magic));
			RadarInstance->SetScalarParameterValue(TEXT("V3"), Ratio(Speed));
			RadarInstance->SetScalarParameterValue(TEXT("V4"), FMath::Clamp(Critical / 0.5f, 0.08f, 1.f));
		}
	}
}

void UNUCharacterInfoPage::RefreshEquipment(ANUPlayerState* State)
{
	const UNUEquipmentComponent* Equipment = State ? State->GetEquipmentComponent() : nullptr;
	const auto Worn = [Equipment](ENUEquipSlot EquipSlot) -> FName
	{
		if (!Equipment) return NAME_None;
		const TArray<FNUEquippedItem>& Items = Equipment->GetEquippedItems();
		const int32 Index = static_cast<int32>(EquipSlot);
		return Items.IsValidIndex(Index) && Items[Index].InstanceId.IsValid() ? Items[Index].ItemRowName : NAME_None;
	};
	const auto Show = [this](const TCHAR* Key, FName RowName)
	{
		UTexture2D* Icon = RowName.IsNone() ? nullptr : NUItemUI::LoadItemIcon(RowName, true);
		if (UImage* Image = Cast<UImage>(GetWidgetFromName(FName(FString::Printf(TEXT("Equip%sIcon"), Key)))))
		{
			Image->SetBrushFromTexture(Icon);
			Image->SetVisibility(Icon ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
			// Newly worn item: short pop so the change reads at a glance.
			if (NUCharacterMenuMotion::TrackName(this, FName(Key), RowName) && Icon) NUCharacterMenuMotion::Pulse(Image, 1.2f, 0.22f);
		}
		if (UWidget* Empty = GetWidgetFromName(FName(FString::Printf(TEXT("Equip%sEmpty"), Key))))
			Empty->SetVisibility(Icon ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	};
	Show(TEXT("Weapon"), Worn(ENUEquipSlot::Weapon));
	Show(TEXT("Head"), Worn(ENUEquipSlot::Head));
	Show(TEXT("Chest"), Worn(ENUEquipSlot::Chest));
	// The card has one accessory row: the first worn ring or earring.
	FName Accessory = Worn(ENUEquipSlot::Ring1);
	if (Accessory.IsNone()) Accessory = Worn(ENUEquipSlot::Ring2);
	if (Accessory.IsNone()) Accessory = Worn(ENUEquipSlot::Earring);
	Show(TEXT("Accessory"), Accessory);
}

void UNUCharacterInfoPage::RefreshNaiaPartner(ANUPlayerState* State)
{
	const UNUNaiaRosterComponent* Roster = State ? State->GetNaiaRosterComponent() : nullptr;
	const ANUSpiritState* Naia = Roster ? Roster->GetActiveNaia() : nullptr;
	const UGameInstance* GI = GetGameInstance();
	const UNUNaiaDataSubsystem* Data = GI ? GI->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
	const FNUNaiaDefinition* Definition = IsValid(Naia) && Data ? Data->FindDefinitionById(Naia->GetSpiritId()) : nullptr;
	const bool bHasNaia = IsValid(Naia);
	for (const TCHAR* Name : { TEXT("NaiaNameText"), TEXT("NaiaBondText"), TEXT("NaiaBondBar"), TEXT("NaiaPortrait") })
		if (UWidget* Widget = GetWidgetFromName(Name)) Widget->SetVisibility(bHasNaia ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (UWidget* Empty = GetWidgetFromName(TEXT("NaiaEmptyText")))
		Empty->SetVisibility(bHasNaia ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	UNUFlipbookImage* Portrait = Cast<UNUFlipbookImage>(GetWidgetFromName(TEXT("NaiaPortrait")));
	if (!bHasNaia)
	{
		if (Portrait) Portrait->SetFlipbook(nullptr, 0.f);
		return;
	}
	if (UTextBlock* Name = Cast<UTextBlock>(GetWidgetFromName(TEXT("NaiaNameText"))))
		Name->SetText(Definition && !Definition->KoreanName.IsEmpty() ? Definition->KoreanName : FText::AsNumber(Naia->GetSpiritId()));
	const int32 Points = Naia->GetAffection();
	const int32 Tier = NUNaiaAffection::ResolveTier(Points);
	if (UTextBlock* Bond = Cast<UTextBlock>(GetWidgetFromName(TEXT("NaiaBondText"))))
		Bond->SetText(FText::Format(NSLOCTEXT("NUCharacterMenu", "NaiaBond", "유대 Lv.{0}"), FText::AsNumber(Tier)));
	if (UProgressBar* Bar = Cast<UProgressBar>(GetWidgetFromName(TEXT("NaiaBondBar"))))
	{
		const int32 From = NUNaiaAffection::GetTierThreshold(Tier);
		const int32 To = NUNaiaAffection::GetTierThreshold(FMath::Min(Tier + 1, NUNaiaAffection::MaxTier));
		Bar->SetPercent(Tier >= NUNaiaAffection::MaxTier || To <= From ? 1.f : FMath::Clamp(float(Points - From) / float(To - From), 0.f, 1.f));
	}
	if (Portrait)
	{
		if (UPaperFlipbook* Flipbook = Definition ? Definition->PreviewFlipbook.LoadSynchronous() : nullptr)
		{
			// Whole-pixel scale keeps the pixel art crisp; preview cells are 48 or 128 px tall.
			const UPaperSprite* First = Flipbook->GetSpriteAtFrame(0);
			const float CellHeight = First ? First->GetSlateAtlasData().GetSourceDimensions().Y : 0.f;
			Portrait->SetFlipbookAtScale(Flipbook, CellHeight > 0.f ? FMath::Max(1.f, FMath::RoundToFloat(200.f / CellHeight)) : 2.f);
		}
		else
		{
			Portrait->SetFlipbook(nullptr, 0.f);
			if (UTexture2D* Icon = Definition ? Definition->IconTexture.LoadSynchronous() : nullptr) Portrait->SetBrushFromTexture(Icon, true);
		}
	}
}

void UNUCharacterInfoPage::HandleAttributeChanged(const FOnAttributeChangeData& Data)
{
	RefreshVitalsAndStats(BoundState.Get());
}

void UNUCharacterInfoPage::RefreshPortrait(ANUPlayerState* State)
{
	UWorld* World = GetWorld();
	if (!World || !State || !CharacterPortrait || !PortraitFallback) return;
	if (!IsValid(PreviewActor))
	{
		if (UNUCharacterPreviewSubsystem* Pool = World->GetSubsystem<UNUCharacterPreviewSubsystem>()) PreviewActor = Pool->Acquire();
	}
	if (!IsValid(PreviewActor)) return;
	PreviewActor->RefreshEquipment(State->GetEquipmentComponent(), State->GetInventoryComponent());
	UTextureRenderTarget2D* Target = PreviewActor->GetRenderTarget();
	if (!PortraitInstance)
	{
		if (UMaterialInterface* Material = PortraitMaterial.LoadSynchronous()) PortraitInstance = UMaterialInstanceDynamic::Create(Material, this);
	}
	if (!Target || !PortraitInstance)
	{
		ReleasePortrait();
		return;
	}
	PortraitInstance->SetTextureParameterValue(TEXT("PreviewTexture"), Target);
	CharacterPortrait->SetBrushFromMaterial(PortraitInstance);
	FSlateBrush Brush = CharacterPortrait->GetBrush();
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = FVector2D(Target->SizeX, Target->SizeY);
	CharacterPortrait->SetBrush(Brush);
	CharacterPortrait->SetVisibility(ESlateVisibility::HitTestInvisible);
	PortraitFallback->SetVisibility(ESlateVisibility::Collapsed);

	// Optional on the information page: the floor samples the same live capture,
	// so equipment, appearance and idle frames stay in sync without another camera.
	if (UImage* Reflection = Cast<UImage>(GetWidgetFromName(TEXT("CharacterReflection"))))
	{
		UMaterialInstanceDynamic* ReflectionMaterial = Reflection->GetDynamicMaterial();
		if (ReflectionMaterial)
		{
			ReflectionMaterial->SetTextureParameterValue(TEXT("PreviewTexture"), Target);
		}
		Reflection->SetVisibility(ReflectionMaterial ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UNUCharacterInfoPage::ReleasePortrait()
{
	if (UImage* Reflection = Cast<UImage>(GetWidgetFromName(TEXT("CharacterReflection"))))
	{
		Reflection->SetVisibility(ESlateVisibility::Hidden);
		if (UMaterialInstanceDynamic* ReflectionMaterial = Reflection->GetDynamicMaterial())
		{
			// Drop the pooled actor's render target while retaining the authored material
			// for the next NativeConstruct of this cached page.
			ReflectionMaterial->ClearParameterValues();
		}
	}
	if (IsValid(PreviewActor))
	{
		if (UWorld* World = GetWorld())
		{
			if (UNUCharacterPreviewSubsystem* Pool = World->GetSubsystem<UNUCharacterPreviewSubsystem>()) Pool->Release(PreviewActor);
		}
	}
	PreviewActor = nullptr;
	PortraitInstance = nullptr;
	if (CharacterPortrait)
	{
		CharacterPortrait->SetBrushFromTexture(nullptr);
		CharacterPortrait->SetVisibility(ESlateVisibility::Hidden);
	}
	if (PortraitFallback) PortraitFallback->SetVisibility(ESlateVisibility::HitTestInvisible);
}
