// Enhanced Input 사용자 설정(키 재설정 저장본)의 프로젝트 클래스 --- 엔진은 DefaultInput.ini의 UserSettingsClass / DefaultPlayerMappableKeyProfileClass로 이 둘을 만든다.
//
// 왜 상속하나: 엔진은 키 설정을 적용(RebuildControlMappings)할 때 IMC의 기본 매핑 하나하나에 "같은 장치 종류(패드/키보드)의 플레이어 키"만 바꿔 끼운다.
// 그래서 IMC에 패드 기본 매핑이 없는 동작(캐릭터 정보 등)에 키 설정 창이 새 슬롯으로 준 패드 키는 표기·저장만 되고 어디에도 안 붙는다.
// 프로필이 리빌드 질의를 재정의해 그런 패드 키를 키보드 매핑 자리에 같이 얹는다 (엔진 주석이 열어 둔 확장점).
//
// 사용자 설정 클래스도 바꾸는 이유: 저장본(Saved/SaveGames/EnhancedInputUserSettings.sav)에는 옛 클래스의 프로필 객체가 들어 있어
// 프로필 클래스만 바꿔서는 안 먹는다 (설정 클래스가 바뀌어 엔진이 설정 객체를 새로 만들 때도 옛 프로필 객체를 그대로 넘겨받는다).
// 그래서 Initialize에서 활성 프로필이 UNUKeyProfile이 아니면 새로 만들어 쓴다 = 키 설정 1회 초기화 (옛 프로필은 저장본에 남지만 안 쓴다).
// 그 밖에 시작 때 키 설정 IMC 등록(프로필에 행이 생기고 저장본의 키에 기본 키가 채워진다)을 맡는다.
#pragma once

#include "CoreMinimal.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "NUInputUserSettings.generated.h"

UCLASS()
class PROJECTM_API UNUKeyProfile : public UEnhancedPlayerMappableKeyProfile
{
	GENERATED_BODY()

public:
	// 엔진(Super)은 기본 매핑과 같은 장치 종류의 키만 고른다. 기본 키가 없는 슬롯(키 설정 창이 패드 키를 위해 새로 만든 것)의
	// 패드 키는 IMC에 짝이 없으니 키보드 매핑 줄에 더한다
	virtual int32 GetPlayerMappedKeysForRebuildControlMappings(const FEnhancedActionKeyMapping& DefaultMapping, TArray<FKey>& OutKeys) const override;
};

UCLASS()
class PROJECTM_API UNUInputUserSettings : public UEnhancedInputUserSettings
{
	GENERATED_BODY()

public:
	// 활성 프로필이 UNUKeyProfile이 아니면 새로 만들고, 프로젝트 세팅(NU UI)의 KeyBindingContexts를 등록한다 --- 게임 코드가 IMC를 붙일 때는
	// 등록하지 않으므로(엔진 기본값) 여기서 한다. 불러온 직후 첫 리빌드(ApplySettings)보다 앞이라 저장본의 키가 처음부터 제 슬롯(기본 키)과 짝지어진다
	virtual void Initialize(ULocalPlayer* LP) override;
};
