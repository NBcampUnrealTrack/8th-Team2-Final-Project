#include "Core/NUInputUserSettings.h"

#include "EnhancedActionKeyMapping.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "UI/NUUISettings.h"

int32 UNUKeyProfile::GetPlayerMappedKeysForRebuildControlMappings(const FEnhancedActionKeyMapping& DefaultMapping, TArray<FKey>& OutKeys) const
{
	Super::GetPlayerMappedKeysForRebuildControlMappings(DefaultMapping, OutKeys);
	if (DefaultMapping.Key.IsGamepadKey() || !DefaultMapping.IsPlayerMappable())
	{
		return OutKeys.Num();
	}
	if (const FKeyMappingRow* Row = FindKeyMappingRow(DefaultMapping.GetMappingName()))
	{
		for (const FPlayerKeyMapping& Mapping : Row->Mappings)
		{
			if (!Mapping.GetDefaultKey().IsValid() && Mapping.GetCurrentKey().IsGamepadKey())
			{
				OutKeys.AddUnique(Mapping.GetCurrentKey());
			}
		}
	}
	return OutKeys.Num();
}

void UNUInputUserSettings::Initialize(ULocalPlayer* LP)
{
	Super::Initialize(LP);
	if (Cast<UNUKeyProfile>(GetActiveKeyProfile()) == nullptr)
	{
		// 저장본에서 온 옛 클래스 프로필 --- 리빌드 질의 재정의가 없어 패드 키가 안 붙는다. 우리 클래스로 새로 만들어 활성으로 (키 설정 1회 초기화)
		UE_LOG(LogTemp, Log, TEXT("[키 설정] 키 프로필을 %s로 새로 만듭니다 (옛 %s) --- 키 설정이 기본값으로 돌아갑니다"),
			*UNUKeyProfile::StaticClass()->GetName(), *GetNameSafe(GetActiveKeyProfile() != nullptr ? GetActiveKeyProfile()->GetClass() : nullptr));
		FPlayerMappableKeyProfileCreationArgs Args;
		Args.ProfileType = UNUKeyProfile::StaticClass();
		Args.ProfileStringIdentifier = TEXT("NU");
		Args.UserId = LP != nullptr ? LP->GetPlatformUserId() : PLATFORMUSERID_NONE;
		Args.bSetAsCurrentProfile = true;
		CreateNewKeyProfile(Args);
	}
	for (const TSoftObjectPtr<UInputMappingContext>& Context : GetDefault<UNUUISettings>()->KeyBindingContexts)
	{
		const UInputMappingContext* Loaded = Context.LoadSynchronous();
		if (Loaded != nullptr && !IsMappingContextRegistered(Loaded))
		{
			RegisterInputMappingContext(Loaded);
		}
	}
}
