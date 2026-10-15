// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleDataSubsystem.h"

#include "NUActiveSkillContext.h"
#include "NUActiveSkillDefinition.h"
#include "NUBattleSkillInfo.h"
#include "NUBattleDataSettings.h"
#include "NUJobSkillDefinition.h"
#include "NUJobResourceDefinition.h"
#include "NUJobSkillContext.h"
#include "NUPassiveDefinition.h"
#include "NUSkillVFXProfile.h"
#include "NUStatusEffectDefinition.h"
#include "Battle/Enemy/NUEnemyPatternTypes.h"
#include "Engine/DataTable.h"

namespace
{
	// 행 구조체가 맞는 표만 받음, 경로가 비면 조용히 없음 (보스 표처럼 나중에 붙는 표)
	template <typename RowType>
	UDataTable* LoadBattleTable(const TSoftObjectPtr<UDataTable>& Path, const TCHAR* Label)
	{
		if (Path.IsNull())
		{
			return nullptr;
		}
		
		UDataTable* Loaded = Path.LoadSynchronous();
		if (!IsValid(Loaded) || Loaded->GetRowStruct() != RowType::StaticStruct())
		{
			UE_LOG(LogTemp, Error, TEXT("BattleDataSubsystem: %s 테이블을 읽지 못했습니다. 경로와 행 구조체를 확인하세요 : %s"),
				Label, *Path.ToString());
			return nullptr;
		}
		
		UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: %s 테이블 로드=%s, Rows=%d"),
			Label, *Loaded->GetName(), Loaded->GetRowMap().Num());
		
		return Loaded;
	}
}

void UNUBattleDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 프로젝트 세팅 설정
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	if (!IsValid(Settings))
	{
		return;
	}
	
	// 상태이상 테이블 할당
	if (!Settings->StatusEffectTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->StatusEffectTable.LoadSynchronous();
        
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUStatusEffectDefinition::StaticStruct())
		{
			StatusEffectTable = LoadedTable;
            
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 상태이상 테이블 로드=%s, Rows=%d"),
				*StatusEffectTable->GetName(),
				StatusEffectTable->GetRowMap().Num()
			);
		}
	}
	
	// 액티브스킬 테이블 할당
	if (!Settings->ActiveSkillTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->ActiveSkillTable.LoadSynchronous();
        
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUActiveSkillDefinition::StaticStruct())
		{
			ActiveSkillTable = LoadedTable;
            
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 액티브 스킬 테이블 로드=%s, Rows=%d"),
				*ActiveSkillTable->GetName(),
				ActiveSkillTable->GetRowMap().Num()
			);
		}
	}
	
	// 스킬 패링 정보 테이블 할당
	if (!Settings->SkillParryTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->SkillParryTable.LoadSynchronous();
        
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUSkillParryDefinition::StaticStruct())
		{
			SkillParryTable = LoadedTable;
            
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 스킬 패링 테이블 로드=%s, Rows=%d"),
				*SkillParryTable->GetName(),
				SkillParryTable->GetRowMap().Num()
			);
		}
	}
	
	// 보스 전용 스킬 셋, 효과 인덱스가 같이 담으므로 효과 표보다 먼저 읽음
	BossSkillTable = LoadBattleTable<FNUActiveSkillDefinition>(Settings->BossSkillTable, TEXT("보스 스킬"));
	BossSkillEffectTable = LoadBattleTable<FNUActiveEffectRow>(Settings->BossSkillEffectTable, TEXT("보스 스킬 효과"));
	BossSkillParryTable = LoadBattleTable<FNUSkillParryDefinition>(Settings->BossSkillParryTable, TEXT("보스 스킬 패링"));
	WarnBossSkillIdOverlap();
	
	// 스킬이 부여하는 효과 테이블 할당
	if (!Settings->SkillEffectTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->SkillEffectTable.LoadSynchronous();
        
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUActiveEffectRow::StaticStruct())
		{
			SkillEffectTable = LoadedTable;
            
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 스킬 효과 테이블 로드=%s, Rows=%d"),
				*SkillEffectTable->GetName(),
				SkillEffectTable->GetRowMap().Num()
			);
			
		}
		else
		{
			UE_LOG(LogTemp, Error,
				TEXT("BattleDataSubsystem: 스킬 효과 테이블을 읽지 못했습니다. 경로와 행 구조체(FNUActiveEffectRow)를 확인하세요 : %s"),
				*Settings->SkillEffectTable.ToString());
		}
	}
	
	// 나이아 효과 표가 없어도 보스 효과는 담겨야 해서 바깥에서 한 번 만듦
	BuildSkillEffectIndex();
	
	// 상태이상의 해제 아이템과 면역 패시브 관계 테이블 할당
	if (!Settings->StatusRelationTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->StatusRelationTable.LoadSynchronous();
		
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUStatusRelationRow::StaticStruct())
		{
			StatusRelationTable = LoadedTable;
			
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 상태이상 관계 테이블 로드=%s, Rows=%d"),
				*StatusRelationTable->GetName(),
				StatusRelationTable->GetRowMap().Num()
			);
			
			BuildStatusRelationIndex();
		}
	}
	
	// 패시브 효과 행 할당
	if (!Settings->PassiveEffectTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->PassiveEffectTable.LoadSynchronous();
		
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUPassiveEffectRow::StaticStruct())
		{
			PassiveEffectTable = LoadedTable;
			
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 패시브 효과 테이블 로드=%s, Rows=%d"),
				*PassiveEffectTable->GetName(),
				PassiveEffectTable->GetRowMap().Num()
			);
			
			BuildPassiveEffectIndex();
		}
	}
	
	// 패시브 정의 할당
	if (!Settings->PassiveTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->PassiveTable.LoadSynchronous();
		
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUPassiveDefinition::StaticStruct())
		{
			PassiveTable = LoadedTable;
			
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 패시브 테이블 로드=%s, Rows=%d"),
				*PassiveTable->GetName(),
				PassiveTable->GetRowMap().Num()
			);
		}
	}

	// 플레이어 직업 스킬 할당
	if (!Settings->JobSkillDefinitionTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->JobSkillDefinitionTable.LoadSynchronous();
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUJobSkillDefinition::StaticStruct())
		{
			JobSkillDefinitionTable = LoadedTable;
			BuildJobSkillIndex();
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 직업 스킬 정의 테이블 로드=%s, Rows=%d"),
				*JobSkillDefinitionTable->GetName(),
				JobSkillDefinitionTable->GetRowMap().Num());
		}
	}
	
	// 직업 스킬 효과 행 할당
	if (!Settings->JobSkillEffectTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->JobSkillEffectTable.LoadSynchronous();

		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUJobSkillEffectRow::StaticStruct())
		{
			JobSkillEffectTable = LoadedTable;
			BuildJobSkillEffectIndex();

			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 직업 스킬 효과 테이블 로드=%s, Rows=%d"),
				*JobSkillEffectTable->GetName(),
				JobSkillEffectTable->GetRowMap().Num());
		}
	}

	// 직업별 자원 할당
	if (!Settings->JobResourceTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->JobResourceTable.LoadSynchronous();

		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUJobResourceDefinition::StaticStruct())
		{
			JobResourceTable = LoadedTable;
			BuildJobResourceIndex();

			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 직업 자원 테이블 로드=%s, Rows=%d"),
				*JobResourceTable->GetName(),
				JobResourceTable->GetRowMap().Num());
		}
	}

	// 갬블러 카드 확률표
	if (!Settings->FateCardRateTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->FateCardRateTable.LoadSynchronous();

		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUFateCardRateRow::StaticStruct())
		{
			FateCardRateTable = LoadedTable;

			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 갬블러 카드 확률 테이블 로드=%s, Rows=%d"),
				*FateCardRateTable->GetName(),
				FateCardRateTable->GetRowMap().Num());
		}
	}

	// 스킬 연출 이펙트 테이블 할당
	if (!Settings->SkillVFXTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->SkillVFXTable.LoadSynchronous();
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUSkillVFXProfile::StaticStruct())
		{
			SkillVFXTable = LoadedTable;
			BuildSkillVFXIndex();
			UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem: 스킬 연출 테이블 로드=%s, Rows=%d"),
				*SkillVFXTable->GetName(),
				SkillVFXTable->GetRowMap().Num());
		}
	}

	// 적 패턴 표 넷
	// 넷을 다 읽은 뒤에 한 번만 인덱스를 만듦, 중간에 만들면 반쪽짜리가 됨
	if (!Settings->EnemyPatternMapTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->EnemyPatternMapTable.LoadSynchronous();
		
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUEnemyPatternMapRow::StaticStruct())
		{
			EnemyPatternMapTable = LoadedTable;
		}
	}
	
	if (!Settings->EnemyPatternRuleTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->EnemyPatternRuleTable.LoadSynchronous();
		
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUEnemyPatternRuleRow::StaticStruct())
		{
			EnemyPatternRuleTable = LoadedTable;
		}
	}
	
	if (!Settings->EnemyPatternConditionTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->EnemyPatternConditionTable.LoadSynchronous();
		
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUEnemyPatternCondition::StaticStruct())
		{
			EnemyPatternConditionTable = LoadedTable;
		}
	}
	
	if (!Settings->EnemyPatternPhaseTable.IsNull())
	{
		UDataTable* LoadedTable = Settings->EnemyPatternPhaseTable.LoadSynchronous();
		
		if (IsValid(LoadedTable) && LoadedTable->GetRowStruct() == FNUEnemyPhaseRow::StaticStruct())
		{
			EnemyPatternPhaseTable = LoadedTable;
		}
	}
	
	BuildEnemyPatternIndex();
	
	BindTableChanged();
}

void UNUBattleDataSubsystem::Deinitialize()
{
	// 표 포인터를 밀기 전에 끊어야 함, 끊을 대상을 잃으면 구독이 표에 남음
	UnbindTableChanged();
	
	StatusEffectTable = nullptr;
	ActiveSkillTable = nullptr;
	SkillEffectTable = nullptr;
	StatusRelationTable = nullptr;
	PassiveTable = nullptr;
	PassiveEffectTable = nullptr;
	JobSkillDefinitionTable = nullptr;
	SkillParryTable = nullptr;
	BossSkillTable = nullptr;
	BossSkillEffectTable = nullptr;
	BossSkillParryTable = nullptr;
	EnemyPatternMapTable = nullptr;
	EnemyPatternRuleTable = nullptr;
	EnemyPatternConditionTable = nullptr;
	EnemyPatternPhaseTable = nullptr;
	
	SkillEffectBySkill.Reset();
	StatusRelationsByStatus.Reset();
	StatusIdsByCureItem.Reset();
	JobSkillRowNameById.Reset();
	PatternByNaiaId.Reset();
	
	Super::Deinitialize();
}

const FNUStatusEffectDefinition* UNUBattleDataSubsystem::FindStatusEffectDefinition(int32 StatusEffectId) const
{
	if (!IsValid(StatusEffectTable) || StatusEffectId <= 0)
	{
		return nullptr;
	}
	
	return StatusEffectTable->FindRow<FNUStatusEffectDefinition>(
		FName(*FString::FromInt(StatusEffectId)),
		TEXT("BattleDataSubsystem::FindStatusEffectDefinition"),
		false
		);
}

