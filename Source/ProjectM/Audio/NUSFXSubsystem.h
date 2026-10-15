// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUSFXSubsystem.generated.h"

class UNUSFXConfig;

/*
효과음 재생

소리를 낼 곳은 Play(this, NU::SFX::UI::Click) 한 줄만 부른다.
어떤 소리가 나는지·음량·연타 간격은 효과음 표(UNUSFXConfig)가 정하고, 표에 없는 태그는 소리 없이 넘어간다.
전부 2D 재생이다 --- 화면이 고정 구도라 위치에 따른 감쇠를 쓰지 않는다
*/
UCLASS()
class PROJECTM_API UNUSFXSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// WorldContextObject 는 월드에 속한 아무 객체(위젯·액터·컴포넌트·서브시스템)
	UFUNCTION(BlueprintCallable, Category = "NU|Audio", meta = (WorldContext = "WorldContextObject", Categories = "NU.SFX"))
	static void Play(const UObject* WorldContextObject, FGameplayTag Event, float VolumeScale = 1.0f);

	// 공용음(버튼 클릭, 알림, 창 열림·닫힘, 확인·취소)을 낸다 --- 같은 프레임에 그 동작만의 소리(Play)가 났으면 내지 않는다.
	// 버튼을 눌러 탭이 바뀌면 탭 소리만, 확인을 눌러 아이템을 버리면 버리는 소리만 난다. 그것을 보려고 프레임 끝까지 기다렸다 낸다.
	// 공용음끼리 겹치면 마지막에 불린 것만 난다 (버튼을 눌러 창이 열리면 창 소리만)
	static void PlayCommon(const UObject* WorldContextObject, FGameplayTag Event);

	// 그 애셋(전투 이펙트)에 걸린 소리 태그, 표에 없으면 빈 태그
	static FGameplayTag FindSourceEvent(const UObject* WorldContextObject, const FSoftObjectPath& Source);

private:
	static UNUSFXSubsystem* Get(const UObject* WorldContextObject);

	// 표에서 찾아 재생한다 => 소리가 났으면 true
	bool PlayEntry(const FGameplayTag& Event, float VolumeScale);

	// 표를 쥐고 있어야 소리 애셋이 메모리에 남아 첫 재생이 끊기지 않는다
	UPROPERTY(Transient)
	TArray<TObjectPtr<UNUSFXConfig>> Configs;

	// 태그별 마지막 재생 시각 (FNUSFXEntry::MinInterval 용)
	TMap<FGameplayTag, double> LastPlaySeconds;

	// 그 동작만의 소리가 마지막으로 난 프레임 / 공용음이 불린 횟수 (PlayCommon 이 본다)
	uint64 LastActionFrame = 0;
	uint32 CommonSerial = 0;
};
