#include "UI/NUNameplateComponent.h"

#include "UI/NUNameplateWidget.h"
#include "UI/NUUISettings.h"

void UNUNameplateComponent::BeginPlay()
{
	// 위젯은 부모 BeginPlay(InitWidget)가 만든다 --- 그 전에 클래스를 바꿔 둔다 (재생 전이라 SetWidgetClass는 값만 바꾼다)
	if (UClass* NameplateClass = GetDefault<UNUUISettings>()->NameplateClass.LoadSynchronous())
	{
		SetWidgetClass(NameplateClass);
		SetDrawAtDesiredSize(true);
	}
	Super::BeginPlay();
}
