// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Field/NUFieldTypes.h"
#include "Field/NUSpriteVisualSettings.h"
#include "NUSpriteVisualComponent.generated.h"

class UDecalComponent;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class UPaperZDAnimationComponent;
class UTexture;

// Paper2D 스프라이트 표현을 담당하는 공용 컴포넌트.
UCLASS(meta = (BlueprintSpawnableComponent))
class PROJECTM_API UNUSpriteVisualComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UNUSpriteVisualComponent();

	// 액터 생성자에서 만든 컴포넌트와 설정을 넘겨받고, 실제 적용은 BeginPlay에서 함
	void Setup(UPaperFlipbookComponent* InSprite, UPaperZDAnimationComponent* InAnimation, 
		const FNUSpriteVisualSettings& InSettings);

	// 빌보드 회전, 수동 갱신이 필요한 액터의 Tick에서 호출
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void UpdateBillboard();

	// 정적인 NPC처럼 별도 Actor Tick이 필요 없는 대상의 빌보드만 저주기로 자동 갱신.
	// 레이어를 등록한 액터에는 쓰지 말 것 --- 같은 틱이 레이어 높이도 맞추므로 간격을 두면 머리·장비가 몸보다 늦는다
	void EnableAutomaticBillboardUpdates(float UpdateInterval);

	// 4방향 이동 상태를 PaperZD로 넘김
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void SetLocomotion(EFieldFacing Facing, bool bMoving);

	// 스프라이트 좌우 반전
	// 나이아 아트는 왼쪽을 보게 그리는 것이 기준이라, 오른쪽을 볼 때만 키면 됨
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void SetHorizontalFlip(bool bInFlip);

	// 스프라이트 크기 배율 (설정의 RelativeScale 을 바꾼다). 빌보드 세로 보정은 그 위에 그대로 곱해진다
	void SetSizeScale(float InScale);

	// 일시적인 색 변경 (전투중 톤다운 등)
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void SetColorOverride(const FLinearColor& InColor);

	// 색을 설정의 기본값으로 되돌림
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void ClearColorOverride();

	// 디졸브 진행값 (0 = 원래대로, 1 = 완전히 사라짐)
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void SetDissolveAmount(float InAmount);

	// 깜빡임 같은 일시적인 불투명도 (1 = 원래대로, 0 = 안 보임), 디졸브와 곱해짐
	// 반투명 머티리얼에서만 부드럽게 보임 - 마스크드는 기준값에서 끊겨 보였다 사라졌다 함
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void SetOpacity(float InOpacity);

	// 본체와 모든 레이어의 머티리얼을 통째로 덮어씀
	// null 을 넘기면 RefreshAppearance 로 원래 상태로 돌림
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void OverrideMaterial(UMaterialInterface* InMaterial);

	// 머티리얼·색·스케일·그림자를 다시 입힘
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void RefreshAppearance();

	// 나이아 정의의 AnimationClass로 애님 BP를 교체함
	// 정의를 못 찾거나 값이 비어 있으면 프리셋 값을 그대로 두고 false
	// 전투 아바타·필드 나이아·원격 나이아가 전부 이 컴포넌트를 갖고 있어 여기 둠
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	bool ApplyNaiaAnimBlueprint(int32 NaiaId);

	// 애님 BP를 직접 지정함, 프리셋 대신 호출부가 고른 값을 쓸 때 사용
	// 플레이어 전투 아바타가 필드와 같은 외형을 쓰려고 사용함
	bool ApplyAnimBlueprintClass(const TSoftClassPtr<UPaperZDAnimInstance>& AnimBlueprint);

	// 장비 겹쳐 그리기용 보조 렌더 컴포넌트를 애니메이션 레이어에 연결 (PaperZD 레이어 시스템).
	// 본체와 같은 애님 재생을 공유하므로 프레임·방향·위치가 자동으로 동기화된다.
	// 레이어에 무엇을 그릴지는 스킨(ApplyAnimationSkinToLayer)이 정한다 — 여기서는 연결만.
	// 레이어 번호는 애님 소스(AS_Player)의 Additional Layer Data 순서 + 1 (0번은 본체)
	// BounceScale: 몸통 프레임 바운스를 얼마나 따라갈지 (1=그대로, 0=고정).
	// FrameDelay: 몸통보다 몇 프레임 늦게 따라갈지 (팔처럼 박자가 다른 부위용).
	// 좌우·상하 기준 위치는 컴포넌트의 RelativeLocation을 그대로 쓴다 (등록 시점 값이 기준)
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void RegisterLayerRenderComponent(UPaperFlipbookComponent* InComponent, int32 LayerIndex,
		float BounceScale = 1.0f, int32 FrameDelay = 0);

	// 장비 겹침 레이어를 몸통 프레임의 세로 흔들림에 맞춰 올렸다 내림.
	// 레이어가 있으면 이 컴포넌트의 틱이 매 프레임 애님 컴포넌트 뒤에서 부른다 --- 액터 Tick에서 부르지 말 것 (몸 프레임보다 한 틱 늦는다).
	// 부위당 그림이 한 장뿐인 장비도 Idle 바운스를 따라가게 하는 보정이다.
	// 장비를 프레임별로 그려 받았다면 보정이 이중으로 들어가므로 nu.EquipFrameBounce 0으로 끈다
	UFUNCTION(BlueprintCallable, Category = "NU|Sprite")
	void UpdateLayerFrameOffsets();

	// 장비 레이어 미세조정 (콘솔 nu.Equip.Tune). 아트를 갈지 않고 눈으로 맞추는 용도다.
	// XOffset/ZOffset: 등록 위치에서 좌우·위아래로 밀기 (스프라이트 피벗이 몸통과 안 맞을 때 여기서 잡는다)
	// BounceScale: 몸통 바운스 추종 비율 / FrameDelay: 몇 프레임 늦게 따라갈지 (팔처럼 박자가 다른 부위용)
	void TuneLayer(int32 LayerIndex, float XOffset, float ZOffset, float BounceScale, int32 FrameDelay);

	// 커마 베이스 몸(레이어 0)과 파츠(머리·외모, 레이어 6·7)를 입힌다. 장비와 같은 방식이고 레이어만 다르다.
	// 몸은 종족·성별·직업으로 몸 표(NUAppearance::FindBody)에서, 파츠는 번호로 찾는다.
	// 값은 서버가 돌려준 것(FNUCharacterSummary / SVR_Field_Appeared의 race_id, gender, hair_id, face_id)을 그대로 넘기면 된다.
	// JobId는 직업 컴포넌트의 현재 직업 --- 표에 그 직업 몸이 있으면 몸통만 바뀐다. 없거나 모르면 0(기본 몸)
	void RefreshAppearanceSkins(int32 RaceId, int32 Gender, int32 HairId, int32 FaceId, int32 JobId);

	// 장착한 장비의 겹침 스킨을 레이어에 입힌다. 안 낀 부위는 스킨을 떼고 레이어를 숨긴다.
	// 필드 캐릭터와 정보창 미리보기가 같은 그림을 쓰도록 여기 둔다 --- 두 곳에 나뉘면 언젠가 어긋난다
	void RefreshEquipmentSkins(const class UNUEquipmentComponent* Equipment,
		class UNUInventoryComponent* Inventory);

	// 전투 그림을 입힘 --- 몸(레이어 0)·머리(6)·얼굴(7)을 몸 줄의 전투 스킨으로 덮고 장비 레이어는 숨긴다.
	// 머리는 HairId 의 머리 파츠에 맞는 전투 머리(FNUBodyRow::FindCombatHairSkin), 머리 색도 필드처럼 입힌다.
	// 전투 애님 BP 를 먼저 건 뒤에 불러야 한다. 몸 줄에 전투 스킨이 없으면 false (부르는 쪽이 필드 모습으로 돌아간다)
	bool RefreshCombatAppearanceSkins(int32 RaceId, int32 Gender, int32 HairId, int32 JobId);

	// 레이어 바운스를 켜고 끈다. 필드 머리는 모든 몸이 공용이라 바운스 표가 몸을 따라가게 하지만,
	// 전투 그림은 머리·얼굴이 프레임마다 그려져 있어 끈다 --- 켜 두면 표에 없는 플립북을
	// 그림 윗선으로 자동 측정해서 머리가 두 번 흔들린다
	void SetLayerBounceEnabled(bool bEnabled);

	// 쓰러짐을 애님 BP 에 넣는다 (전투 애님 BP 가 Dead 상태로 넘어간다).
	// 애님 BP 가 새로 만들어지면 기본값으로 돌아가므로 여기서 기억했다가 다시 넣는다
	void SetAnimDeadState(bool bDead);

	// 위아래 보정만 바꾼다 (좌우·바운스·지연은 그대로) --- 방향마다 달라지는 값에 쓴다.
	// Z를 컴포넌트에 직접 넣으면 매 프레임 바운스 보정이 덮어쓰므로 여기를 거쳐야 한다
	void SetLayerTuneZ(int32 LayerIndex, float ZOffset);

	UPaperFlipbookComponent* GetSprite() const;

	// 4방향 → PaperZD가 받는 입력 벡터
	static FVector2D FacingToDirection(EFieldFacing Facing);

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ApplyAnimBlueprint();
	void ApplyColor();

	// 발밑 블롭 그림자 데칼을 만들고 크기·위치를 맞춤 (Settings.BlobShadowRadius 가 0이면 숨김)
	void RefreshBlobShadow();

	// 등록된 레이어 컴포넌트들을 현재 애님 플레이어에 연결 (애님 BP 교체 시 재호출 필요)
	void ApplyLayerLinks();

