// 패드 모드에서 포커스가 어디 있는지 보여 주는 테두리 (임시 --- 공용 버튼에 포커스 상태 그림이 생기면 이 컨트롤러와 WBP_PadFocusFrame을 지운다).
// 모양은 WBP(프로젝트 세팅 NU UI > PadFocusFrameClass)가 갖고, 자리는 매 프레임 포커스된 버튼의 화면 사각형(NUUIGeometry)에 맞춘다.
// 매니저가 패드 모드 변경을 알려 주면 켜고 끈다. 포커스가 서는 조작부(버튼·슬라이더·체크박스·목록 줄 등)에 그리되
// 창 자체가 포커스를 잡는 위젯(대화창 --- 화면 전체라 테두리가 맵을 두른다)과 글자칸(커서가 보인다), 목록·스크롤 상자 같은 그릇은 뺀다.
// 아이템 칸(WBP_ItemCell)처럼 창이 아닌 중첩 위젯이 포커스를 받으면 그린다.
// 그릇에 포커스가 서면(D패드가 그 사각형으로 들어온 것) 안의 항목·조작부로 바로 넘긴다
#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UI/Controller/NUUIController.h"
#include "NUPadFocusUIController.generated.h"

class UUserWidget;

UCLASS()
class PROJECTM_API UNUPadFocusUIController : public UNUUIController
{
	GENERATED_BODY()

public:
	// 패드 모드로 들어가면 틱을 걸고 나오면 뗀다 (틱은 패드 모드 동안만 돈다)
	void SetActive(bool bActive);

	// 이전 월드의 테두리 위젯은 버린다 (필요하면 다음 틱에 새로 만든다)
	virtual void OnPlayerControllerChanged(APlayerController* NewPC) override;

protected:
	virtual void OnDeinitialize() override;

private:
	bool TickFrame(float DeltaTime);
	void DropFrame();

	// 포커스가 바뀐 프레임에 한 번. 목록 자체면 항목 이동으로, 스크롤 상자면 안의 첫 조작부로 넘기고,
	// 목록 줄에 바로 섰으면 그 항목을 고른 상태로 맞춘다. 그릇이나 창 루트에 섰으면 true (테두리를 안 그린다)
	bool AdjustFocus(const TSharedPtr<SWidget>& Focused);

	TWeakPtr<SWidget> LastFocused;
	bool bSkipFrame = false;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Frame;

	FTSTicker::FDelegateHandle TickHandle;
};
