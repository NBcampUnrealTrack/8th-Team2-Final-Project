#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "NUNaiaTypes.generated.h"

class UNUNaiaRosterComponent;

UENUM(BlueprintType)
enum class ENUNaiaCategory : uint8
{
    Unknown     UMETA(DisplayName = "미지정"),
    Starter     UMETA(DisplayName = "스타팅"),
    Wild        UMETA(DisplayName = "야생"),
    Boss        UMETA(DisplayName = "보스 나이아"),
};

UENUM(BlueprintType)
enum class ENUNaiaGrade : uint8
{
    Unknown     UMETA(DisplayName = "미지정"),
    Normal      UMETA(DisplayName = "일반"),
    Rare        UMETA(DisplayName = "희귀"),
    Legendary   UMETA(DisplayName = "전설"),
};

UENUM(BlueprintType)
enum class ENUNaiaEvolutionStage : uint8
{
    None        UMETA(DisplayName = "없음"),
    Stage1      UMETA(DisplayName = "1단계"),
    Stage2      UMETA(DisplayName = "2단계"),
    Stage3      UMETA(DisplayName = "3단계"),
};

USTRUCT(BlueprintType)
struct PROJECTM_API FNUNaiaStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float MaxHealth = 1.f;

    UPROPERTY(BlueprintReadOnly)
    float MaxSP = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float AttackPower = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float MagicAttackPower = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Defense = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float Speed = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float CriticalRate = 0.f;

    UPROPERTY(BlueprintReadOnly)
    float CriticalDamage = 1.f;
};

// --------------------
//    나이아 보관 관련 
// --------------------

namespace NUNaiaContainer
{
    constexpr int32 Party = 1;      // 대기목록, 0번 슬롯이 전투에 참가
    constexpr int32 Storage = 2;    // 보관함
}

// 나이아 한 마리의 기록
// 서버는 파티·보관함을 가리지 않고 이 모양으로 보내고, 어디 놓였는지는 Container 가 가름
// 클라에선 파티는 SpiritState, 보관함은 구조체를 사용해서 다름
USTRUCT(BlueprintType)
struct FNUNaiaInstance : public FFastArraySerializerItem
{
    GENERATED_BODY()
    
    // 개체 식별자, 같은 나이아 여러 마리 보유할수도 있음
    // 서버 발급분은 양수, 서버 로스터를 아직 못 받은 로컬 생성분은 음수 (겹치지 않게 구분)
    UPROPERTY(BlueprintReadOnly)
    int64 InstanceId = 0;
    
    // 나이아 ID
    UPROPERTY(BlueprintReadOnly)
    int32 NaiaId = 0;
    
    // 레벨
    UPROPERTY(BlueprintReadOnly)
    int32 Level = 0;
    
    // 지금 레벨 안에서 모은 경험치, 누적 총량 X
    // 나이아별 Base경험치, 성장 경험치가 달라 레벨 내 경험치로 저장해야 함
    // 서버가 분배해줌, 전투에 나온 파티 0번이 100%, 파티 1~3번이 50%, 보관함은 못 받음
    UPROPERTY(BlueprintReadOnly)
    int32 Experience = 0;
    
    // 보관함 공간
    UPROPERTY(BlueprintReadOnly)
    int32 SlotNo = INDEX_NONE;
    
    // 체력 비율
    // 절대값을 저장하지 않는 이유는 최대치가 패시브·장비·레벨로 변경되는데
    // 보관함에는 ASC 가 없어 그 값을 알 수 없기 때문
    UPROPERTY(BlueprintReadOnly)
    float HealthRatio = 1.f;
    
    // 애정도 누적 포인트
    // 0.1 포인트 단위 정수, 1253 = 125.3 포인트
    UPROPERTY(BlueprintReadOnly)
    int32 Affection = 0;

    // 포획할 때 굴려서 영구 고정된 패시브, 0이면 없음
    // 후보와 확률은 정의에 있고 그중 하나를 골라 여기 저장함
    UPROPERTY(BlueprintReadOnly)
    int32 PassiveSkillId = 0;
    
    // 해당 나이아가 배운 액티브 스킬
    // 서버가 저장하는 값이라 개체 데이터에 둠
    UPROPERTY(BlueprintReadOnly)
    TArray<int32> ActiveSkillIds;

    bool IsValid() const
    {
        return NaiaId > 0;
    }
};

// 소유자 전용(COND_OwnerOnly)으로 복제되는 나이아 리스트 (FastArray 델타 직렬화)
USTRUCT()
struct FNUNaiaList : public FFastArraySerializer
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FNUNaiaInstance> Naias;

    // 수신 콜백을 컴포넌트로 전달하는 백포인터 (컴포넌트 생성자에서 세팅, 복제 대상 아님)
    UPROPERTY(NotReplicated)
    TObjectPtr<UNUNaiaRosterComponent> OwnerComponent;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FastArrayDeltaSerialize<FNUNaiaInstance, FNUNaiaList>(Naias, DeltaParms, *this);
    }
    
    // 항목별 콜백은 같은 배치의 다른 변경이 미적용 상태일 수 있어 로그 전용.
    // 외부 알림은 배치당 1회 불리는 PostReplicatedReceive에서만 한다.
    void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
    void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
    void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
    void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& Parameters);
};

template<>
struct TStructOpsTypeTraits<FNUNaiaList> : public TStructOpsTypeTraitsBase2<FNUNaiaList>
{
    enum
    {
        WithNetDeltaSerializer = true
    };
};
