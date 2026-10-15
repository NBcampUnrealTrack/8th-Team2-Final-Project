#include "UI/CharacterMenu/NUCharacterNaiaPage.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Naia/Data/NUNaiaTypes.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	FNUNaiaInstance MakeInstance(int64 InstanceId, int32 NaiaId, int32 SlotNo, int32 Level)
	{
		FNUNaiaInstance Instance;
		Instance.InstanceId = InstanceId;
		Instance.NaiaId = NaiaId;
		Instance.SlotNo = SlotNo;
		Instance.Level = Level;
		Instance.HealthRatio = .5f;
		Instance.PassiveSkillId = 7;
		Instance.ActiveSkillIds = { 2101, 2103 };
		return Instance;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUCharacterNaiaSlotRulesTest, "ProjectM.UI.CharacterNaia.SlotRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUCharacterNaiaSlotRulesTest::RunTest(const FString& Parameters)
{
	// Storage instances land in their own slot numbers; out-of-range and empty ids are ignored.
	const TArray<FNUNaiaInstance> Instances = { MakeInstance(11, 1001, 3, 12), MakeInstance(12, 1002, 0, 5),
		MakeInstance(13, 1003, 40, 9), MakeInstance(14, 0, 1, 1) };
	const TArray<FNUCharacterNaiaSlotView> Storage = NUCharacterNaiaView::CollectStorage(Instances, 12);
	TestEqual(TEXT("Storage has a view per slot"), Storage.Num(), 12);
	TestTrue(TEXT("Slot 0 filled"), Storage[0].bFilled);
	TestTrue(TEXT("Slot 3 filled"), Storage[3].bFilled);
	TestFalse(TEXT("Empty Naia id is not shown"), Storage[1].bFilled);
	TestEqual(TEXT("Slot 3 keeps its level"), Storage[3].Level, 12);
	TestEqual(TEXT("Slot 3 keeps its skills"), Storage[3].ActiveSkillIds.Num(), 2);
	TestEqual(TEXT("Container is storage"), Storage[3].Container, NUNaiaContainer::Storage);
	TestEqual(TEXT("Filled count ignores the out-of-range slot"),
		Storage.FilterByPredicate([](const FNUCharacterNaiaSlotView& View) { return View.bFilled; }).Num(), 2);
	TestEqual(TEXT("First free storage slot"), NUCharacterNaiaView::FindFreeSlot(Storage), 1);

	TArray<FNUCharacterNaiaSlotView> Full = Storage;
	for (FNUCharacterNaiaSlotView& View : Full) { View.bFilled = true; }
	TestEqual(TEXT("Full container has no free slot"), NUCharacterNaiaView::FindFreeSlot(Full), (int32)INDEX_NONE);

	// Selection: previous Naia kept, else first party, else first storage, else nothing.
	TArray<FNUCharacterNaiaSlotView> Party;
	Party.SetNum(4);
	for (int32 Index = 0; Index < Party.Num(); ++Index) { Party[Index].Container = NUNaiaContainer::Party; Party[Index].SlotNo = Index; }
	int32 Container = 0, SlotNo = INDEX_NONE;
	TestTrue(TEXT("Storage-only roster selects storage"), NUCharacterNaiaView::ResolveSelection(Party, Storage, 0, Container, SlotNo));
	TestEqual(TEXT("…the first filled storage slot"), SlotNo, 0);
	TestEqual(TEXT("…in storage"), Container, NUNaiaContainer::Storage);

	NUCharacterNaiaView::ResolveSelection(Party, Storage, 11, Container, SlotNo);
	TestEqual(TEXT("Previous selection is kept by instance id"), SlotNo, 3);

	Party[2].bFilled = true; Party[2].InstanceId = 21;
	NUCharacterNaiaView::ResolveSelection(Party, Storage, 999, Container, SlotNo);
	TestEqual(TEXT("A vanished selection falls back to the party"), Container, NUNaiaContainer::Party);
	TestEqual(TEXT("…its first filled slot"), SlotNo, 2);

	TArray<FNUCharacterNaiaSlotView> EmptyParty = Party, EmptyStorage = Storage;
	for (FNUCharacterNaiaSlotView& View : EmptyParty) { View.bFilled = false; }
	for (FNUCharacterNaiaSlotView& View : EmptyStorage) { View.bFilled = false; }
	TestFalse(TEXT("Empty roster selects nothing"), NUCharacterNaiaView::ResolveSelection(EmptyParty, EmptyStorage, 11, Container, SlotNo));
	TestEqual(TEXT("…no slot"), SlotNo, (int32)INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUCharacterNaiaPageContractTest, "ProjectM.UI.CharacterNaia.PageContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUCharacterNaiaPageContractTest::RunTest(const FString& Parameters)
{
	UClass* PageClass = LoadClass<UUserWidget>(nullptr,
		TEXT("/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterNaiaPage.WBP_CharacterNaiaPage_C"));
	if (!TestNotNull(TEXT("Naia page WBP loads"), PageClass)) return false;
	if (!TestTrue(TEXT("Naia page is driven by UNUCharacterNaiaPage"), PageClass->IsChildOf(UNUCharacterNaiaPage::StaticClass()))) return false;

	TStrongObjectPtr<UUserWidget> Page(NewObject<UUserWidget>(GetTransientPackage(), PageClass));
	Page->Initialize();
	TArray<FString> Required = { TEXT("PartyHeaderCount"), TEXT("StorageHeaderCount"), TEXT("SelectedNaiaName"), TEXT("SelectedNaiaLevel"),
		TEXT("SelectedNaiaClassification"), TEXT("SelectedNaiaPortrait"), TEXT("SelectedLocationLabel"), TEXT("StatMaxHealthValue"),
		TEXT("StatAttackPowerValue"), TEXT("StatDefenseValue"), TEXT("StatSpeedValue"), TEXT("ActiveSkill2101Name"), TEXT("ActiveSkill2103Name"),
		TEXT("PassiveName"), TEXT("PassiveDescription"), TEXT("MoveToStorageButton"), TEXT("MoveToStorageLabel") };
	for (int32 Index = 0; Index < 4; ++Index)
	{
		for (const TCHAR* Part : { TEXT("Button"), TEXT("Portrait"), TEXT("Empty") })
		{
			Required.Add(FString::Printf(TEXT("PartySlot%d%s"), Index, Part));
		}
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		for (const TCHAR* Part : { TEXT("Button"), TEXT("Portrait"), TEXT("Name"), TEXT("Level"), TEXT("EmptyMark") })
		{
			Required.Add(FString::Printf(TEXT("StorageSlot%d%s"), Index, Part));
		}
	}
	for (const FString& Name : Required)
	{
		TestNotNull(*FString::Printf(TEXT("Widget %s exists"), *Name), Page->GetWidgetFromName(*Name));
	}
	return true;
}
#endif
