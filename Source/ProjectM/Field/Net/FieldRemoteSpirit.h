// 다른 플레이어의 정령을 필드에 그리는 액터.
//
// ANUFieldSpirit을 상속하지 않는 이유: 그쪽은 "어떤 ANUSpiritState의 필드 아바타"라
// ASC와 INUCombatantInterface를 들고 있다. 남의 정령에는 SpiritState가 없으므로
// 그걸 재사용하면 AbilitySystemOwner가 null인 반쪽짜리 전투원이 월드에 떠다니게 된다.
// AFieldRemoteCharacter가 AFieldPlayerCharacter를 상속하지 않은 것과 같은 이유다.
//
// 대신 표현과 팔로우 이동은 ANUFieldSpirit과 컴포넌트를 공유한다 —
// UNUSpriteVisualComponent / UNUSpiritFollowComponent. 값이 갈라지지 않는다.
//
// 위치는 서버에서 받지 않는다. 정령은 주인을 따라다니므로 주인 위치에서 계산한다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Field/NUFieldTypes.h"
#include "FieldRemoteSpirit.generated.h"

class UNUSpiritFollowComponent;
class UNUSpriteVisualComponent;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;

UCLASS()
class PROJECTM_API AFieldRemoteSpirit : public AActor
{
	GENERATED_BODY()

public:
	AFieldRemoteSpirit();

	virtual void Tick(float DeltaSeconds) override;

	/** 따라다닐 주인 (AFieldRemoteCharacter). */
	void SetFollowTarget(AActor* InFollowTarget);

	/** 주인의 활동 상태를 그대로 받는다. 표현 방법은 이 클래스가 정한다. */
	void SetActivity(EFieldActivity InActivity);

	/** Naia table `Data Index` 나중에 이 값으로 Sprite를 바꾼다 */
	void SetSpiritId(int32 InSpiritId);

	int32 GetSpiritId() const { return SpiritId; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Animation")
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	// 머티리얼·색·스케일·Anim BP·빌보드 담당. ANUFieldSpirit과 같은 설정을 사용한다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Visual")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Follow")
	TObjectPtr<UNUSpiritFollowComponent> FollowComponent;

private:
	UPROPERTY()
	TObjectPtr<AActor> FollowTarget;

	int32 SpiritId = 0;
};
