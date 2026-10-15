// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "NUGAS/Interface/NUCombatantInterface.h"
#include "GameFramework/Actor.h"
#include "NUFieldSpirit.generated.h"

class ANUSpiritState;
class UNUSpiritFollowComponent;
class UNUSpriteVisualComponent;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;

UCLASS()
class PROJECTM_API ANUFieldSpirit : public AActor, public IAbilitySystemInterface, public INUCombatantInterface
{
	GENERATED_BODY()

public:
	ANUFieldSpirit();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	virtual UNUAbilitySystemComponent* GetNUASC() const override;

	void InitializeFieldSpirit(ANUSpiritState* InSpiritState, AActor* InFollowTarget);

	void SetFollowTarget(AActor* NewFollowTarget);

protected:
	virtual void BeginPlay() override;

	// 소유 나이아의 AnimationClass로 외형을 맞춤
	void ApplyNaiaVisual();

	UFUNCTION()
	void OnRep_AbilitySystemOwner();

	UFUNCTION()
	void OnRep_FollowTarget();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Animation", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	// 머티리얼·색·스케일·애님BP·빌보드는 이 컴포넌트가 담당
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Visual", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	// 팔로우 오프셋·보간·부유도 원격 정령과 같아야 하므로 컴포넌트로 분리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NU|Follow", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNUSpiritFollowComponent> FollowComponent;

	UPROPERTY(ReplicatedUsing = OnRep_AbilitySystemOwner)
	TObjectPtr<ANUSpiritState> AbilitySystemOwner;

	UPROPERTY(ReplicatedUsing = OnRep_FollowTarget)
	TObjectPtr<AActor> FollowTarget;
};
