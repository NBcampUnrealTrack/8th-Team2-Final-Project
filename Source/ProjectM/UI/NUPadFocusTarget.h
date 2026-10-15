#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "NUPadFocusTarget.generated.h"

class UWidget;

// 창 안에 든 위젯(통합 메뉴의 페이지 등)이 패드 첫 자리를 정하는 계약 --- 시트 창의 FocusForPad와 같은 역할.
// UI 매니저가 창을 열 때·패드를 잡을 때 맨 위 창 안에서 이것을 구현한 보이는 위젯을 찾아, 돌려준 위젯에 포커스를 준다.
// 포커스는 매니저 한 곳에서만 준다 --- 여기서는 자리만 알려 준다 (nullptr이면 매니저가 첫 버튼에 준다)
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UNUPadFocusTarget : public UInterface
{
	GENERATED_BODY()
};

class PROJECTM_API INUPadFocusTarget
{
	GENERATED_BODY()

public:
	virtual UWidget* GetPadFocusTarget() const { return nullptr; }
};
