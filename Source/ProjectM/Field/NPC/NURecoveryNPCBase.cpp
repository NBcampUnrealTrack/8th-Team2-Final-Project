#include "Field/NPC/NURecoveryNPCBase.h"

#include "Recovery/Component/NURecoveryProviderComponent.h"

ANURecoveryNPCBase::ANURecoveryNPCBase()
{
	RecoveryProvider = CreateDefaultSubobject<UNURecoveryProviderComponent>(TEXT("RecoveryProvider"));
}
