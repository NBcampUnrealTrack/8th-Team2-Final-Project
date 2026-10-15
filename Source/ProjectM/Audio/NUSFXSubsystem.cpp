// Fill out your copyright notice in the Description page of Project Settings.

#include "Audio/NUSFXSubsystem.h"

#include "Audio/NUAudioSettings.h"
#include "Audio/NUSFXConfig.h"
#include "Audio/NUSFXTags.h"

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

// 어떤 효과음이 불리는지 보려면 콘솔에서 log LogNUSFX Verbose
DEFINE_LOG_CATEGORY_STATIC(LogNUSFX, Log, All);

// 효과음을 이름으로 들어 본다 (PIE·게임 콘솔): nu.SFX.Play UI.Click --- 앞의 NU.SFX. 는 생략해도 된다
static FAutoConsoleCommandWithWorldAndArgs GNUSFXPlayCommand(
	TEXT("nu.SFX.Play"),
	TEXT("효과음 태그를 재생한다 --- nu.SFX.Play UI.Click"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.IsEmpty())
		{
			return;
		}

		const FString TagName = Args[0].StartsWith(TEXT("NU.SFX.")) ? Args[0] : TEXT("NU.SFX.") + Args[0];
		UNUSFXSubsystem::Play(World, FGameplayTag::RequestGameplayTag(FName(*TagName), false));
	}));

void UNUSFXSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UNUAudioSettings* Settings = GetDefault<UNUAudioSettings>();
	if (!IsValid(Settings))
	{
		return;
	}

	for (const TSoftObjectPtr<UNUSFXConfig>& ConfigPath : Settings->SFXConfigs)
	{
		if (UNUSFXConfig* LoadedConfig = ConfigPath.LoadSynchronous())
		{
			Configs.Add(LoadedConfig);
		}
		else if (!ConfigPath.IsNull())
		{
			UE_LOG(LogTemp, Warning, TEXT("SFXSubsystem: 효과음 표를 읽지 못함 (%s)"), *ConfigPath.ToString());
		}
	}
}

UNUSFXSubsystem* UNUSFXSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine != nullptr
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	const UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUSFXSubsystem>() : nullptr;
}

void UNUSFXSubsystem::Play(const UObject* WorldContextObject, FGameplayTag Event, float VolumeScale)
{
	UNUSFXSubsystem* Self = Get(WorldContextObject);

	// 호버·커서 이동·발소리는 누른 동작과 상관없이 나므로 공용음을 가리지 않는다
	if (IsValid(Self) && Self->PlayEntry(Event, VolumeScale)
		&& Event != NU::SFX::UI::Hover && Event != NU::SFX::UI::Cursor && Event != NU::SFX::Field::Footstep
		&& Event != NU::SFX::Field::FootstepMale && Event != NU::SFX::Field::FootstepArmor)
	{
		Self->LastActionFrame = GFrameCounter;
	}
}

void UNUSFXSubsystem::PlayCommon(const UObject* WorldContextObject, FGameplayTag Event)
{
	UNUSFXSubsystem* Self = Get(WorldContextObject);
	if (!IsValid(Self))
	{
		return;
	}

	// 입력 처리는 프레임 앞쪽에서 끝나고 코어 티커는 프레임 끝에 돈다 --- 버튼이 낼 소리는 그 사이에 이미 났다
	const uint64 Frame = GFrameCounter;
	const uint32 Serial = ++Self->CommonSerial;
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(Self, [Self, Event, Frame, Serial](float)
	{
		if (Self->LastActionFrame < Frame && Self->CommonSerial == Serial)
		{
			Self->PlayEntry(Event, 1.0f);
		}

		return false;
	}));
}

FGameplayTag UNUSFXSubsystem::FindSourceEvent(const UObject* WorldContextObject, const FSoftObjectPath& Source)
{
	const UNUSFXSubsystem* Self = Source.IsNull() ? nullptr : Get(WorldContextObject);
	if (IsValid(Self))
	{
		for (const UNUSFXConfig* Config : Self->Configs)
		{
			const FGameplayTag Event = Config->FindSourceEvent(Source);
			if (Event.IsValid())
			{
				return Event;
			}
		}
	}

	return FGameplayTag();
}

bool UNUSFXSubsystem::PlayEntry(const FGameplayTag& Event, float VolumeScale)
{
	const FNUSFXEntry* Entry = nullptr;
	for (const UNUSFXConfig* Config : Configs)
	{
		Entry = Config->FindEntry(Event);
		if (Entry != nullptr)
		{
			break;
		}
	}

	if (Entry == nullptr || Entry->Sounds.IsEmpty())
	{
		return false;
	}

	// 빈 칸(지워진 소리 등)을 골랐으면 안 난 것으로 친다 --- 난 것으로 치면 같은 프레임의 공용음까지 막힌다
	USoundBase* Sound = Entry->Sounds[FMath::RandHelper(Entry->Sounds.Num())];
	if (Sound == nullptr)
	{
		return false;
	}

	if (Entry->MinInterval > 0.0f)
	{
		const double Now = FPlatformTime::Seconds();
		double& Last = LastPlaySeconds.FindOrAdd(Event);
		if (Now - Last < Entry->MinInterval)
		{
			return false;
		}

		Last = Now;
	}

	UE_LOG(LogNUSFX, Verbose, TEXT("%s => %s"), *Event.ToString(), *GetNameSafe(Sound));
	const float Volume = Entry->Volume * VolumeScale;
	const float Pitch = 1.0f + FMath::FRandRange(-Entry->PitchRandom, Entry->PitchRandom);

	// 전용 서버와 소리를 끈 실행에서는 엔진이 여기서 걸러 준다
	UGameplayStatics::PlaySound2D(GetGameInstance()->GetWorld(), Sound, Volume, Pitch);

	return true;
}
