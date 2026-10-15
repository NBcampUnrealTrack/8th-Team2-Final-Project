// 캐릭터 미리보기 액터 대여소.
//
// 미리보기 액터는 만드는 값이 있어(컴포넌트 8개 + 렌더 타깃) 창을 여닫을 때마다 새로 만들 이유가 없다.
// 여기서 빌려주고 돌려받아 재사용한다.
//
// 필요한 개수는 "본 사람 수"가 아니라 "동시에 떠 있는 창 수"다 --- 지나가는 사람을 아무리 많이 눌러도
// 남의 정보창은 보통 하나뿐이고, 대상만 바뀌면 같은 액터에 장비만 갈아입히면 된다.
// 그래서 상한을 낮게 두고, 넘치면 안 빌려준다 (창은 자리표시로 남는다).
//
// 월드 서브시스템인 이유: 빌려주는 것이 액터라 월드와 수명을 같이해야 한다.
// 레벨이 바뀌면 액터도 같이 사라지고, 여기 남은 참조도 함께 정리된다.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NUCharacterPreviewSubsystem.generated.h"

class ANUCharacterPreviewActor;

UCLASS()
class PROJECTM_API UNUCharacterPreviewSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 쉬고 있는 액터를 빌려준다. 없으면 상한 안에서 하나 만든다. 상한을 넘으면 nullptr
	ANUCharacterPreviewActor* Acquire();

	// 다 쓴 액터를 돌려받는다 --- 캡처를 끄고 다음 대여를 기다린다
	void Release(ANUCharacterPreviewActor* Actor);

	virtual void Deinitialize() override;

private:
	// 동시에 띄울 수 있는 미리보기 수. 내 정보창 + 남의 정보창이면 2로 충분하고,
	// 파티창처럼 여러 명을 한 화면에 세우게 되면 이 값을 올린다
	static constexpr int32 MaxActors = 4;

	// 만들어 둔 액터 전부 (빌려준 것 + 쉬는 것)
	UPROPERTY(Transient)
	TArray<TObjectPtr<ANUCharacterPreviewActor>> Actors;

	// 지금 빌려나가 있는 것
	UPROPERTY(Transient)
	TSet<TObjectPtr<ANUCharacterPreviewActor>> InUse;
};
