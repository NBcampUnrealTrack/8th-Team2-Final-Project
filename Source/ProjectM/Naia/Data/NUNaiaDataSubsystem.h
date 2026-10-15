#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUNaiaDataSubsystem.generated.h"

class UDataTable;
class UNUEnemyPatternAsset;
class UNUEnemyPatternRegistry;
struct FNUNaiaDefinition;

UCLASS()
class PROJECTM_API UNUNaiaDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// 인스턴스 생성될 때 호출하여 Naia 데이터 로딩
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// 게임 종료 시 데이터 테이블 비우기
	virtual void Deinitialize() override;

	// 데이터 조회
	// NaiaId = DT_NaiaDefinition RowName
	const FNUNaiaDefinition* FindDefinition(FName NaiaId) const;
	const FNUNaiaDefinition* FindDefinitionById(int32 NaiaId) const;
	TArray<const FNUNaiaDefinition*> GetStarterDefinitions() const;

	// 표의 모든 나이아, DataIndex 오름차순 (VFX 뷰어의 나이아 목록)
	TArray<const FNUNaiaDefinition*> GetAllDefinitions() const;

	// 데이터 존재 확인
	bool HasDefinition(FName NaiaId) const;

	// 적으로 나올 때 쓰는 행동 패턴 조회
	// 등록되지 않았으면 nullptr, 호출측은 기본 공격으로 넘어가면 됨
	const UNUEnemyPatternAsset* FindEnemyPattern(FName NaiaId) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> NaiaDefinitionTable = nullptr;
	
	UPROPERTY(Transient)
	TObjectPtr<UNUEnemyPatternRegistry> EnemyPatternRegistry = nullptr;
};
