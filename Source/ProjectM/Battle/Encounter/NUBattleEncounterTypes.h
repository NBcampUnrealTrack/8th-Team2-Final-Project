#pragma once

#include "CoreMinimal.h"
#include "NUBattleEncounterTypes.generated.h"

class AActor;
class ANUPlayerState;
class ANUBattleEncounter;
class UWorld;
class UNUBattleDefinition;

// 전투에 출현할 액터의 정보
USTRUCT(BlueprintType)
struct PROJECTM_API FNUEnemySpawnSpec
{
    GENERATED_BODY()

    // DT_EnemyDefinitions의 RowName
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Battle")
    FName EnemyId = NAME_None;

    bool IsValid() const
    {
        return !EnemyId.IsNone();
    }
};

UENUM(BlueprintType)
enum class ENUBattleEncounterSource : uint8
{
    Symbol,
    Random
};

// 필드 전투 시작 요청 처리 결과
UENUM(BlueprintType)
enum class ENUBattleEncounterStartResult : uint8
{
    Started,                // Encounter 초기화 성공

    NotAuthority,           // 서버 권한 X
    InvalidRequest,         // enemy 유효 정보 X
    InitiatorMismatch,      // 요청 보낸 플레이어와 Request의 Initiator 불일치
    AlreadyInBattle,        // 플레이어가 다른 전투 참가 중
    NoValidParticipants,    // 유효하지 않은 전투 참가자 
    EncounterClassMissing,  // EncounterClass 설정 X
    SpawnFailed,            // Encouter Actor 생성 실패
    InitializeFailed        // InitializeEncounter() 실패
};

// 필드 에서 배틀 시스템에 전투 시작 요청.
USTRUCT(BlueprintType)
struct PROJECTM_API FNUBattleEncounterRequest
{
    GENERATED_BODY()

    // 전투를 발생시킨 플레이어
    UPROPERTY(BlueprintReadWrite, Category = "NU|Battle")
    TObjectPtr<ANUPlayerState> Initiator = nullptr;

    // Enemy 목록
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Battle")
    TArray<FNUEnemySpawnSpec> Enemies;

    // 요청으로 생성할 BattleEncounter 클래스
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Battle")
    TSoftClassPtr<ANUBattleEncounter> EncounterClass;

    // 이 전투가 쓸 배경 서브레벨, 비어 있으면 배경을 바꾸지 않음
    // 인카운터 존과 심볼 인카운터가 각각 자기 것을 넘겨줌
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Battle")
    TSoftObjectPtr<UWorld> BattleStageLevel;

    // 몬스터 인카운터 방식
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Battle")
    ENUBattleEncounterSource Source = ENUBattleEncounterSource::Random;

    // 심볼 인카운터 -> AFieldEnemy, 어떤 몬스터가 전투를 발생시켰는지 필요
    // 랜덤 인카운터 -> nullptr.
    UPROPERTY(BlueprintReadWrite, Category = "NU|Battle")
    TObjectPtr<AActor> SourceActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NU|Battle")
    TObjectPtr<UNUBattleDefinition> BattleDefinition = nullptr;

    bool IsValid() const;
};

