#include "UI/NUGameMenuWidget.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Net/EmberServerManager.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Field/NUCharacterPreviewActor.h"
#include "Field/NUCharacterPreviewSubsystem.h"
#include "Inventory/NUInventoryComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "WorldLocation/Component/NUWorldLocationComponent.h"
#include "WorldLocation/Data/NUWorldDataSubsystem.h"

namespace
{
	FString HubMottoFile()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/HubProfiles.ini"));
	}

	FString DefaultHubMotto()
	{
		return NSLOCTEXT("NUGameMenu", "DefaultMotto", "안녕하세요").ToString();
	}
}

void UNUGameMenuWidget::InitializeHubMotto()
{
	UEditableTextBox* Input = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("HubMottoInput")));
	if (!Input) return;
	FString Identity = TEXT("OfflinePreview");
	int64 CharacterId = 0;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UEmberServerManager* Server = GI->GetSubsystem<UEmberServerManager>(); Server && !Server->GetUserId().IsEmpty())
		{
			Identity = Server->GetServerEndPoint() + TEXT("|") + Server->GetUserId();
		}
		if (const UNUCharacterManager* Characters = GI->GetSubsystem<UNUCharacterManager>())
		{
			CharacterId = Characters->GetSelectedCharacterIdx();
		}
	}
	const FTCHARToUTF8 IdentityUtf8(*Identity);
	const FString IdentityHash = FMD5::HashBytes(reinterpret_cast<const uint8*>(IdentityUtf8.Get()), IdentityUtf8.Length());
	HubMottoSection = FString::Printf(TEXT("Profile_%s_%lld"), *IdentityHash, CharacterId);
	FConfigFile Profiles;
	Profiles.Read(HubMottoFile());
	SavedHubMotto = DefaultHubMotto();
	Profiles.GetString(*HubMottoSection, TEXT("Motto"), SavedHubMotto);
	Input->SetText(FText::FromString(SavedHubMotto));
	Input->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleHubMottoCommitted);
}

void UNUGameMenuWidget::HandleHubMottoCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	UEditableTextBox* Input = Cast<UEditableTextBox>(GetWidgetFromName(TEXT("HubMottoInput")));
	if (!Input) return;
	if (CommitMethod == ETextCommit::OnCleared)
	{
		Input->SetText(FText::FromString(SavedHubMotto));
		Input->SetError(FText::GetEmpty());
		return;
	}
	FString Value = Text.ToString().Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).Replace(TEXT("\t"), TEXT(" "));
	Value.TrimStartAndEndInline();
	// Limit on commit, so Korean IME composition is never interrupted mid-syllable.
	Value = Value.Left(32);
	if (!Value.IsEmpty() && Value[Value.Len() - 1] >= 0xD800 && Value[Value.Len() - 1] <= 0xDBFF) Value.LeftChopInline(1);
	if (Value.IsEmpty()) Value = DefaultHubMotto();
	FConfigFile Profiles;
	Profiles.Read(HubMottoFile());
	Profiles.SetString(*HubMottoSection, TEXT("Motto"), *Value);
	if (!Profiles.Write(HubMottoFile()))
	{
		Input->SetError(NSLOCTEXT("NUGameMenu", "MottoSaveFailed", "저장하지 못했습니다. 다시 입력해 주세요."));
		return;
	}
	SavedHubMotto = Value;
	Input->SetText(FText::FromString(Value));
	Input->SetError(FText::GetEmpty());
}

