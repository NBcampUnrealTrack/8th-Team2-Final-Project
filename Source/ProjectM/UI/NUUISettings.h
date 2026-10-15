// UI 공용 설정 (프로젝트 세팅 > 게임 > NU UI).
// UI 매니저는 서브시스템이라 블루프린트에서 프로퍼티를 지정할 수 없어, 공용 위젯 클래스는 여기서 받는다.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "NUUISettings.generated.h"

class UNUCharacterMenuBase;
class UNUCharacterProfileWidget;
class UUserWidget;
class UNU_CombatantStatusWidget;
class UNUConfirmDialog;
class UNUGameMenuWidget;
class UNUNameplateWidget;
class UNUNoticeBannerWidget;
class UNUOptionsWindowWidget;
class UNUSplitStackDialog;
class UNUItemTooltipWidget;
class UNUItemCellWidget;
class UNUInventoryWindowWidget;
class UNUTradeWindowWidget;
class UNUCharacterStatsWidget;
class UNUDialogueWidget;
class UNUAdvancementSelectionWidget;
class UNUJobSelectionWidget;
class UNUStarterNaiaSelectionWidget;
class UNUProgressionNodeWidget;
class UNUProgressionNodeTooltipWidget;
class UNUProgressionScreenWidget;
class UNUPlayerSkillWindowWidget;
class UNUWorldLocationWidget;
class UNUWorldMarkerWidget;
class UMaterialInterface;
class UNUKeyBindingsWindow;

// 키 설정 창에 열어 줄 동작 하나. 표시 이름·묶음·필수 여부를 여기서 쥔다 ---
// 입력 액션(IA) 애셋은 여러 사람이 나눠 갖고 있어, 그쪽에는 재설정 표시만 켜고 나머지는 이 목록으로 관리한다.
USTRUCT()
struct FNUKeyBindingEntry
{
	GENERATED_BODY()

	// IA의 PlayerMappableKeySettings에 준 이름과 같아야 목록에 잡힌다
	UPROPERTY(EditAnywhere, Category = "NU|UI")
	FName MappingName;

	// 줄에 보일 이름. 비우면 IA가 가진 표시 이름을 쓴다
	UPROPERTY(EditAnywhere, Category = "NU|UI")
	FText DisplayName;

	// 묶음 제목 ("이동", "창 열기"). 같은 글자끼리 이어서 그린다
	UPROPERTY(EditAnywhere, Category = "NU|UI")
	FText GroupName;

	// 참이면 키가 빈 채로는 적용할 수 없다 (이동·상호작용)
	UPROPERTY(EditAnywhere, Category = "NU|UI")
	bool bRequired = false;

	// 패드 칸을 이 글자로 고정한다 (이동 = "왼쪽 스틱" --- 스틱은 지정 대상이 아니다). 비우면 패드 칸도 바꿀 수 있다
	UPROPERTY(EditAnywhere, Category = "NU|UI")
	FText GamepadFixedText;
};

