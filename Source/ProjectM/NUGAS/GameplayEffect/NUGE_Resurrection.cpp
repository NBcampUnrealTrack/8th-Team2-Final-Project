#include "NUGAS/GameplayEffect/NUGE_Resurrection.h"

#include "NUGAS/Attribute/NUVitalAttributeSet.h"
#include "NUGAS/NUGameplayTags.h"

UNUGE_Resurrection::UNUGE_Resurrection()
{
	// 부활도 부모의 HP 회복만 사용한다. SP는 사망 직전 값을 유지한다.
}
