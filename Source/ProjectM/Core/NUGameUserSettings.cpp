#include "Core/NUGameUserSettings.h"

#include "Audio/NUAudioSettings.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

// 볼륨 슬라이더 지각 곡선 지수 --- 1이면 선형, 크면 아래쪽에서 더 빨리 작아진다.
// BGM은 마스터 아래라 두 슬라이더를 같이 내리면 지수가 두 배로 먹는다 (2.0은 과했음)
static constexpr float NUAudio_VolumeCurveExponent = 1.25f;

// 업스케일 모드별 렌더 해상도(%). 품질/균형/성능은 TSR·DLSS·FSR이 공통으로 쓰는 비율
static constexpr float NUOptions_UpscalePercent[UNUGameUserSettings::UpscaleModeCount] = { 100.0f, 66.7f, 58.8f, 50.0f };

// r.AntiAliasingMethod 값 --- 4 TSR (엔진 EAntiAliasingMethod)
// 업스케일 모드와 무관하게 항상 이 값을 쓰므로 "없음"(0)은 더 두지 않는다
static constexpr int32 NUOptions_AAMethodTSR = 4;

UNUGameUserSettings* UNUGameUserSettings::Get()
{
	return CastChecked<UNUGameUserSettings>(UGameUserSettings::GetGameUserSettings());
}

void UNUGameUserSettings::ApplyNonResolutionSettings()
{
	Super::ApplyNonResolutionSettings(); // 스케일러빌리티(렌더 해상도 비율 포함)와 프레임 제한이 여기서 걸린다

	// 안티에일리어싱 방식은 프로젝트 설정(DefaultEngine.ini)이 프로젝트 우선순위로 잡고 있어 게임 설정 우선순위로는
	// 덮이지 않는다 --- 콘솔 우선순위로 쓴다. 엔진 시작과 [적용] 양쪽에서 이 함수를 지나므로 재시작 뒤에도 유지된다
	//
	// ★ 업스케일 모드와 상관없이 항상 TSR이다. 카메라 오클루전 페이드가 DitherTemporalAA로 그려지는데
	//    시간 누적이 없으면 흩뿌린 디더 점이 모기장처럼 그대로 보인다 --- 인게임에서 확인함.
	//    모드 0은 렌더 해상도가 100%라 해상도 손해 없이 AA만 얻는다
	if (IConsoleVariable* AAMethod = IConsoleManager::Get().FindConsoleVariable(TEXT("r.AntiAliasingMethod")))
	{
		AAMethod->Set(NUOptions_AAMethodTSR, ECVF_SetByConsole);
	}
	ApplyAudioVolumes();
}

void UNUGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();
	MasterVolume = 1.0f;
	BgmVolume = 1.0f;
	SfxVolume = 1.0f;
	SetUpscaleMode(0);
}

void UNUGameUserSettings::ValidateSettings()
{
	Super::ValidateSettings();
	// ini를 손으로 고친 값도 범위 안으로. 렌더 해상도 비율은 모드에서 다시 끌어낸다 (둘이 어긋나면 모드가 기준)
	MasterVolume = FMath::Clamp(MasterVolume, 0.0f, 1.0f);
	BgmVolume = FMath::Clamp(BgmVolume, 0.0f, 1.0f);
	SfxVolume = FMath::Clamp(SfxVolume, 0.0f, 1.0f);
	SetUpscaleMode(UpscaleMode);
}

void UNUGameUserSettings::SetUpscaleMode(int32 Mode)
{
	UpscaleMode = FMath::Clamp(Mode, 0, UpscaleModeCount - 1);
	SetResolutionScaleValueEx(NUOptions_UpscalePercent[UpscaleMode]);
}

void UNUGameUserSettings::ApplyAudioVolumes() const
{
	FAudioDeviceManager* Manager = GEngine != nullptr ? GEngine->GetAudioDeviceManager() : nullptr;
	if (Manager == nullptr)
	{
		return; // -nosound, 커맨드릿
	}

	// 기본 믹스는 디바이스가 시작할 때 스스로 올린다 --- 오버라이드만 걸어 두면 활성 여부와 무관하게 다음 갱신부터 먹는다
	USoundMix* BaseMix = Cast<USoundMix>(GetDefault<UAudioSettings>()->DefaultBaseSoundMix.TryLoad());
	if (BaseMix == nullptr)
	{
		return;
	}

	// 클래스가 비어 있으면 엔진이 그 줄을 무시한다
	const UNUAudioSettings* Audio = GetDefault<UNUAudioSettings>();
	USoundClass* MasterClass = Audio->MasterSoundClass.LoadSynchronous();
	USoundClass* BgmClass = Audio->BgmSoundClass.LoadSynchronous();
	USoundClass* SfxClass = Audio->SfxSoundClass.LoadSynchronous();
	// 디바이스 수와 무관하게 한 번만 계산한다
	const float MasterGain = FMath::Pow(MasterVolume, NUAudio_VolumeCurveExponent);
	const float BgmGain = FMath::Pow(BgmVolume, NUAudio_VolumeCurveExponent);
	const float SfxGain = FMath::Pow(SfxVolume, NUAudio_VolumeCurveExponent);

	// 마스터는 자식 클래스까지 --- BGM에는 마스터 x BGM이, 효과음에는 마스터 x 효과음이 곱해진다. 페이드 0: 슬라이더를 따라 바로 들리게
	Manager->IterateOverAllDevices([&](Audio::FDeviceId, FAudioDevice* Device)
	{
		Device->SetSoundMixClassOverride(BaseMix, MasterClass, MasterGain, 1.0f, 0.0f, true);
		Device->SetSoundMixClassOverride(BaseMix, BgmClass, BgmGain, 1.0f, 0.0f, true);
		Device->SetSoundMixClassOverride(BaseMix, SfxClass, SfxGain, 1.0f, 0.0f, true);
	});
}
