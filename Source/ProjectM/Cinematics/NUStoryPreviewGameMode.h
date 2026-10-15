#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "NUStoryPreviewGameMode.generated.h"

class ULevelSequence;
class UNUStorySlideshowData;
class ANUStorySequenceCue;

// 검토용 맵에서만 실행한다. 서버 진행 상태나 퀘스트를 변경하지 않는다.
UCLASS()
class PROJECTM_API ANUStoryPreviewGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ANUStoryPreviewGameMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TObjectPtr<UNUStorySlideshowData> StoryData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Story")
	TObjectPtr<ULevelSequence> StorySequence;

	UPROPERTY(Transient)
	TObjectPtr<ANUStorySequenceCue> ActiveCue;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleStoryFinished(bool bCompleted);
};
