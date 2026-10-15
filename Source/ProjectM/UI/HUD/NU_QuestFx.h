// 퀘스트 수락·목표 달성·완료 순간 화면 위쪽에 잠깐 뜨는 띠 (Docs/HUD_QuestMap_Proposal_20261014).
// 주인 위젯(퀘스트 위젯) 안에 이름으로 놓인 조각을 찾아 움직인다 --- 조각이 없는 옛 WBP에서는 아무 일도 없다.
//
// 이름 계약 (전부 선택)
//   QuestFxRoot      띠 전체. 평소 Collapsed
//   QuestFxGlow      뒤의 짙은 그늘          QuestFxRays / QuestFxRing   완료 때만: 빛살, 퍼지는 고리 (평소 불투명도 0)
//   QuestFxCapRow / QuestFxCap (글자)       머리말 "새 의뢰" 등
//   QuestFxTitleRow / QuestFxTitle (글자)   제목 줄, QuestFxRuleL / QuestFxRuleR 양옆 금 줄
//   QuestFxSub (글자)                       아랫줄 (첫 목표, 보고 대상, 받은 보상)
//   QuestFxSpark0..7 (캔버스 자리) / QuestFxSpark0Img..7Img (그림, 평소 불투명도 0)   반짝이
//
// 여러 개가 겹치면 차례로 띄우고, 전투 화면·지역 진입 배너가 떠 있는 동안 생긴 것은 그것이 끝난 뒤 띄운다.
#pragma once

#include "CoreMinimal.h"
#include "UI/NUScreenMotion.h"

class UUserWidget;

class PROJECTM_API FNUQuestFx
{
public:
	enum class EKind : uint8
	{
		Accepted,
		Ready,
		Completed,
	};

	// 띠를 줄에 세우고 번호를 돌려준다. 보상 창은 HasStarted(번호) 뒤에 세기 시작한다 (띠가 기다리는 동안 먼저 뜨지 않게)
	int32 Queue(EKind Kind, const FText& Title, const FText& Detail);

	// 그 번호의 띠가 화면에 떴는지 (차례로 뜨므로 번호 비교로 충분하다). 주인이 닫혀 줄이 비워졌으면 true
	bool HasStarted(int32 Id) const { return Id <= LastStartedId; }

	// 주인 위젯의 NativeTick에서 부른다
	void Tick(UUserWidget* Owner, float DeltaTime);

	// 화면이 닫힐 때: 지금 것을 끝내고 줄을 비운다
	void Reset(UUserWidget* Owner);

	static float GetLength(EKind Kind);

private:
	struct FItem
	{
		EKind Kind = EKind::Accepted;
		FText Title;
		FText Detail;
		int32 Id = 0;
		double QueuedAt = 0.0;   // 줄에 들어온 시각 --- 같은 순간의 지역 배너가 뜨는지 잠깐 지켜본 뒤 시작한다
	};

	void Start(UUserWidget* Owner, const FItem& Item);
	void Hide(UUserWidget* Owner);

	TArray<FItem> Pending;
	FNUScreenMotion In;
	FNUScreenMotion Out;
	float Time = -1.f;        // 지금 띠의 경과 시간 (-1 = 없음)
	float Length = 0.f;
	int32 NextId = 0;
	int32 LastStartedId = 0;
};
