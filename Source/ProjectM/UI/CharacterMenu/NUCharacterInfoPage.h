#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUCharacterInfoPage.generated.h"

class ANUCharacterPreviewActor;
class ANUPlayerState;
class UImage;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UNUCharacterManager;
struct FOnAttributeChangeData;

// Content only: the containing character menu owns navigation, input and closing.
UCLASS(Abstract)
class PROJECTM_API UNUCharacterInfoPage : public UUserWidget
{

	GENERATED_BODY()

public:
	UNUCharacterInfoPage(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = "NU|CharacterMenu")
	void RefreshPage();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> CharacterPortrait;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidget> PortraitFallback;

	UPROPERTY(EditDefaultsOnly, Category = "NU|CharacterMenu|Preview")
	TSoftObjectPtr<UMaterialInterface> PortraitMaterial;

private:
	// Card dashboard: every card widget is optional and found by name, like the existing text rows.
	void RefreshVitalsAndStats(ANUPlayerState* State);
	void RefreshEquipment(ANUPlayerState* State);
	void RefreshNaiaPartner(ANUPlayerState* State);
	void HandleAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshPortrait(ANUPlayerState* State);
	void ReleasePortrait();
	void UnbindData();

	TWeakObjectPtr<ANUPlayerState> BoundState;
	TWeakObjectPtr<UNUCharacterManager> BoundCharacters;
	UPROPERTY(Transient) TObjectPtr<ANUCharacterPreviewActor> PreviewActor;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> PortraitInstance;
};
