// Fill out your copyright notice in the Description page of Project Settings.

#include "Inventory/UI/NUCharacterInfoWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterProgression/UI/NUProgressionUIController.h"
#include "CharacterProgression/UI/Stats/NUCharacterStatsWidget.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "CharacterState/NUPlayerState.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Inventory/NUInventoryComponent.h"
#include "NUGAS/Attribute/NUCombatAttributeSet.h"
#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUAbilitySystemComponent.h"
#include "Styling/CoreStyle.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUUIManager.h"

void UNUCharacterInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// StatsDetailButton은 순정 Button(레거시) / UNU_CommonButtonWidget(WBP_CommonButton) 둘 다
	// 지원한다. 재오픈 중복 구독 방지 --- 해제 선행 후 구독
	if (UButton* ButtonPure = Cast<UButton>(StatsDetailButton))
	{
		ButtonPure->OnClicked.RemoveDynamic(this, &UNUCharacterInfoWidget::HandleStatsDetailClicked);
		ButtonPure->OnClicked.AddDynamic(this, &UNUCharacterInfoWidget::HandleStatsDetailClicked);
	}
	else if (UNU_CommonButtonWidget* CommonButton = Cast<UNU_CommonButtonWidget>(StatsDetailButton))
	{
		CommonButton->OnCommonClicked.RemoveDynamic(this, &UNUCharacterInfoWidget::HandleStatsDetailClicked);
		CommonButton->OnCommonClicked.AddDynamic(this, &UNUCharacterInfoWidget::HandleStatsDetailClicked);
	}
}

void UNUCharacterInfoWidget::HandleStatsDetailClicked()
{
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	UNUProgressionUIController* Progression = UI != nullptr ? UI->GetController<UNUProgressionUIController>() : nullptr;
	if (Progression == nullptr)
	{
		return;
	}

	Progression->ToggleCharacterStats();

	// 열렸으면 이 창 오른쪽에 붙인다 --- 정보창을 끌어 옮겨 놨어도 그 자리를 따라온다
	bStatsOpenedHere = Progression->IsCharacterStatsOpen();
	if (bStatsOpenedHere)
	{
		PositionStatsWindow(5);
	}
}

void UNUCharacterInfoWidget::NativeDestruct()
{
	// 여기서 연 스탯 창은 정보창과 수명을 같이한다 (P키·ESC·전투 진입 어느 경로로 닫혀도)
	if (bStatsOpenedHere)
	{
		bStatsOpenedHere = false;
		if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
		{
			if (UNUProgressionUIController* Progression = UI->GetController<UNUProgressionUIController>())
			{
				if (Progression->IsCharacterStatsOpen())
				{
					Progression->ToggleCharacterStats();
				}
			}
		}
	}
	Super::NativeDestruct();
}

void UNUCharacterInfoWidget::PositionStatsWindow(int32 AttemptsLeft)
{
	if (GetWorld() == nullptr)
	{
		return;
	}
	GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, AttemptsLeft]()
	{
		UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
		UNUProgressionUIController* Progression =
			UI != nullptr ? UI->GetController<UNUProgressionUIController>() : nullptr;
		UUserWidget* Stats = (Progression != nullptr && Progression->IsCharacterStatsOpen())
			? Progression->GetCharacterStatsWidget() : nullptr;
		if (Stats == nullptr)
		{
			return; // 그새 닫혔다 --- 옮길 것이 없다
		}

		// 스탯 창의 보이는 몸체 (진영 님 WBP의 WindowFrame). 남의 창이라 이름으로 찾는다 --- 이름이 바뀌면 기본 위치로 뜰 뿐 깨지지는 않는다
		UWidget* StatsFrame = Stats->GetWidgetFromName(TEXT("WindowFrame"));
		const float Scale = UWidgetLayoutLibrary::GetViewportScale(this);
		if (StatsFrame == nullptr || Scale <= KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogNUInventory, Warning,
				TEXT("스탯 창 위치 잡기 실패 --- 스탯 몸체 %d / 배율 %.2f"), StatsFrame != nullptr, Scale);
			return;
		}

		const FGeometry& MyGeo = WindowBG->GetCachedGeometry();
		const FGeometry& StatsGeo = StatsFrame->GetCachedGeometry();
		if (MyGeo.GetAbsoluteSize().IsNearlyZero() || StatsGeo.GetAbsoluteSize().IsNearlyZero())
		{
			// 갓 열린 창은 아직 배치 전이라 지오메트리가 비어 있다 --- 잡힐 때까지 몇 틱 더 본다
			if (AttemptsLeft > 0)
			{
				PositionStatsWindow(AttemptsLeft - 1);
			}
			else
			{
				UE_LOG(LogNUInventory, Warning, TEXT("스탯 창 위치 잡기 포기 --- 지오메트리가 잡히지 않음"));
			}
			return;
		}

		const FVector2D Delta =
			(MyGeo.GetAbsolutePosition() + FVector2D(MyGeo.GetAbsoluteSize().X + 12.0f * Scale, 0.0f)
				- StatsGeo.GetAbsolutePosition()) / Scale;
		Stats->SetRenderTranslation(Stats->GetRenderTransform().Translation + Delta);
	}));
}

