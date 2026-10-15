// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "UObject/SoftObjectPtr.h"
#include "NUBattleDataSettings.generated.h"


class UDataTable;
class UNiagaraSystem;

USTRUCT()
struct FNUNetActionMotionPreset
{
	GENERATED_BODY()

	FNUNetActionMotionPreset() = default;

	FNUNetActionMotionPreset(float InTelegraphTime, float InApproachTime, float InActionTime, float InReturnTime)
		: TelegraphTime(InTelegraphTime), ApproachTime(InApproachTime)
		, ActionTime(InActionTime), ReturnTime(InReturnTime)
	{
	}

	UPROPERTY(EditAnywhere)
	ENUBattleActionMotion Motion = ENUBattleActionMotion::MoveToCenter;

	UPROPERTY(EditAnywhere)
	float TelegraphTime = 0.f;

	UPROPERTY(EditAnywhere)
	float ApproachTime = 0.45f;

	UPROPERTY(EditAnywhere)
	float ActionTime = 0.6f;

	UPROPERTY(EditAnywhere)
	float ReturnTime = 0.25f;

	UPROPERTY(EditAnywhere)
	float HitTimeNormalized = 0.5f;

	UPROPERTY(EditAnywhere)
	float HitInterval = 0.3f; // 연속 타격 간격, NUPlayerBattleAbility.cpp의 기본값과 같음
};

// 직업 기본 공격 연출, 직업마다 공격 모션이 다름
// 행이 있으면 어빌리티 기본 연출을 통째로 덮음 - 빈 칸은 안 나감
// 이펙트가 뜨는 시각은 공격 모션의 Cast·Hit 표시가 정함
USTRUCT(BlueprintType)
struct FNUJobAttackVFX
{
	GENERATED_BODY()

	// 모션의 Cast 지점에서 시전자에게 뜸
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> CastVFX;

	// Cast 지점에서 출발해 Hit 지점에 대상에게 닿는 투사체
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> ProjectileVFX;

	// 탄환 도착 지점, 탄환이 없으면 쓰이지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	// 위 VFX들의 User Parameter 색상
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FLinearColor EffectColor = FLinearColor::White;
};
	// 갬블러 [운명 읽기] 카드 확률 한 줄, 흉은 나머지(100 - 대길 - 길)
	// 기획 [운명 읽기 확률표] 가 표 시트로 들어오기 전까지 설정에 둔다 — 표가 생기면 거기서 읽도록 옮길 것
	USTRUCT(BlueprintType)
	struct FNUGamblerCardOdds
	{
		GENERATED_BODY()

		UPROPERTY(EditAnywhere, BlueprintReadOnly)
		int32 Luck = 0;

		// 퍼센트
		UPROPERTY(EditAnywhere, BlueprintReadOnly)
		float GreatPercent = 0.f;

		UPROPERTY(EditAnywhere, BlueprintReadOnly)
		float GoodPercent = 0.f;
	};

// 남은 SP 를 전부 쓰고 그만큼 세지는 직업 스킬 한 줄 (3203 나이아 개화)
// 표가 이 동작을 칸으로 못 적어(효과 줄이 Special 하나) 설정에 둔다, 표에 칸이 생기면 거기서 읽도록 옮길 것
USTRUCT(BlueprintType)
struct FNUSpendAllSPSkill
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SkillId = 0;

	// 평시 계수표(쓴 SP)에 곱하는 배율, 3203 은 1.25
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CoefficientScale = 1.f;

	// 계수의 기준 스탯, 마법 공격력이면 마법 피해라 약점이 쌓임
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ENUStatusAffectedStat DamageStat = ENUStatusAffectedStat::MagicAttackPower;

	// 쓴 라운드에는 행동 뒤 SP 가 다시 차지 않음 (그 턴 방어 불가 — SP 가 없어 패링 · 회피를 못 함), 다음 명령 선택 때 참
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bNoRefillThisRound = true;
};


