#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCharacterNaiaPage.generated.h"

class ANUPlayerState;
class UButton;
class UNUNaiaRosterComponent;
struct FNUNaiaInstance;

// One party or storage slot as the page shows it. Built from the roster without touching widgets,
// so the selection and slot rules can be tested on their own.
struct FNUCharacterNaiaSlotView
{
	bool bFilled = false;
	int32 Container = 0;		// NUNaiaContainer::Party / Storage
	int32 SlotNo = INDEX_NONE;
	int64 InstanceId = 0;
	int32 NaiaId = 0;
	int32 Level = 0;
	int32 Affection = 0;
	float HealthRatio = 1.f;
	int32 PassiveSkillId = 0;
	TArray<int32> ActiveSkillIds;
};

namespace NUCharacterNaiaView
{
	// Party slots come from the spawned spirit states; storage from the replicated instances.
	PROJECTM_API TArray<FNUCharacterNaiaSlotView> CollectParty(const UNUNaiaRosterComponent& Roster, int32 SlotCount);
	PROJECTM_API TArray<FNUCharacterNaiaSlotView> CollectStorage(const TArray<FNUNaiaInstance>& Instances, int32 SlotCount);

	// Keeps the previous selection while it still holds the same Naia; otherwise the first filled
	// party slot, then the first filled storage slot. Returns false when the roster is empty.
	PROJECTM_API bool ResolveSelection(const TArray<FNUCharacterNaiaSlotView>& Party, const TArray<FNUCharacterNaiaSlotView>& Storage,
		int64 PreviousInstanceId, int32& OutContainer, int32& OutSlotNo);

	// First empty slot, or INDEX_NONE when the container is full.
	PROJECTM_API int32 FindFreeSlot(const TArray<FNUCharacterNaiaSlotView>& Slots);
}

// Unified character menu, Naia tab (WBP_CharacterNaiaPage). Fills the designer's named widgets:
// PartySlot{0..3}{Button,Portrait,Level,Number,Empty,HealthFill}, StorageSlot{0..11}{Button,Portrait,Name,Level,EmptyMark},
// PartyHeaderCount, StorageHeaderCount, SelectedNaia{Name,Level,Classification,Portrait}, SelectedLocation{Label,Hint},
// Stat*Value, ActiveSkill2101/2103 rows (first and second active skill), Passive{Name,Kind,Description,EmptyMark},
// MoveToStorageButton / MoveToStorageLabel. Every name is optional.
UCLASS()
class PROJECTM_API UNUCharacterNaiaPage : public UUserWidget
{
	GENERATED_BODY()

public:
	void Refresh();
	void SelectSlot(int32 Container, int32 SlotNo);

	int32 GetSelectedContainer() const { return SelectedContainer; }
	int32 GetSelectedSlotNo() const { return SelectedSlotNo; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION() void HandleRosterChanged();
	UFUNCTION() void HandleMoveClicked();
	UFUNCTION() void HandlePartySlot0() { SelectSlot(1, 0); }
	UFUNCTION() void HandlePartySlot1() { SelectSlot(1, 1); }
	UFUNCTION() void HandlePartySlot2() { SelectSlot(1, 2); }
	UFUNCTION() void HandlePartySlot3() { SelectSlot(1, 3); }
	UFUNCTION() void HandleStorageSlot0() { SelectSlot(2, 0); }
	UFUNCTION() void HandleStorageSlot1() { SelectSlot(2, 1); }
	UFUNCTION() void HandleStorageSlot2() { SelectSlot(2, 2); }
	UFUNCTION() void HandleStorageSlot3() { SelectSlot(2, 3); }
	UFUNCTION() void HandleStorageSlot4() { SelectSlot(2, 4); }
	UFUNCTION() void HandleStorageSlot5() { SelectSlot(2, 5); }
	UFUNCTION() void HandleStorageSlot6() { SelectSlot(2, 6); }
	UFUNCTION() void HandleStorageSlot7() { SelectSlot(2, 7); }
	UFUNCTION() void HandleStorageSlot8() { SelectSlot(2, 8); }
	UFUNCTION() void HandleStorageSlot9() { SelectSlot(2, 9); }
	UFUNCTION() void HandleStorageSlot10() { SelectSlot(2, 10); }
	UFUNCTION() void HandleStorageSlot11() { SelectSlot(2, 11); }

	UNUNaiaRosterComponent* GetRoster() const;
	void BindButtons(bool bBind);
	void FillSlots();
	void FillDetail();
	const FNUCharacterNaiaSlotView* FindView(int32 Container, int32 SlotNo) const;
	void SetText(const TCHAR* Name, const FText& Text) const;
	void SetShown(const TCHAR* Name, bool bShown) const;
	// TargetHeight > 0 scales the pixel art by a whole number so it stays crisp.
	void SetPortrait(const TCHAR* Name, int32 NaiaId, float TargetHeight = 0.f) const;

	TWeakObjectPtr<UNUNaiaRosterComponent> BoundRoster;
	TArray<FNUCharacterNaiaSlotView> PartyViews;
	TArray<FNUCharacterNaiaSlotView> StorageViews;
	int32 SelectedContainer = 0;
	int32 SelectedSlotNo = INDEX_NONE;
	int64 SelectedInstanceId = 0;
};
