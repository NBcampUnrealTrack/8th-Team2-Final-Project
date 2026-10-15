// 채팅(근접·전체·파티)을 말한 캐릭터 머리 위 말풍선으로 띄우는 UI 컨트롤러 (상주형 --- PC BeginPlay에서 생긴다).
// 채팅 서브시스템의 OnChatMessage를 구독만 한다. 채팅 코드와 캐릭터 코드는 건드리지 않는다.
//
// 말한 사람 찾기: 내 말은 내 폰, 남의 말은 표시 이름이 같은 원격 캐릭터.
//   채팅 알림에 player_id가 실려 오지 않아 이름으로 찾는다 (캐릭터 이름은 서버에서 유일하다)
// 파티 말은 파티원에게만 온다(파티 채널에 들어가 있을 때만 받는다) --- 그래서 거르지 않고 모양만 가른다.
// 말풍선은 처음 말할 때 액터에 스크린 스페이스 WidgetComponent를 하나 붙이고, 다음 말부터는 문구만 갈아 끼운다.
// 모양과 표시 시간은 WBP 소유 (프로젝트 세팅 NU UI > SpeechBubbleClass, 파티는 PartySpeechBubbleClass)
#pragma once

#include "CoreMinimal.h"
#include "UI/Controller/NUUIController.h"
#include "NUSpeechBubbleUIController.generated.h"

class UNUChatSubsystem;
enum class ENUChatChannel : uint8;

UCLASS()
class PROJECTM_API UNUSpeechBubbleUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	// 말한 액터 머리 위에 채널에 맞는 모양으로 문구를 띄운다. 이미 떠 있으면 문구를 갈아 끼우고 표시 시간을 다시 센다
	void ShowBubble(AActor* Speaker, const FText& Message, ENUChatChannel Channel);

protected:
	virtual void OnInitialize() override;
	virtual void OnDeinitialize() override;

private:
	void HandleChatMessage(ENUChatChannel Channel, const FText& SenderName, const FText& Message, bool bIsMine);

	UNUChatSubsystem* GetChatSubsystem() const;
};
