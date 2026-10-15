#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/TimerHandle.h"
#include "NUStorySequenceCue.generated.h"

class APlayerController;
class ALevelSequenceActor;
class ULevelSequence;
class ULevelSequencePlayer;
class UNUStorySlideshowData;
class UNUStorySlideshowWidget;
class USoundMix;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNUStoryPlaybackFinished, bool, bCompleted);

// 시퀀스 재생과 슬라이드 표시 상태를 함께 소유한다. 속성 이름은 기존 트랙과 연결된다.
UCLASS()
class PROJECTM_API ANUStorySequenceCue : public AActor
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	int32 SlideIndex = 0;

	// -1은 사진 전환 등 대사를 숨기는 구간에 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	int32 LineIndex = 0;

	// 시퀀스에서 보간한 값을 화면에서 내림하여 표시할 글자 수로 사용한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float RevealedCharacters = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float PhotoOpacity = 1.0f;

	// 사진 뒤 검은 바탕. 0이면 시퀀스 카메라나 게임 화면이 그대로 보인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float BackdropOpacity = 1.0f;

	// 사진 이동(-1 ~ 1). 도트 그림책(bPixelPerfectPhotos)이면 화면 밖 여백 안에서 움직이고 그림 한 칸 단위로 끊긴다.
	// 일반 사진이면 화면 크기 대비 비율로 옮긴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float PhotoPanX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float PhotoPanY = 0.0f;

	// 일반 사진의 확대(1 = 화면에 맞춤). 도트 그림책은 깨지지 않게 정수 배율만 쓰므로 무시한다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float PhotoScale = 1.0f;

	// 대사(자막) 불투명도. 문장을 한 번에 띄우고 페이드로 넣고 뺄 때 쓴다. 트랙이 없으면 1이라 기존 연출은 그대로다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float CaptionOpacity = 1.0f;

	// 그림 위를 떠다니는 도트 빛 입자의 양(0~1). 0이면 그리지 않는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float MoteIntensity = 0.0f;

	// 위아래 시네마 레터박스(0~1). 데이터의 LetterboxFraction 만큼 덮는다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float LetterboxAmount = 0.0f;

	// 화면 전체를 덮는 빛(데이터의 FlashColor). 장면 전환을 빛으로 덮을 때 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float FlashOpacity = 0.0f;

	// 타이틀 크기 배율(가운데 기준)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float TitleScale = 1.0f;

	// 데이터의 TitleImage 불투명도
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float TitleOpacity = 0.0f;

	// 0이 아니면 그 프레임에 플레이어 폰에 월드 축 이동 입력을 넣는다(크기 1 = 최대 속도). 입력 차단 중에도 들어간다.
	// 연출 화면을 걷어 낸 뒤 실제 캐릭터를 몇 걸음 걷게 할 때 쓴다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float PawnMoveX = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Interp, Category = "NU|Story")
	float PawnMoveY = 0.0f;

	ANUStorySequenceCue();
	virtual void Tick(float DeltaSeconds) override;

	// 재생 중이면 새 요청을 거절한다. 실패한 요청에는 OnFinished를 보내지 않는다.
	UFUNCTION(BlueprintCallable, Category = "NU|Story")
	bool PlayStory(APlayerController* Controller, UNUStorySlideshowData* Data, ULevelSequence* Sequence);

	UFUNCTION(BlueprintCallable, Category = "NU|Story")
	void StopStory();

	// true는 끝까지 재생, false는 스킵 또는 외부 종료다. 퀘스트 완료를 의미하지 않는다.
	UPROPERTY(BlueprintAssignable, Category = "NU|Story")
	FNUStoryPlaybackFinished OnFinished;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "NU|Story")
	TObjectPtr<ULevelSequencePlayer> SequencePlayer;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "NU|Story")
	TObjectPtr<UNUStorySlideshowWidget> Slideshow;

	// 필드에서는 HUD와 트래커 위를 덮도록 호출자가 높인다.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	int32 ViewportZOrder = 0;

	// OnFinished(false) 중 사용자가 명시적으로 스킵한 경우만 true다.
	UPROPERTY(BlueprintReadOnly, Transient, Category = "NU|Story")
	bool bWasSkipped = false;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleSequenceFinished();

	UFUNCTION()
	void HandleSequenceStopped();

	void FinishPendingPlayback();
	FTimerHandle PendingFinishHandle;
	bool bPendingNaturalFinish = false;

	UFUNCTION()
	void HandleSkipRequested();

	void FinishPlayback(bool bCompleted);

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> SequenceActor;

	// 재생 시작 때 건 믹스. 끝날 때 같은 믹스를 걷는다.
	UPROPERTY(Transient)
	TObjectPtr<USoundMix> ActiveSoundMix;

	bool bPlaying = false;
	bool bAllowSkip = true;

	// 걷기 입력을 넣을 플레이어. 재생을 시작한 컨트롤러다.
	TWeakObjectPtr<APlayerController> PlayingController;
};