const FNUActiveSkillDefinition* UNUBattleDataSubsystem::FindActiveSkillDefinition(int32 SkillId) const
{
	if (SkillId <= 0)
	{
		return nullptr;
	}
	
	// 보스 스킬도 같은 구조체라 여기서 이어 찾음, 실행기 · 패턴 · 넷 재생이 따로 알 필요가 없음
	const FName RowName(*FString::FromInt(SkillId));
	for (const UDataTable* Table : { ActiveSkillTable.Get(), BossSkillTable.Get() })
	{
		if (!IsValid(Table))
		{
			continue;
		}
		
		if (const FNUActiveSkillDefinition* Found = Table->FindRow<FNUActiveSkillDefinition>(
			RowName, TEXT("BattleDataSubsystem::FindActiveSkillDefinition"), false))
		{
			return Found;
		}
	}
	
	return nullptr;
}

bool UNUBattleDataSubsystem::TryGetBattleSkillInfo(int32 SkillId, FNUBattleSkillInfo& OutInfo) const
{
	OutInfo = FNUBattleSkillInfo();
	if (SkillId <= 0)
	{
		return false;
	}

	// 나이아 스킬 정보
	if (const FNUActiveSkillDefinition* Definition = FindActiveSkillDefinition(SkillId))
	{
		OutInfo.SkillId = SkillId;
		OutInfo.Source = ENUBattleSkillSource::Naia;
		OutInfo.DisplayName = Definition->KoreanName;
		OutInfo.Description = Definition->Description;
		OutInfo.SPCost = Definition->SPCost;
		OutInfo.HitCount = Definition->HitCount;
		OutInfo.TargetPolicy = Definition->TargetPolicy;
		OutInfo.bExecutable = true;
		OutInfo.bDealsDamage = NUActiveSkill::GatherDamageParams(FindSkillEffects(SkillId)).bHasAnyDamageRow;
		return true;
	}

	// 플레이어 직업 스킬 정보
	const FNUJobSkillDefinition* Definition = FindJobSkillDefinition(SkillId);
	static const FName ActiveType(TEXT("액티브"));
	static const FName UltimateType(TEXT("필살기"));
	if (!Definition || (Definition->SkillType != ActiveType && Definition->SkillType != UltimateType))
	{
		return false;
	}

	OutInfo.SkillId = SkillId;
	OutInfo.Source = ENUBattleSkillSource::Job;
	OutInfo.DisplayName = Definition->Name_KR;
	OutInfo.Description = Definition->Description;
	OutInfo.SPCost = Definition->SP_Cost;
	OutInfo.AGCost = Definition->AG_Cost;
	OutInfo.HPCostRatio = Definition->HP_Cost;
	OutInfo.HitCount = Definition->HitCount;
	OutInfo.bUltimate = Definition->SkillType == UltimateType;
	OutInfo.TargetPolicy = Definition->TargetPolicy;

	const TArray<FNUJobSkillEffectRow>* Effects = FindJobSkillEffects(SkillId);
	OutInfo.bExecutable = NUJobSkill::CanRunFromTable(*Definition, Effects);
	OutInfo.MaxUsesPerBattle = NUJobSkill::FindMaxUsesPerBattle(Effects);
	// 남은 SP 를 전부 쓰는 스킬(3203 나이아 개화)은 표에 피해 줄이 없어도 피해를 줌
	OutInfo.bDealsDamage = NUJobSkill::HasDamageRow(Effects) || NUJobSkill::FindSpendAllSPSkill(SkillId) != nullptr;
	return true;
}

bool UNUBattleDataSubsystem::HasJobSkillRecipient(int32 SkillId, bool bOtherCharacterAlive, bool bNaiaAlive) const
{
	const FNUJobSkillDefinition* Definition = FindJobSkillDefinition(SkillId);

	return !Definition
		|| NUJobSkill::HasAllySideRecipient(*Definition, FindJobSkillEffects(SkillId), bOtherCharacterAlive, bNaiaAlive);
}

const FNUSkillParryDefinition* UNUBattleDataSubsystem::FindSkillParryDefinition(int32 SkillId) const
{
	if (SkillId <= 0)
	{
		return nullptr;
	}
	
	// 행이 있으면 패링, 없으면 회피 — 보스 스킬도 같은 규칙 (옹이 잔광은 행이 없어 회피)
	const FName RowName(*FString::FromInt(SkillId));
	for (const UDataTable* Table : { SkillParryTable.Get(), BossSkillParryTable.Get() })
	{
		if (!IsValid(Table))
		{
			continue;
		}
		
		if (const FNUSkillParryDefinition* Found = Table->FindRow<FNUSkillParryDefinition>(
			RowName, TEXT("BattleDataSubsystem::FindSkillParryDefinition"), false))
		{
			return Found;
		}
	}
	
	return nullptr;
}

const FNUPassiveDefinition* UNUBattleDataSubsystem::FindPassiveDefinition(int32 PassiveId) const
{
	if (!IsValid(PassiveTable) || PassiveId <= 0)
	{
		return nullptr;
	}
	
	return PassiveTable->FindRow<FNUPassiveDefinition>(
		FName(*FString::FromInt(PassiveId)),
		TEXT("BattleDataSubsystem::FindPassiveDefinition"),
		false
		);
}