void UNUCharacterInfoWidget::RefreshData()
{
	if (!ResolveComponents())
	{
		return;
	}

	bInBattle = Inventory->IsOwnerInBattle();

	if (const UNUVitalAttributeSet* Vital = ASC->GetVitalAttributeSet())
	{
		Health = Vital->GetHealth();
		MaxHealth = Vital->GetMaxHealth();
		SP = Vital->GetSP();
		MaxSP = Vital->GetMaxSP();
	}
	if (const UNUCombatAttributeSet* Combat = ASC->GetCombatAttributeSet())
	{
		AttackPower = Combat->GetAttackPower();
		MagicAttackPower = Combat->GetMagicAttackPower();
		Defense = Combat->GetDefense();
		Speed = Combat->GetSpeed();
		CriticalRate = Combat->GetCriticalRate();
		CriticalDamage = Combat->GetCriticalDamage();
	}

	PopulateProfile();
	PopulateStatsList();
	PopulateCharacterPreview();
	OnStatsChanged();
}

void UNUCharacterInfoWidget::PopulateProfile()
{
	const ANUPlayerState* OwnerState = Cast<ANUPlayerState>(Inventory->GetOwner());
	if (OwnerState == nullptr)
	{
		return;
	}

	if (UTextBlock* NameText = FindTextByName(TEXT("NameText")))
	{
		// 캐릭터 닉네임을 쓴다 --- PlayerState 이름은 접속 계정 기본값(컴퓨터 이름)이라 다르다
		FString DisplayName = OwnerState->GetPlayerName();
		const UGameInstance* GI = GetGameInstance();
		const UNUCharacterManager* Characters = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
		FNUCharacterSummary Selected;
		if (Characters != nullptr && Characters->GetSelectedCharacter(Selected) && !Selected.Name.IsEmpty())
		{
			DisplayName = Selected.Name;
		}
		NameText->SetText(FText::FromString(DisplayName));
	}

	if (const UNUProgressionComponent* Progression = OwnerState->GetProgressionComponent())
	{
		Level = Progression->GetLevel();
	}
	if (UTextBlock* LevelText = FindTextByName(TEXT("LevelText")))
	{
		LevelText->SetText(FText::Format(
			NSLOCTEXT("NUCharacterInfo", "LevelFormat", "Lv. {0}"), FText::AsNumber(Level)));
	}

	// 직업은 번호만 들고 있어 표기를 성장 데이터 표에서 찾는다.
	// 전직을 했으면 "직업 : 전직명"으로 붙여 쓴다 (스탯 창과 같은 표기)
	JobName = NSLOCTEXT("NUCharacterInfo", "JobNone", "노비스");
	const UNUJobComponent* JobComponent = OwnerState->GetJobComponent();
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
	const UNUProgressionDataSubsystem* Data = GameInstance != nullptr
		? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>()
		: nullptr;
	if (JobComponent != nullptr && JobComponent->HasJob() && Data != nullptr)
	{
		if (const FNUJobDefinitionRow* Job = Data->FindJob(JobComponent->GetJobId()))
		{
			JobName = Job->DisplayName;
			if (const FNUJobAdvancementDefinitionRow* Advancement = Data->FindAdvancement(JobComponent->GetAdvancementId()))
			{
				JobName = FText::Format(
					NSLOCTEXT("NUCharacterInfo", "JobWithAdvancement", "{0} : {1}"),
					Job->DisplayName, Advancement->DisplayName);
			}
		}
	}
	if (UTextBlock* JobText = FindTextByName(TEXT("JobText")))
	{
		JobText->SetText(JobName);
	}
}

