// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/UI/NUCharacterCreateWidget.h"

#include "Blueprint/WidgetTree.h"
#include "CharacterSelect/Data/NUAppearanceDataSettings.h"
#include "CharacterSelect/NUCharacterCreatePreview.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "Engine/GameInstance.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PaperFlipbook.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUConfirmDialog.h"

namespace
{
	// 파츠 섬네일 --- 아이콘 원본이 몸 캔버스(69x96) 그대로라서 통째로 넣으면 콩알만 해진다.
	// 파츠는 계약상 캔버스의 머리 자리(위쪽)에 그려지므로 그 영역만 확대해 칸 중앙에 둔다
	// (칸 크기는 WBP마다 달라 클래스 기본값 ThumbBoxSize로 옮겼다)
	constexpr float PartCanvasWidth = 69.0f;   // 몸 캔버스 규격 --- 이 크기의 아이콘만 확대한다
	constexpr float PartCanvasHeight = 96.0f;
	constexpr float ThumbHeadSpan = 44.0f;     // 칸에 채울 캔버스 가로 폭 (머리 너비 + 여유)
	constexpr float ThumbHeadCenterY = 18.0f;  // 칸 중앙에 올 캔버스 세로 위치 (위에서부터, 머리 중심)
}

/////////////////////////////////////////////////////////////////
// 준비
/////////////////////////////////////////////////////////////////

void UNUCharacterCreateWidget::Setup(FOnNUCharacterCreateConfirmed InOnConfirmed,
	TSubclassOf<UNUConfirmDialog> InConfirmDialogClass)
{
	// NativeConstruct에서 읽으므로 뷰포트에 올리기 전에 호출해야 한다
	OnConfirmed = InOnConfirmed;
	ConfirmDialogClass = InConfirmDialogClass;
}

void UNUCharacterCreateWidget::SetPreviewStands(const TArray<ANUCharacterCreatePreview*>& InStands)
{
	PreviewStands.Reset();
	for (ANUCharacterCreatePreview* Stand : InStands)
	{
		if (IsValid(Stand))
		{
			PreviewStands.Add(Stand);
			Stand->OnStandClicked.BindUObject(this, &UNUCharacterCreateWidget::HandleStandClicked);
		}
	}
	ApplyStandLocks();
}

void UNUCharacterCreateWidget::ApplyStandLocks()
{
	for (const TWeakObjectPtr<ANUCharacterCreatePreview>& StandPtr : PreviewStands)
	{
		ANUCharacterCreatePreview* Stand = StandPtr.Get();
		if (Stand == nullptr)
		{
			continue;
		}
		const int32 StandRaceId = Stand->GetRaceId();
		const FRaceEntry* Entry = Races.FindByPredicate(
			[StandRaceId](const FRaceEntry& Race) { return Race.RaceId == StandRaceId; });
		Stand->SetLocked(Entry == nullptr || !Entry->bPlayable);
	}
}

void UNUCharacterCreateWidget::HandleStandClicked(int32 RaceId)
{
	const int32 Found = Races.IndexOfByPredicate(
		[RaceId](const FRaceEntry& Entry) { return Entry.RaceId == RaceId; });
	if (Found == INDEX_NONE || !Races[Found].bPlayable)
	{
		return; // 표에 없는 자리거나 미개방 종족 (스탠드도 잠기지만 순서 경합 대비 이중 방어)
	}

	RaceIndex = Found;
	SetFormVisible(true);
	RefreshDisplay();
}

void UNUCharacterCreateWidget::SetFormVisible(bool bVisible)
{
	if (FormPanel != nullptr)
	{
		FormPanel->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UNUCharacterCreateWidget::SetOverviewCamera(AActor* InCamera)
{
	OverviewCamera = InCamera;
}

UNUCharacterManager* UNUCharacterCreateWidget::GetManager() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
}

/////////////////////////////////////////////////////////////////
// 배선
/////////////////////////////////////////////////////////////////

void UNUCharacterCreateWidget::NativeConstruct()
{
	Super::NativeConstruct();

	LoadRaces();
	LoadParts();    // 초안이 파츠 번호를 찾으려면 목록이 먼저 있어야 한다
	RestoreDraft(); // 레벨을 오갔거나 다시 들어왔으면 고르던 것을 되살린다

	// AddUniqueDynamic이라 창을 다시 올려도 중복 구독되지 않는다
	NameTextBox->OnTextChanged.AddUniqueDynamic(this, &UNUCharacterCreateWidget::HandleNameChanged);

	// 한 명 무대 WBP는 고르는 단계가 없다 --- 열려 있는 첫 종족으로 시작한다 (초안이 살아 있으면 그쪽이 이미 골랐다)
	if (bAutoPickFirstRace && !Races.IsValidIndex(RaceIndex))
	{
		RaceIndex = Races.IndexOfByPredicate([](const FRaceEntry& Entry) { return Entry.bPlayable; });
	}

	BuildRaceList();
	BuildHairColorList();

	// 종족을 고르기 전에는 선술집만 보여 준다 --- 캐릭터를 누르면 폼이 열린다.
	// 초안이 남아 있으면(레벨을 오갔다) 고른 상태이므로 바로 연다
	SetFormVisible(Races.IsValidIndex(RaceIndex));

	// 썸네일 칸 --- 개수가 WBP마다 달라 이름으로 찾는다. 있는 만큼만 쓴다
	HairThumbs.Reset();
	FaceThumbs.Reset();
	SkinThumbs.Reset();
	for (int32 SlotIndex = 0; SlotIndex < MaxVisibleThumbs; ++SlotIndex)
	{
		if (UBorder* Box = Cast<UBorder>(GetWidgetFromName(FName(*FString::Printf(TEXT("HairThumb%d"), SlotIndex)))))
		{
			HairThumbs.Add(Box);
		}
		if (UBorder* Box = Cast<UBorder>(GetWidgetFromName(FName(*FString::Printf(TEXT("FaceThumb%d"), SlotIndex)))))
		{
			FaceThumbs.Add(Box);
		}
		if (UBorder* Box = Cast<UBorder>(GetWidgetFromName(FName(*FString::Printf(TEXT("SkinThumb%d"), SlotIndex)))))
		{
			SkinThumbs.Add(Box);
		}
	}

	if (const UNUCharacterManager* Manager = GetManager())
	{
		const FString& DraftName = Manager->GetDraftParams().Name;
		if (!DraftName.IsEmpty())
		{
			NameTextBox->SetText(FText::FromString(DraftName));
		}
	}

	// 순정 Button(레거시) / UNU_CommonButtonWidget(WBP_CommonButton) 둘 다 지원해야 해서
	// UFUNCTION 이름으로 묶는 BindButton을 쓴다 (AddDynamic은 매크로라 표로 못 돈다)
	BindButton(NameCheckButton, TEXT("HandleNameCheckClicked"));
	BindButton(GenderFemaleButton, TEXT("HandleGenderFemaleClicked"));
	BindButton(GenderMaleButton, TEXT("HandleGenderMaleClicked"));
	BindButton(HairPrevButton, TEXT("HandleHairPrevClicked"));
	BindButton(HairNextButton, TEXT("HandleHairNextClicked"));
	BindButton(FacePrevButton, TEXT("HandleFacePrevClicked"));
	BindButton(FaceNextButton, TEXT("HandleFaceNextClicked"));
	BindButton(SkinPrevButton, TEXT("HandleSkinPrevClicked"));
	BindButton(SkinNextButton, TEXT("HandleSkinNextClicked"));
	BindButton(RandomButton, TEXT("HandleRandomClicked"));
	BindButton(ResetButton, TEXT("HandleResetClicked"));
	BindButton(CreateButton, TEXT("HandleCreateClicked"));
	BindButton(CancelButton, TEXT("HandleCancelClicked"));
	// 종족 넘기기(< >)는 선택 --- 목록 방식 WBP에는 없다 (BindButton은 없으면 조용히 무시한다)
	BindButton(RacePrevButton, TEXT("HandleRacePrevClicked"));
	BindButton(RaceNextButton, TEXT("HandleRaceNextClicked"));

	// 이름 확인(140) 응답 구독 (재구성 중복 방지 --- 해제 선행)
	if (UNUCharacterManager* Manager = GetManager())
	{
		Manager->OnCharacterNameChecked.RemoveDynamic(this, &UNUCharacterCreateWidget::HandleNameCheckResult);
		Manager->OnCharacterNameChecked.AddDynamic(this, &UNUCharacterCreateWidget::HandleNameCheckResult);
	}

	RefreshDisplay();

	// 등장 연출 --- 막이 걷히고, 시트가 오른쪽에서 들어오며 시트 안 줄이 위에서부터 차례로 뜬다 (접힌 줄은 건너뛴다)
	if (bPlayIntro)
	{
		IntroMotion.Add(GetWidgetFromName(TEXT("IntroVeil")), 0.f, 0.6f, FVector2D::ZeroVector, 1.f);
		IntroMotion.Add(GetWidgetFromName(TEXT("CancelPanel")), 0.2f, 0.45f, FVector2D(-16.f, 0.f));
		IntroMotion.Add(GetWidgetFromName(TEXT("FormPanel")), 0.1f, 0.5f, FVector2D(40.f, 0.f));
		IntroMotion.AddChildren(GetWidgetFromName(TEXT("SheetBody")), 0.25f, 0.045f, 0.38f, FVector2D(0.f, 10.f));
		IntroMotion.Start();
	}

	// 이름부터 치는 것이 자연스럽다. 창도 포커스를 받을 수 있어야 입력칸 밖을 눌러도 ESC가 온다
	SetIsFocusable(true);
	NameTextBox->SetKeyboardFocus();
}

void UNUCharacterCreateWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	IntroMotion.Advance(InDeltaTime);
	LookMotion.Advance(InDeltaTime);
}

