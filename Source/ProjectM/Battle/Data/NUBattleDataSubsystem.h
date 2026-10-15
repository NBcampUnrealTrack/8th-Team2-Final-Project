// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Battle/Enemy/NUEnemyPatternTypes.h"
#include "Battle/Data/NUPassiveDefinition.h"
#include "Battle/Data/NUJobResourceDefinition.h"
#include "Battle/Data/NUFateCardRate.h"
#include "NUBattleDataSubsystem.generated.h"

struct FNUSkillParryDefinition;
struct FNUActiveEffectRow;
struct FNUActiveSkillDefinition;
struct FNUBattleSkillInfo;
struct FNUStatusEffectDefinition;
struct FNUStatusRelationRow;
struct FNUPassiveDefinition;
struct FNUJobSkillDefinition;
struct FNUJobSkillEffectRow;
struct FNUSkillVFXProfile;
class UDataTable;

UCLASS()
class PROJECTM_API UNUBattleDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// StatusEffectId = DT_StatusEffect RowName
	const FNUStatusEffectDefinition* FindStatusEffectDefinition(int32 StatusEffectId) const;
	
	// SkillId = DT_ActiveSkill RowName
	const FNUActiveSkillDefinition* FindActiveSkillDefinition(int32 SkillId) const;

	// 나이아 액티브/직업 액티브/필살기의 표시 정보, 패시브와 없는 ID는 false.
	// 재임포트된 정의를 바로 읽으며 실패하면 OutInfo를 초기화한다.
	bool TryGetBattleSkillInfo(int32 SkillId, FNUBattleSkillInfo& OutInfo) const;

	// 아군에게 거는 직업 스킬을 받을 사람이 있는지, 인자는 자신을 뺀 살아 있는 같은 편 기준 (솔로의 3413 · 3428 · 3429 는 없음)
	bool HasJobSkillRecipient(int32 SkillId, bool bOtherCharacterAlive, bool bNaiaAlive) const;
	
	// SkillId = DT_SkillParry RowName, 없으면 nullptr -> 회피
	const FNUSkillParryDefinition* FindSkillParryDefinition(int32 SkillId) const;
	
	// PassiveId = DT_Passive RowName
	const FNUPassiveDefinition* FindPassiveDefinition(int32 PassiveId) const;

	// 이 패시브의 효과 행들, 없으면 nullptr
	// 한 패시브가 여러 줄을 가질 수 있음
	const TArray<FNUPassiveEffectRow>* FindPassiveEffects(int32 PassiveId) const;

	// 직업 스킬 정의
	const FNUJobSkillDefinition* FindJobSkillDefinition(int32 SkillId) const;

	// 이 직업 스킬의 효과 행들, 없으면 nullptr
	// 한 스킬이 여러 줄을 가지며 EffectIndex 순으로 정렬돼 있음
	const TArray<FNUJobSkillEffectRow>* FindJobSkillEffects(int32 SkillId) const;

	// 직업 자원 정의, 효과표가 부르는 이름(영력, 템포 등)으로 찾음
	// 현재 두 표를 잇는 것이 이름이라 이름으로 찾음
	const FNUJobResourceDefinition* FindJobResource(FName ResourceName) const;

	// 직업 자원 번호로 찾음, 서버는 이름이 아니라 번호(res_jobresourcetable 의 Resource_ID)로 줌
	const FNUJobResourceDefinition* FindJobResourceById(int32 ResourceId) const;
	
	// 자원을 전부 반환함
	const TMap<FName, FNUJobResourceDefinition>& GetJobResources() const;

	// 갬블러 카드 확률 줄, 그 갈래의 것만 LUK 낮은 순으로 돌려줌, 표가 없으면 빈 배열
	void GetFateCardRates(ENUFateArcana Arcana, TArray<FNUFateCardRateRow>& OutRows) const;

	// DT_SkillVFX 의 SkillId 컬럼으로 찾음, 없으면 nullptr -> 호출부가 기본 프로필로 처리
	// 행 이름이 아니라 컬럼을 쓰므로 표의 행 이름은 자유롭게 지어도 됨
	const FNUSkillVFXProfile* FindSkillVFXProfile(int32 SkillId) const;

	// DT_SkillVFX 에 행이 있는 SkillId 전부, 오름차순 (VFX 뷰어의 스킬 목록)
	TArray<int32> GetSkillVFXSkillIds() const;

	// 보스 전용 스킬 표(DT_BossSkill)의 스킬인지
	bool IsBossSkill(int32 SkillId) const;
	
	// 이 스킬이 부여하는 효과 행들, 없으면 nullptr
	const TArray<FNUActiveEffectRow>* FindSkillEffects(int32 SkillId) const;
	
	// 이 상태이상을 해제하는 행들, 해제 아이템과 면역 패시브가 섞여 있음, 없으면 nullptr
	const TArray<FNUStatusRelationRow>* FindStatusRelations(int32 StatusEffectId) const;
	
	// 이 아이템이 해제하는 상태이상 Id 들, 없으면 nullptr
	// 1307 만병 치료제는 아이템 하나가 여러 상태이상을 지우므로 역방향 검색이 필요함
	const TArray<int32>* FindCurableStatusIds(int32 ItemId) const;
	
	// 이 몬스터가 적으로 나올 때 쓰는 행동 패턴, 등록되지 않았으면 nullptr
	// NaiaId = DT_NaiaDefinition 의 RowName, 호출측은 nullptr 이면 기본 공격으로 넘어가면 됨
	const FNUEnemyPattern* FindEnemyPattern(FName NaiaId) const;
	
