#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Naia/Data/NUNaiaTypes.h"
#include "NUNaiaDefinition.generated.h"

class UPaperFlipbook;
class UPaperZDAnimInstance;
class UPaperZDAnimSequence;
class UTexture2D;


USTRUCT(BlueprintType)
struct PROJECTM_API FNUNaiaDefinition : public FTableRowBase
{
    GENERATED_BODY()

    // --------------------
    // 식별(Identity)
    // --------------------

    // 나이아 ID
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Identity")
    int32 DataIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Identity")
    FName DisplayNameKey = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Identity")
    FText KoreanName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Identity")
    FText EnglishName;

    // --------------------
    // 도감 (Profile)
    // --------------------
    
    // 설명
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Profile")
    FText Description;

    // 외형
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Profile")
    FText AppearanceDescription;

    // 특징
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Profile")
    FText FeatureDescription;

    // 키
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Profile")
    float Height = 0.0f;

    // 몸무게
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Profile")
    float Weight = 0.0f;

    // --------------------
    // 분류 (Classification)
    // --------------------

    // 분류
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Classification")
    ENUNaiaCategory Category = ENUNaiaCategory::Unknown;

    // 속성
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Classification")
    FGameplayTag ElementTag;

    // 이중 속성 나이아의 두 번째 속성, 없으면 None
    // 속성 계산은 아직 ElementTag만 사용하고, 규칙 확정 후 함께 반영할 예정
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Classification")
    FGameplayTag SecondaryElementTag;

    // 등급
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Classification")
    ENUNaiaGrade Grade = ENUNaiaGrade::Unknown;

    // 진화 단계
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Evolution")
    ENUNaiaEvolutionStage EvolutionStage = ENUNaiaEvolutionStage::None;

    // 진화 충족 레벨
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Evolution")
    int32 EvolutionLevel = 0;

    // 다음 진화체 Id
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Evolution")
    int32 NextEvolutionId = 0;

    // 포획 가능 여부
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Capture")
    bool bCapturable = false;

    // 최소 스폰 레벨
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Spawn")
    int32 SpawnLevel = 1;

    // 최대 스폰 레벨
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Spawn")
    int32 MaxSpawnLevel = 100;

    // --------------------
    // 기본 스탯 (Base Stats)
    // --------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float BaseHealth = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float BaseSP = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float BaseAttackPower = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float BaseMagicAttackPower = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float BaseDefense = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float BaseSpeed = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float CriticalRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Stats")
    float CriticalDamage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Progression")
    int32 BaseRequiredExperience = 0;

    // --------------------
    // 성장 (Growth / Level)
    // --------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Growth")
    float HealthGrowth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Growth")
    float SPGrowth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Growth")
    float AttackGrowth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Growth")
    float MagicAttackGrowth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Growth")
    float DefenseGrowth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Growth")
    float SpeedGrowth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Progression")
    int32 RequiredExperienceGrowth = 0;

    // --------------------
    // 보상 (Reward)
    // --------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Reward")
    int32 Experience = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Reward")
    int32 Gold = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Reward")
    FName DropItem1Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Reward")
    FName DropItem2Id = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Reward")
    float DropRate = 0.0f;

    // --------------------
    // 스킬 (Skill)
    // --------------------

    // 패시브는 두 값 중 확률로 하나를 뽑음, 하나만 있으면 확률 1.0
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Skill")
    int32 PassiveSkill1Id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Skill")
    float PassiveSkill1Chance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Skill")
    int32 PassiveSkill2Id = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Skill")
    float PassiveSkill2Chance = 0.0f;
    
    // 액티브 스킬은 스킬 테이블이 속성으로 가짐

    // --------------------
    // 리소스 (Resource)
    // --------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Resource")
    TSoftObjectPtr<UTexture2D> IconTexture;

    // 적으로 등장, 아군으로 사용할때 공용 스프라이트를 사용함
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Resource")
    TSoftClassPtr<UPaperZDAnimInstance> AnimationClass;

    // UI 초상화용 플립북 (편성창 파티 카드 등)
    // 정지 아이콘이 필요하면 Flipbook의 첫 프레임 사용할 수 있음
    // 별도 작은 아이콘이 필요할때 IconTexture 사용하면 됨
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Resource")
    TSoftObjectPtr<UPaperFlipbook> PreviewFlipbook;

    // 전투에서 피격될 때 한 번 재생할 시퀀스. 비우면 밀려남·점등만 보인다
    // AnimationClass 와 같은 애님 소스여야 재생됨
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Resource")
    TSoftObjectPtr<UPaperZDAnimSequence> HitAnimation;

    // 전투에서 앞으로 나가는 행동(공격 · 피해 스킬)에 재생할 시퀀스, 비우면 어빌리티에 물린 시퀀스를 따라감
    // AnimationClass 와 같은 애님 소스여야 재생됨, 타격 · 시전 시각은 시퀀스에 찍은 NU Battle Motion Marker 가 정함
    // 적 · 아군 나이아가 같이 씀
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Resource")
    TSoftObjectPtr<UPaperZDAnimSequence> AttackAnimation;

    // 전투에서 앞으로 나가지 않고 제자리에서 공격함 (보스처럼 큰 나이아)
    // 접근 · 복귀 시간은 제자리에서 그대로 보내므로 피해 · 반응 창 시각은 다른 나이아와 같음
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|Naia|Resource")
    bool bAttackInPlace = false;

    
    // 기본 스탯을 반영할 기준 레벨, 현재는 기획서상 10레벨이 기준값
    static constexpr int32 BaseStatLevel = 10;

	bool IsStarterSelectionCandidate() const { return DataIndex > 0 && Category == ENUNaiaCategory::Starter && EvolutionStage == ENUNaiaEvolutionStage::Stage1; }
    
    // AffectionTier 는 누적 포인트가 아니라 단계, NUNaiaAffection::ResolveTier 로 바꿔 넘길 것
    // 적처럼 애정도가 없는 대상은 0 을 넘김
    // 기본값을 두지 않은 건 빠뜨렸을 때 0이 되는 쪽보다 컴파일이 막히는 쪽이 낫기 때문
    FNUNaiaStats ResolveStats(int32 Level, int32 AffectionTier) const;
    int32 GetRequiredExperienceForNextLevel(int32 Level) const;
    int32 GetMaxSpawnLevel() const;
};