void UNUCharacterCreateWidget::NativeDestruct()
{
	if (UNUCharacterManager* Manager = GetManager())
	{
		Manager->OnCharacterNameChecked.RemoveDynamic(this, &UNUCharacterCreateWidget::HandleNameCheckResult);
	}
	Super::NativeDestruct();
}

FReply UNUCharacterCreateWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CancelCreate();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UNUCharacterCreateWidget::BindButton(UWidget* Widget, const TCHAR* FunctionName)
{
	if (Widget == nullptr)
	{
		return;
	}

	// AddDynamic은 함수 이름을 매크로로 박아야 해서, 표로 묶으려면 이 형태를 쓴다.
	// 순정 Button(레거시) / UNU_CommonButtonWidget(WBP_CommonButton) 둘 다 지원한다 --- 둘 다
	// 파라미터 없는 단순 델리게이트라 FScriptDelegate 하나로 양쪽에 그대로 쓸 수 있다
	FScriptDelegate Delegate;
	Delegate.BindUFunction(this, FName(FunctionName));
	if (UButton* Button = Cast<UButton>(Widget))
	{
		Button->OnClicked.Remove(Delegate); // 재구성 시 중복 구독 방지
		Button->OnClicked.Add(Delegate);
	}
	else if (UNU_CommonButtonWidget* CommonButton = Cast<UNU_CommonButtonWidget>(Widget))
	{
		CommonButton->OnCommonClicked.Remove(Delegate);
		CommonButton->OnCommonClicked.Add(Delegate);
	}
}

UTextBlock* UNUCharacterCreateWidget::FindText(const TCHAR* Name) const
{
	return Cast<UTextBlock>(GetWidgetFromName(FName(Name)));
}

/////////////////////////////////////////////////////////////////
// 종족 목록
/////////////////////////////////////////////////////////////////