const FNUJobSkillDefinition* UNUBattleDataSubsystem::FindJobSkillDefinition(int32 SkillId) const
{
	const FName* RowName = JobSkillRowNameById.Find(SkillId);
	if (!IsValid(JobSkillDefinitionTable) || RowName == nullptr)
	{
		return nullptr;
	}

	return JobSkillDefinitionTable->FindRow<FNUJobSkillDefinition>(
		*RowName,
		TEXT("BattleDataSubsystem::FindJobSkillDefinition"),
		false);
}

const FNUSkillVFXProfile* UNUBattleDataSubsystem::FindSkillVFXProfile(int32 SkillId) const
{
	// SkillId 가 0 이하인 행은 인덱스에 안 담기므로 여기서 따로 거르지 않아도 됨
	const FName* RowName = SkillVFXRowNameById.Find(SkillId);
	if (!IsValid(SkillVFXTable) || RowName == nullptr)
	{
		return nullptr;
	}

	return SkillVFXTable->FindRow<FNUSkillVFXProfile>(
		*RowName,
		TEXT("BattleDataSubsystem::FindSkillVFXProfile"),
		false
		);
}

const TArray<FNUActiveEffectRow>* UNUBattleDataSubsystem::FindSkillEffects(int32 SkillId) const
{
	return SkillEffectBySkill.Find(SkillId);
}

void UNUBattleDataSubsystem::SortSkillEffectIndex()
{
	// 표 순회 순서는 보장되지 않음, 한 스킬 안의 순번을 맞춰둠
	for (TPair<int32, TArray<FNUActiveEffectRow>>& Pair : SkillEffectBySkill)
	{
		Pair.Value.Sort([](const FNUActiveEffectRow& Left, const FNUActiveEffectRow& Right)
			{
				return Left.EffectIndex < Right.EffectIndex;
			}
			);
	}
}

void UNUBattleDataSubsystem::WarnBossSkillIdOverlap() const
{
	if (!IsValid(BossSkillTable) || !IsValid(ActiveSkillTable))
	{
		return;
	}
	
	// 조회가 액티브 표를 먼저 보므로 번호가 겹치면 보스 행이 조용히 가려짐
	for (const TPair<FName, uint8*>& Pair : BossSkillTable->GetRowMap())
	{
		if (ActiveSkillTable->GetRowMap().Contains(Pair.Key))
		{
			UE_LOG(LogTemp, Error,
				TEXT("BattleDataSubsystem: 보스 스킬 %s 의 번호가 액티브 스킬과 겹칩니다 — 액티브 행이 쓰입니다"),
				*Pair.Key.ToString());
		}
	}
}

void UNUBattleDataSubsystem::BuildSkillEffectIndex()
{
	SkillEffectBySkill.Reset();
	
	// 보스 스킬 효과도 같은 인덱스에 담음, 번호 대역이 달라 섞이지 않음
	if (IsValid(BossSkillEffectTable))
	{
		BossSkillEffectTable->ForeachRow<FNUActiveEffectRow>(
			TEXT("BattleDataSubsystem::BuildSkillEffectIndex"),
			[this](const FName& RowName, const FNUActiveEffectRow& Row)
			{
				if (Row.SkillId > 0)
				{
					SkillEffectBySkill.FindOrAdd(Row.SkillId).Add(Row);
				}
			}
			);
	}
	
	if (!IsValid(SkillEffectTable))
	{
		SortSkillEffectIndex();
		return;
	}
	
	// SkillId에 해당하는 행들을 Map에 저장
	SkillEffectTable->ForeachRow<FNUActiveEffectRow>(
		TEXT("BattleDataSubsystem::BuildSkillEffectIndex"),
		[this](const FName& RowName, const FNUActiveEffectRow& Row)
		{
			if (Row.SkillId <= 0)
			{
				return;
			}
			SkillEffectBySkill.FindOrAdd(Row.SkillId).Add(Row);
		}
		);
	
	SortSkillEffectIndex();
	
	UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem : 스킬 효과 인덱싱 완료, 스킬 개수 = %d"),
		SkillEffectBySkill.Num())
}

const TArray<FNUPassiveEffectRow>* UNUBattleDataSubsystem::FindPassiveEffects(int32 PassiveId) const
{
	return PassiveEffectByPassive.Find(PassiveId);
}

void UNUBattleDataSubsystem::BuildPassiveEffectIndex()
{
	PassiveEffectByPassive.Reset();
	if (!IsValid(PassiveEffectTable))
	{
		return;
	}
	
	PassiveEffectTable->ForeachRow<FNUPassiveEffectRow>(
		TEXT("BattleDataSubsystem::BuildPassiveEffectIndex"),
		[this](const FName& RowName, const FNUPassiveEffectRow& Row)
		{
			if (Row.PassiveId <= 0)
			{
				return;
			}
			PassiveEffectByPassive.FindOrAdd(Row.PassiveId).Add(Row);
		}
		);
	
	// 표 순회 순서는 보장되지 않음, 한 패시브 안의 순번을 맞춰둠
	for (TPair<int32, TArray<FNUPassiveEffectRow>>& Pair : PassiveEffectByPassive)
	{
		Pair.Value.Sort([](const FNUPassiveEffectRow& Left, const FNUPassiveEffectRow& Right)
			{
				return Left.EffectIndex < Right.EffectIndex;
			}
			);
	}
	
	UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem : 패시브 효과 인덱싱 완료, 패시브 개수 = %d"),
		PassiveEffectByPassive.Num())
}

const TArray<FNUJobSkillEffectRow>* UNUBattleDataSubsystem::FindJobSkillEffects(int32 SkillId) const
{
	return JobEffectBySkill.Find(SkillId);
}

