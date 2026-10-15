#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCharacterMenuBase.generated.h"

class UButton;
class UNamedSlot;
class UWidgetAnimation;
class ANUPlayerState;
class UNUCharacterManager;

UENUM(BlueprintType)
enum class ENUCharacterMenuSection : uint8
{
	Character,
	Equipment,
	Stats,
	Traits,
	Naia,
	// Append to preserve the section values already serialized in Widget Blueprints.
	Inventory
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNUCharacterMenuSectionChanged, ENUCharacterMenuSection, Section);

// Shared shell only. Each feature supplies its own page; all layout and artwork live in the WBP.
UCLASS(Abstract)
class PROJECTM_API UNUCharacterMenuBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UNUCharacterMenuBase(const FObjectInitializer& ObjectInitializer);

	// Uses the existing window manager for input, cursor, focus and ESC ownership.
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	static UNUCharacterMenuBase* Show(APlayerController* OwningPlayer, TSubclassOf<UNUCharacterMenuBase> WidgetClass,
		ENUCharacterMenuSection InitialSection = ENUCharacterMenuSection::Character);

	// Plays the WBP's CloseFade first when it has one, then closes.
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void RequestClose();

	// Skips CloseFade --- for closes forced by another system (battle start, trade).
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void CloseImmediately();

	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void SelectSection(ENUCharacterMenuSection Section);

	// Registered instances retain their own selection/scroll state when another page is shown.
	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void RegisterPage(ENUCharacterMenuSection Section, UWidget* Page);

	UFUNCTION(BlueprintPure, Category = "NU|CharacterMenu")
	ENUCharacterMenuSection GetSelectedSection() const { return SelectedSection; }

	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void RefreshProfile();

	UPROPERTY(BlueprintAssignable, Category = "NU|CharacterMenu")
	FNUCharacterMenuSectionChanged OnSectionChanged;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

	// Optional lazy creation. Unassigned pages leave the content area empty for later integration.
	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu")
	TMap<ENUCharacterMenuSection, TSubclassOf<UUserWidget>> PageClasses;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "NU|CharacterMenu")
	ENUCharacterMenuSection SelectedSection = ENUCharacterMenuSection::Character;

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	FLinearColor SelectedNavigationColor = FLinearColor(0.026f, 0.057f, 0.10f, 1.f);
	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Style")
	FLinearColor IdleNavigationColor = FLinearColor(0.87f, 0.88f, 0.85f, 1.f);

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UNamedSlot> NamedSlot_PageContent;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NavCharacterButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NavEquipmentButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> NavInventoryButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NavStatsButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NavTraitsButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> NavNaiaButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CloseButton;

	// Optional close transition. The window stays registered (and ignores input) until it finishes.
	UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) TObjectPtr<UWidgetAnimation> CloseFade;

private:
	void UpdateNavigation();
	void DisplaySelectedPage();
	UFUNCTION() void HandleCharacter() { SelectSection(ENUCharacterMenuSection::Character); }
	UFUNCTION() void HandleEquipment() { SelectSection(ENUCharacterMenuSection::Equipment); }
	UFUNCTION() void HandleInventory() { SelectSection(ENUCharacterMenuSection::Inventory); }
	UFUNCTION() void HandleStats() { SelectSection(ENUCharacterMenuSection::Stats); }
	UFUNCTION() void HandleTraits() { SelectSection(ENUCharacterMenuSection::Traits); }
	UFUNCTION() void HandleNaia() { SelectSection(ENUCharacterMenuSection::Naia); }
	UFUNCTION() void HandleCloseFadeFinished();

	bool bClosing = false;

	UPROPERTY(Transient)
	TMap<ENUCharacterMenuSection, TObjectPtr<UWidget>> CachedPages;
	TWeakObjectPtr<ANUPlayerState> ProfileState;
	TWeakObjectPtr<UNUCharacterManager> ProfileCharacters;
};