void UNUCharacterCreateWidget::SetMarkVisible(const FString& WidgetName, bool bVisible) const
{
	if (UWidget* Mark = GetWidgetFromName(FName(*WidgetName)))
	{
		Mark->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UNUCharacterCreateWidget::SetOptionRowEmpty(const TCHAR* Prefix, bool bEmpty) const
{
	// 조작부는 안의 버튼이 눌려야 하므로 자기만 클릭을 흘린다
	if (UWidget* Options = GetWidgetFromName(FName(*FString::Printf(TEXT("%sOptions"), Prefix))))
	{
		Options->SetVisibility(bEmpty ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	SetMarkVisible(FString::Printf(TEXT("%sEmpty"), Prefix), bEmpty);
}

void UNUCharacterCreateWidget::LoadRaces()
{
	Races.Reset();

	if (const UDataTable* Table = RaceTable.LoadSynchronous())
	{
		Table->ForeachRow<FNURaceRow>(TEXT("NUCharacterCreate"),
			[this](const FName& RowName, const FNURaceRow& Row)
			{
				FRaceEntry& Entry = Races.AddDefaulted_GetRef();
				Entry.RowName = RowName;
				Entry.RaceId = Row.RaceId;
				Entry.DisplayName = Row.DisplayName.IsEmpty() ? FText::FromName(RowName) : Row.DisplayName;
				Entry.Description = Row.Description;
				Entry.Emblem = Row.Emblem;
				Entry.SortOrder = Row.SortOrder;
				Entry.bPlayable = Row.bPlayable;
			});

		Races.Sort([](const FRaceEntry& A, const FRaceEntry& B)
			{
				return A.SortOrder != B.SortOrder
					? A.SortOrder < B.SortOrder
					: A.RowName.LexicalLess(B.RowName);
			});
	}

	// 표가 없거나 비었으면 임시 목록으로 --- 표가 붙는 순간 이 경로는 안 탄다
	if (Races.IsEmpty())
	{
		const TArray<FNURaceFallback>& Fallbacks = NURace::GetFallbacks();
		UE_LOG(LogNUCharSelect, Log, TEXT("종족 표가 없어 임시 목록을 쓴다 (%d종)"), Fallbacks.Num());
		for (const FNURaceFallback& Fallback : Fallbacks)
		{
			FRaceEntry& Entry = Races.AddDefaulted_GetRef();
			Entry.RowName = Fallback.RowName;
			Entry.RaceId = Fallback.RaceId;
			Entry.DisplayName = FText::FromString(Fallback.DisplayName);
			Entry.Description = FText::FromString(Fallback.Description);
			Entry.Emblem = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(Fallback.EmblemPath));
			Entry.bPlayable = Fallback.bPlayable;
		}
	}

	// 고르고 있던 종족이 잠겼으면 놓는다 (초안 복원이 잠긴 종족을 되살리는 것 방지)
	if (Races.IsValidIndex(RaceIndex) && !Races[RaceIndex].bPlayable)
	{
		RaceIndex = INDEX_NONE;
	}
	ApplyStandLocks();

	// 안 고른 상태(INDEX_NONE)는 그대로 둔다 --- 표가 줄었을 때만 범위로 당긴다
	if (Races.IsValidIndex(RaceIndex))
	{
		RaceIndex = FMath::Min(RaceIndex, Races.Num() - 1);
	}
	else if (RaceIndex != INDEX_NONE)
	{
		RaceIndex = INDEX_NONE;
	}
}

/////////////////////////////////////////////////////////////////
// 초안 (매니저에 보관 --- 레벨을 오가도 남는다)
/////////////////////////////////////////////////////////////////

void UNUCharacterCreateWidget::RestoreDraft()
{
	const UNUCharacterManager* Manager = GetManager();
	if (Manager == nullptr)
	{
		return;
	}

	const FNUCharacterCreateParams& Draft = Manager->GetDraftParams();
	Gender = PickOpenGender(Draft.Gender); // 초안의 성별이 그 사이 잠겼으면 열린 쪽으로
	// 초안도 번호로 들고 있다 --- 표가 바뀌어 사라진 파츠면 첫 칸으로 떨어진다
	// 머리 번호에는 머리 색이 같이 실려 있다 --- 견본에서 빠진 색이면 원래 색으로 떨어진다
	int32 DraftHairPartId = 0;
	int32 DraftHairColor = 0;
	NUAppearance::SplitHairId(Draft.HairId, DraftHairPartId, DraftHairColor);
	HairIndex = FindPartIndex(HairParts, DraftHairPartId);
	HairColorIndex = GetDefault<UNUAppearanceDataSettings>()->HairColors.IsValidIndex(DraftHairColor) ? DraftHairColor : 0;
	FaceIndex = FindPartIndex(FaceParts, Draft.FaceId);
	SkinIndex = FindPartIndex(SkinParts, Draft.SkinId);

	// 종족은 이름으로 들고 있으므로 지금 목록에서 자리를 찾는다 (표가 바뀌었을 수 있다)
	if (!Draft.RaceRowName.IsNone())
	{
		const int32 Found = Races.IndexOfByPredicate(
			[&Draft](const FRaceEntry& Entry) { return Entry.RowName == Draft.RaceRowName && Entry.bPlayable; });
		RaceIndex = Found; // 표에서 사라졌거나 잠긴 종족이면 안 고른 상태로 돌아간다
	}
}

void UNUCharacterCreateWidget::SaveDraft() const
{
	if (UNUCharacterManager* Manager = GetManager())
	{
		Manager->SetDraftParams(BuildParams());
	}
}

/////////////////////////////////////////////////////////////////
// 표시 갱신
/////////////////////////////////////////////////////////////////

void UNUCharacterCreateWidget::RefreshPreview()
{
	const bool bPicked = Races.IsValidIndex(RaceIndex);
	const int32 PickedRaceId = bPicked ? Races[RaceIndex].RaceId : INDEX_NONE;

	// 캐릭터들은 늘 서 있다 --- 고르면 그 자리만 원래 색으로 남고 나머지가 어두워진다.
	// 아직 안 골랐으면 넷 다 밝게 (고르는 중이니 하나를 편들 이유가 없다)
	AActor* ViewTarget = OverviewCamera.Get();
	for (const TWeakObjectPtr<ANUCharacterCreatePreview>& Weak : PreviewStands)
	{
		ANUCharacterCreatePreview* Stand = Weak.Get();
		if (Stand == nullptr)
		{
			continue;
		}

		const int32 StandRaceId = Stand->GetRaceId();
		const bool bIsPicked = bPicked && StandRaceId == PickedRaceId;
		Stand->SetHighlighted(!bPicked || bIsPicked);

		// 고른 자리만 생성창의 선택(성별·머리·외모·피부)을 입는다. 나머지 자리는 자기 기본 모습(기본 성별 몸, 파츠 없음)으로
		// 되돌린다 --- 여성으로 보던 자리도 다른 종족으로 옮기면 남성 몸으로 초기화된다 (9/4 유민 결정).
		// 파츠 목록이 고른 성별 기준이라 다른 자리에 얹으면 몸과 안 맞는다
		if (bIsPicked)
		{
			Stand->ShowAppearance(BuildParams(),
				HairParts.IsValidIndex(HairIndex) ? HairParts[HairIndex].Flipbook.LoadSynchronous() : nullptr,
				FaceParts.IsValidIndex(FaceIndex) ? FaceParts[FaceIndex].Flipbook.LoadSynchronous() : nullptr,
				SkinParts.IsValidIndex(SkinIndex) ? SkinParts[SkinIndex].Tint : FLinearColor::White);
		}
		else
		{
			Stand->ResetAppearance();
		}

		if (bIsPicked)
		{
			// 자리마다 카메라를 따로 물릴 수 있게는 해 두었다 --- 지금은 넷 다 전경 하나를 본다.
			// 배경을 종족별로 뽑으면 그때 카메라만 벌리면 된다
			ViewTarget = Stand->GetStandCamera() != nullptr ? Stand->GetStandCamera() : Stand;
		}
	}

	APlayerController* PC = GetOwningPlayer();
	if (PC == nullptr || ViewTarget == nullptr || PC->GetViewTarget() == ViewTarget)
	{
		return; // 레벨에 카메라를 아직 안 놓았거나 이미 그 자리를 보고 있다
	}

	// 화면이 열릴 때는 즉시 잡아야 첫 프레임이 흔들리지 않는다
	PC->SetViewTargetWithBlend(ViewTarget, bPreviewShown ? PreviewBlendTime : 0.0f);
	bPreviewShown = true;
}

void UNUCharacterCreateWidget::LoadPartTable(const TSoftObjectPtr<UDataTable>& Table,
	TArray<FNUAppearancePartRow>& Out) const
{
	Out.Reset();

	const UDataTable* Loaded = Table.LoadSynchronous();
	if (Loaded != nullptr)
	{
		const int32 PickedRaceId = Races.IsValidIndex(RaceIndex) ? Races[RaceIndex].RaceId : 0;
		const int32 PickedGender = static_cast<int32>(Gender);
		Loaded->ForeachRow<FNUAppearancePartRow>(TEXT("NUCharacterCreate"),
			[&Out, PickedRaceId, PickedGender](const FName& RowName, const FNUAppearancePartRow& Row)
			{
				// 종족·성별 전용 파츠는 그 몸일 때만. 0/-1은 공용이라 늘 보인다. 직업 전용 파츠는 직업이 없는 생성창에 안 나온다
				if (Row.FitsBody(PickedRaceId, PickedGender, 0))
				{
					Out.Add(Row);
				}
			});

		Out.Sort([](const FNUAppearancePartRow& A, const FNUAppearancePartRow& B)
			{
				return A.SortOrder != B.SortOrder ? A.SortOrder < B.SortOrder : A.PartId < B.PartId;
			});
	}

	// 표 자체가 없으면 번호만 도는 더미로 --- 파츠가 오기 전에도 화면은 성립해야 한다.
	// PartId를 0부터 매겨 두면 표가 붙기 전에 만든 캐릭터와 값이 어긋나지 않는다.
	// 표는 있는데 이 몸에 맞는 파츠가 없는 경우(여캐 파츠 도착 전)는 빈 목록 그대로 --- 몸만 나온다
	if (Loaded == nullptr && Out.IsEmpty())
	{
		Out.SetNum(FMath::Max(1, FallbackPartCount));
		for (int32 i = 0; i < Out.Num(); ++i)
		{
			Out[i].PartId = i;
		}
	}
}

int32 UNUCharacterCreateWidget::FindPartIndex(const TArray<FNUAppearancePartRow>& Parts, int32 PartId)
{
	const int32 Found = Parts.IndexOfByPredicate(
		[PartId](const FNUAppearancePartRow& Entry) { return Entry.PartId == PartId; });
	return Found != INDEX_NONE ? Found : 0;
}

void UNUCharacterCreateWidget::LoadParts()
{
	const int32 PickedRaceId = Races.IsValidIndex(RaceIndex) ? Races[RaceIndex].RaceId : 0;
	const int32 PickedGender = static_cast<int32>(Gender);
	if (PickedRaceId == PartsRaceId && PickedGender == PartsGender)
	{
		return; // 몸(종족·성별)이 그대로면 목록도 그대로다 --- 표를 다시 읽을 이유가 없다
	}
	PartsRaceId = PickedRaceId;
	PartsGender = PickedGender;

	// 목록이 바뀌어도 고르던 파츠를 놓치지 않게 번호를 먼저 붙들어 둔다
	const int32 KeepHair = HairParts.IsValidIndex(HairIndex) ? HairParts[HairIndex].PartId : 0;
	const int32 KeepFace = FaceParts.IsValidIndex(FaceIndex) ? FaceParts[FaceIndex].PartId : 0;
	const int32 KeepSkin = SkinParts.IsValidIndex(SkinIndex) ? SkinParts[SkinIndex].PartId : 0;

	const UNUAppearanceDataSettings* Settings = GetDefault<UNUAppearanceDataSettings>();
	LoadPartTable(Settings->HairPartTable, HairParts);
	LoadPartTable(Settings->FacePartTable, FaceParts);
	LoadPartTable(Settings->SkinPartTable, SkinParts);

	HairIndex = FindPartIndex(HairParts, KeepHair);
	FaceIndex = FindPartIndex(FaceParts, KeepFace);
	SkinIndex = FindPartIndex(SkinParts, KeepSkin);
}

void UNUCharacterCreateWidget::BuildRaceList()
{
	// 버튼 모양은 WBP가 갖는다 --- 종족이 늘 수 있어 RaceButton0부터 이어지는 이름으로 찾는다
	RaceButtons.Reset();
	while (UNU_CommonButtonWidget* Button = Cast<UNU_CommonButtonWidget>(
		GetWidgetFromName(FName(*FString::Printf(TEXT("RaceButton%d"), RaceButtons.Num())))))
	{
		RaceButtons.Add(Button);
	}
	if (RaceButtons.Num() < Races.Num())
	{
		UE_LOG(LogNUCharSelect, Warning, TEXT("종족 버튼이 %d개뿐이라 종족 %d개가 목록에 안 나온다 (WBP에 RaceButton%d부터 더 놓을 것)"),
			RaceButtons.Num(), Races.Num() - RaceButtons.Num(), RaceButtons.Num());
	}

	for (int32 ButtonIndex = 0; ButtonIndex < RaceButtons.Num(); ++ButtonIndex)
	{
		UNU_CommonButtonWidget* Button = RaceButtons[ButtonIndex];
		if (!Races.IsValidIndex(ButtonIndex))
		{
			Button->SetVisibility(ESlateVisibility::Collapsed); // 종족보다 많이 놓인 버튼
			SetMarkVisible(FString::Printf(TEXT("RaceLock%d"), ButtonIndex), false);
			SetMarkVisible(FString::Printf(TEXT("RaceSelected%d"), ButtonIndex), false);
			continue;
		}

		const FRaceEntry& Race = Races[ButtonIndex];
		Button->Tag = ButtonIndex; // 누르면 이 번호가 온다 --- WBP에 적힌 Tag보다 이름 순서가 이긴다
		Button->OnCommonClickedWithTag.AddUniqueDynamic(this, &UNUCharacterCreateWidget::HandleRaceButtonClicked);
		Button->SetIsEnabled(Race.bPlayable); // 미개방 종족은 눌리지 않는다
		Button->SetLabelText(Race.bPlayable
			? Race.DisplayName
			: FText::Format(NSLOCTEXT("NUCharacter", "RaceLocked", "{0} (준비 중)"), Race.DisplayName));
		Button->SetIconTexture(Race.Emblem.LoadSynchronous()); // 임포트 전이면 아이콘 자리가 접힌다
		SetMarkVisible(FString::Printf(TEXT("RaceLock%d"), ButtonIndex), !Race.bPlayable); // 잠금 표시는 WBP가 그린다
	}
}

void UNUCharacterCreateWidget::BuildHairColorList()
{
	// 견본 모양도 WBP가 갖는다 --- HairColorButton0부터 이어지는 이름으로 찾는다. 색은 프로젝트 세팅(HairColors)에서 온다
	HairColorButtons.Reset();
	while (UNU_CommonButtonWidget* Button = Cast<UNU_CommonButtonWidget>(
		GetWidgetFromName(FName(*FString::Printf(TEXT("HairColorButton%d"), HairColorButtons.Num())))))
	{
		HairColorButtons.Add(Button);
	}

	const TArray<FLinearColor>& Colors = GetDefault<UNUAppearanceDataSettings>()->HairColors;
	for (int32 Index = 0; Index < HairColorButtons.Num(); ++Index)
	{
		UNU_CommonButtonWidget* Button = HairColorButtons[Index];
		const bool bUsed = Colors.IsValidIndex(Index);
		// 견본을 감싼 자리가 있으면 그것째, 없으면 버튼만 접는다 (견본보다 많이 놓인 칸)
		UWidget* Cell = GetWidgetFromName(FName(*FString::Printf(TEXT("HairColorCell%d"), Index)));
		(Cell != nullptr ? Cell : static_cast<UWidget*>(Button))->SetVisibility(
			bUsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		if (!bUsed)
		{
			continue;
		}

		Button->Tag = Index; // 누르면 이 번호가 온다 --- 이름 순서가 곧 색 번호다
		Button->OnCommonClickedWithTag.AddUniqueDynamic(this, &UNUCharacterCreateWidget::HandleHairColorClicked);
		if (UImage* Fill = Cast<UImage>(GetWidgetFromName(FName(*FString::Printf(TEXT("HairColorFill%d"), Index)))))
		{
			FLinearColor Color = Colors[Index];
			Color.A = 1.0f;
			Fill->SetColorAndOpacity(Color);
		}
	}
}

void UNUCharacterCreateWidget::RefreshDisplay()
{
	// 고른 성별이 지금 종족에서 잠겼으면 열린 쪽으로 옮긴다. 종족을 고르기 전에는 성별 몸 줄이 종족 전용이라
	// 두 성별이 다 잠긴 것으로 나와 기본값(여성)이 남는다 --- 종족을 고르거나 초기화한 뒤에도 그대로 남지 않게 여기서 맞춘다
	Gender = PickOpenGender(Gender);

	// 종족 전용 파츠가 있어 종족이 바뀌면 목록을 다시 짠다 (안 바뀌었으면 바로 빠져나온다)
	LoadParts();

	if (HairIndexText != nullptr)
	{
		HairIndexText->SetText(FText::Format(NSLOCTEXT("NUCharacter", "PageIndex", "{0} / {1}"),
			FText::AsNumber(HairParts.IsEmpty() ? 0 : HairIndex + 1), FText::AsNumber(HairParts.Num())));
	}
	if (FaceIndexText != nullptr)
	{
		FaceIndexText->SetText(FText::Format(NSLOCTEXT("NUCharacter", "PageIndex", "{0} / {1}"),
			FText::AsNumber(FaceParts.IsEmpty() ? 0 : FaceIndex + 1), FText::AsNumber(FaceParts.Num())));
	}

	if (UTextBlock* SkinIndexText = FindText(TEXT("SkinIndexText")))
	{
		SkinIndexText->SetText(FText::Format(NSLOCTEXT("NUCharacter", "PageIndex", "{0} / {1}"),
			FText::AsNumber(SkinParts.IsEmpty() ? 0 : SkinIndex + 1), FText::AsNumber(SkinParts.Num())));
	}

	// 머리 썸네일은 고른 머리 색으로 물들여 보여 준다
	FLinearColor HairTint;
	const bool bHairTinted = NUAppearance::GetHairTint(HairColorIndex, HairTint);
	RefreshThumbs(HairThumbs, HairParts, HairIndex, bHairTinted ? &HairTint : nullptr);
	RefreshThumbs(FaceThumbs, FaceParts, FaceIndex);
	RefreshThumbs(SkinThumbs, SkinParts, SkinIndex);

	// 고를 것이 없는 줄은 화살표만 남지 않게 한다 (WBP에 그 위젯이 있을 때만)
	SetOptionRowEmpty(TEXT("Hair"), HairParts.IsEmpty());
	SetOptionRowEmpty(TEXT("Face"), FaceParts.IsEmpty());
	SetOptionRowEmpty(TEXT("Skin"), SkinParts.IsEmpty());

	// 머리 색 --- 머리 파츠가 없는 몸(머리가 몸에 그려진 몸)은 물들일 레이어가 없어 줄째 접는다
	if (UWidget* HairColorRow = GetWidgetFromName(TEXT("HairColorRow")))
	{
		HairColorRow->SetVisibility(!HairColorButtons.IsEmpty() && !HairParts.IsEmpty()
			? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
	for (int32 ColorIndex = 0; ColorIndex < HairColorButtons.Num(); ++ColorIndex)
	{
		SetMarkVisible(FString::Printf(TEXT("HairColorSelected%d"), ColorIndex), ColorIndex == HairColorIndex);
	}

	// 세 줄 모두 고를 것이 없으면 줄마다 같은 말을 되풀이하지 않고 안내 한 줄로 바꾼다 (WBP에 그 위젯이 있을 때만)
	if (UWidget* LookNote = GetWidgetFromName(TEXT("LookEmptyNote")))
	{
		const bool bNothingToPick = HairParts.IsEmpty() && FaceParts.IsEmpty() && SkinParts.IsEmpty();
		LookNote->SetVisibility(bNothingToPick ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (UWidget* LookRows = GetWidgetFromName(TEXT("LookRows")))
		{
			LookRows->SetVisibility(bNothingToPick ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		}
		if (UTextBlock* NoteText = Cast<UTextBlock>(LookNote))
		{
			NoteText->SetText(FText::Format(
				NSLOCTEXT("NUCharacter", "LookEmptyNote", "{0} 외형은 준비 중입니다. 기본 모습으로 만들어집니다."),
				Gender == ENUCharacterGender::Female
					? NSLOCTEXT("NUCharacter", "GenderFemale", "여성")
					: NSLOCTEXT("NUCharacter", "GenderMale", "남성")));
		}
	}

	const FRaceEntry* Race = Races.IsValidIndex(RaceIndex) ? &Races[RaceIndex] : nullptr;

	// 문양 --- 텍스처가 없으면(임포트 전·표에 안 채움) 빈 네모 그대로
	if (RaceEmblem != nullptr)
	{
		UTexture2D* EmblemTexture = Race != nullptr ? Race->Emblem.LoadSynchronous() : nullptr;
		if (EmblemTexture != nullptr)
		{
			RaceEmblem->SetBrushColor(FLinearColor::White); // 빈 칸용 어두운 틴트를 걷는다
			RaceEmblem->SetBrushFromTexture(EmblemTexture);
		}
		else
		{
			RaceEmblem->SetBrushColor(ThumbNormalColor);
		}
	}

	RaceNameText->SetText(Race != nullptr ? Race->DisplayName : FText::GetEmpty());
	RaceDescText->SetText(Race != nullptr ? Race->Description : FText::GetEmpty());

	if (GenderText != nullptr)
	{
		GenderText->SetText(Gender == ENUCharacterGender::Female
			? NSLOCTEXT("NUCharacter", "GenderFemale", "여성")
			: NSLOCTEXT("NUCharacter", "GenderMale", "남성"));
	}

	// 고른 성별은 켜진 채 고른 모양으로, 몸이 없는 성별은 종족처럼 잠근다 --- 꺼지고 "(준비 중)"이 붙는다.
	// 고른 쪽까지 꺼 두면 잠긴 것과 같은 회색이라 둘 다 못 고르는 것처럼 보였다 (유민 지적, 9/4)
	// 버튼 순서(여성, 남성)와 코드값(남성 0, 여성 1)이 다르므로 인덱스가 아니라 열거형으로 맞춘다
	UWidget* const GenderButtons[] = { GenderFemaleButton, GenderMaleButton };
	const ENUCharacterGender ButtonGenders[] = { ENUCharacterGender::Female, ENUCharacterGender::Male };
	const FText GenderLabels[] = {
		NSLOCTEXT("NUCharacter", "GenderFemale", "여성"),
		NSLOCTEXT("NUCharacter", "GenderMale", "남성") };
	const int32 CurrentRaceId = Race != nullptr ? Race->RaceId : 0;
	for (int32 i = 0; i < 2; ++i)
	{
		UWidget* Widget = GenderButtons[i];
		if (Widget == nullptr)
		{
			continue;
		}
		const bool bOpen = NUAppearance::IsGenderOpen(CurrentRaceId, static_cast<int32>(ButtonGenders[i]));
		Widget->SetIsEnabled(bOpen);
		const bool bSelected = Gender == ButtonGenders[i];
		// 고른 쪽에만 보이는 덧그림 --- 글자색까지 바뀌는 모양은 WBP가 이 위젯 하나로 통째로 그린다
		SetMarkVisible(i == 0 ? TEXT("GenderFemaleSelected") : TEXT("GenderMaleSelected"), bSelected);
		const FText Label = bOpen
			? GenderLabels[i]
			: FText::Format(NSLOCTEXT("NUCharacter", "GenderLocked", "{0} (준비 중)"), GenderLabels[i]);

		// 순정 Button(레거시)은 라벨이 직접 자식(GetChildAt(0))이지만, UNU_CommonButtonWidget은
		// 라벨이 내부 트리에 있어서 SetLabelText로 넣어야 한다
		if (UButton* Button = Cast<UButton>(Widget))
		{	
			Button->SetBackgroundColor(bSelected ? ThumbSelectedColor : FLinearColor::White); // 순정 Button은 고른 그림이 없어 색으로
			if (UTextBlock* LabelText = Cast<UTextBlock>(Button->GetChildAt(0)))
			{
				LabelText->SetText(Label);
			}
		}
		else if (UNU_CommonButtonWidget* CommonButton = Cast<UNU_CommonButtonWidget>(Widget))
		{
			CommonButton->SetSelected(bSelected); // 고른 모양은 버튼 인스턴스의 SelectedButtonTexture (WBP가 정한다)
			CommonButton->SetLabelText(Label);
		}
	}

	// 고른 종족 버튼만 고른 모양 (성별 버튼과 같다 --- 그림은 버튼 인스턴스의 SelectedButtonTexture)
	for (int32 ButtonIndex = 0; ButtonIndex < RaceButtons.Num(); ++ButtonIndex)
	{
		RaceButtons[ButtonIndex]->SetSelected(ButtonIndex == RaceIndex);
		SetMarkVisible(FString::Printf(TEXT("RaceSelected%d"), ButtonIndex), ButtonIndex == RaceIndex);
	}

	RefreshPreview();

	// 외형이 바뀌었으면 캐릭터 둘레에서 작은 반짝임(LookSparkle0..N)이 차례로 터졌다 사라진다 (처음 그릴 때와 이름만 바뀐 경우는 아니다)
	const FNUCharacterCreateParams Look = BuildParams();
	const FString LookKey = FString::Printf(TEXT("%d/%d/%d/%d/%d"),
		Look.RaceId, static_cast<int32>(Look.Gender), Look.HairId, Look.FaceId, Look.SkinId);
	if (!LastLook.IsEmpty() && LastLook != LookKey)
	{
		// 순서를 섞어 둘레를 돌며 터지게 한다 (이웃한 반짝임이 연달아 터지지 않게)
		for (int32 Index = 0; ; ++Index)
		{
			UWidget* Sparkle = GetWidgetFromName(FName(*FString::Printf(TEXT("LookSparkle%d"), Index)));
			if (Sparkle == nullptr)
			{
				break;
			}
			LookMotion.AddPulse(Sparkle, 0.045f * ((Index * 3) % 7), 0.42f, 1.f, 0.6f);
		}
		LookMotion.Start();
	}
	LastLook = LookKey;

	RefreshNameState();
	SaveDraft(); // 고른 것이 바뀔 때마다 매니저에 적어 둔다
	OnCreateStateChanged();
}

void UNUCharacterCreateWidget::RefreshNameState()
{
	const FString Name = GetTrimmedName();

	// 서버가 쓸 수 있다고 답한 그 이름 그대로여야 생성이 열린다.
	// 한 글자만 고쳐도 다시 물어봐야 한다 (이승진 님 지적, 8/26)
	const bool bChecked = !Name.IsEmpty() && Name.Equals(CheckedName, ESearchCase::CaseSensitive);

	if (Name.IsEmpty())
	{
		NameState = ENUNameCheck::TooShort;
	}
	else
	{
		NameState = NUCharacterName::Check(Name);

		// 형식은 맞는데 서버가 이미 물린 이름이면 그 사유가 이긴다 (중복·금칙어·보호 기간)
		if (NameState == ENUNameCheck::Ok && Name.Equals(RejectedName, ESearchCase::CaseSensitive))
		{
			NameState = RejectedReason;
		}
	}

	// 답을 기다리는 중에는 "확인 중..."을 덮지 않는다 (종족을 바꿔도 이 함수가 불린다)
	if (!bNameCheckInFlight)
	{
		if (Name.IsEmpty())
		{
			// 아무것도 안 쳤을 때는 빨간 글씨로 다그치지 않는다
			SetNameMessage(FText::GetEmpty(), false);
		}
		else if (NameState != ENUNameCheck::Ok)
		{
			SetNameMessage(NUCharacterName::GetMessage(NameState), true);
		}
		else if (bChecked)
		{
			SetNameMessage(NSLOCTEXT("NUCharacter", "NameAvailable", "사용 가능한 이름입니다."), false);
		}
		else
		{
			// 형식은 맞지만 아직 안 물어봤다 --- 생성이 왜 안 눌리는지 알려 준다
			SetNameMessage(NSLOCTEXT("NUCharacter", "NameNeedCheck", "중복 확인을 눌러 주세요."), false);
		}
	}

	// 종족은 생성 후 못 바꾼다 --- 고르지 않은 채로 만들어지면 안 된다
	CreateButton->SetIsEnabled(NameState == ENUNameCheck::Ok && bChecked
		&& Races.IsValidIndex(RaceIndex)
		&& !bNameCheckInFlight && !bConfirmInFlight);

	// 답을 기다리는 중이거나 이미 통과한 이름이면 또 물어볼 것이 없다
	NameCheckButton->SetIsEnabled(!bNameCheckInFlight && !bChecked && !bConfirmInFlight);

	// 눌릴 수 있을 때만 생성 버튼에 빛이 지나간다 (WBP에 있을 때만)
	SetMarkVisible(TEXT("CreateShine"), CreateButton->GetIsEnabled());

	// 통과한 이름에만 보이는 표시 (WBP에 있을 때만)
	SetMarkVisible(TEXT("NameOkMark"), !bNameCheckInFlight && NameState == ENUNameCheck::Ok && bChecked);
}

void UNUCharacterCreateWidget::RefreshThumbs(const TArray<TObjectPtr<UBorder>>& Thumbs,
	const TArray<FNUAppearancePartRow>& Parts, int32 Index, const FLinearColor* Tint) const
{
	if (Thumbs.IsEmpty())
	{
		return;
	}

	// 가짓수가 칸 수보다 많으면 고른 것이 가운데 오도록 창을 민다
	const int32 Count = Parts.Num();
	const int32 Visible = Thumbs.Num();
	const int32 Start = Count > Visible
		? FMath::Clamp(Index - Visible / 2, 0, Count - Visible)
		: 0;

	for (int32 SlotIndex = 0; SlotIndex < Visible; ++SlotIndex)
	{
		UBorder* Box = Thumbs[SlotIndex];
		if (Box == nullptr)
		{
			continue;
		}
		const int32 Which = Start + SlotIndex;
		const bool bUsed = Which < Count;
		Box->SetVisibility(bUsed ? ESlateVisibility::Visible
			: (bCollapseUnusedThumbs ? ESlateVisibility::Collapsed : ESlateVisibility::Hidden));
		// 칸 크기를 SizeBox로 잡은 WBP는 그 SizeBox가 자리를 차지한다 --- 접을 때는 같이 접어야 빈자리가 안 남는다
		if (bCollapseUnusedThumbs)
		{
			// 칸을 겹쳐 그리는 WBP는 감싼 자리에 "칸 이름 + Cell"을 붙여 둔다 --- 없으면 바로 위 SizeBox
			UWidget* Cell = GetWidgetFromName(FName(*(Box->GetName() + TEXT("Cell"))));
			if (Cell == nullptr)
			{
				Cell = Cast<USizeBox>(Box->GetParent());
			}
			if (Cell != nullptr)
			{
				Cell->SetVisibility(bUsed ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
			}
		}
		// 고른 칸에만 보이는 덧그림 (WBP에 있을 때만) --- 테두리를 그림 위에 얹을 수 있다
		SetMarkVisible(Box->GetName() + TEXT("Selected"), bUsed && Which == Index);

		// 아이콘이 있으면 그림을, 없으면 빈 네모 (파츠 오기 전)
		UTexture2D* Icon = Parts.IsValidIndex(Which) ? Parts[Which].Icon.LoadSynchronous() : nullptr;
		if (Icon != nullptr)
		{
			SetThumbIcon(Box, Icon, Which == Index, Tint);
		}
		else
		{
			// 그림 있던 칸이 빈 칸으로 돌아올 수 있다 --- 이미지만 걷는다
			if (Cast<UImage>(Box->GetContent()) != nullptr)
			{
				Box->SetContent(nullptr);
			}
			Box->SetBrushColor(Which == Index ? ThumbSelectedColor : ThumbNormalColor);
		}
	}
}

void UNUCharacterCreateWidget::SetThumbIcon(UBorder* Box, UTexture2D* Icon, bool bSelected, const FLinearColor* Tint) const
{
	if (WidgetTree == nullptr)
	{
		return;
	}

	// 칸 테두리는 색만 --- 그림은 안쪽 이미지가 맡는다 (확대해도 칸 밖으로 안 새게 자른다)
	Box->SetBrushColor(bSelected ? ThumbSelectedColor : ThumbNormalColor);
	Box->SetClipping(EWidgetClipping::ClipToBounds);

	// WBP 칸(Border)은 비어 있다 --- 그림은 안쪽 이미지가 맡는다
	UImage* Image = Cast<UImage>(Box->GetContent());
	if (Image == nullptr)
	{
		Image = WidgetTree->ConstructWidget<UImage>();
		Box->SetContent(Image); // 기본 Fill --- 이미지 위젯이 칸을 꽉 채운다
	}

	FSlateBrush Brush;
	Brush.SetResourceObject(Icon);
	Brush.ImageSize = FVector2D(ThumbBoxSize, ThumbBoxSize);
	// 머리 색 --- 그림 대신 물들이는 UI 머티리얼을 그린다 (머티리얼이 없으면 원래 그림)
	if (UMaterialInterface* TintMaterial = Tint != nullptr
		? GetDefault<UNUAppearanceDataSettings>()->HairTintUIMaterial.LoadSynchronous() : nullptr)
	{
		UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(TintMaterial, Image);
		Instance->SetTextureParameterValue(TEXT("SpriteTexture"), Icon);
		Instance->SetVectorParameterValue(TEXT("HairTint"), *Tint);
		Brush.SetResourceObject(Instance);
	}
	Image->SetBrush(Brush);

	if (Icon->GetSizeX() == static_cast<int32>(PartCanvasWidth)
		&& Icon->GetSizeY() == static_cast<int32>(PartCanvasHeight))
	{
		// 몸 캔버스 원본 --- 칸에 눌려 들어간 그림에 배율·이동을 걸어 머리 영역만 확대한다.
		// (위젯 크기를 키우는 방식은 SizeBox가 크기를 강제해 잘려 나갔다)
		const float Scale = ThumbBoxSize / ThumbHeadSpan; // 캔버스 1px가 칸에서 차지할 픽셀
		FWidgetTransform Zoom;
		Zoom.Scale = FVector2D(PartCanvasWidth, PartCanvasHeight) * (Scale / ThumbBoxSize);
		Zoom.Translation.X = ThumbBoxSize * 0.5f - PartCanvasWidth * 0.5f * Scale;
		Zoom.Translation.Y = ThumbBoxSize * 0.5f - ThumbHeadCenterY * Scale;
		Image->SetRenderTransformPivot(FVector2D::ZeroVector);
		Image->SetRenderTransform(Zoom);
	}
	else
	{
		Image->SetRenderTransform(FWidgetTransform()); // 전용 아이콘이 오면 그대로 쓴다
	}

	// 고른 칸 표시는 그림 밝기로 (안 고른 칸을 살짝 죽인다)
	Image->SetColorAndOpacity(bSelected ? FLinearColor::White : ThumbIconDimColor);
}

void UNUCharacterCreateWidget::SetNameMessage(const FText& Message, bool bIsError)
{
	NameErrorText->SetText(Message);
	NameErrorText->SetColorAndOpacity(FSlateColor(bIsError ? NameErrorColor : NameMessageColor));
}

FString UNUCharacterCreateWidget::GetTrimmedName() const
{
	return NUCharacterName::Trim(NameTextBox->GetText().ToString());
}

FNUCharacterCreateParams UNUCharacterCreateWidget::BuildParams() const
{
	FNUCharacterCreateParams Params;
	Params.Name = GetTrimmedName();
	Params.RaceRowName = Races.IsValidIndex(RaceIndex) ? Races[RaceIndex].RowName : NAME_None;
	Params.RaceId = Races.IsValidIndex(RaceIndex) ? Races[RaceIndex].RaceId : 0;
	Params.Gender = Gender;
	// 목록 순서가 아니라 표의 고유 번호를 보낸다 (표가 바뀌어도 외형이 안 밀린다)
	// 머리 색은 머리 번호에 같이 싣는다 (서버에 칸이 따로 없다). 머리 파츠가 없는 몸은 색도 없다
	Params.HairId = HairParts.IsValidIndex(HairIndex)
		? NUAppearance::PackHairId(HairParts[HairIndex].PartId, HairColorIndex) : 0;
	Params.FaceId = FaceParts.IsValidIndex(FaceIndex) ? FaceParts[FaceIndex].PartId : 0;
	Params.SkinId = SkinParts.IsValidIndex(SkinIndex) ? SkinParts[SkinIndex].PartId : 0;
	return Params;
}

/////////////////////////////////////////////////////////////////
// 입력 처리
/////////////////////////////////////////////////////////////////

void UNUCharacterCreateWidget::HandleNameChanged(const FText& Text)
{
	// 확인을 기다리던 이름이 아니게 됐다 --- 늦게 온 응답이 새 입력의 결과처럼 보이면 안 된다
	bNameCheckInFlight = false;
	RefreshNameState();
	SaveDraft();
}

void UNUCharacterCreateWidget::HandleNameCheckClicked()
{
	if (bNameCheckInFlight)
	{
		return; // 응답 대기 중 --- 연타 방지
	}

	// 뻔한 것(길이·문자·자모)은 서버 왕복 없이 그 자리에서 거른다
	const FString Name = GetTrimmedName();
	const ENUNameCheck Result = NUCharacterName::Check(Name);
	if (Result != ENUNameCheck::Ok)
	{
		NameState = Result;
		SetNameMessage(NUCharacterName::GetMessage(Result), true);
		return;
	}

	UNUCharacterManager* Manager = GetManager();
	if (Manager == nullptr)
	{
		return;
	}

	bNameCheckInFlight = true;
	PendingCheckName = Name;

	// 보내기 전에 기다리는 모양부터 잡는다 --- 서버 없는 개발 경로는 RequestNameCheck 안에서 바로 답이 와서,
	// 이 두 줄이 뒤에 있으면 그 답(사용 가능·중복)의 문구를 "확인 중..."이 덮었다
	RefreshNameState(); // 기다리는 동안 두 버튼을 잠근다 (문구는 덮지 않는다)
	SetNameMessage(NSLOCTEXT("NUCharacter", "NameChecking", "확인 중..."), false);

	Manager->RequestNameCheck(Name); // 응답은 HandleNameCheckResult로
}

void UNUCharacterCreateWidget::HandleNameCheckResult(int32 ResultCode)
{
	if (!bNameCheckInFlight)
	{
		return; // 내가 물어본 것이 아니다 (다른 화면의 확인 등)
	}
	bNameCheckInFlight = false;

	// 확인하는 사이 입력이 바뀌었으면 결과가 그 이름의 것이 아니다 --- 입력 검증으로 되돌린다
	if (!GetTrimmedName().Equals(PendingCheckName, ESearchCase::CaseSensitive))
	{
		RefreshNameState();
		return;
	}

	if (ResultCode == 200)
	{
		// 여기서 200이어도 생성은 1202로 튕길 수 있다 (확인과 생성 사이에 남이 가져감).
		// 진짜 방어선은 생성 시점이고 이건 미리 알려주는 편의 기능이다 (가이드 CharacterNameCheck)
		CheckedName = PendingCheckName;
		RejectedName.Reset();
	}
	else
	{
		// 1202(중복)와 1211(보호 기간)을 꼭 갈라서 띄운다 --- 남이 쓰는 것과 "3일 뒤 풀림"은 다른 상황
		RejectedName = PendingCheckName;
		RejectedReason = NUCharacterName::FromResultCode(ResultCode);
		CheckedName.Reset();
	}

	RefreshNameState(); // 문구와 버튼은 한 곳에서 정한다
}

void UNUCharacterCreateWidget::HandleGenderFemaleClicked()
{
	SelectGender(ENUCharacterGender::Female);
}

void UNUCharacterCreateWidget::HandleGenderMaleClicked()
{
	SelectGender(ENUCharacterGender::Male);
}

void UNUCharacterCreateWidget::SelectGender(ENUCharacterGender InGender)
{
	// 잠긴 성별은 버튼이 꺼져 있지만, WBP가 버튼을 자기 방식으로 그리면 눌릴 수 있어 여기서도 막는다
	const int32 RaceId = Races.IsValidIndex(RaceIndex) ? Races[RaceIndex].RaceId : 0;
	if (!NUAppearance::IsGenderOpen(RaceId, static_cast<int32>(InGender)))
	{
		return;
	}
	if (Gender == InGender)
	{
		return; // 고른 쪽 버튼은 켜져 있다 --- 다시 눌러도 할 일이 없다
	}
	Gender = InGender;
	RefreshDisplay();
}

ENUCharacterGender UNUCharacterCreateWidget::PickOpenGender(ENUCharacterGender Preferred) const
{
	const int32 RaceId = Races.IsValidIndex(RaceIndex) ? Races[RaceIndex].RaceId : 0;
	if (NUAppearance::IsGenderOpen(RaceId, static_cast<int32>(Preferred)))
	{
		return Preferred;
	}
	const ENUCharacterGender Other = Preferred == ENUCharacterGender::Female ? ENUCharacterGender::Male : ENUCharacterGender::Female;
	return NUAppearance::IsGenderOpen(RaceId, static_cast<int32>(Other)) ? Other : Preferred;
}

void UNUCharacterCreateWidget::HandleHairPrevClicked()
{
	HairIndex = (HairIndex + HairParts.Num() - 1) % FMath::Max(1, HairParts.Num());
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleHairNextClicked()
{
	HairIndex = (HairIndex + 1) % FMath::Max(1, HairParts.Num());
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleFacePrevClicked()
{
	FaceIndex = (FaceIndex + FaceParts.Num() - 1) % FMath::Max(1, FaceParts.Num());
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleFaceNextClicked()
{
	FaceIndex = (FaceIndex + 1) % FMath::Max(1, FaceParts.Num());
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleSkinPrevClicked()
{
	SkinIndex = (SkinIndex + SkinParts.Num() - 1) % FMath::Max(1, SkinParts.Num());
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleSkinNextClicked()
{
	SkinIndex = (SkinIndex + 1) % FMath::Max(1, SkinParts.Num());
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleRaceButtonClicked(int32 RaceButtonIndex)
{
	if (Races.IsValidIndex(RaceButtonIndex) && Races[RaceButtonIndex].bPlayable)
	{
		RaceIndex = RaceButtonIndex;
		RefreshDisplay();
	}
}

void UNUCharacterCreateWidget::HandleHairColorClicked(int32 ColorIndex)
{
	if (HairColorButtons.IsValidIndex(ColorIndex)
		&& GetDefault<UNUAppearanceDataSettings>()->HairColors.IsValidIndex(ColorIndex))
	{
		HairColorIndex = ColorIndex;
		RefreshDisplay();
	}
}

void UNUCharacterCreateWidget::HandleRacePrevClicked()
{
	// 미개방 종족은 건너뛴다. 한 바퀴 안에 열린 종족이 없으면 그대로 둔다
	for (int32 Step = 1; Step <= Races.Num(); ++Step)
	{
		const int32 Candidate = Races.IsValidIndex(RaceIndex)
			? (RaceIndex + Races.Num() - Step) % Races.Num()
			: Races.Num() - Step;
		if (Races.IsValidIndex(Candidate) && Races[Candidate].bPlayable)
		{
			RaceIndex = Candidate;
			RefreshDisplay();
			return;
		}
	}
}

void UNUCharacterCreateWidget::HandleRaceNextClicked()
{
	for (int32 Step = 1; Step <= Races.Num(); ++Step)
	{
		const int32 Candidate = Races.IsValidIndex(RaceIndex)
			? (RaceIndex + Step) % Races.Num()
			: Step - 1;
		if (Races.IsValidIndex(Candidate) && Races[Candidate].bPlayable)
		{
			RaceIndex = Candidate;
			RefreshDisplay();
			return;
		}
	}
}

void UNUCharacterCreateWidget::HandleRandomClicked()
{
	// 이름은 건드리지 않는다 --- 중복 확인을 통과한 이름을 지어주려면 서버 패킷이 필요하고,
	// 사용자가 이미 친 이름을 지워버리는 것도 곤란하다 (기획서 4.1은 패킷이 오면 붙인다)
	// 종족 뽑기는 열린 것 중에서만
	TArray<int32> Playable;
	for (int32 Index = 0; Index < Races.Num(); ++Index)
	{
		if (Races[Index].bPlayable)
		{
			Playable.Add(Index);
		}
	}
	if (!Playable.IsEmpty())
	{
		RaceIndex = Playable[FMath::RandRange(0, Playable.Num() - 1)];
	}
	// 성별 뽑기도 열린 것 중에서만
	Gender = PickOpenGender(FMath::RandBool() ? ENUCharacterGender::Female : ENUCharacterGender::Male);
	HairIndex = FMath::RandRange(0, FMath::Max(0, HairParts.Num() - 1));
	// 머리 색도 뽑는다 --- 견본이 놓인 WBP에서만
	HairColorIndex = FMath::RandRange(0, FMath::Max(0,
		FMath::Min(HairColorButtons.Num(), GetDefault<UNUAppearanceDataSettings>()->HairColors.Num()) - 1));
	FaceIndex = FMath::RandRange(0, FMath::Max(0, FaceParts.Num() - 1));
	SkinIndex = FMath::RandRange(0, FMath::Max(0, SkinParts.Num() - 1));
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleResetClicked()
{
	// 첫 번째로 열려 있는 종족으로 (0번이 잠겨 있을 수 있다)
	RaceIndex = Races.IndexOfByPredicate([](const FRaceEntry& Entry) { return Entry.bPlayable; });
	HairIndex = 0;
	FaceIndex = 0;
	SkinIndex = 0;
	HairColorIndex = 0;
	Gender = ENUCharacterGender::Female;
	NameTextBox->SetText(FText::GetEmpty());
	RefreshDisplay();
}

void UNUCharacterCreateWidget::HandleCreateClicked()
{
	RequestCreate();
}

void UNUCharacterCreateWidget::HandleCancelClicked()
{
	CancelCreate();
}

/////////////////////////////////////////////////////////////////
// 생성 / 취소
/////////////////////////////////////////////////////////////////

void UNUCharacterCreateWidget::RequestCreate()
{
	if (bConfirmInFlight)
	{
		return;
	}

	const FString Name = GetTrimmedName();
	const ENUNameCheck Result = NUCharacterName::Check(Name);
	if (Result != ENUNameCheck::Ok)
	{
		NameState = Result;
		SetNameMessage(NUCharacterName::GetMessage(Result), true);
		NameTextBox->SetKeyboardFocus();
		return;
	}

	if (!Races.IsValidIndex(RaceIndex))
	{
		SetNameMessage(NSLOCTEXT("NUCharacter", "NeedRace", "종족을 골라 주세요."), true);
		return;
	}

	// 버튼은 잠가 두지만 이 함수는 BP에서도 부를 수 있다 --- 확인을 통과한 이름인지 한 번 더 본다
	if (!Name.Equals(CheckedName, ESearchCase::CaseSensitive))
	{
		SetNameMessage(NSLOCTEXT("NUCharacter", "NameNeedCheck", "중복 확인을 눌러 주세요."), true);
		NameTextBox->SetKeyboardFocus();
		return;
	}

	const FNUCharacterCreateParams Params = BuildParams();
	const FText RaceName = Races.IsValidIndex(RaceIndex) ? Races[RaceIndex].DisplayName : FText::GetEmpty();

	// 이름과 종족은 생성 후 바꿀 수 없다 --- 되돌릴 수 없는 선택 앞에는 확인 창을 세운다 (기획서 5장)
	const FText Message = FText::Format(
		NSLOCTEXT("NUCharacter", "CreateConfirmMessage",
			"이대로 생성하시겠습니까?\n\n이름 : {0}\n종족 : {1}\n성별 : {2}\n\n이름과 종족은 생성 후 변경할 수 없습니다."),
		FText::FromString(Params.Name),
		RaceName,
		Gender == ENUCharacterGender::Female
			? NSLOCTEXT("NUCharacter", "GenderFemale", "여성")
			: NSLOCTEXT("NUCharacter", "GenderMale", "남성"));

	UNUConfirmDialog* Dialog = UNUConfirmDialog::Show(
		GetOwningPlayer(), ConfirmDialogClass, Message,
		NSLOCTEXT("NUCharacter", "CreateConfirm", "생성하기"),
		FSimpleDelegate::CreateWeakLambda(this, [this, Params]()
			{
				bConfirmInFlight = false;
				OnConfirmed.ExecuteIfBound(Params);
				// 창은 닫지 않는다 --- 서버가 이름을 거절할 수 있어 입력값을 남겨 둔다.
				// 성공하면 호출부(선택창)가 Close()를 부른다
				RefreshNameState();
			}),
		FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				bConfirmInFlight = false;
				RefreshNameState();
			}),
		NSLOCTEXT("NUCharacter", "CreateModify", "수정하기"),
		ENUConfirmIntent::Positive); // 생성은 만드는 동작이다 --- 빨강이면 취소로 오해한다

	if (Dialog == nullptr)
	{
		// 확인 창 WBP가 없으면 확인 없이 진행한다 (개발 편의 --- 선택창 삭제와 같은 규칙)
		UE_LOG(LogNUCharSelect, Warning, TEXT("확인 창 WBP 미지정 --- 확인 없이 생성 요청"));
		OnConfirmed.ExecuteIfBound(Params);
		return;
	}

	bConfirmInFlight = true;
	RefreshNameState();
}

void UNUCharacterCreateWidget::ShowNameRejected(int32 ResultCode)
{
	// 생성이 튕겼다 --- 그 이름을 그대로 두는 동안 사유를 붙여 두고 생성을 다시 잠근다
	RejectedName = GetTrimmedName();
	RejectedReason = NUCharacterName::FromResultCode(ResultCode);
	CheckedName.Reset();

	RefreshNameState();
	NameTextBox->SetKeyboardFocus();
}

void UNUCharacterCreateWidget::CancelCreate()
{
	// 되돌아갈 스텝이 없는 한 화면이라, 친 것이 있을 때만 물어본다
	const bool bHasInput = !GetTrimmedName().IsEmpty();
	if (!bHasInput)
	{
		Close();
		return;
	}

	UNUConfirmDialog* Dialog = UNUConfirmDialog::Show(
		GetOwningPlayer(), ConfirmDialogClass,
		NSLOCTEXT("NUCharacter", "CreateCancelMessage", "생성을 취소하시겠습니까?"),
		NSLOCTEXT("NUCharacter", "CreateCancelYes", "취소하기"),
		FSimpleDelegate::CreateWeakLambda(this, [this]()
			{
				// 취소하고 나갔다 다시 들어오면 빈 화면으로 시작한다
				if (UNUCharacterManager* Manager = GetManager())
				{
					Manager->ClearDraftParams();
				}
				Close();
			}),
		FSimpleDelegate(),
		NSLOCTEXT("NUCharacter", "CreateCancelNo", "계속 만들기"));

	if (Dialog == nullptr)
	{
		Close();
	}
}

void UNUCharacterCreateWidget::Close()
{
	if (bClosed)
	{
		return; // 취소 => 성공처럼 두 경로가 겹칠 수 있다
	}
	bClosed = true;

	RemoveFromParent();

	// 통지 중에 호출부가 다시 열 수 있으므로 떼어내고 실행한다
	FOnNUCharacterCreateClosed Pending = OnClosed;
	OnClosed.Unbind();
	Pending.ExecuteIfBound();
}