void UNUGameMenuWidget::RefreshHubProfile()
{
	UImage* Portrait = Cast<UImage>(GetWidgetFromName(TEXT("HubPortrait")));
	if (Portrait) Portrait->SetVisibility(ESlateVisibility::Hidden);
	if (UWidget* Fallback = GetWidgetFromName(TEXT("HubPortraitFallback"))) Fallback->SetVisibility(ESlateVisibility::HitTestInvisible);
	const auto SetText = [this](const FName Name, const FText& Value)
	{
		if (UTextBlock* Text = Cast<UTextBlock>(GetWidgetFromName(Name))) Text->SetText(Value);
	};
	const FText Missing = FText::FromString(TEXT("—"));
	SetText(TEXT("HubName"), NSLOCTEXT("NUGameMenu", "Adventurer", "모험가"));
	for (const FName Name : { FName(TEXT("HubLevel")), FName(TEXT("HubExperience")),
		FName(TEXT("HubLocation")), FName(TEXT("HubJob")), FName(TEXT("HubGold")), FName(TEXT("HubCharacterId")) })
	{
		SetText(Name, Missing);
	}
	UProgressBar* ExperienceBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("HubExperienceBar")));
	if (ExperienceBar) ExperienceBar->SetPercent(0.0f);

	const ANUPlayerState* State = Cast<ANUPlayerState>(GetOwningPlayerState());
	UGameInstance* GI = GetGameInstance();
	if (!State || !GI) return;

	FNUCharacterSummary Selected;
	const UNUCharacterManager* Characters = GI->GetSubsystem<UNUCharacterManager>();
	if (Characters && Characters->GetSelectedCharacter(Selected))
	{
		if (!Selected.Name.IsEmpty()) SetText(TEXT("HubName"), FText::FromString(Selected.Name));
		if (Selected.CharacterIdx > 0) SetText(TEXT("HubCharacterId"), FText::FromString(LexToString(Selected.CharacterIdx)));
	}
	if (const UNUProgressionComponent* Progression = State->GetProgressionComponent())
	{
		SetText(TEXT("HubLevel"), FText::Format(NSLOCTEXT("NUGameMenu", "HubLevel", "Lv. {0}"), FText::AsNumber(Progression->GetLevel())));
		const int64 Required = Progression->GetRequiredExperienceToNext();
		SetText(TEXT("HubExperience"), Required > 0
			? FText::Format(NSLOCTEXT("NUGameMenu", "HubExperience", "{0} / {1}"), FText::AsNumber(Progression->GetExperienceIntoLevel()), FText::AsNumber(Required))
			: NSLOCTEXT("NUGameMenu", "MaxLevel", "최고 레벨"));
		if (ExperienceBar) ExperienceBar->SetPercent(FMath::Clamp(Progression->GetExperienceProgressRatio(), 0.0f, 1.0f));
	}
	const UNUProgressionDataSubsystem* ProgressionData = GI->GetSubsystem<UNUProgressionDataSubsystem>();
	const UNUJobComponent* Job = State->GetJobComponent();
	if (Job && ProgressionData)
	{
		FText JobName = NSLOCTEXT("NUGameMenu", "Novice", "노비스");
		if (const FNUJobDefinitionRow* Row = ProgressionData->FindJob(Job->GetJobId())) JobName = Row->DisplayName;
		if (const FNUJobAdvancementDefinitionRow* Row = ProgressionData->FindAdvancement(Job->GetAdvancementId())) JobName = Row->DisplayName;
		SetText(TEXT("HubJob"), JobName);
	}
	if (const UNUInventoryComponent* Inventory = State->GetInventoryComponent()) SetText(TEXT("HubGold"), FText::AsNumber(Inventory->GetGold()));
	const UNUWorldLocationComponent* Location = State->FindComponentByClass<UNUWorldLocationComponent>();
	const UNUWorldDataSubsystem* WorldData = GI->GetSubsystem<UNUWorldDataSubsystem>();
	if (Location && WorldData)
	{
		if (const FNULocationDefinition* Row = WorldData->FindLocationDefinition(Location->GetCurrentLocationId())) SetText(TEXT("HubLocation"), Row->DisplayName);
		else if (const FNUAreaDefinition* Area = WorldData->FindAreaDefinition(Location->GetLastKnownAreaId())) SetText(TEXT("HubLocation"), Area->DisplayName);
	}

	// 정보창과 같은 풀/장비 렌더러를 사용하고 닫을 때 반드시 반환한다.
	UWorld* World = GetWorld();
	if (!Portrait || !World) return;
	if (!IsValid(HubPreviewActor))
	{
		if (UNUCharacterPreviewSubsystem* Pool = World->GetSubsystem<UNUCharacterPreviewSubsystem>()) HubPreviewActor = Pool->Acquire();
	}
	if (!IsValid(HubPreviewActor)) return;
	HubPreviewActor->RefreshEquipment(State->GetEquipmentComponent(), State->GetInventoryComponent());
	UTextureRenderTarget2D* Target = HubPreviewActor->GetRenderTarget();
	UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Developers/dbals/UI/M_NUCharacterPreview.M_NUCharacterPreview"));
	if (Target && Material)
	{
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Material, this);
		Dynamic->SetTextureParameterValue(TEXT("PreviewTexture"), Target);
		Portrait->SetBrushFromMaterial(Dynamic);
		FSlateBrush Brush = Portrait->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Portrait->SetBrush(Brush);
		Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UWidget* Fallback = GetWidgetFromName(TEXT("HubPortraitFallback"))) Fallback->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNUGameMenuWidget::ReleaseHubPortrait()
{
	if (IsValid(HubPreviewActor))
	{
		if (UWorld* World = GetWorld())
		{
			if (UNUCharacterPreviewSubsystem* Pool = World->GetSubsystem<UNUCharacterPreviewSubsystem>()) Pool->Release(HubPreviewActor);
		}
	}
	HubPreviewActor = nullptr;
	if (UImage* Portrait = Cast<UImage>(GetWidgetFromName(TEXT("HubPortrait")))) Portrait->SetBrushFromTexture(nullptr);
	if (UWidget* Fallback = GetWidgetFromName(TEXT("HubPortraitFallback"))) Fallback->SetVisibility(ESlateVisibility::HitTestInvisible);
}