private:
	void BuildSkillEffectIndex();
	void SortSkillEffectIndex();
	void WarnBossSkillIdOverlap() const;
	void BuildPassiveEffectIndex();
	void BuildStatusRelationIndex();
	void BuildJobSkillIndex();
	void BuildJobSkillEffectIndex();
	void BuildJobResourceIndex();
	void BuildSkillVFXIndex();
	
	// 표 넷을 합쳐 NaiaId 로 바로 찾을 수 있는 패턴을 만듦
	void BuildEnemyPatternIndex();
	
	// 표를 다시 임포트했을 때 파생 인덱스를 다시 만들도록 구독을 걸고 끊음
	// 구독 대상은 파생 인덱스를 들고 있는 표들, 나머지는 FindRow 로 그때그때 읽음
	void BindTableChanged();
	void UnbindTableChanged();
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> StatusEffectTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> ActiveSkillTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SkillEffectTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SkillParryTable = nullptr;
	
	// 보스 전용 스킬 셋, 구조체가 위 셋과 같아 조회만 이어 붙임
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> BossSkillTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> BossSkillEffectTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> BossSkillParryTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> StatusRelationTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> PassiveTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> PassiveEffectTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> JobSkillDefinitionTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> JobSkillEffectTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> JobResourceTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> FateCardRateTable = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> SkillVFXTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> EnemyPatternMapTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> EnemyPatternRuleTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> EnemyPatternConditionTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> EnemyPatternPhaseTable = nullptr;
	
	// 재임포트 알림 구독, 표는 에셋이라 GameInstance 보다 오래 살아서 반드시 끊어야 함
	FDelegateHandle SkillEffectChangedHandle;
	FDelegateHandle BossSkillEffectChangedHandle;
	FDelegateHandle StatusRelationChangedHandle;
	FDelegateHandle JobSkillDefinitionChangedHandle;
	FDelegateHandle JobSkillEffectChangedHandle;
	FDelegateHandle JobResourceChangedHandle;
	FDelegateHandle SkillVFXChangedHandle;
	
	// 패턴은 표 넷을 합쳐 만드는 사본이라 넷 다 구독해야 함
	// 하나만 재임포트하면 나머지 셋이 옛 내용인 채로 인덱스가 만들어지므로 넷을 다 다시 임포트할 것
	FDelegateHandle EnemyPatternMapChangedHandle;
	FDelegateHandle EnemyPatternRuleChangedHandle;
	FDelegateHandle EnemyPatternConditionChangedHandle;
	FDelegateHandle EnemyPatternPhaseChangedHandle;
	
	// 스킬과 해당 스킬이 부여하는 효과들을 저장함
	// <SkillId, 부여하는 효과들>
	TMap<int32, TArray<FNUActiveEffectRow>> SkillEffectBySkill;

	// PassiveId -> 효과 행들, EffectIndex 오름차순으로 정렬해서 담음
	// TMap 값이 TArray 라 UPROPERTY 를 못 붙임, 위 SkillEffectBySkill 도 같은 이유
	TMap<int32, TArray<FNUPassiveEffectRow>> PassiveEffectByPassive;

	// 직업 스킬 번호 -> 그 스킬의 효과 줄들
	TMap<int32, TArray<FNUJobSkillEffectRow>> JobEffectBySkill;

	// 자원 이름 -> 그 자원의 정의
	TMap<FName, FNUJobResourceDefinition> JobResourceByName;

	// 자원 번호 -> 그 자원의 이름, FindJobResourceById 가 JobResourceByName 으로 다시 찾는 열쇠
	TMap<int32, FName> JobResourceNameById;
	
	// 상태이상과 그 상태이상을 해제하는 관계들을 저장함
	// <StatusEffectId, 관계 행들>
	TMap<int32, TArray<FNUStatusRelationRow>> StatusRelationsByStatus;

	// JobSkillDefinition.csv의 행 이름은 Skill_Key이므로 Skill_ID에서 행 이름으로 역인덱싱
	TMap<int32, FName> JobSkillRowNameById;

	// DT_SkillVFX 도 같은 이유로 SkillId 컬럼에서 행 이름으로 역인덱싱
	TMap<int32, FName> SkillVFXRowNameById;
	
	// 해제 아이템과 그 아이템이 지우는 상태이상들
	// <ItemId, StatusEffectId 들>
	TMap<int32, TArray<int32>> StatusIdsByCureItem;
	
	// 적 패턴을 NaiaId 로 펼쳐 담음
	// PatternId 로 담지 않는 이유는 대체 행동이 매핑 행에 있어서, NaiaId 로 펼쳐야
	// 패턴 하나가 자기 대체 행동까지 들고 있게 되고 셀렉터가 그대로 돌기 때문임
	// 패턴 공유는 CSV 의 PatternId 로 저작하고 여기서 사본을 뜸
	// <NaiaId, 조립된 패턴>
	TMap<FName, FNUEnemyPattern> PatternByNaiaId;
};