void UNUBattleDataSubsystem::BuildJobSkillEffectIndex()
{
	JobEffectBySkill.Reset();
	if (!IsValid(JobSkillEffectTable))
	{
		return;
	}

	int32 RunnableRows = 0;

	JobSkillEffectTable->ForeachRow<FNUJobSkillEffectRow>(
		TEXT("BattleDataSubsystem::BuildJobSkillEffectIndex"),
		[this, &RunnableRows](const FName& RowName, const FNUJobSkillEffectRow& Row)
		{
			if (Row.JobSkillId <= 0)
			{
				return;
			}

			if (Row.ImplementBy == ENUEffectImplementBy::Data)
			{
				++RunnableRows;
			}

			JobEffectBySkill.FindOrAdd(Row.JobSkillId).Add(Row);
		}
		);

	// 표 순회 순서는 보장되지 않음, 한 스킬 안의 순번을 맞춰둠
	for (TPair<int32, TArray<FNUJobSkillEffectRow>>& Pair : JobEffectBySkill)
	{
		Pair.Value.Sort([](const FNUJobSkillEffectRow& Left, const FNUJobSkillEffectRow& Right)
			{
				return Left.EffectIndex < Right.EffectIndex;
			}
			);
	}

	// 표가 덜 온 행(Pending)과 전용 구현이 필요한 행(Code*)은 데이터만으로 돌지 않음
	// 해당하는 행의 수를 찍어둠
	UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem : 직업 스킬 효과 인덱싱 완료, 스킬 %d개 / 데이터로 도는 줄 %d"),
		JobEffectBySkill.Num(),
		RunnableRows)
}

const FNUJobResourceDefinition* UNUBattleDataSubsystem::FindJobResource(FName ResourceName) const
{
	return JobResourceByName.Find(ResourceName);
}

const FNUJobResourceDefinition* UNUBattleDataSubsystem::FindJobResourceById(int32 ResourceId) const
{
	const FName* ResourceName = JobResourceNameById.Find(ResourceId);

	return ResourceName ? JobResourceByName.Find(*ResourceName) : nullptr;
}

const TMap<FName, FNUJobResourceDefinition>& UNUBattleDataSubsystem::GetJobResources() const
{
	return JobResourceByName;
}

void UNUBattleDataSubsystem::BuildJobResourceIndex()
{
	JobResourceByName.Reset();
	JobResourceNameById.Reset();
	if (!IsValid(JobResourceTable))
	{
		return;
	}

	JobResourceTable->ForeachRow<FNUJobResourceDefinition>(
		TEXT("BattleDataSubsystem::BuildJobResourceIndex"),
		[this](const FName& RowName, const FNUJobResourceDefinition& Row)
		{
			if (Row.KoreanName.IsNone())
			{
				return;
			}

			// 이름이 겹치면 어느 자원인지 갈리지 않음, 변환기가 먼저 막지만 여기서도 알림
			if (JobResourceByName.Contains(Row.KoreanName))
			{
				UE_LOG(LogTemp, Warning, TEXT("직업 자원 이름이 겹칩니다 : %s"),
					*Row.KoreanName.ToString());
				return;
			}

			JobResourceByName.Add(Row.KoreanName, Row);
			JobResourceNameById.Add(Row.ResourceId, Row.KoreanName);
		}
		);

	UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem : 직업 자원 인덱싱 완료, 자원 %d종"),
		JobResourceByName.Num())
}

void UNUBattleDataSubsystem::GetFateCardRates(ENUFateArcana Arcana, TArray<FNUFateCardRateRow>& OutRows) const
{
	OutRows.Reset();

	if (!IsValid(FateCardRateTable))
	{
		return;
	}

	FateCardRateTable->ForeachRow<FNUFateCardRateRow>(
		TEXT("BattleDataSubsystem::GetFateCardRates"),
		[&OutRows, Arcana](const FName& RowName, const FNUFateCardRateRow& Row)
		{
			if (Row.ArcanaType == Arcana)
			{
				OutRows.Add(Row);
			}
		}
		);

	OutRows.Sort([](const FNUFateCardRateRow& A, const FNUFateCardRateRow& B)
	{
		return A.LUK < B.LUK;
	});
}

const TArray<FNUStatusRelationRow>* UNUBattleDataSubsystem::FindStatusRelations(int32 StatusEffectId) const
{
	return StatusRelationsByStatus.Find(StatusEffectId);
}

const TArray<int32>* UNUBattleDataSubsystem::FindCurableStatusIds(int32 ItemId) const
{
	return StatusIdsByCureItem.Find(ItemId);
}

void UNUBattleDataSubsystem::BuildStatusRelationIndex()
{
	StatusRelationsByStatus.Reset();
	StatusIdsByCureItem.Reset();
	
	if (!IsValid(StatusRelationTable))
	{
		return;
	}
	
	// 행을 값으로 복사해서 담음
	// 재임포트하면 행 메모리가 재할당되므로 포인터를 들고 있으면 안 됨
	StatusRelationTable->ForeachRow<FNUStatusRelationRow>(
		TEXT("BattleDataSubsystem::BuildStatusRelationIndex"),
		[this](const FName& RowName, const FNUStatusRelationRow& Row)
		{
			if (Row.StatusEffectId <= 0 || Row.ReferenceId <= 0)
			{
				return;
			}
			
			StatusRelationsByStatus.FindOrAdd(Row.StatusEffectId).Add(Row);
			
			// 해제 아이템은 "이 아이템이 무엇을 지우나" 로도 물어보므로 역방향도 만들어둠
			if (Row.RelationType == ENUStatusRelationType::CureItem)
			{
				StatusIdsByCureItem.FindOrAdd(Row.ReferenceId).AddUnique(Row.StatusEffectId);
			}
		}
		);
	
	UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem : 상태이상 관계 인덱싱 완료, 상태이상 개수 = %d, 해제 아이템 개수 = %d"),
		StatusRelationsByStatus.Num(), StatusIdsByCureItem.Num())
}

