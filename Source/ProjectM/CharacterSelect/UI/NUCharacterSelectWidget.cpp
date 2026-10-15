// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/UI/NUCharacterSelectWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "UI/NUUISettings.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterSelectGameMode.h"
#include "CharacterSelect/UI/NUCharacterSlotWidget.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUUIManager.h"

namespace
{
	// 알려진 서버 결과 코드는 한국어로, 나머지는 코드+원문 그대로
	FText DescribeCharacterError(int32 ResultCode, const FString& Message)
	{
		switch (ResultCode)
		{
		case 411:  return NSLOCTEXT("NUCharacter", "InField", "필드에 있는 동안은 할 수 없습니다");
		case 1200: return NSLOCTEXT("NUCharacter", "NotMine", "없는 캐릭터거나 내 캐릭터가 아닙니다");
		case 1203: return NSLOCTEXT("NUCharacter", "SlotFull", "캐릭터 슬롯이 가득 찼습니다");
		case 1205: return NSLOCTEXT("NUCharacter", "DbError", "서버 DB 오류가 발생했습니다");
		case 1210: return NSLOCTEXT("NUCharacter", "NameBanShort", "사용할 수 없는 단어가 포함된 이름입니다");
		case 1211: return NSLOCTEXT("NUCharacter", "NameReservedShort", "현재 사용할 수 없는 이름입니다");
		case 1212: return NSLOCTEXT("NUCharacter", "DeletePending", "삭제 예약 중인 캐릭터입니다");
		case 1213: return NSLOCTEXT("NUCharacter", "DeleteNotPending", "삭제 예약이 걸려 있지 않습니다");
		default:
			return FText::Format(
				NSLOCTEXT("NUCharacter", "GenericError", "요청 실패 ({0}) {1}"),
				FText::FromString(FString::FromInt(ResultCode)),
				FText::FromString(Message));
		}
	}
}

void UNUCharacterSelectWidget::NativeConstruct()
{
	Super::NativeConstruct();

	bCreateInFlight = false;
	bEnterInFlight = false;

	// 재오픈 중복 구독 방지 --- 해제 선행 후 구독
	CreateButton->OnClicked.RemoveDynamic(this, &UNUCharacterSelectWidget::HandleCreateClicked);
	CreateButton->OnClicked.AddDynamic(this, &UNUCharacterSelectWidget::HandleCreateClicked);
	EnterButton->OnClicked.RemoveDynamic(this, &UNUCharacterSelectWidget::HandleEnterClicked);
	EnterButton->OnClicked.AddDynamic(this, &UNUCharacterSelectWidget::HandleEnterClicked);
	TitleButton->OnClicked.RemoveDynamic(this, &UNUCharacterSelectWidget::HandleTitleClicked);
	TitleButton->OnClicked.AddDynamic(this, &UNUCharacterSelectWidget::HandleTitleClicked);

	// 재오픈 중복 구독 방지 — 해제 선행 후 구독
	if (UNUCharacterManager* Manager = GetManager())
	{
		Manager->OnCharacterListChanged.RemoveDynamic(this, &UNUCharacterSelectWidget::HandleListChanged);
		Manager->OnCharacterListChanged.AddDynamic(this, &UNUCharacterSelectWidget::HandleListChanged);
		Manager->OnCharacterError.RemoveDynamic(this, &UNUCharacterSelectWidget::HandleError);
		Manager->OnCharacterError.AddDynamic(this, &UNUCharacterSelectWidget::HandleError);

		// 서버에 로그인돼 있으면 목록을 받아온다 (응답이 오면 HandleListChanged로 갱신)
		Manager->RequestCharacterList();
	}

	RefreshData();

	// 등장 연출 --- 막이 걷히고 시트가 오른쪽에서 들어오며 시트 안 줄이 차례로 뜬다
	if (bPlayIntro)
	{
		IntroMotion.Add(GetWidgetFromName(TEXT("IntroVeil")), 0.f, 0.6f, FVector2D::ZeroVector, 1.f);
		IntroMotion.Add(GetWidgetFromName(TEXT("TitlePanel")), 0.2f, 0.45f, FVector2D(-16.f, 0.f));
		IntroMotion.Add(GetWidgetFromName(TEXT("FormPanel")), 0.1f, 0.5f, FVector2D(40.f, 0.f));
		IntroMotion.AddChildren(GetWidgetFromName(TEXT("SheetBody")), 0.25f, 0.045f, 0.38f, FVector2D(0.f, 10.f));
		IntroMotion.Start();
		PlayRowsIntro(); // 목록이 이미 있으면(더미·재방문) 지금, 서버 목록이면 받았을 때
	}
}

void UNUCharacterSelectWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	IntroMotion.Advance(InDeltaTime);
	HeroMotion.Advance(InDeltaTime);
}

void UNUCharacterSelectWidget::PlayRowsIntro()
{
	if (!bPlayIntro || bRowsIntroPlayed || Characters.IsEmpty())
	{
		return;
	}
	bRowsIntroPlayed = true;
	// 시트가 거의 들어온 뒤에 줄이 위에서부터 뜬다 (목록이 늦게 와도 지금부터 같은 간격)
	IntroMotion.AddChildren(CharacterListBox, 0.4f, 0.05f, 0.35f, FVector2D(18.f, 0.f));
	IntroMotion.Start();
}

void UNUCharacterSelectWidget::NativeDestruct()
{
	if (UNUCharacterManager* Manager = GetManager())
	{
		Manager->OnCharacterListChanged.RemoveDynamic(this, &UNUCharacterSelectWidget::HandleListChanged);
		Manager->OnCharacterError.RemoveDynamic(this, &UNUCharacterSelectWidget::HandleError);
	}
	Super::NativeDestruct();
}

void UNUCharacterSelectWidget::OpenCreateWindow()
{
	// 생성은 전용 레벨이다 --- 레벨을 옮기면 이 위젯은 사라지므로 여기서 할 일은 요청뿐이다
	if (ANUCharacterSelectGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ANUCharacterSelectGameMode>() : nullptr)
	{
		GameMode->TravelToCharacterCreate();
		return;
	}

	UE_LOG(LogNUCharSelect, Error, TEXT("선택창 게임모드가 없어 생성 레벨로 갈 수 없습니다"));
	SetErrorText(NSLOCTEXT("NUCharacter", "CreateWindowFailed", "생성 화면을 열지 못했습니다"));
	bCreateInFlight = false;
	RefreshCreateButtonState();
}

void UNUCharacterSelectWidget::SelectAndEnter(int64 CharacterIdx)
{
	// 요청만 보낸다. 실제 이동은 서버가 선택을 확정한 뒤(게임모드가 OnCharacterSelected를 받아 처리)
	if (UNUCharacterManager* Manager = GetManager())
	{
		Manager->SelectCharacter(CharacterIdx);
	}
}

void UNUCharacterSelectWidget::HandleListChanged()
{
	bCreateInFlight = false;

	// 방금 만든 캐릭터가 있으면 그 칸을 골라 둔다 (기획서 5.1). RefreshData 전에 정해야 강조가 바로 그려진다
	if (UNUCharacterManager* Manager = GetManager())
	{
		if (const int64 CreatedIdx = Manager->ConsumePendingCreatedIdx())
		{
			SelectedCharacterIdx = CreatedIdx;
		}
	}

	RefreshData();
	PlayRowsIntro();
}

void UNUCharacterSelectWidget::HandleError(int32 ResultCode, const FString& Message)
{
	UE_LOG(LogNUCharSelect, Warning, TEXT("캐릭터 오류 (%d) %s"), ResultCode, *Message);
	bCreateInFlight = false;
	bEnterInFlight = false; // 입장 거절(411 등)이면 다시 시도할 수 있게 버튼을 풀어준다
	RefreshCreateButtonState();
	RefreshEnterButtonState();
	SetErrorText(DescribeCharacterError(ResultCode, Message)); // 버튼 상태 갱신 뒤에 — 서버 사유가 최종 표시
	OnCharacterErrorReceived(ResultCode, Message);
}

void UNUCharacterSelectWidget::HandleCreateClicked()
{
	if (bCreateInFlight)
	{
		return;
	}
	bCreateInFlight = true;
	RefreshCreateButtonState();
	OpenCreateWindow(); // 레벨이 바뀌므로 이 위젯은 곧 사라진다 (잠금은 이동 실패 시에만 의미가 있다)
}