UCLASS(config = Game, defaultconfig, meta = (DisplayName = "NU UI"))
class PROJECTM_API UNUUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// ── 공용 색 ──────────────────────────────────────────────
	// C++가 직접 그리는 위젯이 쓰는 색이다. WBP가 있는 창은 색이 WBP 소유라 여기를 안 본다.
	//
	// 뜻이 같은 색은 한 값이어야 한다 --- 실제로 삭제 버튼이 확인창에서는 (0.55,0.16,0.16),
	// 선택창에서는 (0.42,0.13,0.13)로 갈려 있었다. 그런 어긋남을 막으려고 모아 둔다.

	// 만드는·받아들이는 동작 (생성·수락·나누기·삭제취소)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor PositiveColor = FLinearColor(0.18f, 0.48f, 0.26f, 1.0f);

	// 되돌릴 수 없는 파괴 (삭제·버리기)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor DestructiveColor = FLinearColor(0.55f, 0.16f, 0.16f, 1.0f);

	// 물러나는 동작 (취소). 파괴는 아니라 빨강까지 가지 않는다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor BackoutColor = FLinearColor(0.30f, 0.20f, 0.22f, 1.0f);

	// 아이템 칸·목록 줄처럼 내용이 든 칸의 바탕
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor SlotColor = FLinearColor(0.16f, 0.17f, 0.25f, 1.0f);

	// 선택된 탭 (상점 구매/판매, 옵션 탭) --- 칸 색보다 한 단계 밝다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor TabSelectedColor = FLinearColor(0.24f, 0.26f, 0.38f, 1.0f);

	// 창 바탕 (칸보다 어둡다) --- 인벤 창 WBP의 WindowBG와 같은 값 (창 톤이 갈리면 안 된다)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor PanelColor = FLinearColor(0.01f, 0.01f, 0.022f, 0.94f);

	// 본문 글자
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor TextColor = FLinearColor(0.92f, 0.92f, 0.95f, 1.0f);

	// 곁들이는 글자 (레벨·직업처럼 이름 아래 붙는 줄)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor SubTextColor = FLinearColor(0.62f, 0.62f, 0.70f, 1.0f);

	// 오류·거절 문구
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|색")
	FLinearColor ErrorColor = FLinearColor(0.95f, 0.45f, 0.45f, 1.0f);

	// ─────────────────────────────────────────────────────────

	// 되돌릴 수 없는 동작 앞에 세우는 공용 확인 창.
	// 창 WBP가 자기 ConfirmDialogClass를 지정하지 않았을 때 이것이 쓰인다 --- 창마다 지정할 필요가 없다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUConfirmDialog> ConfirmDialogClass;

	// 서버 연결이 끊겼을 때 띄우는 안내 창. [재접속] / [초기화면으로] 두 버튼을 쓴다.
	// 미지정이면 창 없이 로그만 남긴다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUConfirmDialog> SessionEndedDialogClass;

	// 재접속을 포기했을 때 돌아갈 화면 (로그인 레벨).
	// 미지정이면 창에 [초기화면으로]를 달지 않는다 --- 갈 곳이 없는 버튼은 안 만든다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftObjectPtr<UWorld> LoginLevel;

	// 화면 아래의 일반 알림 배너 표시 여부. 장소 진입 알림은 별도 설정을 사용한다.
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	bool bEnableNoticeBanner = true;

	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	bool bEnableLocationEntryNotice = true;

	// WBP로 지역/장소 표시의 외형을 교체할 수 있다.
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|WorldLocation")
	TSoftClassPtr<UNUWorldLocationWidget> WorldLocationWidgetClass;

	// NPC 머리 위 퀘스트 표시(UNUWorldMarkerComponent). 지정하면 NPC BP에 적힌 위젯·크기 대신 이것으로 그린다 ---
	// NPC BP마다 바꾸지 않고 여기 한 줄로 바꾼다. 미지정이면 BP 그대로
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|머리 위 표시")
	TSoftClassPtr<UNUWorldMarkerWidget> HeadMarkerClass;

	// 위 위젯을 그릴 재질. 비우면 컴포넌트 기본(마스크드 --- 가장자리가 잘린다). 반투명 재질이면 위젯을 반투명으로 그린다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|머리 위 표시")
	TSoftObjectPtr<UMaterialInterface> HeadMarkerMaterial;

	// 위젯을 그리는 해상도(픽셀). 월드 크기(cm)는 HeadMarkerWorldSize --- 해상도를 화면에 보이는 크기보다 크게 잡아 가장자리를 매끈하게 한다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|머리 위 표시")
	FIntPoint HeadMarkerDrawSize = FIntPoint(160, 160);

	UPROPERTY(config, EditAnywhere, Category = "NU|UI|머리 위 표시", meta = (ClampMin = "1"))
	float HeadMarkerWorldSize = 80.f;

	// 위젯의 어느 점을 컴포넌트 자리에 둘지 (0.5, 0.5 = 가운데). 값을 키우면 표시가 머리에서 위로 뜬다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|머리 위 표시")
	FVector2D HeadMarkerPivot = FVector2D(0.5, 0.5);

	// 0 이상이면 표시를 NPC 그림 꼭대기에서 이만큼(cm) 위에 둔다 --- NPC BP 마다 적힌 높이(90~120)와 NPC 크기 보정과 상관없이
	// 머리 바로 위에 놓인다(피벗 = 배지 아래 꼭지). 음수면 BP 에 적힌 자리 그대로
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|머리 위 표시")
	float HeadMarkerGap = -1.f;

	// 화면 아래에 잠깐 떴다 사라지는 알림 배너.
	// UNUUIManager::ShowNotice()가 쓰므로 어느 기능에서든 문구만 넘기면 된다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUNoticeBannerWidget> NoticeBannerClass;

	// 채팅 말풍선 (근접·전체, 말한 캐릭터 머리 위). 알림 배너와 같은 클래스라 문구·표시 시간(Lifetime) 계약이 같다 ---
	// UNUSpeechBubbleUIController가 캐릭터마다 붙인다. 미지정이면 말풍선을 띄우지 않는다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUNoticeBannerWidget> SpeechBubbleClass;

	// 파티 채팅 말풍선. 파티 말은 파티원에게만 오므로 모양만 일반 말풍선과 가른다. 미지정이면 일반 말풍선을 쓴다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUNoticeBannerWidget> PartySpeechBubbleClass;

	// 전체 채팅 말풍선. 근처 말과 가르려고 모양만 다르게 둔다. 미지정이면 일반 말풍선을 쓴다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUNoticeBannerWidget> WorldSpeechBubbleClass;

	// 필드 캐릭터(내 것·남의 것) 머리 위 이름표. UNUNameplateComponent가 BeginPlay에서 입히고 WBP의 원하는 크기로 그린다 ---
	// 크기는 WBP가 정한다(가장 긴 닉네임이 들어가게). 미지정이면 코드 폴백(UNUNameplateWidget)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUNameplateWidget> NameplateClass;

	// 전투 아바타 머리 위 상태 표시(이름·레벨·HP). 지정하면 아바타 BP에 적힌 클래스 대신 이것을 쓴다 (아군/적 따로).
	// 미지정이면 아바타 BP 그대로. 서버 전투에서도 같은 위젯에 값이 들어간다(SetVitalsDirect 등)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNU_CombatantStatusWidget> BattleAllyTagClass;

	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNU_CombatantStatusWidget> BattleEnemyTagClass;

	// 수량 입력 창 (인벤 나누기·상점 구매·판매 공용). UNUSplitStackDialog::Show/ShowCount가 만든다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUSplitStackDialog> CountDialogClass;

	// 아이템 툴팁 (인벤 칸·장비창·상점 줄·거래 창·남의 정보창 공용). UNUSheetWidgetBase::BuildItemTooltip이 만든다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUItemTooltipWidget> ItemTooltipClass;

	// 아이템 칸 (인벤·창고·장비창 공용). 창이 칸 수만큼 만들어 격자에 놓는다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUItemCellWidget> ItemCellClass;

	// 아이템 칸 메뉴 (패드 X / Ctrl+우클릭). UNUItemContextMenu::Show가 만든다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<class UNUItemContextMenu> ItemContextMenuClass;

	// 패드 모드에서 포커스가 어디 있는지 보여 주는 테두리 위젯. UI 매니저가 매 프레임 포커스 위젯의 화면 사각형에 맞춘다.
	// 공용 버튼에 포커스 상태 그림이 생기면 비워도 된다 (비우면 안 그린다)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|패드")
	TSoftClassPtr<UUserWidget> PadFocusFrameClass;

	// 창고 창 (계정 창고 | 캐릭터 창고 두 패널). 인벤 창과 같은 클래스(UNUInventoryWindowWidget)의 전용 WBP ---
	// PlayerState BP마다 지정하지 않고 여기 하나로 둔다 (상점 창과 같은 이유)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUInventoryWindowWidget> StorageWindowClass;

	// 1:1 거래 창. UI 컨트롤러가 소유하므로 블루프린트가 아니라 여기서 지정한다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUTradeWindowWidget> TradeWindowClass;

	// 거래 신청을 받았을 때의 수락/거절 창. 공용 확인 창 WBP를 그대로 지정해도 된다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUConfirmDialog> TradeInviteDialogClass;

	// 남의 캐릭터 정보 창 (우클릭 [캐릭터 정보]). UI 컨트롤러가 소유하므로 여기서 지정한다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUCharacterProfileWidget> CharacterProfileWindowClass;

	// 통합 캐릭터 메뉴의 셸 (캐릭터 정보·장비·인벤토리·스탯·특성·나이아 페이지를 담는 창).
	// 소지품 키·캐릭터 정보 키와 전체 메뉴의 같은 항목이 이 창을 연다. 비우면 옛 소지품 창·정보창을 연다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUCharacterMenuBase> CharacterMenuClass;

	// 아이템 아이콘 폴더 --- {폴더}/T_Item_{RowName} 이름 규칙으로 찾는다. 없는 아이템은 글자 표기
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	FString ItemIconFolder = TEXT("/Game/Developers/dbals/UI/ItemIcons");

	// 상태이상 아이콘 폴더 --- {폴더}/T_Status_{태그 마지막 조각} 이름 규칙으로 찾는다 (NU.State.Ailment.Burn -> T_Burn)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	FString StatusIconFolder = TEXT("/Game/Developers/dongh/Texture/Icon/Battle");

	// 캐릭터 스탯 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUCharacterStatsWidget> CharacterStatsWidgetClass;

	// 기존 화면 하단 대화 패널과 그 위쪽 선택지 영역을 함께 가진 단일 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUDialogueWidget> DialogueWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "NU|UI|대화 입력")
	TArray<FKey> DialogueAdvanceKeys = { EKeys::F, EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom };

	UPROPERTY(config, EditAnywhere, Category = "NU|UI|대화 입력")
	TArray<FKey> DialogueConfirmOptionKeys = { EKeys::F, EKeys::Enter, EKeys::SpaceBar, EKeys::Gamepad_FaceButton_Bottom };

	// 키보드와 패드를 같이 받으려고 배열 (9/15 패드)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|대화 입력")
	TArray<FKey> DialogueNavigateUpKeys = { EKeys::Up, EKeys::Gamepad_DPad_Up };

	UPROPERTY(config, EditAnywhere, Category = "NU|UI|대화 입력")
	TArray<FKey> DialogueNavigateDownKeys = { EKeys::Down, EKeys::Gamepad_DPad_Down };

	UPROPERTY(config, EditAnywhere, Category = "NU|UI|대화 입력")
	TArray<FKey> DialogueCloseKeys = { EKeys::Escape, EKeys::Gamepad_FaceButton_Right };

	// 직업 선택 UIController가 생성하는 전체 화면 직업 선택 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUJobSelectionWidget> JobSelectionWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUStarterNaiaSelectionWidget> StarterNaiaSelectionWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUAdvancementSelectionWidget> AdvancementSelectionWidgetClass;

	// 성장 트리 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUProgressionScreenWidget> ProgressionScreenWidgetClass;

	// 성장 트리의 노드 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUProgressionNodeWidget> ProgressionNodeWidgetClass;

	// 성장 노드에 마우스를 올렸을 때 정보 설명 툴팁 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUProgressionNodeTooltipWidget> ProgressionNodeTooltipWidgetClass;

	// 성장 노드 획득하기 전에 표시할 전용 확인창 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUConfirmDialog> ProgressionNodeConfirmWidgetClass;

	// 플레이어가 보유한 액티브/패시브 직업 스킬을 표시하는 WBP
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUPlayerSkillWindowWidget> PlayerSkillWindowClass;

	// ESC 전체 메뉴 (닫을 창이 없을 때). 항목은 각 기능이 UNUUIManager::RegisterGameMenuEntry로 등록한다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUGameMenuWidget> GameMenuClass;

	// 옵션 창 (전체 메뉴 > 옵션). 미지정이면 C++ 최소 UI로 뜬다 --- 이름 계약은 UNUOptionsWindowWidget 참고
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<UNUOptionsWindowWidget> OptionsWindowClass;

	// 전체 지도 창 (전체 메뉴 > 지도). 지도 그림·마커는 안에 배치한 지도 뷰 조각(WBP_MapImageView)이 담당
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftClassPtr<class UNU_WorldMapWindowWidget> WorldMapWindowClass;

	// 전체 메뉴의 "캐릭터 선택으로"가 이동할 레벨
	UPROPERTY(config, EditAnywhere, Category = "NU|UI")
	TSoftObjectPtr<UWorld> CharacterSelectLevel;

	// 키 설정 창 (옵션창 조작 탭 > 키 설정)
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|Input")
	TSoftClassPtr<UNUKeyBindingsWindow> KeyBindingsWindowClass;

	// 키 설정 창에 열어 줄 동작 목록. 여기 없는 동작은 바꿀 수 없다 ---
	// UI 조작(확인·취소·ESC)까지 열면 잘못 바꿨을 때 창을 닫지 못해 갇힌다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|Input")
	TArray<FNUKeyBindingEntry> KeyBindingEntries;

	// 키 설정 창이 열릴 때 엔진 설정에 등록할 매핑 상황(IMC) ---
	// 등록하지 않으면 그 상황의 키가 "지정 안 됨"으로 보인다. 우리 게임은 상황마다 IMC를 붙였다 떼므로
	// 지금 붙어 있는 것만으로는 목록을 다 채울 수 없다
	UPROPERTY(config, EditAnywhere, Category = "NU|UI|Input")
	TArray<TSoftObjectPtr<class UInputMappingContext>> KeyBindingContexts;
};