void UNUCharacterInfoWidget::PopulateStatsList()
{
	UPanelWidget* List = FindPanelByName(TEXT("StatsListBox"));
	if (List == nullptr || WidgetTree == nullptr)
	{
		return;
	}
	List->ClearChildren();

	const FLinearColor LabelColor(1.0f, 0.85f, 0.4f);   // "주요 스탯" 머리글과 같은 옅은 노랑
	const FLinearColor ValueColor(0.92f, 0.92f, 0.95f);

	// 치확·치피는 0~1 이라 표기할 때 퍼센트로 환산한다
	struct FStatRow { FText Label; FString Value; bool bDividerAbove; };
	const FStatRow Rows[] = {
		{ NSLOCTEXT("NUCharacterInfo", "StatHP", "HP"), FString::Printf(TEXT("%.0f / %.0f"), Health, MaxHealth), false },
		{ NSLOCTEXT("NUCharacterInfo", "StatSP", "SP"), FString::Printf(TEXT("%.0f / %.0f"), SP, MaxSP), false },
		{ NSLOCTEXT("NUCharacterInfo", "StatAttack", "공격력"), FString::Printf(TEXT("%.0f"), AttackPower), true },
		{ NSLOCTEXT("NUCharacterInfo", "StatMagicAttack", "마법 공격력"), FString::Printf(TEXT("%.0f"), MagicAttackPower), false },
		{ NSLOCTEXT("NUCharacterInfo", "StatDefense", "방어력"), FString::Printf(TEXT("%.0f"), Defense), false },
		{ NSLOCTEXT("NUCharacterInfo", "StatSpeed", "속도"), FString::Printf(TEXT("%.0f"), Speed), false },
		{ NSLOCTEXT("NUCharacterInfo", "StatCritRate", "치명타 확률"), FString::Printf(TEXT("%.0f%%"), CriticalRate * 100.f), false },
		{ NSLOCTEXT("NUCharacterInfo", "StatCritDamage", "치명타 피해"), FString::Printf(TEXT("%.0f%%"), CriticalDamage * 100.f), false },
	};

	for (const FStatRow& Row : Rows)
	{
		if (Row.bDividerAbove)
		{
			// 생존 수치(HP·SP)와 전투 수치 사이 가로선
			UBorder* Divider = WidgetTree->ConstructWidget<UBorder>();
			Divider->SetBrushColor(FLinearColor(0.35f, 0.36f, 0.45f, 0.8f));
			Divider->SetPadding(FMargin(0.f, 0.5f)); // 자식 없는 Border는 패딩이 곧 높이 --- 1px 선
			if (UVerticalBoxSlot* DividerSlot = Cast<UVerticalBoxSlot>(List->AddChild(Divider)))
			{
				DividerSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 8.f));
			}
		}

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();

		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
		Label->SetText(Row.Label);
		Label->SetColorAndOpacity(FSlateColor(LabelColor));
		Label->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 16));
		if (UHorizontalBoxSlot* LabelSlot = Cast<UHorizontalBoxSlot>(Line->AddChild(Label)))
		{
			LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); // 이름이 왼쪽, 수치가 오른쪽 끝
		}

		UTextBlock* Value = WidgetTree->ConstructWidget<UTextBlock>();
		Value->SetText(FText::FromString(Row.Value));
		Value->SetColorAndOpacity(FSlateColor(ValueColor));
		Value->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 16));
		Line->AddChild(Value);

		if (UVerticalBoxSlot* LineSlot = Cast<UVerticalBoxSlot>(List->AddChild(Line)))
		{
			LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
		}
	}
}

void UNUCharacterInfoWidget::CloseSelf()
{
	if (Inventory.IsValid())
	{
		Inventory->ToggleCharacterInfoUI();
	}
}
