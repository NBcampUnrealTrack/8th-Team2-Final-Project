// 개발용 콘솔 명령 --- 새 HUD의 상황별 모습을 서버 없이 띄워 본다 (Shipping 제외). 게임 로직은 바꾸지 않는다.
//   nu.HUD.Notice [문구]            공용 알림 배너
//   nu.HUD.PartyInvite [이름]        파티 초대 창
//   nu.HUD.ChatSample               채팅에 예시 줄 몇 개
//   nu.HUD.ChatOpen                 채팅 입력 열기
//   nu.HUD.Location [지역] [장소]    지역 이름 줄 + 진입 배너
//   nu.HUD.QuestFx <accept|ready|done> [제목]   퀘스트 수락·목표 달성·완료 띠 (+ 추적 줄 연출)
//   nu.HUD.JobSelect                첫 직업 선택 창 (실제 여는 경로 --- UI 매니저 창, 서버 없이도 고르기·화면은 된다)
//   nu.HUD.NaiaSelect               시작 나이아 선택 창 (실제 여는 경로)
//   nu.HUD.BattleResult [levelup 0|1] [capture 0|1]   전투 결과창 (예시 보상 · 파티 · 획득, 실제로 띄우는 PlayerController 경로)
#if !UE_BUILD_SHIPPING

#include "Chat/NUChatTypes.h"
#include "Chat/UI/NUChatWidget.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Party/UI/NUPartyPromptWidget.h"
#include "PlayerController/NUPlayerController.h"
#include "UI/NUUIManager.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "Quest/UI/NUQuestWidget.h"
#include "CharacterProgression/UI/NUProgressionUIController.h"
#include "Naia/UI/NUStarterNaiaSelectionUIController.h"
#include "WorldLocation/UI/NUWorldLocationWidget.h"
#include "Battle/UI/NUBattleResultTypes.h"