#if WITH_EDITOR
	// 에디터는 텍스처를 뒤에서 준비하고, 끝나면 그 텍스처를 가리키는 머티리얼을 쓰는 컴포넌트만 다시 그리게 한다.
	// 스프라이트 텍스처는 머티리얼이 아니라 스프라이트가 넘기는 것이라 여기서 빠진다 --- 준비 전에 만들어진 스프라이트는
	// 그림은 바뀌어도 임시 텍스처의 필터 설정을 계속 쥐어 흐리게 그려진다(에디터를 켠 뒤 첫 PIE). 플립북이 바뀌는 캐릭터는
	// 저절로 풀리지만 대기 그림 하나뿐인 나이아는 그대로 남으므로, 준비가 끝난 텍스처를 쓰는 스프라이트를 직접 다시 그리게 한다
	void HandleTexturesCompiled(const TArrayView<UTexture* const>& Textures);
#endif

	// 뒷모습이면 몸 줄의 BackViewPartTuneZ를 머리·외모 레이어에 넣고, 다른 방향이면 되돌린다
	void ApplyBackViewPartTune();

	// 머리 레이어의 머티리얼을 머리 색에 맞춘다 --- 물들였으면 물들이는 머티리얼, 아니면 다른 레이어와 같은 것.
	// 레이어 머티리얼을 다시 꽂는 곳(레이어 연결, 덮어쓴 머티리얼 해제) 뒤에 다시 불러야 한다
	void ApplyHairTintMaterial();

