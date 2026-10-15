// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UNUInventoryComponent;
class UWorld;

// 임시 인벤토리 디버그 오버레이 (Slate — 에셋 불필요). I키/nu.Inv.UI 로 토글.
// 아이템 줄을 클릭하면 필드에서 바로 사용(ServerUseItem). 정식 인벤토리 UI(UMG)가 생기면 삭제 대상.
class SNUInventoryDebugOverlay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNUInventoryDebugOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UWorld* InWorld);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FText GetStatusText() const;

	// UMG의 자동 DPI 스케일을 수동 적용 — 없으면 고해상도에서 아주 작게 그려진다
	float GetViewportScale() const;

	// 인벤토리 내용이 바뀐 프레임에만 버튼 목록을 다시 만든다 (시그니처 비교)
	void RebuildItemList(UNUInventoryComponent* Inventory, bool bInBattle);
	FReply HandleItemClicked(FGuid InstanceId);

	TWeakObjectPtr<UWorld> World;
	TSharedPtr<SVerticalBox> ItemListBox;
	FString CachedSignature;
};

namespace NUInventoryDebugUI
{
	// 로컬(첫 번째) 플레이어의 인벤토리 컴포넌트 조회 — 콘솔 명령/오버레이 공용
	UNUInventoryComponent* FindLocalInventory(UWorld* World, bool bWarnIfMissing = true);

	// 로컬 플레이어가 전투 중인지 (복제되는 Encounter 참조 기준)
	bool IsLocalPlayerInBattle(UWorld* World);

	// 월드(=PIE 창)별 오버레이 표시/숨김 토글
	void Toggle(UWorld* World);
}

#endif // !UE_BUILD_SHIPPING
