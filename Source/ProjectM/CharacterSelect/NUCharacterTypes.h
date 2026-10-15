// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NUCharacterTypes.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogNUCharSelect, Log, All);

// 캐릭터 선택창에 표시되는 캐릭터 1명의 요약.
// 서버 연동 시 캐릭터 목록 응답(response_data JSON)의 한 항목이 이 구조로 들어온다.
USTRUCT(BlueprintType)
struct FNUCharacterSummary
{
	GENERATED_BODY()

	// 서버 캐릭터 식별자 (UserInfo.Idx와 같은 패턴). 더미 단계에서는 클라가 임시 발급
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int64 CharacterIdx = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	FString Name;

	// 직업 번호. spirit_id처럼 서버는 값 검증 없이 저장·전달만 하고, 해석은 클라 몫.
	// 직업 기획 미확정 — 당분간 더미값
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 JobId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 Level = 1;

	// 소지금 잔액 --- 서버가 목록 응답에 실어준다 (지급은 전투·퀘스트 보상, 사용처는 상점 예정).
	// 인벤 컴포넌트가 서버 인벤을 받을 때 이 값으로 자기 골드를 맞춘다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int64 Gold = 0;

	// 외형 --- 서버는 저장·에코만 하고 해석하지 않는다 (생성 때 클라가 정한 값 그대로)
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 RaceId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 Gender = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 HairId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 FaceId = 0;

	// 삭제 예약 중이면 true --- DeleteRemainSec(초)로 "삭제 예정 · N시간 남음"을 그린다.
	// 예약 중에는 선택이 1212로 거절되고, 슬롯은 계속 차지한다
	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	bool bDeletePending = false;

	UPROPERTY(BlueprintReadOnly, Category = "NU|Character")
	int32 DeleteRemainSec = 0;
};

// 남의 캐릭터 정보 조회(142) 응답 1건. C++ UI만 소비해서 USTRUCT가 아니다.
// 골드·스탯·인벤은 서버가 의도적으로 뺐다 (남에게 보이면 안 되는 값).
// 시야 밖 대상도 조회된다 (존 제한 없음) --- 파티 목록처럼 필드 밖 경로에서도 쓸 수 있다
struct FNUCharacterProfile
{
	int64 CharacterIdx = 0;
	FString Name;
	int32 Level = 1;
	int32 JobId = 0;

	// 낀 부위만 온다. 키 = 서버 부위 번호(NUEquip::To/FromServerEquipSlot 표 --- ENUEquipSlot 값과 다르다),
	// 값 = 아이템 표 NumericId. 아이콘·이름 해석은 클라 몫
	TMap<int32, int32> Equipment;
};
