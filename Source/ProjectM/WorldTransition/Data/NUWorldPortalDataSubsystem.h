#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldTransition/Data/NUWorldPortalDefinition.h"
#include "WorldTransition/Data/NUWorldPortalEntryAsset.h"
#include "NUWorldPortalDataSubsystem.generated.h"

class UDataTable;

// 포탈 정적 정의를 검증하고 논리적 연결을 UE 목적지로 변환한다.
UCLASS()
class PROJECTM_API UNUWorldPortalDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool ReloadData();
	bool IsDataReady() const;
	// 지금 열린 기본 레벨에 맞춘 장소 연결 (맵 변형 적용)
	const TArray<FNUWorldPortalEntryBinding>& GetEntryBindings() const;
	static bool ReadEntryAssets(const TArray<TSoftObjectPtr<UNUWorldPortalEntryAsset>>& Assets, TArray<FNUWorldPortalEntryBinding>& OutBindings, TArray<FString>& OutErrors);
	// MapPackage가 기본 레벨이거나 사본 목적 레벨인 맵 변형이 있으면 연결의 목적 레벨을 바꾼다
	static void ApplyMapVariant(const FString& MapPackage, TArray<FNUWorldPortalEntryBinding>& InOutBindings);
	const FNUWorldPortalDefinition* FindPortalDefinition(FName PortalId) const;
	bool ResolvePortal(FName PortalId, ENUWorldPortalSide PlacementSide, FNUWorldPortalRoute& OutRoute, FString& OutError) const;
	static bool ValidateTable(const UDataTable* Portals, const UDataTable* Areas, const UDataTable* Locations, TArray<FString>& OutErrors);
	static bool ResolveRoute(const FNUWorldPortalDefinition& Definition, bool bReverseDirection, const TArray<FNUWorldPortalEntryBinding>& Bindings, FNUWorldPortalRoute& OutRoute, FString& OutError);

private:
	void ResetData();
	void HandleTableChanged();

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> PortalTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> AreaTable;
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LocationTable;
	TArray<FNUWorldPortalEntryBinding> EntryBindings;
	// 마지막으로 맵 변형을 적용한 기본 레벨과 그 결과
	mutable TOptional<FString> VariantMap;
	mutable TArray<FNUWorldPortalEntryBinding> VariantBindings;
	bool bDataReady = false;
};
