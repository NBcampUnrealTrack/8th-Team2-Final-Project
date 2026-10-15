// 다른 플레이어(원격)를 필드에 그리는 액터.
//
// AFieldPlayerCharacter를 상속하지 않는 이유: 캡슐 충돌이 살아 있으면 원격 플레이어가
// **내 캐릭터를 물리적으로 밀어낸다.** (원격은 SetActorLocation으로 위치를 바꾸므로)
// 그 외에도 인카운터 컴포넌트/카메라가 인원수만큼 생긴다.
//
// 스프라이트 표현은 UNUSpriteVisualComponent를 AFieldPlayerCharacter와 공유한다.
// 상속 관계가 없어도 머티리얼·그림자·빌보드·로코모션이 갈라지지 않는다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Field/NUFieldTypes.h" // EFieldFacing, EFieldActivity
#include "FieldRemoteCharacter.generated.h"

class UCapsuleComponent;
class UNUSpriteVisualComponent;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;

/**
 * 원격 플레이어. 보간이 아니라 **타겟 추격**으로 움직인다 —
 * 서버가 준 좌표를 '목표'로 삼고 내 캐릭터와 같은 속도로 그리 걸어간다.
 * 목표가 계속 갱신되므로 도착하지 못하고 계속 걷는 것처럼 보인다.
 */
UCLASS()
class PROJECTM_API AFieldRemoteCharacter : public AActor
{
	GENERATED_BODY()

public:
	AFieldRemoteCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void Destroyed() override;

	/** 서버가 보낸 최신 상태 반영. */
	void ApplyServerState(const FVector& InTarget, EFieldFacing InFacing, EFieldActivity InActivity);

	/** 스폰 직후 1회 — 걸어가지 않고 그 자리에 즉시 놓는다. */
	void TeleportToServerState(const FVector& InLocation, EFieldFacing InFacing, EFieldActivity InActivity);

	void SetActivity(EFieldActivity InActivity);

	/** 이 플레이어가 데리고 다니는 정령, 여기서 만들어서 로컬로 따라다닌다. */
	void SetSpirit(int32 InSpiritId);

	// 외형(종족·성별·직업 몸 + 머리·외모 파츠)을 입힌다. 몸 스킨도 이때 같이 입혀지므로
	// 파츠가 없어도(0, 0) 등장 시 한 번은 불러야 한다. 전직은 Appeared가 다시 와서 여기로 들어온다
	void SetAppearance(int32 InRaceId, int32 InGender, int32 InHairId, int32 InFaceId, int32 InJobId);

	// 서버 플레이어 id (UserInfo.Idx), 이동·전투상태·정령이 이 값을 사용
	UPROPERTY(BlueprintReadOnly, Category = "Ember|Field")
	int64 PlayerId = 0;

	// 파티·거래·귓속말이 이 값을 사용함
	UPROPERTY(BlueprintReadOnly, Category = "Ember|Field")
	int64 CharacterIdx = 0;

	// 표시용 이름, 비어 있으면 id로 대체
	FText GetDisplayName() const;
	
	// 서버에서 해당 캐릭터의 이름 지정
	void SetCharacterName(const FString& InCharacterName);

	UPROPERTY(BlueprintReadOnly, Category = "Ember|Field")
	EFieldActivity Activity = EFieldActivity::Idle;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Visual")
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Animation")
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	// 머티리얼·그림자·애님BP·빌보드·로코모션 담당. AFieldPlayerCharacter와 같은 설정을 사용한다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Visual")
	TObjectPtr<UNUSpriteVisualComponent> SpriteVisual;

	// 커마 파츠 렌더러 --- 위치·레이어 정의는 AFieldPlayerCharacter와 같다.
	// 장비 겹침 레이어(1~5)는 만들지 않는다 (8/25 결정: 장비 외형은 무기만)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Visual")
	TObjectPtr<UPaperFlipbookComponent> HairSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Visual")
	TObjectPtr<UPaperFlipbookComponent> FaceSprite;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Collision")
	TObjectPtr<UCapsuleComponent> SelectCollision;

	// 머리 위 이름표 (스크린 스페이스). SetCharacterName이 표시 이름을 채운다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ember|Visual")
	TObjectPtr<class UWidgetComponent> NameplateWidget;

private:
	FVector TargetLocation = FVector::ZeroVector;
	EFieldFacing Facing = EFieldFacing::Down;

	// 캐릭터 이름, SVR_Field_Appeared.character_name로 이름 지정함
	UPROPERTY(BlueprintReadOnly, Category = "Ember|Field", meta = (AllowPrivateAccess = "true"))
	FString CharacterName;
	
	// 내가 데리고 있는 정령. 나와 수명을 같이한다
	UPROPERTY()
	TObjectPtr<class AFieldRemoteSpirit> Spirit;

	int32 SpiritId = 0;

	// 마지막으로 입힌 외형 값. -1 = 미적용 --- 전부 0이 와도 첫 호출이 실행되게 한다
	int32 RaceId = -1;
	int32 Gender = -1;
	int32 HairId = -1;
	int32 FaceId = -1;
	int32 JobId = -1;
};
