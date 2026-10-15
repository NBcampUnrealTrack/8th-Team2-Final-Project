// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUFieldTypes.generated.h"

// 필드 계열이 공유하는 값 타입.

// 4방향 바라보는 방향, 서버 FieldFacing과 값이 1:1
UENUM(BlueprintType)
enum class EFieldFacing : uint8
{
	Down,
	Up,
	Left,
	Right
};

// 활동 상태, 서버 FieldActivity와 값이 1:1
UENUM(BlueprintType)
enum class EFieldActivity : uint8
{
	Idle = 0,
	Walk = 1,
	Battle = 2,
};

// 필드에 등장한 플레이어의 전체 상태, PlayerId는 따로 받고 상태만
USTRUCT()
struct FNUFieldAppearance
{
	GENERATED_BODY()

	FVector        Location = FVector::ZeroVector;
	EFieldFacing   Facing   = EFieldFacing::Down;
	EFieldActivity Activity = EFieldActivity::Idle;

	// Naia Table의 Data Index, 0 = 없음, 1001 부터 시작
	int32 SpiritId = 0;

	// 파티·거래·귓속말이 대상을 지목하는 키, 캐릭터 미선택이면 0
	int64 CharacterIdx = 0;

	// 미선택이면 빈 문자열
	FString CharacterName;

	// 외형 --- CharacterList의 값 그대로 (race_id, gender, hair_id, face_id). 미선택이면 전부 0.
	// 몸은 종족·성별로 몸 표에서, 파츠는 번호로 찾는다
	int32 RaceId = 0;
	int32 Gender = 0;
	int32 HairId = 0;
	int32 FaceId = 0;

	// 직업 (CharacterJobSelect로 저장된 값, 0 = 미선택). 외형 4종과 달리 필드에 있는 동안 바뀐다 ---
	// 누가 전직하면 서버가 그를 보는 사람들에게 Appeared를 한 번 더 보낸다 (가이드 9/14). 몸 표의 직업 줄로 몸통만 바뀐다
	int32 JobId = 0;

	// 서버가 확정한 가시성 공간, 원격 표시 정책은 이 값을 기준으로 확장한다.
	int32 AreaId = 0;
	int32 LocationId = 0;
	bool bIndoor = false;
};

namespace NUField
{
	// 캐릭터 idx는 10억부터 시작
	constexpr int64 CHARACTER_IDX_START = 1000000000;
}
