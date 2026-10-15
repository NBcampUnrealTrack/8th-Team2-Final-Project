#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProjectMCombatArtLibrary.generated.h"

// 전투 그림(PaperZD 스킨 · 공격 시퀀스)을 에디터 파이썬에서 연결하는 도구
// 스킨의 시퀀스 → 플립북 표와 시퀀스의 알림 시각은 파이썬에 안 열려 있어 C++ 로 둠
// 에셋 복제 · 저장 · 몸 줄(ini) 연결은 파이썬 쪽이 함 (예: 여캐 직업 전투 그림, 2026-10-13)
UCLASS()
class PROJECTMEDITOR_API UProjectMCombatArtLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 시퀀스의 길이와 Hit · Cast 표시 시각(초), 없는 표시는 -1
	// 예: "Duration=0.920 Hit=0.400 Cast=0.250 Source=/Game/.../MCh_Gambler_Body_Combat_Attack"
	UFUNCTION(BlueprintCallable, Category = "ProjectM|CombatArt")
	static FString DescribeCombatSequence(const FString& SequencePath);

	// 스킨의 시퀀스 → 플립북 표를 사람이 읽는 줄로 (시퀀스=플립북들 | 좌우반전)
	UFUNCTION(BlueprintCallable, Category = "ProjectM|CombatArt")
	static FString DescribeCombatSkin(const FString& SkinPath);

	// 공격 시퀀스의 원본 플립북(레이어 0)을 바꾸고 Hit · Cast 표시를 그 시각(초)에 둠
	// CastTime 이 0 보다 작으면 Cast 표시를 지움, 표시가 없으면 새로 만듦. 결과 설명을 돌려줌 (실패면 "ERROR: ...")
	UFUNCTION(BlueprintCallable, Category = "ProjectM|CombatArt")
	static FString SetupCombatAttackSequence(const FString& SequencePath, const FString& BodyFlipbookPath,
		float HitTime, float CastTime);

	// 스킨의 시퀀스 → 플립북 표를 통째로 다시 씀 (두 배열은 같은 길이)
	// 플립북 경로가 비면 그 시퀀스 칸은 비워 둠 — 그 레이어는 그 동작에서 안 그려짐
	// 좌우반전 칸은 기존 표의 첫 줄 값을 그대로 씀
	UFUNCTION(BlueprintCallable, Category = "ProjectM|CombatArt")
	static FString SetCombatSkinEntries(const FString& SkinPath, const TArray<FString>& SequencePaths,
		const TArray<FString>& FlipbookPaths);
};
