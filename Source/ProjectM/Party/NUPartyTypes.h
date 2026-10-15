// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUPartyTypes.generated.h"

// 파티 목록이 변경되는 이유, 서버 PartyUpdateReason과 값이 1:1
UENUM(BlueprintType)
enum class ENUPartyUpdateReason : uint8
{
	None			= 0,
	Joined			= 1,	// 가입
	Left			= 2,	// 탈퇴, 캐릭터 삭제
	Kicked			= 3,	// 추방
	LeaderChanged	= 4,	// 파티장 변경
	Disbanded		= 5		// 1명만 남아 자동 해산
};

// 파티 멤버 구조체
USTRUCT(BlueprintType)
struct FNUPartyMember
{
	GENERATED_BODY()

	// character_idx, 파티 요청의 키로 사용함, player_id가 아님
	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	int64 Idx = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	FString Name;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	bool bIsLeader = false;
};

// 파티 정보, 서버가 주는 목록을 통째로 갈아끼우는 방식으로 사용함
USTRUCT(BlueprintType)
struct FNUPartySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	int64 PartyId = 0;	// 0이면 파티 없음

	// 리더의 character_idx
	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	int64 LeaderIdx = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	int32 MaxCount = 4;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Party")
	TArray<FNUPartyMember> Members;

	bool IsExistParty() const { return PartyId != 0; }
	bool IsPartyLeader(int64 Idx) const { return PartyId != 0 && LeaderIdx == Idx; }

	// 이름 표시용, 없으면 빈 문자열
	const FString* FindMemberName(int64 Idx) const;
};

namespace NUParty
{
	// 서버 ResultCode
	constexpr int32 RC_SUCCESS = 200;
	
	// 파티원 목록 JSON을 읽음. 실패하면 false, Out은 건드리지 않음.
	// PartyMemberList 응답 / PartyRespond 수락 응답 / SVR_Party_Updated 에서 사용
	// 목록이 비어서 오면 false 리턴
	PROJECTM_API bool ParseMemberList(const FString& ResponseData, FNUPartySnapshot& Out);

	// 서버 ResultCode를 사용자에게 보일 문구로 변환
	PROJECTM_API FText GetResultMessage(int32 ResultCode);

	// 파티 구성 변경 알림 문구
	PROJECTM_API FText GetUpdateMessage(ENUPartyUpdateReason Reason, const FString& TargetName);
}
