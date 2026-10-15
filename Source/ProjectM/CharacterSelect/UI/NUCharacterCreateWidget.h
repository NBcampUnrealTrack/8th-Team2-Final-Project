// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "UI/NUScreenMotion.h"
#include "NUCharacterCreateWidget.generated.h"

class ANUCharacterCreatePreview;
class UBorder;
class UButton;
class UDataTable;
class UTexture2D;
class UEditableTextBox;
class UNUConfirmDialog;
class UNU_CommonButtonWidget;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FOnNUCharacterCreateConfirmed, const FNUCharacterCreateParams& /*Params*/);
// 어떤 경로로든 창이 닫혔을 때. 호출부가 자기 상태(생성 버튼 잠금 등)를 푸는 데 쓴다
DECLARE_DELEGATE(FOnNUCharacterCreateClosed);

// 캐릭터 생성 화면 --- 한 화면에서 이름·성별·머리·외모·종족을 다 고른다.
// (기획서 흐름도는 3스텝이지만 한 화면으로 가기로 정했다, 8/25)
//
// 생성은 **전용 레벨**이다 (L_CharacterCreate). 종족마다 기본 외형이 갈리고 커마가 늘어날 예정이라,
// 미리보기 캐릭터·카메라를 레벨에 배치해 두는 편이 낫다고 판단했다 (8/25).
// 그래서 이 위젯은 모달이 아니라 게임모드가 만들어 뷰포트에 올리는 전체 화면이다.
//
// 모양은 WBP_CharacterCreate가 갖고 C++는 상태와 자료만 넣는다 (9/4 레브 님 지적 뒤 코드 최소 UI 폴백을 걷어냈다).
// 고른 표시도 WBP가 정한다 (9/11): 종족·성별 버튼은 버튼 인스턴스의 SelectedButtonTexture로 바뀌고(SetSelected, 비워 두면 표시가 안 난다),
// 머리·외모·피부 칸 색과 이름 확인 문구 색은 아래 "NU|Character|색" 값(WBP 클래스 기본값)을 쓴다.
// 아래 위젯은 BindWidget이라 WBP에 같은 이름·타입이 없으면 WBP 컴파일이 실패한다 --- 이름이 바뀌어도 조용히 죽지 않는다:
//   필수: NameTextBox / NameCheckButton / NameErrorText / GenderFemaleButton / GenderMaleButton
//         HairPrevButton / HairNextButton / FacePrevButton / FaceNextButton / SkinPrevButton / SkinNextButton
//         RaceNameText / RaceDescText / RaceEmblem
//         RandomButton / ResetButton / CreateButton / CancelButton / FormPanel(종족을 고르기 전에는 감춘다)
//   선택: GenderText / HairIndexText / FaceIndexText (글자 표시), RacePrevButton / RaceNextButton (넘기기 방식 WBP용)
//   선택 (10/13, 한 명 무대 WBP용 --- 이름으로 찾고 없으면 조용히 넘어간다):
//         SkinIndexText, GenderFemaleSelected / GenderMaleSelected · RaceSelected0..N (고른 것에만 보이는 덧그림),
//         RaceLock0..N (잠긴 종족에만 보이는 표시),
//         HairOptions·HairEmpty / FaceOptions·FaceEmpty / SkinOptions·SkinEmpty (고를 것이 없으면 Options를 접고 Empty를 보인다),
//         LookEmptyNote·LookRows (세 줄 모두 고를 것이 없으면 LookRows를 접고 안내 글 LookEmptyNote 하나만 보인다),
//         HairThumb0Selected.. (고른 칸에만 보이는 덧그림, 칸 이름 + Selected), HairThumb0Cell.. (칸을 감싼 자리, 안 쓰는 칸과 같이 접힌다),
//         NameOkMark (확인을 통과한 이름일 때만 보이는 표시), CreateShine (생성 버튼이 눌릴 수 있을 때만 보이는 반짝임),
//         등장 연출(bPlayIntro): IntroVeil(화면을 덮었다 걷히는 막) · CancelPanel · FormPanel · SheetBody(자식이 차례로),
//         LookSparkle0..N (외형을 바꿀 때 캐릭터 둘레에서 차례로 반짝였다 사라지는 그림 --- 평소 불투명도 0으로 둔다)
//         머리 색: HairColorButton0..N (공용 버튼, 견본 한 칸씩 --- 색 번호 순서), HairColorFill0..N (C++가 견본 색을 칠하는 Image),
//         HairColorSelected0..N (고른 견본에만 보이는 덧그림), HairColorCell0..N (견본을 감싼 자리, 견본보다 많이 놓인 칸은 접힌다),
//         HairColorRow (머리 파츠가 없는 몸이면 줄째 접힌다). 견본 색과 개수는 프로젝트 세팅 NU Appearance Data의 HairColors
//   HairThumb0..N / FaceThumb0..N / SkinThumb0..N 은 개수가 WBP마다 달라 이름으로 찾는다 (있는 만큼만 쓴다)
//   RaceButton0..N (공용 버튼)도 종족이 늘 수 있어 이름으로 찾는다. 0번부터 이어 놓고, 종족보다 많이 놓인 버튼은 접는다.
//     이름·문양·잠금은 C++가 종족 표에서 넣는다 --- WBP에 적어 둔 라벨·아이콘은 디자이너 미리보기용이다
//     (민기 님 요청 9/11: C++가 줄을 만들던 때는 디자이너에서 목록이 비어 보여 손댈 수 없었다)
//
// 서버에 종족·성별·외형을 저장할 필드가 아직 없다 --- 고른 값은 여기서 들고 있다가
// 필드가 생기면 생성 패킷에 얹기만 하면 된다 (요청해 둔 상태).
UCLASS(Abstract)
class PROJECTM_API UNUCharacterCreateWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 게임모드가 뷰포트에 올리기 전에 호출한다. InOnConfirmed는 최종 확인까지 마친 뒤에만 불린다
	void Setup(FOnNUCharacterCreateConfirmed InOnConfirmed, TSubclassOf<UNUConfirmDialog> InConfirmDialogClass);

	// 레벨에 세워 둔 종족별 미리보기 자리들. 종족을 고르면 그 자리로 카메라가 줌인한다 (TOS 방식)
	void SetPreviewStands(const TArray<ANUCharacterCreatePreview*>& InStands);

	// 종족을 고르기 전에 배경 전체를 비추는 카메라
	void SetOverviewCamera(AActor* InCamera);

	// 서버가 이름을 거절했을 때 창을 닫지 않고 그 자리에 사유를 띄운다
	// (기획서 6장: 입력 값을 날리지 않는 것이 핵심)
	void ShowNameRejected(int32 ResultCode);

	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void RequestCreate();

	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void CancelCreate();

	// 화면을 내린다. 레벨을 되돌리는 것은 게임모드 몫이라 여기서는 통지만 한다
	void Close();

	// 취소·성공 --- 어떤 이유로 닫히든 한 번 불린다. 게임모드가 이걸 받고 선택 레벨로 돌아간다
	FOnNUCharacterCreateClosed OnClosed;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// ESC = 취소 확인 (한 화면이라 되돌아갈 스텝이 없다)
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// WBP가 직접 그리고 싶을 때 쓰는 통지 (이름 계약을 안 쓰는 경우)
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Character")
	void OnCreateStateChanged();

	// 종족 표. 미지정이면 코드에 박아 둔 임시 목록을 쓴다 (표가 생기면 그쪽이 이긴다)
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	TSoftObjectPtr<UDataTable> RaceTable;

	// 종족을 고를 때 카메라가 그 자리로 넘어가는 시간(초). 0이면 즉시
	UPROPERTY(EditAnywhere, Category = "NU|Character", meta = (ClampMin = 0.0))
	float PreviewBlendTime = 0.6f;

	// 파츠 표는 프로젝트 세팅(NU Appearance Data)에 있다 --- 필드 캐릭터도 같은 표를 본다.
	// 가짓수는 표의 줄 수에서 나오므로 파츠가 몇 개 오든 코드를 안 고친다.
	// 표가 없을 때 만들 더미 칸 수 (파츠 오기 전 화면 유지용)
	UPROPERTY(EditAnywhere, Category = "NU|Character", meta = (ClampMin = 1))
	int32 FallbackPartCount = 5;

	// 열려 있는 첫 종족을 고른 채로 시작한다 --- 켜면 첫 화면부터 폼이 보인다 (한 명 무대 WBP용, 10/13).
	// 꺼 두면(기본) 지금처럼 레벨의 캐릭터를 눌러야 폼이 열린다. 초안이 남아 있으면 어느 쪽이든 초안이 이긴다
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	bool bAutoPickFirstRace = false;

	// 가짓수보다 많이 놓인 썸네일 칸을 자리째 접는다. 꺼 두면(기본) 자리는 남기고 감춘다 --- 줄 너비가 안 흔들린다
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	bool bCollapseUnusedThumbs = false;

	// 썸네일 칸(HairThumb0 등) 안쪽 한 변의 길이 --- 몸 캔버스 그림에서 머리만 확대할 때 이 크기를 기준으로 맞춘다.
	// WBP의 칸 크기와 같아야 머리가 칸 가운데 온다 (기본 64는 WBP_CharacterCreate의 칸)
	UPROPERTY(EditAnywhere, Category = "NU|Character", meta = (ClampMin = 8))
	float ThumbBoxSize = 64.0f;

	// 열릴 때 등장 연출 (막이 걷히고 시트가 들어오며 줄이 차례로 뜬다). 위젯 이름은 위 목록 --- 없는 것은 건너뛴다
	UPROPERTY(EditAnywhere, Category = "NU|Character")
	bool bPlayIntro = false;

	// --- 색 (WBP 클래스 기본값에서 바꾼다) ---
	// 종족·성별 버튼의 고른 모양은 색이 아니라 버튼 인스턴스의 SelectedButtonTexture다

	// 머리·외모·피부 칸 중 고른 칸의 바탕 (순정 Button으로 만든 성별 버튼도 고르면 이 색)
	UPROPERTY(EditAnywhere, Category = "NU|Character|색")
	FLinearColor ThumbSelectedColor = FLinearColor(0.30f, 0.42f, 0.68f, 1.0f);

	// 나머지 칸의 바탕. 종족 문양 그림이 없을 때 문양 자리도 이 색
	UPROPERTY(EditAnywhere, Category = "NU|Character|색")
	FLinearColor ThumbNormalColor = FLinearColor(0.13f, 0.14f, 0.20f, 1.0f);

	// 안 고른 칸의 파츠 그림에 곱하는 색 (고른 칸은 그림 원래 색)
	UPROPERTY(EditAnywhere, Category = "NU|Character|색")
	FLinearColor ThumbIconDimColor = FLinearColor(0.55f, 0.55f, 0.60f, 1.0f);

	// 이름 확인 오류 문구 (형식·중복·금칙어·보호 기간)
	UPROPERTY(EditAnywhere, Category = "NU|Character|색")
	FLinearColor NameErrorColor = FLinearColor(0.95f, 0.45f, 0.45f, 1.0f);

	// 이름 확인 안내 문구 (사용 가능·확인 중·중복 확인 요청)
	UPROPERTY(EditAnywhere, Category = "NU|Character|색")
	FLinearColor NameMessageColor = FLinearColor(0.55f, 0.85f, 0.60f, 1.0f);