namespace NUHudDebug
{
	ANUPlayerController* GetPC(UWorld* World)
	{
		return World != nullptr ? Cast<ANUPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	FString Join(const TArray<FString>& Args, const TCHAR* Fallback)
	{
		return Args.Num() > 0 ? FString::Join(Args, TEXT(" ")) : FString(Fallback);
	}

	// 서버 없이 실행하면 컨트롤러가 채팅·파티 창을 만들지 않는다(필드 입장 응답 뒤에 만든다).
	// 미리보기용으로 컨트롤러에 지정된 클래스(ClassProperty)를 읽어 한 장 띄운다 --- 컨트롤러에는 등록하지 않는다
	template <typename TWidget>
	TWidget* EnsurePreview(ANUPlayerController* PC, const TCHAR* ClassProperty, int32 Layer)
	{
		if (PC == nullptr)
		{
			return nullptr;
		}
		for (TObjectIterator<TWidget> It; It; ++It)
		{
			if (It->GetWorld() == PC->GetWorld() && It->IsInViewport())
			{
				return *It;
			}
		}
		const FClassProperty* Property = FindFProperty<FClassProperty>(PC->GetClass(), ClassProperty);
		UClass* WidgetClass = Property != nullptr ? Cast<UClass>(Property->GetObjectPropertyValue_InContainer(PC)) : nullptr;
		if (WidgetClass == nullptr || !WidgetClass->IsChildOf(TWidget::StaticClass()))
		{
			return nullptr;
		}
		TWidget* Widget = CreateWidget<TWidget>(PC, WidgetClass);
		if (Widget != nullptr)
		{
			Widget->AddToViewport(Layer);
		}
		return Widget;
	}

	static FAutoConsoleCommandWithWorldAndArgs NoticeCommand(
		TEXT("nu.HUD.Notice"),
		TEXT("새 HUD 미리보기: 알림 배너. nu.HUD.Notice [문구]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UNUUIManager* UI = UNUUIManager::Get(GetPC(World)))
			{
				UI->ShowNotice(FText::FromString(Join(Args, TEXT("뿌리 사이로 · 수레길 진입로에 도착했습니다"))));
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs PartyInviteCommand(
		TEXT("nu.HUD.PartyInvite"),
		TEXT("새 HUD 미리보기: 파티 초대 창. nu.HUD.PartyInvite [이름]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UNUPartyPromptWidget* Prompt = EnsurePreview<UNUPartyPromptWidget>(GetPC(World), TEXT("PartyPromptWidgetClass"), NUUILayer::Popup))
			{
				Prompt->Open(true, Join(Args, TEXT("하늘")), 30);
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs ChatSampleCommand(
		TEXT("nu.HUD.ChatSample"),
		TEXT("새 HUD 미리보기: 채팅에 예시 줄을 넣는다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UNUChatWidget* Chat = EnsurePreview<UNUChatWidget>(GetPC(World), TEXT("ChatWidgetClass"), NUUILayer::HUD);
			if (Chat == nullptr)
			{
				return;
			}
			Chat->AddSystemMessage(FText::FromString(TEXT("리베르그로브 공방 구역에 들어왔습니다.")));
			Chat->AddMessage(ENUChatChannel::Local, FText::FromString(TEXT("모래")), FText::FromString(TEXT("망치 퀘스트 같이 하실 분 계신가요?")), false);
			Chat->AddMessage(ENUChatChannel::Party, FText::FromString(TEXT("하늘")), FText::FromString(TEXT("대장간 앞에서 만나요!")), false);
		}));

	static FAutoConsoleCommandWithWorldAndArgs ChatOpenCommand(
		TEXT("nu.HUD.ChatOpen"),
		TEXT("새 HUD 미리보기: 채팅 입력을 연다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UNUChatWidget* Chat = EnsurePreview<UNUChatWidget>(GetPC(World), TEXT("ChatWidgetClass"), NUUILayer::HUD))
			{
				Chat->BeginFocus();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs LocationCommand(
		TEXT("nu.HUD.Location"),
		TEXT("새 HUD 미리보기: 지역 이름 + 진입 배너. nu.HUD.Location [지역] [장소]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FText Area = FText::FromString(Args.Num() > 0 ? Args[0] : TEXT("펀브룩"));
			const FText Place = FText::FromString(Args.Num() > 1 ? FString::Join(TArray<FString>(Args.GetData() + 1, Args.Num() - 1), TEXT(" ")) : TEXT("수레길 진입로"));
			for (TObjectIterator<UNUWorldLocationWidget> It; It; ++It)
			{
				if (It->GetWorld() == World)
				{
					It->SetLocationNames(Area, Place);
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs QuestFxCommand(
		TEXT("nu.HUD.QuestFx"),
		TEXT("새 HUD 미리보기: 퀘스트 띠. nu.HUD.QuestFx <accept|ready|done> [제목]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const FString Kind = Args.Num() > 0 ? Args[0].ToLower() : TEXT("accept");
			const int32 KindIndex = Kind == TEXT("done") ? 2 : (Kind == TEXT("ready") ? 1 : 0);
			const FText Title = FText::FromString(Args.Num() > 1 ? FString::Join(TArray<FString>(Args.GetData() + 1, Args.Num() - 1), TEXT(" ")) : TEXT("뿌리 사이로"));
			for (TObjectIterator<UNUQuestWidget> It; It; ++It)
			{
				if (It->GetWorld() == World && It->IsInViewport())
				{
					It->DebugPlayQuestFx(KindIndex, Title);
				}
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs JobSelectCommand(
		TEXT("nu.HUD.JobSelect"),
		TEXT("첫 직업 선택 창을 연다 (직업 NPC 대화를 거치지 않고 같은 창). 이미 직업이 있으면 안내만 뜬다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			ANUPlayerController* PC = GetPC(World);
			if (UNUUIManager* UI = PC != nullptr ? UNUUIManager::Get(PC) : nullptr)
			{
				UI->GetController<UNUProgressionUIController>()->OpenJobSelection();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs NaiaSelectCommand(
		TEXT("nu.HUD.NaiaSelect"),
		TEXT("시작 나이아 선택 창을 연다 (직업 선택 뒤에 뜨는 것과 같은 창). 이미 골랐으면 안내만 뜬다"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			ANUPlayerController* PC = GetPC(World);
			if (UNUUIManager* UI = PC != nullptr ? UNUUIManager::Get(PC) : nullptr)
			{
				UI->GetController<UNUStarterNaiaSelectionUIController>()->OpenSelection();
			}
		}));

	// 이기는 치트가 없어 결과창만 따로 띄운다 --- 명단은 실제 규칙(BuildMembers)으로 만들고 경험치·골드·획득은 예시 값.
	// 파티 나이아가 없는 시험 캐릭터면 예시 나이아 셋(푸루나 · 볼또 · 아쿠퐁)을 붙인다. 닫으면 창만 지운다
	static FAutoConsoleCommandWithWorldAndArgs BattleResultCommand(
		TEXT("nu.HUD.BattleResult"),
		TEXT("전투 결과창 미리보기: nu.HUD.BattleResult [levelup 0|1] [capture 0|1] (기본 1 1)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ANUPlayerController* PC = GetPC(World);
			if (PC == nullptr)
			{
				return;
			}
			const bool bLevelUp = Args.Num() < 1 || Args[0] != TEXT("0");
			const bool bCapture = Args.Num() < 2 || Args[1] != TEXT("0");

			FNUBattleResultView View;
			View.Result = ENUBattleResult::Victory;
			View.Experience = 184;
			View.Gold = 62;
			NUBattleResult::BuildMembers(PC, nullptr, View.Members);
			if (View.Members.Num() <= 1)
			{
				const TArray<int32> SampleIds = { 1004, 1001, 1010 };
				const TArray<int32> SampleLevels = { 5, 4, 4 };
				TArray<FNUBattleResultEntry> Names;
				NUBattleResult::BuildNaiaEntries(PC, SampleIds, Names);      // 표의 이름을 빌려 쓴다
				for (int32 i = 0; i < SampleIds.Num(); ++i)
				{
					FNUBattleResultMember& Member = View.Members.AddDefaulted_GetRef();
					Member.NaiaId = SampleIds[i];
					Member.Level = Member.PreviousLevel = SampleLevels[i];
					Member.DisplayName = Names.IsValidIndex(i) ? Names[i].DisplayName : FText::AsNumber(SampleIds[i]);
				}
			}
			if (bLevelUp && View.Members.Num() > 0)
			{
				FNUBattleResultMember& Me = View.Members[0];
				Me.bLeveledUp = true;
				Me.PreviousLevel = FMath::Max(1, Me.Level);
				Me.Level = Me.PreviousLevel + 1;
				Me.bHasExperience = true;
				Me.RequiredExperience = FMath::Max<int64>(Me.RequiredExperience, 500);
				Me.ExperienceIntoLevel = Me.RequiredExperience * 18 / 100;
			}
			NUBattleResult::BuildItemEntries(PC, { 6505, 6001, 6508 }, { 3, 1, 2 }, View.Items);
			if (bCapture)
			{
				NUBattleResult::BuildNaiaEntries(PC, { 1004 }, View.CapturedNaia);
			}
			TWeakObjectPtr<ANUPlayerController> WeakPC = PC;
			PC->ShowBattleResult(View, FSimpleDelegate::CreateLambda([WeakPC]()
			{
				if (WeakPC.IsValid())
				{
					WeakPC->CloseBattleResult();
				}
			}));
		}));
}

#endif
