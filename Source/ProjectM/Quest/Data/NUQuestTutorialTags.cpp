#include "Quest/Data/NUQuestTutorialTags.h"

namespace NU::Quest::Objective
{
	UE_DEFINE_GAMEPLAY_TAG(TutorialAction, "NU.Quest.Objective.TutorialAction");
	UE_DEFINE_GAMEPLAY_TAG(NaiaCalmed, "NU.Quest.Objective.NaiaCalmed");
	UE_DEFINE_GAMEPLAY_TAG(JobSelected, "NU.Quest.Objective.JobSelected");
	UE_DEFINE_GAMEPLAY_TAG(StarterContracted, "NU.Quest.Objective.StarterContracted");
	UE_DEFINE_GAMEPLAY_TAG(SparringWon, "NU.Quest.Objective.SparringWon");
}

namespace NU::Quest::Cue
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Calm, "GameplayCue.Battle.Quest.Calm", "진정 전투 큐 — 달래기로 동요가 줄었을 때 나이아에게");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(CalmFail, "GameplayCue.Battle.Quest.CalmFail", "진정 전투 큐 — 달래기가 닿지 않거나 동요가 올랐을 때 나이아에게");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(MoveStone, "GameplayCue.Battle.Quest.MoveStone", "진정 전투 큐 — 돌 옮기기로 돌이 밀릴 때 돌 자리에");
}
