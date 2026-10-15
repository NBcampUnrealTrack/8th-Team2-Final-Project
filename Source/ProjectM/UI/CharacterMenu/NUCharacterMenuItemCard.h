// 통합 캐릭터 메뉴의 아이템 페이지(인벤토리·장비)가 같이 쓰는 부품.
// 두 페이지는 민기 님 WBP에 그려진 위젯을 이름으로 찾아 값만 넣는다 (C++가 위젯을 만들지 않는다) --- 여기는 그 공통 부분이다:
//   FNUCharacterMenuSlot      칸 하나 (인벤토리 칸 / 장착 칸)
//   FNUCharacterMenuItemCard  오른쪽 상세 카드 (두 WBP가 같은 이름을 쓴다)
//   UNUIndexedButtonRelay     같은 모양 버튼 여러 개를 번호로 구분해 받기
#pragma once

#include "CoreMinimal.h"
#include "Inventory/NUEquipmentTypes.h"
#include "Styling/SlateTypes.h"
#include "UObject/Object.h"
#include "NUCharacterMenuItemCard.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
class UUserWidget;
class UWidget;
struct FNUItemDefRow;

DECLARE_DELEGATE_OneParam(FOnNUIndexedButtonClicked, int32 /*Index*/);

// UButton의 OnClicked는 인자가 없다 --- 어느 버튼인지 번호를 실어 넘기는 중계 객체 (버튼마다 하나, 페이지가 들고 있는다)
UCLASS()
class PROJECTM_API UNUIndexedButtonRelay : public UObject
{
	GENERATED_BODY()

public:
	// Button이 없으면(WBP가 그 버튼을 뺌) 만들지 않고 nullptr
	static UNUIndexedButtonRelay* Bind(UObject* Outer, UButton* Button, int32 InIndex, FOnNUIndexedButtonClicked InOnClicked);

private:
	UFUNCTION()
	void HandleClicked();

	int32 Index = INDEX_NONE;
	FOnNUIndexedButtonClicked OnClicked;
};

// 고른 칸의 모양과 등급 색 --- 보통 칸은 WBP에 그려진 그대로 두고, 고르면 외곽선과 귀퉁이 색만 이 값으로 바꾼다.
// 기본값은 9/28 시안의 고른 칸이고, 페이지 WBP의 클래스 기본값에서 바꾼다 (셸의 내비게이션 색과 같은 방식)
USTRUCT()
struct FNUCharacterMenuSlotStyle
{
	GENERATED_BODY()

	FNUCharacterMenuSlotStyle();

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	FLinearColor SelectedOutlineColor = FLinearColor(0.73f, 0.59f, 0.36f, 0.92f);

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	float SelectedOutlineWidth = 1.4f;

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	FLinearColor SelectedCornerTint = FLinearColor(0.73f, 0.59f, 0.36f, 0.92f);

	// 등급 색 --- 칸의 등급 선과 상세 카드의 등급 글자에 쓴다. 표에 없는 등급은 표시하지 않는다.
	// 희귀·영웅·전설은 9/28 시안의 견본 색 셋, 일반·고급은 시안에 없어 임시로 넣은 값이다
	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	TMap<ENUItemGrade, FLinearColor> GradeColors;
};

// WBP에 그려진 칸 하나 (인벤토리는 칸 WBP 안의 "Cell...", 장비는 페이지 안의 "EquipHead..." 등). 이름은 앞머리 + 꼬리:
//   {Prefix}Button(Button, 필수) / {Prefix}Icon(Image) / {Prefix}Level(TextBlock) / {Prefix}Empty / {Prefix}EmptyMark
//   {Prefix}Rarity(Image) 등급 선 --- 등급이 있는 아이템만 등급 색으로 보인다
//   {Prefix}Held(아무 위젯) 패드로 집은 칸일 때만 보인다 (놓을 곳을 고르는 동안 출발 칸 표시)
//   {Prefix}{TL,TR,BL,BR}{H,V}(Image) 귀퉁이 장식 8개
struct FNUCharacterMenuSlot
{
	// {Prefix}Button이 없으면 false (그 번호의 칸이 WBP에 없다)
	bool Bind(const UUserWidget& Page, const FString& Prefix);

	// 아이콘 (nullptr이면 숨긴다)
	void SetIcon(UTexture2D* Texture) const;

	// 빈 칸 표시(Empty·EmptyMark)
	void SetEmptyMarkVisible(bool bVisible) const;

