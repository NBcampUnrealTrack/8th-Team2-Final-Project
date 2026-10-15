// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUNaiaServerTypes.generated.h"


struct FGrpcRevGameNaiaSlot;

// 서버의 NaiaUpdateReason 번호
UENUM(BlueprintType)
enum class ENUNaiaUpdateReason : uint8
{
	None	  = 0,
	Move	  = 1,
	Acquired  = 2,
	BattleEnd = 3,
	StarterContract = 4,
	Healed = 5,
	Count
};

// 서버가 보내준 나이아 개체 하나 정보
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerNaiaInstance
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int64 InstanceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int32 NaiaId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int32 Level = 0;

	// 0.0~1.0
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	float HealthRatio = 0.f;

	// 지금 레벨 안에서 모은 경험치 (proto NaiaInstance.exp)
	// 필드는 왔지만 서버가 아직 값을 올리지는 않아서 당분간 0 으로 들어옴
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int32 Experience = 0;

	// 누적 포인트, 0.1포인트 단위 정수
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int32 Affection = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int32 PassiveSkillId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	TArray<int32> ActiveSkillIds;

	bool IsValid() const
	{
		return NaiaId > 0;
	}
};

// 서버가 보내준 칸 하나 (Party=1 / Storage=2), instance가 없으면 빈 칸
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerNaiaSlot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int32 Container = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	int32 SlotNo = INDEX_NONE;

	// 빈 칸이면 IsValid() == false
	UPROPERTY(BlueprintReadOnly, Category = "NU|Naia|Server")
	FNUServerNaiaInstance Instance;

	bool IsEmpty() const
	{
		return !Instance.IsValid();
	}
};

// 서버 NaiaMove 패킷과 대응하는 전송용 구조체
// 나이아 위치 교체 요청
USTRUCT(BlueprintType)
struct PROJECTM_API FNUServerNaiaMoveRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "NU|Naia|Server")
	int32 FromContainer = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Naia|Server")
	int32 FromSlot = INDEX_NONE;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Naia|Server")
	int32 ToContainer = 0;

	UPROPERTY(BlueprintReadWrite, Category = "NU|Naia|Server")
	int32 ToSlot = INDEX_NONE;

	// Container : 파티=1, 보관함=2, Slot은 0번부터 유효
	bool IsValid() const
	{
		return FromContainer > 0 && ToContainer > 0 && FromSlot >= 0 && ToSlot >= 0;
	}
};

namespace NUNaiaServer
{
	// TurboLink에서 받은 칸 하나를 프로젝트 전용 값 구조체로 복사함
	// instance 는 단일 메시지라 TSharedPtr 이 아니라 값으로 오므로 IsValid() 검사가 필요 없음
	PROJECTM_API bool ConvertNaiaSlot(const FGrpcRevGameNaiaSlot& Source, FNUServerNaiaSlot& OutSlot, FString& OutError);

	// gRPC result code 를 사용자에게 보여줄 메시지로 변환
	PROJECTM_API FText GetResultMessage(int32 ResultCode);
}