private:
	UFUNCTION()
	void HandleNameChanged(const FText& Text);

	UFUNCTION()
	void HandleNameCheckClicked();

	// 이름 확인(140) 응답 --- 매니저 델리게이트로 온다
	UFUNCTION()
	void HandleNameCheckResult(int32 ResultCode);

	UFUNCTION()
	void HandleGenderFemaleClicked();

	// 잠긴 성별(몸이 없는 성별)은 거른다 --- 버튼·초안 복원·랜덤이 전부 이 둘을 거친다
	void SelectGender(ENUCharacterGender InGender);
	ENUCharacterGender PickOpenGender(ENUCharacterGender Preferred) const;

	UFUNCTION()
	void HandleGenderMaleClicked();

	UFUNCTION()
	void HandleHairPrevClicked();

	UFUNCTION()
	void HandleHairNextClicked();

	UFUNCTION()
	void HandleFacePrevClicked();

	UFUNCTION()
	void HandleFaceNextClicked();

	UFUNCTION()
	void HandleSkinPrevClicked();

	UFUNCTION()
	void HandleSkinNextClicked();

	// 종족 버튼을 눌렀을 때 (버튼 Tag = Races 인덱스)
	UFUNCTION()
	void HandleRaceButtonClicked(int32 RaceButtonIndex);

	// 머리 색 견본을 눌렀을 때 (버튼 Tag = 색 번호)
	UFUNCTION()
	void HandleHairColorClicked(int32 ColorIndex);

	UFUNCTION()
	void HandleRacePrevClicked();

	UFUNCTION()
	void HandleRaceNextClicked();

	UFUNCTION()
	void HandleRandomClicked();

	UFUNCTION()
	void HandleResetClicked();

	UFUNCTION()
	void HandleCreateClicked();

	UFUNCTION()
	void HandleCancelClicked();

	// --- 내부 ---

	// 종족 목록을 표(또는 임시 목록)에서 채운다. 정렬은 SortOrder => RowName
	void LoadRaces();

	// 파츠 목록을 표에서 채운다. 종족 전용 파츠가 있어 고른 종족이 바뀌면 다시 짜야 하는데,
	// 종족이 그대로면 스스로 빠져나온다 (화살표를 누를 때마다 표를 다시 읽지 않게)
	void LoadParts();

	// 표 하나를 목록으로. RaceId가 0이거나 고른 종족과 같은 줄만 남긴다
	void LoadPartTable(const TSoftObjectPtr<UDataTable>& Table, TArray<FNUAppearancePartRow>& Out) const;

	// 고르기 전이면 전경, 고른 뒤면 그 종족 자리로 카메라를 넘긴다
	void RefreshPreview();

	// 레벨의 캐릭터를 눌렀을 때 --- 그 종족을 고르고 폼을 연다
	void HandleStandClicked(int32 RaceId);

	// 오른쪽 폼을 보이거나 감춘다. 종족을 고르기 전에는 선술집만 보여 준다
	void SetFormVisible(bool bVisible);

	// WBP에 놓인 종족 버튼(RaceButton0..N)에 이름·문양·잠금을 넣는다. 종족보다 많이 놓인 버튼은 접는다
	void BuildRaceList();

	// WBP에 놓인 머리 색 견본(HairColorButton0..N)에 색을 칠하고 누름을 잇는다. 견본보다 많이 놓인 칸은 접는다
	void BuildHairColorList();

	// 고른 값들을 화면에 다시 쓴다 (이름 계약 위젯 + BP 통지)
	void RefreshDisplay();

	// 이름 상태만 갱신 (입력 중에 매 글자 불린다)
	void RefreshNameState();

	// 지금 입력된 이름을 trim해서 돌려준다
	FString GetTrimmedName() const;

	// 고른 것 전부를 한 덩어리로
	FNUCharacterCreateParams BuildParams() const;

	// 매니저에 들고 있는 초안을 화면 상태로 되살린다 (레벨을 오갔거나 다시 들어왔을 때)
	void RestoreDraft();

	// 지금 고른 것을 매니저 초안에 적어 둔다
	void SaveDraft() const;

	class UNUCharacterManager* GetManager() const;

	void SetNameMessage(const FText& Message, bool bIsError);

	// 썸네일 줄을 지금 고른 것에 맞춰 다시 칠한다. 아이콘이 있으면 끼우고 없으면 빈 네모
	// 파츠 아이콘 한 칸 --- 몸 캔버스 원본이면 머리 영역만 확대해 중앙에 둔다
	// Tint를 주면 그림을 그 색으로 물들여 그린다 (머리 색)
	void SetThumbIcon(UBorder* Box, UTexture2D* Icon, bool bSelected, const FLinearColor* Tint = nullptr) const;

	void RefreshThumbs(const TArray<TObjectPtr<UBorder>>& Thumbs,
		const TArray<FNUAppearancePartRow>& Parts, int32 Index, const FLinearColor* Tint = nullptr) const;

	// 목록에서 그 PartId가 몇 번째인지. 못 찾으면 첫 칸으로 떨어진다
	static int32 FindPartIndex(const TArray<FNUAppearancePartRow>& Parts, int32 PartId);

	// 한 번에 보여 줄 썸네일 개수 (가짓수가 이보다 많으면 창처럼 밀어서 본다)
	static constexpr int32 MaxVisibleThumbs = 5;

	// 이름 계약 텍스트 위젯 찾기 (없으면 nullptr --- 호출부는 분기하지 않게 전부 널 검사)
	UTextBlock* FindText(const TCHAR* Name) const;

	// 이름으로 찾는 선택 위젯을 켜고 끈다 (없으면 넘어간다). 켤 때는 클릭을 가로채지 않게 HitTestInvisible
	void SetMarkVisible(const FString& WidgetName, bool bVisible) const;

	// 파츠 줄 하나 --- 고를 것이 없으면 <Prefix>Options를 접고 <Prefix>Empty를 보인다 (둘 다 선택 위젯)
	void SetOptionRowEmpty(const TCHAR* Prefix, bool bEmpty) const;

	// 버튼 위젯 + UFUNCTION 이름으로 묶는다 (AddDynamic은 함수 이름을 매크로로 박아야 해서 표로 못 돈다).
	// 순정 Button(레거시) / UNU_CommonButtonWidget(WBP_CommonButton) 둘 다 지원해야 해서 UWidget으로 받는다
	void BindButton(UWidget* Widget, const TCHAR* FunctionName);

	// 종족 목록 (RowName + 표시 내용). 표가 없으면 임시 목록이 들어온다
	struct FRaceEntry
	{
		FName RowName;
		int32 RaceId = 0; // 서버 race_id로 나가는 숫자
		FText DisplayName;
		FText Description;
		TSoftObjectPtr<UTexture2D> Emblem;
		int32 SortOrder = 0;
		bool bPlayable = true; // false면 목록에 "(준비 중)"으로만 보이고 못 고른다
	};
	TArray<FRaceEntry> Races;

	// 미개방 종족의 스탠드를 잠근다 --- 종족 목록과 스탠드가 어긋나지 않게 한 곳에서.
	// 스탠드 등록과 종족 로드의 순서가 어느 쪽이 먼저여도 되도록 양쪽 끝에서 부른다
	void ApplyStandLocks();

	// WBP_CharacterCreate의 위젯들 (BindWidget). 이름·타입은 파일 맨 위 목록과 같다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UEditableTextBox> NameTextBox;

	// 이름 계약 위젯(FindButton/BindButton으로 찾는다): 순정 Button(레거시) / UNU_CommonButtonWidget
	// (WBP_CommonButton) 둘 다 지원해야 해서 UWidget으로 열어둔다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> NameCheckButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameErrorText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> GenderFemaleButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> GenderMaleButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GenderText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> HairPrevButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> HairNextButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HairIndexText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> FacePrevButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> FaceNextButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FaceIndexText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SkinPrevButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> SkinNextButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RaceNameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> RaceDescText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RacePrevButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RaceNextButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> RandomButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> ResetButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CreateButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> CancelButton;

	// 머리·외모 썸네일 칸 (그림이 오기 전까지는 빈 네모)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> HairThumbs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> FaceThumbs;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> SkinThumbs;

	// 종족 문양 자리 (표에 텍스처가 없으면 빈 네모)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UBorder> RaceEmblem;

	// WBP에 놓인 종족 버튼 (RaceButton0..N, Races와 같은 순서). 고른 표시 갱신에 쓴다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNU_CommonButtonWidget>> RaceButtons;

	// WBP에 놓인 머리 색 견본 (HairColorButton0..N, 색 번호 순서). 없는 WBP는 머리 색을 안 고른다 (늘 0번 = 원래 색)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNU_CommonButtonWidget>> HairColorButtons;

	// 레벨의 종족별 미리보기 자리들. 레벨에 없으면 그냥 안 보일 뿐 화면은 성립한다
	TArray<TWeakObjectPtr<ANUCharacterCreatePreview>> PreviewStands;

	// 고르기 전에 쓰는 전경 카메라
	TWeakObjectPtr<AActor> OverviewCamera;

	// 첫 화면은 튀지 않게 즉시 --- 이후 종족을 바꿀 때만 부드럽게 넘어간다
	bool bPreviewShown = false;

	// 오른쪽 폼 --- 종족을 고르기 전에는 선술집만 보이게 감춘다
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidget> FormPanel;

	FOnNUCharacterCreateConfirmed OnConfirmed;

	TSubclassOf<UNUConfirmDialog> ConfirmDialogClass;

	// INDEX_NONE = 아직 안 고름 (전경 카메라 상태). 고르기 전에는 생성도 막는다
	int32 RaceIndex = INDEX_NONE;

	// 고를 수 있는 파츠 (표 또는 더미). 가짓수는 이 배열의 길이다
	TArray<FNUAppearancePartRow> HairParts;
	TArray<FNUAppearancePartRow> FaceParts;
	TArray<FNUAppearancePartRow> SkinParts;

	// 지금 목록이 어느 종족 기준으로 짜였는지. 같으면 표를 다시 안 읽는다
	int32 PartsRaceId = INDEX_NONE;
	int32 PartsGender = INDEX_NONE;
	int32 HairIndex = 0;
	int32 FaceIndex = 0;
	int32 SkinIndex = 0;
	// 머리 색 번호 (프로젝트 세팅 HairColors의 자리). 0 = 그림 원래 색. 서버로는 hair_id에 같이 실려 간다
	int32 HairColorIndex = 0;
	ENUCharacterGender Gender = ENUCharacterGender::Female;

	// 등장 연출과 외형을 바꿀 때의 반짝임 (NativeTick이 돌린다)
	FNUScreenMotion IntroMotion;
	FNUScreenMotion LookMotion;
	// 마지막으로 그린 외형 --- 같으면 반짝이지 않는다 (이름만 바꾼 경우 등). 처음 그릴 때는 비어 있다
	FString LastLook;

	// 클라가 판정한 마지막 이름 상태 (생성 버튼 활성 판단에 쓴다)
	ENUNameCheck NameState = ENUNameCheck::TooShort;

	// 최종 확인 창이 떠 있는 동안 중복 요청을 막는다
	bool bConfirmInFlight = false;

	// [중복확인] 응답을 기다리는 중 (연타 방지 + 응답이 왔을 때만 문구 반영)
	bool bNameCheckInFlight = false;

	// [중복확인]으로 물어본 이름. 답이 오는 사이 입력이 바뀌었는지 이것과 대조한다
	FString PendingCheckName;

	// 서버가 쓸 수 있다고 답한 이름. 이 이름 그대로일 때만 생성 버튼이 열린다
	// (이승진 님 지적, 8/26 --- 확인 없이 생성이 되던 버그)
	FString CheckedName;

	// 서버가 물린 이름과 사유. 그 이름을 그대로 두는 동안은 사유를 계속 띄운다
	FString RejectedName;
	ENUNameCheck RejectedReason = ENUNameCheck::Ok;

	// Close가 두 번 불리는 것을 막는다 (통지도 한 번만 나가야 한다)
	bool bClosed = false;
};
