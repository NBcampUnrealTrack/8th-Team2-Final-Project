#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "NUNameplateComponent.generated.h"

// 필드 캐릭터(내 것·남의 것) 머리 위 이름표 칸 --- 프로젝트 세팅(NU UI > NameplateClass)의 WBP를 입힌다.
// 모양과 크기는 WBP가 갖는다(가장 긴 닉네임이 들어가는 크기, 원하는 크기로 그림).
// 세팅이 비면 캐릭터 생성자가 넣은 코드 폴백(UNUNameplateWidget, 160x20)을 그대로 쓴다
UCLASS(ClassGroup = (NU))
class PROJECTM_API UNUNameplateComponent : public UWidgetComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
};