void UNUCharacterSelectWidget::RefreshData()
{
	if (const UNUCharacterManager* Manager = GetManager())
	{
		Characters = Manager->GetCharacters();
	}

	// 목록에서 사라진 캐릭터(삭제 등)를 골라 두고 있었으면 해제
	if (SelectedCharacterIdx != INDEX_NONE
		&& !Characters.ContainsByPredicate([this](const FNUCharacterSummary& C) { return C.CharacterIdx == SelectedCharacterIdx; }))
	{
		SelectedCharacterIdx = INDEX_NONE;
	}

	// 고른 것이 없으면 첫 캐릭터를 골라 둔다 (켜 둔 WBP에서만). 삭제 예약 중인 캐릭터는 서버가 입장을 거절하므로 뒤로 미룬다
	if (bAutoSelectFirst && SelectedCharacterIdx == INDEX_NONE && !Characters.IsEmpty())
	{
		const FNUCharacterSummary* Open = Characters.FindByPredicate([](const FNUCharacterSummary& C) { return !C.bDeletePending; });
		SelectedCharacterIdx = (Open != nullptr ? *Open : Characters[0]).CharacterIdx;
	}

	PopulateCharacterList();
	RefreshSelectedDisplay();
	SetErrorText(FText::GetEmpty());
	RefreshCreateButtonState(); // 슬롯이 가득이면 안내 문구를 다시 채운다
	RefreshEnterButtonState();
	OnCharacterListUpdated();
}