const FNUEnemyPattern* UNUBattleDataSubsystem::FindEnemyPattern(FName NaiaId) const
{
	if (NaiaId.IsNone())
	{
		return nullptr;
	}
	
	return PatternByNaiaId.Find(NaiaId);
}

void UNUBattleDataSubsystem::BuildEnemyPatternIndex()
{
	PatternByNaiaId.Reset();
	
	// 매핑과 규칙이 없으면 어떤 몬스터도 패턴을 갖지 않음
	if (!IsValid(EnemyPatternMapTable) || !IsValid(EnemyPatternRuleTable))
	{
		return;
	}
	
	// 페이즈는 순서가 의미라 정렬용 인덱스를 같이 들고 다님
	struct FPhaseEntry
	{
		FNUEnemyPhase Phase;
		FName PatternId;
		int32 PhaseIndex = 0;
	};
	
	// 1. 규칙을 행 이름으로 모음
	// 조건이 OwnerId 로 부모를 가리키므로 행 이름이 키여야 함
	// 행을 값으로 복사함, 재임포트하면 행 메모리가 재할당되므로 포인터를 들면 안 됨
	TMap<FName, FNUEnemyPatternRule> RuleByRowName;
	TMap<FName, FName> PatternIdByRuleName;
	
	EnemyPatternRuleTable->ForeachRow<FNUEnemyPatternRuleRow>(
		TEXT("BattleDataSubsystem::BuildEnemyPatternIndex"),
		[&RuleByRowName, &PatternIdByRuleName](const FName& RowName, const FNUEnemyPatternRuleRow& Row)
		{
			if (Row.PatternId.IsNone())
			{
				return;
			}
			
			FNUEnemyPatternRule Rule;
			Rule.AbilityId = Row.AbilityId;
			Rule.SkillId = Row.SkillId;
			Rule.Priority = Row.Priority;
			Rule.Weight = Row.Weight;
			Rule.CooldownTurns = Row.CooldownTurns;
			Rule.MaxUsesPerBattle = Row.MaxUsesPerBattle;
			Rule.PhaseTag = Row.PhaseTag;
			Rule.bOverrideParryTiming = Row.bOverrideParryTiming;
			Rule.ChallengeDuration = Row.ChallengeDuration;
			Rule.SuccessAnchor = Row.SuccessAnchor;
			Rule.SuccessWindow = Row.SuccessWindow;
			
			RuleByRowName.Add(RowName, MoveTemp(Rule));
			PatternIdByRuleName.Add(RowName, Row.PatternId);
		}
		);
	
	// 2. 페이즈도 같은 방식으로, 표가 비어 있으면 아무것도 안 담김
	TMap<FName, FPhaseEntry> PhaseByRowName;
	
	if (IsValid(EnemyPatternPhaseTable))
	{
		EnemyPatternPhaseTable->ForeachRow<FNUEnemyPhaseRow>(
			TEXT("BattleDataSubsystem::BuildEnemyPatternIndex"),
			[&PhaseByRowName](const FName& RowName, const FNUEnemyPhaseRow& Row)
			{
				if (Row.PatternId.IsNone())
				{
					return;
				}
				
				FPhaseEntry Entry;
				Entry.Phase.PhaseTag = Row.PhaseTag;
				Entry.Phase.OnEnterAbilityId = Row.OnEnterAbilityId;
				Entry.PatternId = Row.PatternId;
				Entry.PhaseIndex = Row.PhaseIndex;
				
				PhaseByRowName.Add(RowName, MoveTemp(Entry));
			}
			);
	}
	
	// 3. 조건을 부모에 붙임
	// 4번의 복사보다 먼저 해야 사본에 조건이 실림
	// 부모를 못 찾으면 조건이 통째로 사라져 그 규칙이 무조건 후보가 되므로 로그를 남김
	if (IsValid(EnemyPatternConditionTable))
	{
		EnemyPatternConditionTable->ForeachRow<FNUEnemyPatternCondition>(
			TEXT("BattleDataSubsystem::BuildEnemyPatternIndex"),
			[&RuleByRowName, &PhaseByRowName](const FName& RowName, const FNUEnemyPatternCondition& Row)
			{
				if (Row.OwnerType == ENUEnemyPatternOwnerType::Rule)
				{
					if (FNUEnemyPatternRule* Rule = RuleByRowName.Find(Row.OwnerId))
					{
						Rule->Conditions.Add(Row);
						return;
					}
				}
				else if (Row.OwnerType == ENUEnemyPatternOwnerType::Phase)
				{
					if (FPhaseEntry* Entry = PhaseByRowName.Find(Row.OwnerId))
					{
						Entry->Phase.EnterConditions.Add(Row);
						return;
					}
				}
				
				UE_LOG(LogTemp, Error,
					TEXT("적 패턴: 부모를 찾을 수 없는 조건이라 무시함 : %s (OwnerId=%s)"),
					*RowName.ToString(), *Row.OwnerId.ToString());
			}
			);
	}
	
	// 4. PatternId 로 묶음
	TMap<FName, TArray<FNUEnemyPatternRule>> RulesByPattern;
	for (TPair<FName, FNUEnemyPatternRule>& Pair : RuleByRowName)
	{
		if (const FName* PatternId = PatternIdByRuleName.Find(Pair.Key))
		{
			RulesByPattern.FindOrAdd(*PatternId).Add(MoveTemp(Pair.Value));
		}
	}
	
	TMap<FName, TArray<FPhaseEntry>> PhasesByPattern;
	for (TPair<FName, FPhaseEntry>& Pair : PhaseByRowName)
	{
		PhasesByPattern.FindOrAdd(Pair.Value.PatternId).Add(MoveTemp(Pair.Value));
	}
	
	// 페이즈는 인덱스 순서가 곧 판정 순서라 여기서 정렬해둠
	// 행 순서에 기대면 RowMap 순회 순서라는 구현 세부에 규칙이 얹힘
	for (TPair<FName, TArray<FPhaseEntry>>& Pair : PhasesByPattern)
	{
		Pair.Value.Sort([](const FPhaseEntry& A, const FPhaseEntry& B)
			{
				return A.PhaseIndex < B.PhaseIndex;
			});
	}
	
	// 5. 매핑 행마다 NaiaId 로 펼침
	EnemyPatternMapTable->ForeachRow<FNUEnemyPatternMapRow>(
		TEXT("BattleDataSubsystem::BuildEnemyPatternIndex"),
		[this, &RulesByPattern, &PhasesByPattern](const FName& RowName, const FNUEnemyPatternMapRow& Row)
		{
			if (Row.PatternId.IsNone())
			{
				return;
			}
			
			// 행 이름이 아니라 NaiaId 컬럼을 키로 씀
			// 예전에는 행 이름을 그대로 키로 썼는데, 두 값이 늘 같아서 우연히 맞았을 뿐이고
			// 행 이름만 다르게 지은 행을 넣으면 컬럼과 무관하게 조용히 딴 곳에 인덱싱됐음
			if (Row.NaiaId <= 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("적 패턴 매핑 무시: Row=%s 의 NaiaId가 비어 있음"),
					*RowName.ToString());
				return;
			}
			
			// 조회하는 쪽이 나이아 정의의 행 이름(FName)을 들고 오므로 같은 모양으로 맞춤
			const FName NaiaKey(*FString::FromInt(Row.NaiaId));
			if (PatternByNaiaId.Contains(NaiaKey))
			{
				UE_LOG(LogTemp, Warning, TEXT("중복 적 패턴 매핑 무시: NaiaId=%d Row=%s"),
					Row.NaiaId,
					*RowName.ToString());
				return;
			}
			
			FNUEnemyPattern Pattern;
			Pattern.FallbackAbilityId = Row.FallbackAbilityId;
			Pattern.ConcedeHealthRatio = Row.ConcedeHealthRatio;
			Pattern.ConcedeAnimation = Row.ConcedeAnimation;
			Pattern.ConcedeLoopAnimation = Row.ConcedeLoopAnimation;
			
			if (const TArray<FNUEnemyPatternRule>* Rules = RulesByPattern.Find(Row.PatternId))
			{
				Pattern.Rules = *Rules;
			}
			
			if (const TArray<FPhaseEntry>* Phases = PhasesByPattern.Find(Row.PatternId))
			{
				Pattern.Phases.Reserve(Phases->Num());
				for (const FPhaseEntry& Entry : *Phases)
				{
					Pattern.Phases.Add(Entry.Phase);
				}
			}
			
			PatternByNaiaId.Add(NaiaKey, MoveTemp(Pattern));
		}
		);
	
	UE_LOG(LogTemp, Log, TEXT("BattleDataSubsystem : 적 패턴 인덱싱 완료, 몬스터 개수 = %d"),
		PatternByNaiaId.Num())
}

