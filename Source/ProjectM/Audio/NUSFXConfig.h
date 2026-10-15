// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NUSFXConfig.generated.h"

class USoundBase;

// 효과음 한 줄 --- 태그 하나가 내는 소리
USTRUCT(BlueprintType)
struct FNUSFXEntry
{
	GENERATED_BODY()

	// 여러 개면 재생할 때마다 하나를 고른다 (발소리처럼 같은 소리가 되풀이되는 것).
	// 소리 애셋의 사운드 클래스는 SC_SFX 여야 효과음 슬라이더가 먹는다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX")
	TArray<TObjectPtr<USoundBase>> Sounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX", meta = (ClampMin = "0.0"))
	float Volume = 1.0f;

	// 재생할 때마다 음높이를 1 +- 이 값 안에서 흔든다 (0이면 그대로)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float PitchRandom = 0.0f;

	// 이 시간(초) 안에 다시 불리면 건너뛴다 --- 커서 이동·호버처럼 연타되는 소리가 겹쳐 뭉개지지 않게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SFX", meta = (ClampMin = "0.0"))
	float MinInterval = 0.0f;
};

// 효과음 태그(NU.SFX.*) -> 소리 표.
// 표는 영역별로 나눠 NU Audio 설정의 SFXConfigs 에 여러 장 건다 (uasset 은 병합이 안 되어 한 장에 다 모으면 충돌한다)
UCLASS()
class PROJECTM_API UNUSFXConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	const FNUSFXEntry* FindEntry(const FGameplayTag& Event) const
	{
		return Entries.Find(Event);
	}

	FGameplayTag FindSourceEvent(const FSoftObjectPath& Source) const
	{
		return SourceEvents.FindRef(Source);
	}

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", Categories = "NU.SFX"))
	TMap<FGameplayTag, FNUSFXEntry> Entries;

	// 화면에 뜨는 애셋(전투 이펙트) -> 그때 내는 소리. 같은 이펙트를 쓰는 스킬은 같은 소리가 난다 ---
	// 스킬마다 소리를 고르지 않고 보이는 것에 소리를 맞춘다 (화살 이펙트면 활 소리, 곰 발톱 이펙트면 할퀴는 소리)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true", AllowedClasses = "/Script/Niagara.NiagaraSystem", Categories = "NU.SFX"))
	TMap<FSoftObjectPath, FGameplayTag> SourceEvents;
};