void UNUCharacterSelectWidget::PopulateCharacterList()
{
	CharacterListBox->ClearChildren();

	if (SlotWidgetClass == nullptr)
	{
		UE_LOG(LogNUCharSelect, Warning, TEXT("SlotWidgetClass 미지정 --- 캐릭터 칸을 그릴 수 없습니다 (WBP_CharacterSelect 기본값 확인)"));
		return;
	}

	// 칸 수를 늘 같게 그린다 --- 캐릭터가 늘고 줄 때마다 칸 크기가 달라지던 문제 (8/26 회의).
	// UniformGrid는 남는 자리를 자식끼리 나눠 갖기 때문에 3명일 때와 8명일 때 칸 폭이 달랐다.
	// 그리드가 아닌 패널이면 빈 칸 없이 있는 캐릭터만 차례로 넣는다
	UUniformGridPanel* Grid = Cast<UUniformGridPanel>(CharacterListBox);
	const int32 SlotTotal = Grid != nullptr ? GetSlotCount() : Characters.Num();
	const int32 Columns = FMath::Max(1, GridColumns);
	for (int32 Index = 0; Index < SlotTotal; ++Index)
	{
		UNUCharacterSlotWidget* SlotWidget = CreateWidget<UNUCharacterSlotWidget>(this, SlotWidgetClass);
		if (Characters.IsValidIndex(Index))
		{
			SlotWidget->SetCharacter(Characters[Index], Characters[Index].CharacterIdx == SelectedCharacterIdx);
		}
		else
		{
			SlotWidget->SetEmpty();
		}
		SlotWidget->OnSlotClicked.BindUObject(this, &UNUCharacterSelectWidget::HandleSlotClicked);
		SlotWidget->OnDeleteClicked.BindUObject(this, &UNUCharacterSelectWidget::HandleSlotDeleteClicked);

		if (Grid == nullptr)
		{
			CharacterListBox->AddChild(SlotWidget);
		}
		else if (UUniformGridSlot* GridSlot = Grid->AddChildToUniformGrid(SlotWidget, Index / Columns, Index % Columns))
		{
			// 가운데 정렬이라야 칸이 남는 자리만큼 늘어나지 않는다 (Fill이면 WBP가 정한 칸 크기가 무의미해진다)
			GridSlot->SetHorizontalAlignment(HAlign_Center);
			GridSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
}

int32 UNUCharacterSelectWidget::GetSlotCount() const
{
	// MaxCount는 서버 목록 응답에만 있다 --- 더미 경로(0)에서는 기획값을 쓴다.
	// 어떤 이유로 캐릭터가 칸보다 많아도 가려지지는 않게 한다
	const UNUCharacterManager* Manager = GetManager();
	const int32 MaxCount = Manager != nullptr ? Manager->GetMaxCharacterCount() : 0;
	return FMath::Max(MaxCount > 0 ? MaxCount : DefaultSlotCount, Characters.Num());
}

void UNUCharacterSelectWidget::HandleSlotClicked(int64 CharacterIdx)
{
	if (CharacterIdx == INDEX_NONE)
	{
		HandleCreateClicked(); // 빈 칸을 누르는 것은 [캐릭터 생성]과 같다 --- 가득참·요청 중 검사도 그쪽이 한다
		return;
	}

	// 칸 클릭은 선택만 --- 입장은 EnterButton이 한다 (실수로 바로 들어가는 것 방지)
	if (SelectedCharacterIdx == CharacterIdx)
	{
		return;
	}
	SelectedCharacterIdx = CharacterIdx;
	UNUSFXSubsystem::Play(this, NU::SFX::Char::Select);
	PopulateCharacterList(); // 강조 테두리·걷기 모션 반영 (칸이 몇 개 안 돼 전체 재구성으로 충분)
	RefreshSelectedDisplay();
	RefreshEnterButtonState();
}

void UNUCharacterSelectWidget::RefreshSelectedDisplay()
{
	const FNUCharacterSummary* Selected = Characters.FindByPredicate(
		[this](const FNUCharacterSummary& C) { return C.CharacterIdx == SelectedCharacterIdx; });

	// 무대의 큰 캐릭터 --- 칸과 같은 그림(몸·외모·머리·머리 색)을 정수 배율로 세운다
	const int64 HeroIdx = Selected != nullptr ? Selected->CharacterIdx : INDEX_NONE;
	const bool bHeroChanged = HeroIdx != ShownHeroIdx;
	ShownHeroIdx = HeroIdx;
	if (HeroPortrait != nullptr && (bHeroChanged || Selected == nullptr))
	{
		if (Selected != nullptr)
		{
			const UNUCharacterSlotWidget* SlotDefaults = SlotWidgetClass != nullptr
				? SlotWidgetClass->GetDefaultObject<UNUCharacterSlotWidget>() : nullptr;
			UNUCharacterSlotWidget::BuildPortrait(this, HeroPortrait, *Selected, false, 0.f, HeroPixelScale, false,
				SlotDefaults != nullptr ? SlotDefaults->LoadDefaultIdleFlipbook() : nullptr,
				SlotDefaults != nullptr ? SlotDefaults->LoadDefaultWalkFlipbook() : nullptr);
		}
		else
		{
			HeroPortrait->ClearChildren();
		}
	}
	if (HeroShadow != nullptr)
	{
		HeroShadow->SetVisibility(Selected != nullptr ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	// 다른 캐릭터를 고르면 무대의 캐릭터가 살짝 올라오며 나타난다 (등장 연출 중이면 시트가 들어온 뒤에)
	if (bHeroChanged && Selected != nullptr && bPlayIntro)
	{
		const float Delay = IntroMotion.IsRunning() ? 0.3f : 0.f;
		HeroMotion.Add(HeroPortrait, Delay, 0.32f, FVector2D(0.f, 12.f));
		HeroMotion.Add(HeroShadow, Delay, 0.32f, FVector2D::ZeroVector, 0.f, 0.7f);
		HeroMotion.Start();
	}

	if (SelectedNameText != nullptr)
	{
		SelectedNameText->SetText(Selected != nullptr
			? FText::FromString(Selected->Name)
			: (Characters.IsEmpty()
				? NSLOCTEXT("NUCharacter", "SelectNoneTitle", "환영합니다")
				: NSLOCTEXT("NUCharacter", "SelectPickTitle", "캐릭터 선택")));
	}
	if (SelectedSubText != nullptr)
	{
		SelectedSubText->SetText(Selected != nullptr
			? UNUCharacterSlotWidget::DescribeCharacter(this, *Selected, true)
			: (Characters.IsEmpty()
				? NSLOCTEXT("NUCharacter", "SelectNoneSub", "아직 캐릭터가 없습니다. 새 캐릭터를 만들어 주세요.")
				: NSLOCTEXT("NUCharacter", "SelectPickSub", "목록에서 캐릭터를 골라 주세요.")));
	}
	if (SlotCountText != nullptr)
	{
		SlotCountText->SetText(FText::Format(NSLOCTEXT("NUCharacter", "SlotCount", "{0} / {1}"),
			FText::AsNumber(Characters.Num()), FText::AsNumber(GetSlotCount())));
	}
	if (EmptyNote != nullptr)
	{
		EmptyNote->SetVisibility(Characters.IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUCharacterSelectWidget::HandleSlotDeleteClicked(int64 CharacterIdx, bool bCancelPending)
{
	if (bCancelPending)
	{
		HandleDeleteCancelClicked(CharacterIdx);
	}
	else
	{
		HandleDeleteClicked(CharacterIdx);
	}
}

void UNUCharacterSelectWidget::HandleEnterClicked()
{
	if (SelectedCharacterIdx == INDEX_NONE || bEnterInFlight)
	{
		return;
	}
	// 성공하면 레벨이 넘어가고, 거절되면 HandleError가 풀어준다
	bEnterInFlight = true;
	RefreshEnterButtonState();
	SelectAndEnter(SelectedCharacterIdx);
}

void UNUCharacterSelectWidget::HandleTitleClicked()
{
	// 연결이 끊기는 동작이라 잘못 눌러도 되돌릴 수 있게 한 번 묻는다.
	// 확인 창이 없으면 바로 나간다 --- 나가는 길 자체는 매니저의 것(끊김 안내 창의 [초기화면으로]와 같다)
	UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer());
	if (UI == nullptr)
	{
		return;
	}
	const FSimpleDelegate GoToTitle = FSimpleDelegate::CreateWeakLambda(UI, [UI]() { UI->GoToLoginLevel(); });
	if (UNUConfirmDialog::Show(GetOwningPlayer(), GetDefault<UNUUISettings>()->ConfirmDialogClass.LoadSynchronous(),
			NSLOCTEXT("NUCharacter", "ToTitleConfirm", "타이틀 화면으로 돌아갈까요?"),
			NSLOCTEXT("NUCharacter", "ToTitleConfirmLabel", "돌아가기"),
			GoToTitle, FSimpleDelegate(), FText(), ENUConfirmIntent::Positive) == nullptr)
	{
		GoToTitle.ExecuteIfBound();
	}
}

void UNUCharacterSelectWidget::RefreshEnterButtonState()
{
	EnterButton->SetIsEnabled(SelectedCharacterIdx != INDEX_NONE && !bEnterInFlight);
	// 눌릴 수 있을 때만 시작하기에 빛이 지나간다 (WBP에 있을 때만)
	if (UWidget* Shine = GetWidgetFromName(TEXT("EnterShine")))
	{
		Shine->SetVisibility(EnterButton->GetIsEnabled() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUCharacterSelectWidget::HandleDeleteClicked(int64 CharacterIdx)
{
	const FNUCharacterSummary* Found = Characters.FindByPredicate(
		[CharacterIdx](const FNUCharacterSummary& C) { return C.CharacterIdx == CharacterIdx; });
	const FText Name = Found ? FText::FromString(Found->Name) : FText::GetEmpty();

	// 즉시 지워지지 않는다 --- 24시간 뒤 삭제로 예약되고 그동안 취소할 수 있다.
	// 실제로 지워진 뒤에는 그 이름을 3일간 아무도(본인 포함) 못 쓴다 --- 두 가지를 같이 알린다 (가이드 권고)
	FSimpleDelegate OnConfirmed = FSimpleDelegate::CreateWeakLambda(this, [this, CharacterIdx]()
	{
		if (UNUCharacterManager* Manager = GetManager())
		{
			UNUSFXSubsystem::Play(this, NU::SFX::Char::Delete);
			Manager->DeleteCharacter(CharacterIdx);
		}
	});

	UNUConfirmDialog* Dialog = UNUConfirmDialog::Show(
		GetOwningPlayer(), ConfirmDialogClass,
		FText::Format(NSLOCTEXT("NUCharacter", "DeleteConfirm",
			"[{0}] 캐릭터를 삭제하시겠습니까?\n\n24시간 뒤에 삭제되며, 그동안 취소할 수 있습니다.\n삭제된 캐릭터는 되돌릴 수 없고, 이 이름은 3일간 사용할 수 없습니다."), Name),
		NSLOCTEXT("NUCharacter", "Delete", "삭제"),
		OnConfirmed);
	if (Dialog == nullptr)
	{
		// 확인 창 클래스 미지정 — 팀 선례(RequestConfirm)대로 경고 후 즉시 실행
		UE_LOG(LogNUCharSelect, Warning, TEXT("ConfirmDialogClass 미지정 — 확인 없이 삭제합니다"));
		OnConfirmed.ExecuteIfBound();
	}
}

void UNUCharacterSelectWidget::HandleDeleteCancelClicked(int64 CharacterIdx)
{
	// 취소는 잃을 것이 없다 --- 확인 창 없이 바로 보낸다. 목록이 응답으로 와서 배지가 내려간다
	if (UNUCharacterManager* Manager = GetManager())
	{
		Manager->CancelDeleteCharacter(CharacterIdx);
	}
}

void UNUCharacterSelectWidget::RefreshCreateButtonState()
{
	bool bFull = false;
	if (const UNUCharacterManager* Manager = GetManager())
	{
		// MaxCount는 서버 목록 응답에만 있다 — 더미 경로(0)에서는 제한 없음
		const int32 MaxCount = Manager->GetMaxCharacterCount();
		bFull = MaxCount > 0 && Manager->GetCharacters().Num() >= MaxCount;
	}

	CreateButton->SetIsEnabled(!bFull && !bCreateInFlight);

	if (bFull)
	{
		SetErrorText(NSLOCTEXT("NUCharacter", "SlotFull", "캐릭터 슬롯이 가득 찼습니다"));
	}
}

void UNUCharacterSelectWidget::SetErrorText(const FText& InText)
{
	ErrorText->SetText(InText);
	ErrorText->SetVisibility(InText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

UNUCharacterManager* UNUCharacterSelectWidget::GetManager() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
}
