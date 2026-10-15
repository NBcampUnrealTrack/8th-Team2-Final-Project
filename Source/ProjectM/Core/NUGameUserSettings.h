// 사용자별 설정 --- 엔진 UGameUserSettings(화면·품질·프레임)에 볼륨과 업스케일 모드를 얹는다. 저장도 엔진과 같은 GameUserSettings.ini.
// 엔진은 DefaultEngine.ini의 GameUserSettingsClassName으로 이 클래스를 만든다.
//
// 볼륨은 기본 사운드 믹스(AudioSettings의 DefaultBaseSoundMix)에 사운드 클래스별 볼륨 오버라이드로 건다:
// 애셋은 건드리지 않고, 마스터는 자식 클래스(BGM/SFX)까지 곱해진다. 어느 클래스가 무엇인지는 NUAudioSettings가 안다.
// 거는 시점: 엔진이 시작 때 ApplyNonResolutionSettings를 불러 준다(게임 실행). PIE처럼 오디오 디바이스가 그 뒤에 생기는
// 경우는 UNUUIManager가 로컬 플레이어 생성 때 ApplyAudioVolumes를 다시 부른다. 옵션창은 슬라이더를 움직일 때마다 부른다.
// 값의 출처(로컬 ini)와 거는 방법이 분리돼 있어, 나중에 계정 동기화로 바꿔도 LoadSettings/SaveSettings 쪽만 손대면 된다.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "NUGameUserSettings.generated.h"

UCLASS()
class PROJECTM_API UNUGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	// 엔진이 들고 있는 설정 객체. 클래스 이름이 ini에 잡혀 있지 않으면 설정 오류라 바로 알 수 있게 CastChecked
	static UNUGameUserSettings* Get();

	float GetMasterVolume() const { return MasterVolume; }
	void SetMasterVolume(float Value) { MasterVolume = FMath::Clamp(Value, 0.0f, 1.0f); }

	float GetBgmVolume() const { return BgmVolume; }
	void SetBgmVolume(float Value) { BgmVolume = FMath::Clamp(Value, 0.0f, 1.0f); }

	float GetSfxVolume() const { return SfxVolume; }
	void SetSfxVolume(float Value) { SfxVolume = FMath::Clamp(Value, 0.0f, 1.0f); }

	// 지금 값을 살아 있는 오디오 디바이스 전부에 건다. 저장은 하지 않는다 --- 그건 ApplySettings/SaveSettings의 일
	void ApplyAudioVolumes() const;

	// 통합 메뉴 인벤토리의 칸 보기 (false 큰 칸 / true 작은 칸) --- 메뉴를 다시 열거나 게임을 다시 켜도 고른 보기로 열린다.
	// 옵션 창 항목이 아니라 페이지의 보기 버튼이 바꾸고 바로 저장한다
	bool IsInventoryCompactView() const { return bInventoryCompactView; }
	void SetInventoryCompactView(bool bCompact) { bInventoryCompactView = bCompact; }

	// 업스케일 모드 --- 0 끔 / 1 품질 / 2 균형 / 3 성능. 끔은 렌더 해상도를 화면 그대로(100%) 쓴다.
	// ★ 안티에일리어싱(TSR)은 모드와 무관하게 항상 켜진다 --- 이 모드가 정하는 것은 렌더 해상도 비율뿐이다.
	//    카메라 오클루전 페이드의 디더링이 시간 누적 없이는 모기장으로 보여서 끄는 선택지를 두지 않았다.
	// 켜면 엔진 내장 TSR로 낮은 해상도에서 그려 키운다 (스프라이트도 같이 재구성되므로 아트 확인이 필요한 항목).
	// 렌더 해상도 비율은 엔진의 ResolutionQuality에 같이 쓴다 --- 스케일러빌리티가 r.ScreenPercentage로 건다.
	// 나중에 DLSS/FSR 플러그인이 들어오면 같은 자리(시간축 업스케일러)에 꽂히므로 이 모드는 그대로 쓴다
	static constexpr int32 UpscaleModeCount = 4;
	int32 GetUpscaleMode() const { return UpscaleMode; }
	void SetUpscaleMode(int32 Mode);

	//~ Begin UGameUserSettings Interface
	virtual void ApplyNonResolutionSettings() override;
	virtual void SetToDefaults() override;
	virtual void ValidateSettings() override;
	//~ End UGameUserSettings Interface

private:
	// 0(무음)~1(원음)
	UPROPERTY(Config)
	float MasterVolume = 1.0f;

	UPROPERTY(Config)
	float BgmVolume = 1.0f;

	UPROPERTY(Config)
	float SfxVolume = 1.0f;

	UPROPERTY(Config)
	int32 UpscaleMode = 0;

	UPROPERTY(Config)
	bool bInventoryCompactView = false;
};
