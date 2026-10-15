// UI 컨트롤러 베이스 — 한 도메인(거래·알림 등)의 창을 소유하고 여닫기를 결정한다.
// 매니저가 소유하며 레벨 전환을 넘어 산다. 창을 화면에 올리는 것은 매니저(OpenWindow) 담당.
//
// 세션 수명 데이터(gRPC 세션·파티·채팅)를 그리는 UI가 대상이다.
// 레벨 수명 데이터를 그리는 UI(인벤=PlayerState 복제)는 그 데이터 컴포넌트가 컨트롤러를 겸하는 것이 맞다
// --- 컨트롤러는 자기 데이터와 같은 수명에 살아야 한다.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/PlayerController.h"   // GetOrCreateWidget 본문이 PC->GetWorld()를 부른다 (유니티 빌드 밖에서도 컴파일되게)
#include "NUUIController.generated.h"

class UNUUIManager;

// 도메인별 UI 컨트롤러의 베이스. 매니저의 GetController<T>()로 얻는다.
UCLASS(Abstract)
class PROJECTM_API UNUUIController : public UObject
{
	GENERATED_BODY()

public:
	// 매니저가 생성 직후/해제 직전에 호출한다
	void InitializeController(UNUUIManager* InManager);
	void DeinitializeController();

	// --- 경계 이벤트 (매니저가 전파) ---
	// 컨트롤러는 매니저보다 짧게 사는 것들(PC·PlayerState·월드)을 캐시하지 않고,
	// 이 지점들에서만 다시 붙인다.

	// 레벨 전환으로 PlayerController가 교체됨 (없어지는 경우 nullptr)
	virtual void OnPlayerControllerChanged(APlayerController* NewPC) {}

	// 서버 세션이 끝남 (접속 끊김·강퇴).
	// bCloseOnSessionEnd 창은 매니저가 이미 닫은 뒤이므로, 컨트롤러는 자기 상태만 정리하면 된다
	virtual void OnServerSessionEnded() {}

protected:
	// 자식 구현 지점
	virtual void OnInitialize() {}
	virtual void OnDeinitialize() {}

	UNUUIManager* GetManager() const { return Manager.Get(); }

	// 레벨마다 바뀌므로 캐시하지 않고 매번 해석한다
	APlayerController* GetOwningPlayerController() const;

	// 위젯 캐시 헬퍼 --- 이전 레벨의 위젯을 재사용하지 않도록 월드가 다르면 버리고 새로 만든다
	template<typename TWidget>
	TWidget* GetOrCreateWidget(TObjectPtr<TWidget>& Cached, TSubclassOf<TWidget> WidgetClass)
	{
		APlayerController* PC = GetOwningPlayerController();
		if (PC == nullptr || WidgetClass == nullptr)
		{
			return nullptr;
		}
		if (Cached != nullptr && Cached->GetWorld() != PC->GetWorld())
		{
			Cached = nullptr;
		}
		if (Cached == nullptr)
		{
			Cached = CreateWidget<TWidget>(PC, WidgetClass);
		}
		return Cached;
	}

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UNUUIManager> Manager;
};