private:
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite", meta = (AllowPrivateAccess = "true"))
	FNUSpriteVisualSettings Settings;

	UPROPERTY(Transient)
	TObjectPtr<UPaperFlipbookComponent> Sprite;

	UPROPERTY(Transient)
	TObjectPtr<UPaperZDAnimationComponent> Animation;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> BlobShadow;

	// 로코모션은 바뀔 때만 갱신함
	// 걷는 중 방향만 바뀌는 경우에 방향도 비교 대상
	bool bLocomotionApplied = false;
	bool bLastMoving = false;

	// 마지막으로 발소리를 판정한 몸 프레임 (걷지 않는 동안은 INDEX_NONE)
	int32 LastFootstepFrame = INDEX_NONE;
	EFieldFacing LastFacing = EFieldFacing::Down;

	// 켜지면 UpdateBillboard가 Yaw에 180도를 더해 뒷면을 보여줌(좌우 뒤집기)
	bool bHorizontalFlip = false;

	// 지금 스프라이트에 적용해 둔 세로 늘림 (1/cos(카메라 피치)). 값이 바뀔 때만 스케일을 다시 쓴다
	float AppliedBillboardStretch = 1.0f;

	bool bAutomaticBillboardUpdatesEnabled = false;
	float AutomaticBillboardUpdateInterval = 0.1f;

	// 설정하면 Settings.Color 대신 이 값을 사용
	TOptional<FLinearColor> ColorOverride;

	// 디졸브 진행값, ApplyColor 가 알파에 실어 보냄
	float DissolveAmount = 0.f;
	float Opacity = 1.f;

	// 레이어에 연결할 보조 렌더 컴포넌트와 대상 레이어 번호 (같은 인덱스끼리 짝)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UPaperFlipbookComponent>> LayerComponents;
	TArray<int32> LayerIndices;

	// 등록 시점의 상대 위치 --- 보정은 이 값을 기준으로 더한다
	TArray<float> LayerBaseX;
	TArray<float> LayerBaseZ;

	// 레이어별 바운스 배율 (신발 0, 그 외 1)
	TArray<float> LayerBounceScale;

	// 레이어별 미세조정 — 콘솔로 바꾼다 (기본 0)
	TArray<float> LayerTuneX;
	TArray<float> LayerTuneZ;
	TArray<int32> LayerBounceDelay;

	// 레이어별 마지막으로 적용한 Z — 안 바뀌면 SetRelativeLocation을 건너뛴다
	TArray<float> LayerAppliedZ;

	// 몸통 프레임별 보정값. 몸통 플립북이 바뀔 때만 다시 읽는다
	TWeakObjectPtr<const UPaperFlipbook> BounceFlipbook;
	TArray<float> BounceOffsets;

	// 현재 몸 줄(FNUBodyRow)의 뒷모습 파츠 보정 --- RefreshAppearanceSkins가 채운다
	float BackViewPartTuneZ = 0.0f;

	// 머리 색 --- hair_id에 같이 실려 온 색 번호를 RefreshAppearanceSkins가 풀어 둔다. 색은 머리 레이어의 정점 색으로 나간다
	bool bHairTinted = false;
	FLinearColor HairTint = FLinearColor::White;

	// 레이어 바운스를 쓰는지 --- 전투 그림은 끈다 (SetLayerBounceEnabled)
	bool bLayerBounceEnabled = true;

	// 마지막으로 넣은 쓰러짐, 애님 BP 를 갈면 다시 넣는다
	bool bAnimDead = false;

	// 걸을 때 낼 발소리 --- RefreshAppearanceSkins 가 성별·직업으로 고른다 (비어 있으면 기본 발소리)
	FGameplayTag FootstepSFX;
};