void UNUBattleDataSubsystem::BindTableChanged()
{
	if (IsValid(SkillEffectTable))
	{
		SkillEffectChangedHandle = SkillEffectTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildSkillEffectIndex);
	}
	
	if (IsValid(BossSkillEffectTable))
	{
		BossSkillEffectChangedHandle = BossSkillEffectTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildSkillEffectIndex);
	}
	
	if (IsValid(StatusRelationTable))
	{
		StatusRelationChangedHandle = StatusRelationTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildStatusRelationIndex);
	}

	if (IsValid(JobSkillDefinitionTable))
	{
		JobSkillDefinitionChangedHandle = JobSkillDefinitionTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildJobSkillIndex);
	}

	if (IsValid(JobSkillEffectTable))
	{
		JobSkillEffectChangedHandle = JobSkillEffectTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildJobSkillEffectIndex);
	}

	if (IsValid(JobResourceTable))
	{
		JobResourceChangedHandle = JobResourceTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildJobResourceIndex);
	}

	if (IsValid(SkillVFXTable))
	{
		SkillVFXChangedHandle = SkillVFXTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildSkillVFXIndex);
	}
	
	// 넷 중 아무거나 바뀌어도 인덱스를 통째로 다시 만듦
	if (IsValid(EnemyPatternMapTable))
	{
		EnemyPatternMapChangedHandle = EnemyPatternMapTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildEnemyPatternIndex);
	}
	
	if (IsValid(EnemyPatternRuleTable))
	{
		EnemyPatternRuleChangedHandle = EnemyPatternRuleTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildEnemyPatternIndex);
	}
	
	if (IsValid(EnemyPatternConditionTable))
	{
		EnemyPatternConditionChangedHandle = EnemyPatternConditionTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildEnemyPatternIndex);
	}
	
	if (IsValid(EnemyPatternPhaseTable))
	{
		EnemyPatternPhaseChangedHandle = EnemyPatternPhaseTable->OnDataTableChanged().AddUObject(
			this, &UNUBattleDataSubsystem::BuildEnemyPatternIndex);
	}
}

