// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interaction/Interface/NUInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "NUStorageBoxActor.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UNUInteractionRouterComponent;
class UNUStorageProviderComponent;

// 필드 창고 오브젝트 — 상호작용(E)하면 창고 창(계정|캐릭터)이 열린다.
// 외형은 임시로 에디터에서 메시/스프라이트를 붙여 쓰고, 레벨에 배치해서 사용
UCLASS()
class PROJECTM_API ANUStorageBoxActor : public AActor, public INUInteractableInterface
{
	GENERATED_BODY()

public:
	ANUStorageBoxActor();

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual bool Interact_Implementation(AActor* Interactor) override;

protected:
	// 플레이어 Capsule과 오버랩되는 실제 상호작용 범위
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Storage")
	TObjectPtr<UBoxComponent> InteractionVolume;

	// 임시 외형 (에디터에서 메시 지정 — 없어도 동작)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Storage")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Storage|Interaction")
	TObjectPtr<UNUInteractionRouterComponent> InteractionRouter;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Storage|Interaction")
	TObjectPtr<UNUStorageProviderComponent> StorageProvider;
};
