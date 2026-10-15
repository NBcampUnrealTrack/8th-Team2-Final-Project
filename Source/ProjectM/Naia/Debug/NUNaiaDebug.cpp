// 나이아 로스터 디버그 콘솔 명령 (nu.Naia.*) — 개발 전용, Shipping 제외.
// 등록 방식은 self-registering static (FAutoConsoleCommandWithWorldAndArgs).

#if !UE_BUILD_SHIPPING

#include "CharacterState/NUPlayerState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Naia/Component/NUNaiaRosterComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUNaiaDebug, Log, All);

namespace
{
	UNUNaiaRosterComponent* FindLocalRoster(UWorld* World)
	{
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APlayerState* PS = PC ? PC->PlayerState : nullptr;
		UNUNaiaRosterComponent* Roster = PS ? PS->FindComponentByClass<UNUNaiaRosterComponent>() : nullptr;

		if (Roster == nullptr)
		{
			UE_LOG(LogNUNaiaDebug, Warning, TEXT("[nu.Naia] 나이아 로스터 컴포넌트 없음 — PlayerState가 ANUPlayerState인지 확인"));
		}

		return Roster;
	}
}

// 서버 로스터가 아직 비어 있을 때(계정에 나이아가 없을 때) 로컬로 한 마리 넣어보는 용도.
// 여기서 넣은 개체는 InstanceId가 음수인 임시값
static FAutoConsoleCommandWithWorldAndArgs GDebugAcquireCmd(
	TEXT("nu.Naia.DebugAcquire"),
	TEXT("로컬 나이아 획득: nu.Naia.DebugAcquire <NaiaId> [Level]  (예: nu.Naia.DebugAcquire 1001 10)"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic([](const TArray<FString>& Args, UWorld* World)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogNUNaiaDebug, Warning, TEXT("[nu.Naia.DebugAcquire] 사용법: nu.Naia.DebugAcquire <NaiaId> [Level]"));
			return;
		}

		UNUNaiaRosterComponent* Roster = FindLocalRoster(World);
		if (Roster == nullptr)
		{
			return;
		}

		const int32 NaiaId = FCString::Atoi(*Args[0]);
		const int32 Level = Args.Num() >= 2 ? FCString::Atoi(*Args[1]) : 0;

		// 서버 RPC — Standalone이라 즉시 로컬 실행됨
		Roster->ServerDebugAcquireNaia(NaiaId, Level);
	}));

#endif