// 전투에 사용하는 DataTable를 관리함
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Battle Data"))
class PROJECTM_API UNUBattleDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	virtual FName GetCategoryName() const override
	{
		return TEXT("Game");
	}
	
	// 상태이상 정의
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> StatusEffectTable;
	
	// 액티브 스킬 정의
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> ActiveSkillTable;
	
	// 액티브 스킬이 부여하는 효과 정의
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> SkillEffectTable;
	
	// 패링 가능한 스킬의 타이밍, 없는 스킬은 회피
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> SkillParryTable;
	
	// 보스 전용 스킬 정의, 행 구조체는 액티브 스킬과 같음 (FNUActiveSkillDefinition)
	// 위 셋은 기획 액티브 시트에서 변환기가 통째로 다시 쓰므로 손으로 넣은 보스 행이 지워짐, 그래서 표를 따로 둠
	// 번호는 보스 NaiaId 뒤에 두 자리 (170101 = 1701 의 1번), 조회는 액티브 표가 먼저
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Boss")
	TSoftObjectPtr<UDataTable> BossSkillTable;
	
	// 보스 전용 스킬의 효과 줄, 행 구조체는 액티브 스킬 효과와 같음 (FNUActiveEffectRow)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Boss")
	TSoftObjectPtr<UDataTable> BossSkillEffectTable;
	
	// 보스 전용 스킬의 패링 타이밍, 없는 스킬은 회피 (FNUSkillParryDefinition)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Boss")
	TSoftObjectPtr<UDataTable> BossSkillParryTable;
	
	// 상태이상의 해제 아이템과 면역 패시브 관계들 정의
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> StatusRelationTable;
	
	// 패시브 정의
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> PassiveTable;

	// 패시브 효과 행 (FNUPassiveEffectRow), 패시브 하나가 여러 줄을 가질 수 있음
	// 정의표(PassiveTable)는 이름·등급만 주고 무엇을 하는지는 이쪽이 정한다
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> PassiveEffectTable;

	// 플레이어 직업 스킬 공동 정의
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> JobSkillDefinitionTable;

	// 직업 스킬 효과 행 (FNUJobSkillEffectRow), 스킬 하나가 여러 줄을 가짐
	// 정의표는 계수와 코스트만 주고 무엇을 하는지는 이쪽에서 정함
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> JobSkillEffectTable;

	// 직업별 자원 17종 (FNUJobResourceDefinition)
	// 효과표의 ResourceName · ValuePer 가 이 표의 KoreanName 으로 이어짐
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> JobResourceTable;

	// 갬블러 [운명 읽기] 카드 확률표 (FNUFateCardRateRow), 로컬 전투의 카드 뽑기가 읽음
	// 비어 있으면 아래 GamblerCardOdds 임시 값으로 돌아감
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> FateCardRateTable;

	// 스킬별 연출 이펙트 묶음, 기획 시트가 아니라 클라 전용 표, 없는 스킬은 기본 프로필로 돌아감
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data")
	TSoftObjectPtr<UDataTable> SkillVFXTable;
	
	// 적 패턴 - NaiaId 를 패턴에 잇는 매핑, 여기 없는 몬스터는 기본 공격만 함
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Enemy Pattern")
	TSoftObjectPtr<UDataTable> EnemyPatternMapTable;
	
	// 적 패턴 - 규칙
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Enemy Pattern")
	TSoftObjectPtr<UDataTable> EnemyPatternRuleTable;
	
	// 적 패턴 - 규칙과 페이즈가 같이 쓰는 조건, OwnerType 으로 가름
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Enemy Pattern")
	TSoftObjectPtr<UDataTable> EnemyPatternConditionTable;
	
	// 적 패턴 - 보스 페이즈, 지금은 비어 있음
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Enemy Pattern")
	TSoftObjectPtr<UDataTable> EnemyPatternPhaseTable;


	// 속성별 이펙트 색, DT_SkillVFX 의 bUseElementColor 가 true 인 스킬이 씀
	// Niagara User Parameter 로 그대로 주입되는 HDR 값 (1 이상이면 이미시브 발광)
	// 추후에 DataAsset으로 빼야할듯 함
	
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	FLinearColor FireElementColor = FLinearColor(5.f, 0.5f, 0.3f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	FLinearColor WaterElementColor = FLinearColor(0.3f, 1.f, 5.f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	FLinearColor GrassElementColor = FLinearColor(0.3f, 5.f, 0.5f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	FLinearColor LightningElementColor = FLinearColor(5.f, 5.f, 0.3f);

	// 채도가 낮아 다른 속성보다 세기를 낮게 잡음
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	FLinearColor EarthElementColor = FLinearColor(3.f, 1.5f, 0.5f);

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	FLinearColor WindElementColor = FLinearColor(0.5f, 3.f, 5.f);

	// 속성 태그로 위 색 중 하나를 고름, 무속성이거나 등록되지 않은 속성이면 흰색
	FLinearColor ResolveElementColor(const FGameplayTag& ElementTag) const;


	// 직업별 기본 공격 연출 (JobId -> 연출)
	// DT_SkillVFX 행이 없는 공격(기본 공격 · 행이 없는 직업 스킬)에 플레이어 전투 아바타가 씀
	// 여기 없는 직업은 어빌리티 기본 연출(검기) 그대로, 나이아 · 적은 아래 NaiaElementAttackVFX
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TMap<int32, FNUJobAttackVFX> JobAttackVFX;

	// 나이아 기본 공격 연출 (종의 속성 태그 -> 연출), 아군 · 적 나이아 모두
	// DT_SkillVFX 행이 없는 공격(기본 공격)에 나이아 아바타가 씀, 속성별 작은 투사체와 착탄
	// 여기 없는 속성 · 무속성은 어빌리티 기본 연출(검기) 그대로
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TMap<FGameplayTag, FNUJobAttackVFX> NaiaElementAttackVFX;

	// 갬블러 카드 확률표, LUK 가 낮은 줄부터 적고 줄 사이는 선형으로 이음 (로컬 전투의 카드 뽑기)
	// 비면 길만 나옴, 넷 전투는 서버가 뽑아서 안 씀
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Job")
	TArray<FNUGamblerCardOdds> GamblerCardOdds;

	// 남은 SP 를 전부 쓰는 직업 스킬 (3203 나이아 개화), 로컬 전투가 읽음 — 넷 전투는 서버가 정함
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Job")
	TArray<FNUSpendAllSPSkill> SpendAllSPSkills;

	// 평시 계수표, 계수(%) = Base x SP ^ Exponent
	// 직업의 단일 타격 스킬이 모두 이 식을 따름 (SP 24 -> 170% · 40 -> 320% · 55 -> 479%), 기획 표에 식이 없어 설정에 둠
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Job")
	float SkillCoefficientCurveBase = 3.2f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Job")
	float SkillCoefficientCurveExponent = 1.25f;


	// 포획 흔들림이 끝난 뒤 대상 위에 뜨는 이펙트
	// 아바타 BP 가 셋이라 그쪽에 두면 세 번 지정해야 해서 설정에 둠
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TSoftObjectPtr<UNiagaraSystem> CaptureSuccessVFX;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TSoftObjectPtr<UNiagaraSystem> CaptureFailVFX;

	// 스킬과 상관없이 전투마다 나오는 VFX (피격 · 회복 · SP 회복 · 사망 큐가 띄우는 것)
	// 게임을 시작할 때 비동기로 불러 화면 밖에서 한 번 띄워 둠 (UNUBattleVFXPreloadSubsystem), 첫 피격 · 첫 회복의 끊김을 없앰
	// 큐 블루프린트(GCN_*)의 이펙트를 바꾸면 여기도 같이 바꿀 것
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TArray<TSoftObjectPtr<UNiagaraSystem>> CommonBattleVFX;

	// 자기 임팩트가 없는 공격(기본 공격 · 임팩트 칸이 빈 스킬 · 나이아 · 적)이 대상에게 띄우는 대체 임팩트
	// 피격 큐(GCN_HitEffect)는 모든 타격에 작은 신호만 내고, 화려한 타격감은 행동 임팩트가 맡음 (직업 스킬 임팩트가 묻히지 않게)
	// 적용 규칙은 NUBattlePresentation::ApplyFallbackImpact
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TSoftObjectPtr<UNiagaraSystem> FallbackImpactVFX;

	// 대체 임팩트를 타격 시각에 대상에게 실어 나르는 보이지 않는 투사체 (임팩트는 투사체가 도착할 때만 뜸)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TSoftObjectPtr<UNiagaraSystem> FallbackProjectileVFX;


	// 적이 죽을 때 스프라이트를 가루로 만드는 디졸브 머티리얼 (M_NUSprite_Dissolve)
	// 비워두면 디졸브 없이 예전처럼 곧바로 사라짐
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|VFX")
	TSoftObjectPtr<UMaterialInterface> SpriteDissolveMaterial;


	// --------------------------------
	//   넷 전투 아바타 (서버 권위 전투)
	// --------------------------------
	// 서버 전투는 ANUBattleEncounter를 거치지 않아 그쪽의 BattleAvatarClass를 못 읽음, 
	// 그래서 여기 따로 둠 - 로컬 값과는 별개
	// 적도 ANUBattleEnemyAvatar가 아니라 평범한 ANUBattleAvatar를 사용함, 스탯을 서버가 들고 있어 ASC가 필요 없음

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Net Avatar")
	TSoftClassPtr<class ANUBattleAvatar> NetPlayerAvatarClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Net Avatar")
	TSoftClassPtr<ANUBattleAvatar> NetSpiritAvatarClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Net Avatar")
	TSoftClassPtr<ANUBattleAvatar> NetEnemyAvatarClass;

	// 로컬 ANUBattleEncounter::FadeOutSequence/FadeInSequence와 같은 역할, 인카운터 블루프린트가 없어 여기 둠
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Net Avatar")
	TSoftObjectPtr<class ULevelSequence> NetFadeOutSequence;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Net Avatar")
	TSoftObjectPtr<ULevelSequence> NetFadeInSequence;

	// 스킬별 타이밍이 필요해지면 skill_id를 키로 하는 클라 전용 CSV를 따로 둬야 함
	// 현재 모션 타이밍은 기획 시트가 아니라 GAS 어빌리티 CDO에만 있어 아군/적 프리셋만 사용함
	UPROPERTY(Config, EditAnywhere, Category = "Battle Data|Net Action")
	FNUNetActionMotionPreset NetAllyActionMotion;

	UPROPERTY(Config, EditAnywhere, Category = "Battle Data|Net Action")
	FNUNetActionMotionPreset NetEnemyActionMotion = FNUNetActionMotionPreset(0.3f, 0.4f, 0.8f, 0.3f);

	// DT_SkillVFX에 행이 없는 행동(기본 공격 등)의 연출 기본값을 읽어올 어빌리티
	// 로컬은 발동한 어빌리티 CDO에서 직접 읽는데 넷 전투엔 어빌리티가 없어 같은 에셋을 여기서 가리킴
	// 기본 공격의 검기 VFX와 공격 애니메이션이 여기서 나옴, 비워두면 넷 전투 기본 공격에 연출이 없음
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Battle Data|Net Action")
	TSoftClassPtr<class UNUBattleGameplayAbility> NetDefaultActionAbility;

	// 넷 전투 메뉴에 올릴 스킬, 서버가 실행을 붙인 것만 적음
	// 비어 있으면 거르지 않음 (보유 목록 그대로)
	UPROPERTY(Config, EditAnywhere, Category = "Battle Data|Net Action")
	TArray<int32> NetOfferedSkillIds;
};
