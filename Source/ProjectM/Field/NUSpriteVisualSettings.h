// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUSpriteVisualSettings.generated.h"

class UMaterialInterface;
class UPaperZDAnimInstance;
class UPaperZDAnimSequence;

// Paper2D 스프라이트 표현 설정.

// 스프라이트를 카메라로 돌리는 방식
UENUM(BlueprintType)
enum class ENUBillboardMode : uint8
{
	// 카메라 평면과 평행하게 회전, 화면에서 보이는 기울기가 위치와 무관하게 일정함
	// 필드 계열이 사용
	CameraYaw,

	// 액터에서 카메라를 향하도록 회전함, 액터마다 각도가 달라짐
	// 전투 아바타가 사용, 진형이 좌우로 넓게 퍼져도 각자 카메라를 바라봄
	LookAtCamera
};

USTRUCT(BlueprintType)
struct FNUSpriteVisualSettings
{
	GENERATED_BODY()

	// 스프라이트 머티리얼, null 이면 스프라이트 머티리얼 그대로 사용
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	TSoftObjectPtr<UMaterialInterface> Material;

	// PaperZD 애님 블루프린트, null이면 액터가 직접 지정 (전투 아바타)
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	TSoftClassPtr<UPaperZDAnimInstance> AnimBlueprint;

	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	ENUBillboardMode BillboardMode = ENUBillboardMode::CameraYaw;

	// 빌보드 회전에 더하는 각도
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	float BillboardYawOffset = 90.0f;

	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	FVector RelativeScale = FVector(1.0f);

	// 스프라이트의 상대 Z
	// 플립북 아트의 피벗이 그림 맨 아래가 아닐 수 있어서 보정값으로 사용
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	float SpritePivotZ = 0.0f;

	// 기본 색, 톤다운 같은 일시적 변경은 SetColorOverride로 덮어씀
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	FLinearColor Color = FLinearColor::White;

	// 필드에선 그림자 on, 나머진 상황보고
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	bool bCastShadow = false;

	// 발밑 블롭 그림자 반경(cm). 0이면 블롭을 만들지 않는다.
	// 스프라이트는 카메라를 향하는 빌보드라, 태양이 옆에서 오면 판이 빛과 나란해져
	// 실제 그림자가 거의 생기지 않는다. 정면광이면 그림자가 캐스터 뒤로 숨고
	// 역광이면 얼굴이 죽는다 — 셋을 동시에 만족하는 태양 각도는 없다.
	// 그래서 HD-2D 게임들처럼 접지감을 주는 블롭 데칼을 따로 깐다.
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	float BlobShadowRadius = 0.0f;

	// 부모가 회전해도 빌보드 회전이 유지되게 함
	UPROPERTY(VisibleAnywhere, Category = "NU|Sprite")
	bool bUseAbsoluteRotation = true;
};

// 종류별 기본값, 액터 생성자에서 호출
// SpriteVisual->Setup(Sprite, Animation, NUSpriteVisuals::MakeFieldCharacter()); 와 같은 형태
// 값을 바꾸기 위해 해당 값을 바꾸면 사용하는 BP/C++ 클래스들은 전부 적용됨
// 몸 위에 겹쳐 그리는 레이어 하나의 정의.
//
// 레이어 번호는 애님 소스(AS_Player)의 Additional Layer Data 순서 + 1이다(0은 본체).
// 이 값이 액터마다 따로 적혀 있으면 한쪽만 고쳐져 어긋난다 --- 실제로 필드 캐릭터와
// 정보창 미리보기가 같은 숫자를 각자 적고 있었다. 정의는 여기 한 곳에만 둔다.
struct FNUSpriteLayerDef
{
	// 애님 레이어 번호
	int32 Layer = 0;

	// 몸통 프레임의 세로 흔들림을 얼마나 따라갈지 (1=그대로, 0=고정).
	// 다리·신발은 땅에 붙어 있어 0이다
	float BounceScale = 1.0f;

	// 몸통보다 몇 프레임 늦게 따라갈지 (팔처럼 박자가 다른 부위용)
	int32 FrameDelay = 0;
};

// 레이어 번호. 늘릴 때는 AS_Player의 Additional Layer Data도 같이 늘려야 한다
namespace NUSpriteLayer
{
	// 장비 --- 갑옷·장갑·신발·하의, 그리고 팔보다 앞에 오는 어깨
	constexpr int32 EquipChest = 1;
	constexpr int32 EquipGloves = 2;
	constexpr int32 EquipBoots = 3;
	constexpr int32 EquipLegs = 4;
	constexpr int32 EquipShoulder = 5;

	// 커스터마이징 --- 생성창에서 고른 머리·외모
	constexpr int32 Hair = 6;
	constexpr int32 Face = 7;
}

namespace NUSpriteVisuals
{
	// 캐릭터가 쓰는 레이어 전부 (장비 5 + 커마 2). 액터는 이 순서대로 컴포넌트를 등록한다.
	// 등록만 하면 되고 무엇을 그릴지는 RefreshEquipmentSkins/RefreshAppearanceSkins가 정한다
	PROJECTM_API const TArray<FNUSpriteLayerDef>& GetCharacterLayers();

	// 스프라이트 NPC
	// 실제 애니메이션 세트는 Blueprint에서 지정
	PROJECTM_API FNUSpriteVisualSettings MakeNPC();

	// 로컬/원격 필드 캐릭터
	PROJECTM_API FNUSpriteVisualSettings MakeFieldCharacter();

	// 로컬/원격 필드 정령
	PROJECTM_API FNUSpriteVisualSettings MakeFieldSpirit();

	// 필드 적 심볼
	PROJECTM_API FNUSpriteVisualSettings MakeFieldEnemy();

	// 전투 아바타
	PROJECTM_API FNUSpriteVisualSettings MakeBattleAvatar();
	
	// 전투 그림을 입은 캐릭터의 애님 BP, 몸 줄에 전투 스킨이 있을 때만 사용함
	PROJECTM_API TSoftClassPtr<UPaperZDAnimInstance> GetCombatCharacterAnimBlueprint();
	
	// 그 애님 BP 의 공격 시퀀스 --- 어빌리티에 물린 옛 필드 시퀀스 대신 캐릭터가 재생함
	PROJECTM_API TSoftObjectPtr<UPaperZDAnimSequence> GetCombatCharacterAttackSequence();
	
	// 나중에 NPC 추가하면 위 설정중 하나를 그대로 사용하거나 새로 설정하면 됨
}
