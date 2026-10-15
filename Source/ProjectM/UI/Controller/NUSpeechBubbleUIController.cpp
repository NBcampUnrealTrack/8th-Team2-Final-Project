#include "UI/Controller/NUSpeechBubbleUIController.h"

#include "Chat/NUChatSubsystem.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "Field/Net/FieldRemoteCharacter.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UI/NUNameplateWidget.h"
#include "UI/NUNoticeBannerWidget.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUSpeechBubble, Log, All);

void UNUSpeechBubbleUIController::OnInitialize()
{
	// 채팅은 GameInstance 수명이라 컨트롤러(LocalPlayer 수명)보다 오래 산다 --- 구독은 짧은 쪽이 한다
	if (UNUChatSubsystem* Chat = GetChatSubsystem())
	{
		Chat->OnChatMessage.AddUObject(this, &UNUSpeechBubbleUIController::HandleChatMessage);
	}
}

void UNUSpeechBubbleUIController::OnDeinitialize()
{
	if (UNUChatSubsystem* Chat = GetChatSubsystem())
	{
		Chat->OnChatMessage.RemoveAll(this);
	}
}

UNUChatSubsystem* UNUSpeechBubbleUIController::GetChatSubsystem() const
{
	const UNUUIManager* UI = GetManager();
	const ULocalPlayer* LP = UI ? UI->GetLocalPlayer<ULocalPlayer>() : nullptr;
	const UGameInstance* GI = LP ? LP->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNUChatSubsystem>() : nullptr;
}

void UNUSpeechBubbleUIController::HandleChatMessage(ENUChatChannel Channel, const FText& SenderName, const FText& Message, bool bIsMine)
{
	// 근접·전체·파티만 --- 귓속말은 둘만의 말이고 시스템 알림은 말한 사람이 없다
	if (Channel != ENUChatChannel::Local && Channel != ENUChatChannel::World && Channel != ENUChatChannel::Party)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayerController();
	if (PC == nullptr)
	{
		return;
	}

	if (bIsMine)
	{
		ShowBubble(PC->GetPawn(), Message, Channel);
		return;
	}

	// 말한 사람이 시야 밖이면 원격 캐릭터가 없다 --- 그때는 채팅창에만 남는다
	for (TActorIterator<AFieldRemoteCharacter> It(PC->GetWorld()); It; ++It)
	{
		if (It->GetDisplayName().EqualTo(SenderName))
		{
			ShowBubble(*It, Message, Channel);
			return;
		}
	}
}

void UNUSpeechBubbleUIController::ShowBubble(AActor* Speaker, const FText& Message, ENUChatChannel Channel)
{
	if (Speaker == nullptr)
	{
		return;
	}

	// 파티 · 전체 말은 모양을 가른다. 그 채널용이 미지정이면 일반 말풍선
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const TSoftClassPtr<UNUNoticeBannerWidget>& ClassPtr = (Channel == ENUChatChannel::Party && !Settings->PartySpeechBubbleClass.IsNull())
		? Settings->PartySpeechBubbleClass
		: (Channel == ENUChatChannel::World && !Settings->WorldSpeechBubbleClass.IsNull())
			? Settings->WorldSpeechBubbleClass
			: Settings->SpeechBubbleClass;
	UClass* BubbleClass = ClassPtr.LoadSynchronous();
	if (BubbleClass == nullptr)
	{
		UE_LOG(LogNUSpeechBubble, Warning, TEXT("말풍선 클래스 미지정 (프로젝트 세팅 NU UI > SpeechBubbleClass) --- 띄우지 않음"));
		return;
	}

	// 이미 붙인 말풍선이 있으면 그걸 쓰고, 없으면 이름표 자리에 붙인다 (이름표가 없는 액터는 루트)
	UWidgetComponent* BubbleComp = nullptr;
	USceneComponent* Anchor = Speaker->GetRootComponent();
	for (UWidgetComponent* WidgetComp : TInlineComponentArray<UWidgetComponent*>(Speaker))
	{
		if (Cast<UNUNoticeBannerWidget>(WidgetComp->GetUserWidgetObject()) != nullptr)
		{
			BubbleComp = WidgetComp;
			break;
		}
		if (Cast<UNUNameplateWidget>(WidgetComp->GetUserWidgetObject()) != nullptr)
		{
			Anchor = WidgetComp;
		}
	}

	if (BubbleComp == nullptr)
	{
		BubbleComp = NewObject<UWidgetComponent>(Speaker);
		BubbleComp->SetWidgetSpace(EWidgetSpace::Screen);
		BubbleComp->SetWidgetClass(BubbleClass);
		// 글 길이만큼 커지고 아래 가운데가 이름표에 붙어 긴 말은 위로 자란다. 이름표와의 간격은 WBP의 아래 여백
		BubbleComp->SetDrawAtDesiredSize(true);
		BubbleComp->SetPivot(FVector2D(0.5f, 1.0f));
		BubbleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BubbleComp->SetupAttachment(Anchor);
		BubbleComp->RegisterComponent(); // 이미 BeginPlay한 액터라 등록하면서 위젯이 만들어진다
	}
	else
	{
		// 근접으로 말하다 파티로 말하면 모양이 바뀐다 --- 재생 중에 클래스를 바꾸면 엔진이 위젯을 새로 만든다 (같은 클래스면 그대로)
		BubbleComp->SetWidgetClass(BubbleClass);
	}

	if (UNUNoticeBannerWidget* Bubble = Cast<UNUNoticeBannerWidget>(BubbleComp->GetUserWidgetObject()))
	{
		Bubble->ShowMessage(Message);
	}
}

#if !UE_BUILD_SHIPPING
// 서버 없이 PIE에서 말풍선 모양을 보는 경로 --- 내 캐릭터 머리 위에 띄운다
static void NUSpeechBubbleDebugSay(const TArray<FString>& Args, UWorld* World, ENUChatChannel Channel)
{
	APlayerController* PC = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	if (UNUUIManager* UI = UNUUIManager::Get(PC))
	{
		UI->GetController<UNUSpeechBubbleUIController>()->ShowBubble(PC->GetPawn(), FText::FromString(FString::Join(Args, TEXT(" "))), Channel);
	}
}

// 사용: nu.Bubble.Say 안녕하세요
static FAutoConsoleCommandWithWorldAndArgs GNUBubbleSayCommand(
	TEXT("nu.Bubble.Say"),
	TEXT("내 캐릭터 머리 위에 일반 말풍선 띄우기 (서버 없이 모양 확인). 사용: nu.Bubble.Say 문구"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		NUSpeechBubbleDebugSay(Args, World, ENUChatChannel::Local);
	}));

// 사용: nu.Bubble.SayWorld 안녕하세요
static FAutoConsoleCommandWithWorldAndArgs GNUBubbleSayWorldCommand(
	TEXT("nu.Bubble.SayWorld"),
	TEXT("내 캐릭터 머리 위에 전체 채팅 말풍선 띄우기 (서버 없이 모양 확인). 사용: nu.Bubble.SayWorld 문구"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		NUSpeechBubbleDebugSay(Args, World, ENUChatChannel::World);
	}));

// 사용: nu.Bubble.SayParty 안녕하세요
static FAutoConsoleCommandWithWorldAndArgs GNUBubbleSayPartyCommand(
	TEXT("nu.Bubble.SayParty"),
	TEXT("내 캐릭터 머리 위에 파티 말풍선 띄우기 (서버 없이 모양 확인). 사용: nu.Bubble.SayParty 문구"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		NUSpeechBubbleDebugSay(Args, World, ENUChatChannel::Party);
	}));
#endif
