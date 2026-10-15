// NU_BaseUserWidget.h
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NU_BaseUserWidget.generated.h"

/*
전체 UI의 베이스가 되는 위젯 코드
위젯 생성/파괴 및 초기화, 뷰포트 표시 등을 적용
캐시로 컨트롤러 및 폰 가져오기

여기에는 PlayerController / Pawn 까지만 둔다.
타이틀 / 로그인 / 서버 선택 / 캐릭터 선택 화면은 ANUPlayerState 가 아직 없거나 의미가 없으므로,
로컬 플레이어의 게임플레이 컨텍스트(PlayerState, ASC)가 필요한 위젯은
UNU_BaseInGameWidget 을 상속한다.
 */
UCLASS()
class PROJECTM_API UNU_BaseUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "NU|UI")
	APlayerController* GetNUPlayerController() const;

	UFUNCTION(BlueprintPure, Category = "NU|UI")
	APawn* GetNUPawn() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void CacheNUPlayerController();
	void CacheNUPawn();
	void RefreshNUPawn();

	virtual void InitializeWidget();
	virtual void ReleaseWidget();

protected:
	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	TWeakObjectPtr<APlayerController> CachedNUPlayerController;

	UPROPERTY(BlueprintReadOnly, Category = "NU|UI")
	TWeakObjectPtr<APawn> CachedNUPawn;
};