void UNUBattleDataSubsystem::UnbindTableChanged()
{
	if (IsValid(SkillEffectTable))
	{
		SkillEffectTable->OnDataTableChanged().Remove(SkillEffectChangedHandle);
	}
	
	if (IsValid(BossSkillEffectTable))
	{
		BossSkillEffectTable->OnDataTableChanged().Remove(BossSkillEffectChangedHandle);
	}
	
	if (IsValid(StatusRelationTable))
	{
		StatusRelationTable->OnDataTableChanged().Remove(StatusRelationChangedHandle);
	}

	if (IsValid(JobSkillDefinitionTable))
	{
		JobSkillDefinitionTable->OnDataTableChanged().Remove(JobSkillDefinitionChangedHandle);
	}

	if (IsValid(JobSkillEffectTable))
	{
		JobSkillEffectTable->OnDataTableChanged().Remove(JobSkillEffectChangedHandle);
	}

	if (IsValid(JobResourceTable))
	{
		JobResourceTable->OnDataTableChanged().Remove(JobResourceChangedHandle);
	}

	if (IsValid(SkillVFXTable))
	{
		SkillVFXTable->OnDataTableChanged().Remove(SkillVFXChangedHandle);
	}
	
	if (IsValid(EnemyPatternMapTable))
	{
		EnemyPatternMapTable->OnDataTableChanged().Remove(EnemyPatternMapChangedHandle);
	}
	
	if (IsValid(EnemyPatternRuleTable))
	{
		EnemyPatternRuleTable->OnDataTableChanged().Remove(EnemyPatternRuleChangedHandle);
	}
	
	if (IsValid(EnemyPatternConditionTable))
	{
		EnemyPatternConditionTable->OnDataTableChanged().Remove(EnemyPatternConditionChangedHandle);
	}
	
	if (IsValid(EnemyPatternPhaseTable))
	{
		EnemyPatternPhaseTable->OnDataTableChanged().Remove(EnemyPatternPhaseChangedHandle);
	}
	
	SkillEffectChangedHandle.Reset();
	BossSkillEffectChangedHandle.Reset();
	StatusRelationChangedHandle.Reset();
	JobSkillDefinitionChangedHandle.Reset();
	SkillVFXChangedHandle.Reset();
	EnemyPatternMapChangedHandle.Reset();
	EnemyPatternRuleChangedHandle.Reset();
	EnemyPatternConditionChangedHandle.Reset();
	EnemyPatternPhaseChangedHandle.Reset();
}

TArray<int32> UNUBattleDataSubsystem::GetSkillVFXSkillIds() const
{
	TArray<int32> SkillIds;
	SkillVFXRowNameById.GetKeys(SkillIds);
	SkillIds.Sort();

	return SkillIds;
}

bool UNUBattleDataSubsystem::IsBossSkill(int32 SkillId) const
{
	if (!IsValid(BossSkillTable) || SkillId <= 0)
	{
		return false;
	}

	return BossSkillTable->FindRow<FNUActiveSkillDefinition>(
		FName(*FString::FromInt(SkillId)),
		TEXT("BattleDataSubsystem::IsBossSkill"),
		false
		) != nullptr;
}

void UNUBattleDataSubsystem::BuildSkillVFXIndex()
{
	// 재임포트된 테이블을 기준으로 인덱스 다시 생성
	SkillVFXRowNameById.Reset();
	if (!IsValid(SkillVFXTable))
	{
		return;
	}

	SkillVFXTable->ForeachRow<FNUSkillVFXProfile>(
		TEXT("BattleDataSubsystem::BuildSkillVFXIndex"),
		[this](const FName& RowName, const FNUSkillVFXProfile& Row)
		{
			// 컬럼을 안 채운 행은 어느 스킬 것인지 알 수 없어 영영 안 걸림
			if (Row.SkillId <= 0)
			{
				UE_LOG(LogTemp, Warning, TEXT("스킬 연출 무시: Row=%s 의 SkillId가 비어 있음"),
					*RowName.ToString());
				return;
			}
			if (SkillVFXRowNameById.Contains(Row.SkillId))
			{
				UE_LOG(LogTemp, Warning, TEXT("중복 스킬 연출 무시: SkillId=%d Row=%s"),
					Row.SkillId,
					*RowName.ToString());
				return;
			}

			// RowName을 저장해 DataTable 재임포트 후에도 다시 조회 가능
			SkillVFXRowNameById.Add(Row.SkillId, RowName);
		});
}

void UNUBattleDataSubsystem::BuildJobSkillIndex()
{
	// 재임포트된 테이블을 기준으로 인덱스 다시 생성
	JobSkillRowNameById.Reset();
	if (!IsValid(JobSkillDefinitionTable))
	{
		return;
	}

	JobSkillDefinitionTable->ForeachRow<FNUJobSkillDefinition>(
		TEXT("BattleDataSubsystem::BuildJobSkillIndex"),
		[this](const FName& RowName, const FNUJobSkillDefinition& Row)
		{
			if (Row.Skill_ID <= 0 || Row.Skill_Key.IsNone())
			{
				UE_LOG(LogTemp, Warning, TEXT("직업 스킬 정의 무시: Row=%s Skill_ID=%d Skill_Key=%s"),
					*RowName.ToString(),
					Row.Skill_ID,
					*Row.Skill_Key.ToString());
				return;
			}
			if (JobSkillRowNameById.Contains(Row.Skill_ID))
			{
				UE_LOG(LogTemp, Warning, TEXT("중복 직업 스킬 ID 무시: Skill_ID=%d Row=%s"),
					Row.Skill_ID,
					*RowName.ToString());
				return;
			}

			// RowName을 저장해 DataTable 재임포트 후에도 다시 조회 가능
			JobSkillRowNameById.Add(Row.Skill_ID, RowName);
		});
}