	void SetSelected(bool bSelected, const FNUCharacterMenuSlotStyle& Style) const;

	// 등급 선 (등급이 없거나 색이 정해지지 않았으면 접는다)
	void SetGrade(ENUItemGrade Grade, const FNUCharacterMenuSlotStyle& Style) const;

	// 패드로 집은 칸 표시
	void SetHeld(bool bHeld) const;

	TWeakObjectPtr<UButton> Button;
	TWeakObjectPtr<UImage> Icon;
	TWeakObjectPtr<UImage> Rarity;
	TWeakObjectPtr<UTextBlock> Level;
	TWeakObjectPtr<UWidget> Empty;
	TWeakObjectPtr<UWidget> EmptyMark;
	TWeakObjectPtr<UWidget> Held;
	TArray<TWeakObjectPtr<UImage>, TInlineAllocator<8>> Corners;

	// 고르지 않은 칸의 모양 (WBP에 그려진 것)
	FButtonStyle IdleStyle;
	FSlateColor IdleCornerTint;
};

// 오른쪽 상세 카드. 이름 계약 (없는 위젯은 건너뛴다):
//   SelectedItemIcon(Image) / SelectedItemEmptyMark(고른 것이 없을 때의 표시)
//   SelectedItemName / SelectedItemLevel / SelectedItemRarity / SelectedItemDescription (TextBlock)
//       등급 칸(Rarity)에는 등급이 있으면 "희귀 · 무기" / "희귀 · 소모품"을 등급 색으로, 없으면 분류("소모품", "장비 · 무기")를 WBP 색으로 적는다
//       설명 칸에는 장비면 전용 직업, 소모품이면 설명 아래 줄에 쓸 수 있는 곳을 적는다
//   ItemStatsHeading(TextBlock) + 스탯 줄 4개 Item{Attack,Strength,Critical,Skill}{Label,Value}(TextBlock)·{..}Rule(구분선)
//   EquipActionButton(Button) + EquipActionLabel(TextBlock)
// 줄 이름은 시안의 견본 스탯일 뿐이라 위에서부터 차례로 채운다 --- 값이 있는 스탯이 줄보다 많으면 앞의 것만 보인다.
// 고른 것이 없을 때의 글자("선택한 아이템 없음", "—")와 꺼진 버튼의 투명도는 WBP에 그려진 것을 기억했다 되돌린다
struct FNUCharacterMenuItemCard
{
	void Bind(const UUserWidget& Page);

	// 고른 것이 없는 상태
	void Clear() const;

	// Count = 가진 개수 (장착 칸처럼 개수가 뜻이 없으면 0). ActionLabel이 비면 WBP에 적힌 글자를 둔다
	void Show(const FNUItemDefRow& Def, FName RowName, int32 Count, const FText& ActionLabel, bool bActionEnabled, const FNUCharacterMenuSlotStyle& Style) const;

	UButton* GetActionButton() const { return ActionButton.Get(); }

private:
	struct FStatRow
	{
		TWeakObjectPtr<UTextBlock> Label;
		TWeakObjectPtr<UTextBlock> Value;
		TWeakObjectPtr<UWidget> Rule;
		FText DefaultLabel;
		FText DefaultValue;
	};

	void SetStatRow(const FStatRow& Row, const FText& Label, const FText& Value, bool bVisible) const;
	void SetActionEnabled(bool bEnabled) const;

	TWeakObjectPtr<UImage> Icon;
	TWeakObjectPtr<UWidget> EmptyMark;
	TWeakObjectPtr<UTextBlock> Name;
	TWeakObjectPtr<UTextBlock> Level;
	TWeakObjectPtr<UTextBlock> Kind; // 시안의 등급 자리 --- 등급이나 분류를 적는다
	TWeakObjectPtr<UTextBlock> Description;
	TWeakObjectPtr<UWidget> StatsHeading;
	TWeakObjectPtr<UButton> ActionButton;
	TWeakObjectPtr<UTextBlock> ActionLabel;
	TArray<FStatRow, TInlineAllocator<4>> StatRows;

	FText DefaultName;
	FText DefaultLevel;
	FText DefaultKind;
	FSlateColor DefaultKindColor;
	FText DefaultDescription;
	FText DefaultActionLabel;

	// 시안의 버튼은 꺼진 모양(흐림)으로 그려져 있다 --- 켤 때는 다 보이게, 끌 때는 이 값으로
	float DisabledActionOpacity = 1.f;
};
