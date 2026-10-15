#pragma once

#include "CoreMinimal.h"
#include "LevelInstance/LevelInstanceActor.h"
#include "UObject/Interface.h"
#include "NUInteriorLevelInstance.generated.h"

class UStaticMeshComponent;
class ULevelStreaming;
class UNUWalkableInteriorComponent;

// 실내 Blueprint가 사용하는 표시 로직을 에디터에서도 동일하게 적용한다.
UINTERFACE(BlueprintType)
class PROJECTM_API UNUInteriorEditorView : public UInterface
{
	GENERATED_BODY()
};

class PROJECTM_API INUInteriorEditorView
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "NU|Interior|Editor", meta = (CallInEditor = "true"))
	void ApplyInteriorEditorView(bool bInside);
};

// 외관은 배치 액터에, 방과 문은 별도 레벨에 두고 기존 실내 표시 상태만 연결한다.
UCLASS()
class PROJECTM_API ANUInteriorLevelInstance : public ALevelInstance
{
	GENERATED_BODY()

public:
	ANUInteriorLevelInstance();
	virtual void OnLevelInstanceLoaded() override;

#if WITH_EDITOR
	virtual void OnEdit() override;
	virtual void OnCommit(bool bChanged) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
	virtual void BeginDestroy() override;
#endif

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Interior")
	TObjectPtr<UStaticMeshComponent> ExteriorMesh;

	// 같은 실내 레벨을 공유해도 장소 ID는 건물 배치마다 지정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Location", meta = (ClampMin = "0"))
	int32 LocationId = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Interior|Location", meta = (ClampMin = "0"))
	int32 ExteriorLocationId = 0;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
#if WITH_EDITOR
	void ApplyEditorView();
	void HandleEditorLevelAdded(ULevel* Level, UWorld* World);
#endif

	UFUNCTION()
	void BindInterior();
	UFUNCTION()
	void HandlePresentationChanged(bool bInside);

	TWeakObjectPtr<UNUWalkableInteriorComponent> Interior;
	TWeakObjectPtr<UStaticMeshComponent> InteriorMesh;
	TWeakObjectPtr<ULevelStreaming> InteriorStreaming;
};
