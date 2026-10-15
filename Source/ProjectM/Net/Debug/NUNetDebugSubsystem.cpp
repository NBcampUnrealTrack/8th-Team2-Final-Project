#include "Net/Debug/NUNetDebugSubsystem.h"

#include "Chat/NUChatTypes.h"
#include "Chat/UI/NUChatWidget.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Field/FieldPlayerCharacter.h"
#include "Field/NUFieldTypes.h"
#include "Party/NUPartySubsystem.h"
#include "PlayerController/NUPlayerController.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Net/EmberServerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogNUNetDebug, Log, All);

static constexpr int64 NUNetDebugDefaultFakeId = 9001;

void UNUNetDebugSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

#if !UE_BUILD_SHIPPING
	RegisterCommands();
#endif
}

void UNUNetDebugSubsystem::Deinitialize()
{
	// 이미 전역 객체가 사라졌을 수 있기 때문에 등록된것만 삭제하도록
	IConsoleManager& Console = IConsoleManager::Get();

	TArray<IConsoleObject*> StillRegistered;
	Console.ForEachConsoleObjectThatStartsWith(
		FConsoleObjectVisitor::CreateLambda(
			[&StillRegistered](const TCHAR* /*Name*/, IConsoleObject* Object)
			{
				StillRegistered.Add(Object);
			}),
		TEXT("ember."));

	// 순회 중 해제하면 안되므로 모아놓고 삭제
	for (IConsoleCommand* Command : RegisteredCommands)
	{
		if (Command && StillRegistered.Contains(Command))
		{
			Console.UnregisterConsoleObject(Command);
		}
	}
	RegisteredCommands.Reset();

	Super::Deinitialize();
}

void UNUNetDebugSubsystem::RegisterCommands()
{
	IConsoleManager& Console = IConsoleManager::Get();

	// ── 실제 서버로 보내는 것 ──────────────────────────────────────────────

	// 클라 재시작 없이 다시 접속 시도할 때 사용
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FieldEnter"),
		TEXT("[진단] FieldEnter 재전송 (강제 재동기화)"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UFieldSyncSubsystem* Sync = GetFieldSync();
			if (Sync == nullptr)
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("FieldSyncSubsystem 없음 — 필드 레벨에서 실행하세요"));
				return;
			}
			UE_LOG(LogNUNetDebug, Warning, TEXT("FieldEnter 재전송 요청"));
			Sync->EnterField();
		}),
		ECVF_Default));

	// FieldActivity가 무응답인지 확인하는 용도
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FieldActivity"),
		TEXT("[진단] FieldActivity 단독 전송: ember.FieldActivity <0=Idle 2=Battle>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UEmberServerManager* Server = GetServer();
			if (Server == nullptr)
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("EmberServerManager 없음"));
				return;
			}

			const int32 Value = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 2;
			UE_LOG(LogNUNetDebug, Warning, TEXT("FieldActivity(%d) 수동 전송 — 응답(타입=7)이 오는지 확인"), Value);
			Server->SendFieldActivity(Value);
		}),
		ECVF_Default));

	// 양쪽 클라에서 같이 실행해서 좌표를 대조한다.
	// A의 "내 위치"와 B의 "원격 A" 좌표가 같으면 데이터는 정상이고 화면 차이(카메라/종횡비)이며,
	// 다르면 보고가 낡은 것이다
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FieldDump"),
		TEXT("[진단] 내 위치와 화면에 있는 원격 캐릭터 좌표를 전부 출력"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			const UGameInstance* GI = GetGameInstance();
			UWorld* World = GI ? GI->GetWorld() : nullptr;
			if (World == nullptr)
			{
				return;
			}

			const UFieldSyncSubsystem* Sync = World->GetSubsystem<UFieldSyncSubsystem>();
			const UEmberServerManager* Server = GetServer();

			UE_LOG(LogNUNetDebug, Warning, TEXT("===== FieldDump ====="));
			UE_LOG(LogNUNetDebug, Warning, TEXT("  로그인=%d 필드진입=%d"),
				Server != nullptr && Server->IsLoggedIn() ? 1 : 0,
				Sync != nullptr && Sync->IsInField() ? 1 : 0);

			// 내 캐릭터 idx를 같이 찍음
			// 상대가 본 내 Character Idx와 대조하면 파티 대상 지목이 맞는지 바로 확인됨
			const UNUCharacterManager* Characters = GI->GetSubsystem<UNUCharacterManager>();
			UE_LOG(LogNUNetDebug, Warning, TEXT("  내 charIdx=%lld"),
				Characters != nullptr ? Characters->GetSelectedCharacterIdx() : 0);

			const APlayerController* PC = World->GetFirstPlayerController();
			if (const AFieldPlayerCharacter* Me = PC != nullptr ? Cast<AFieldPlayerCharacter>(PC->GetPawn()) : nullptr)
			{
				const FVector MyLocation = Me->GetActorLocation();
				UE_LOG(LogNUNetDebug, Warning, TEXT("  내 위치    (%.0f, %.0f, %.0f) facing=%d"),
					MyLocation.X, MyLocation.Y, MyLocation.Z, static_cast<int32>(Me->Facing));
			}
			else
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("  내 캐릭터 없음"));
			}

			int32 RemoteCount = 0;
			for (TActorIterator<AFieldRemoteCharacter> It(World); It; ++It)
			{
				const AFieldRemoteCharacter* Remote = *It;
				const FVector RemoteLocation = Remote->GetActorLocation();

				UE_LOG(LogNUNetDebug, Warning,
					TEXT("  원격 %lld [%s]  (%.0f, %.0f, %.0f) activity=%d charIdx=%lld"),
					Remote->PlayerId, *Remote->GetDisplayName().ToString(),
					RemoteLocation.X, RemoteLocation.Y, RemoteLocation.Z,
					static_cast<int32>(Remote->Activity), Remote->CharacterIdx);
				++RemoteCount;
			}

			if (RemoteCount == 0)
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("  보이는 원격 없음"));
			}
		}),
		ECVF_Default));

	// 서버 없이 채팅 로그·자동 스크롤·버퍼 상한·탭 필터를 확인하는 용도
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakeChat"),
		TEXT("[진단] 가짜 채팅: ember.FakeChat [채널 0=지역 1=전체 2=파티 3=귓속말 4=시스템] [내말 0|1] [본문...]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UNUChatWidget* Chat = GetChatWidget();
			if (Chat == nullptr)
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("채팅 위젯 없음 — 필드에 진입한 뒤 실행하세요"));
				return;
			}

			const int32 ChannelValue = Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 0;
			const ENUChatChannel Channel = static_cast<ENUChatChannel>(ChannelValue);

			// 내 말 표시 ((나) 접미) 도 검증할 수 있게 플래그를 받는다
			const bool bIsMine = Args.IsValidIndex(1) && FCString::Atoi(*Args[1]) != 0;

			// 위젯을 직접 부른다 — 서브시스템을 거치면 실제 서버 응답 경로와 섞여
			// 무엇이 가짜인지 구분되지 않는다

			// 본문은 남은 인자를 전부 이어붙인다 (공백이 들어간 문장을 넣을 수 있게)
			FString Body;
			for (int32 Index = 2; Index < Args.Num(); ++Index)
			{
				if (!Body.IsEmpty()) Body.AppendChar(TEXT(' '));
				Body.Append(Args[Index]);
			}
			if (Body.IsEmpty())
			{
				Body = FString::Printf(TEXT("테스트 메시지 %s"),
					*NUChat::GetChannelLabel(Channel).ToString());
			}

			if (Channel == ENUChatChannel::System)
			{
				Chat->AddSystemMessage(FText::FromString(Body));
			}
			else
			{
				Chat->AddMessage(Channel,
					FText::FromString(bIsMine ? TEXT("내캐릭터") : TEXT("테스터")),
					FText::FromString(Body), bIsMine);
			}
		}),
		ECVF_Default));

	// ── 서버 없이 원격 캐릭터를 확인하는 것 ────────────────────────────────

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakeRemote"),
		TEXT("[진단] 가짜 원격 캐릭터 스폰(있으면 갱신): ember.FakeRemote [id=9001] [앞쪽거리cm=200] [정령id=1001] [종족=0] [성별=0] [머리=0] [외모=0] [직업=0]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UFieldSyncSubsystem* Sync = GetFieldSync();
			if (Sync == nullptr)
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("FieldSyncSubsystem 없음 — 필드 레벨에서 실행하세요"));
				return;
			}

			const int64 FakeId = Args.IsValidIndex(0) ? FCString::Atoi64(*Args[0]) : NUNetDebugDefaultFakeId;
			const float Distance = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 200.f;
			const int32 SpiritId = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 1001;

			FNUFieldAppearance Data;
			// 외형·직업은 서버가 Appeared에 실어 주는 값 --- 같은 id로 다시 부르면 갱신 경로(전직)를 탄다 (9/14)
			Data.RaceId = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : 0;
			Data.Gender = Args.IsValidIndex(4) ? FCString::Atoi(*Args[4]) : 0;
			Data.HairId = Args.IsValidIndex(5) ? FCString::Atoi(*Args[5]) : 0;
			Data.FaceId = Args.IsValidIndex(6) ? FCString::Atoi(*Args[6]) : 0;
			Data.JobId  = Args.IsValidIndex(7) ? FCString::Atoi(*Args[7]) : 0;
			Data.Location = GetFakeRemoteLocation(Distance);
			Data.Facing   = EFieldFacing::Down;
			Data.Activity = EFieldActivity::Idle;
			Data.SpiritId = SpiritId;

			// 이름은 서버가 SVR_Field_Appeared로 주는 값, 가짜는 id로 이름을 지정함
			Data.CharacterName = FString::Printf(TEXT("Fake%lld"), FakeId);

			// 앞의 FakeId 에 캐릭터ID 시작값을 더해서 캐릭터 ID로 사용
			Data.CharacterIdx = NUField::CHARACTER_IDX_START + FakeId;

			UE_LOG(LogNUNetDebug, Warning, TEXT("가짜 원격 %lld 스폰 (정령 %d, 이름 %s, charIdx %lld)"),
				FakeId, SpiritId, *Data.CharacterName, Data.CharacterIdx);
			Sync->OnAppeared(FakeId, Data);
		}),
		ECVF_Default));

	// 서버 없이 선택창을 볼 때 목록을 채운다 --- 외형은 서버가 돌려주는 값과 같은 뜻이다 (머리 = 머리 번호 + 색 번호 x 1000)
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("nu.Char.Dummy"),
		TEXT("[진단] 더미 캐릭터를 목록에 추가(서버 미연결일 때만): nu.Char.Dummy [이름] [종족=1] [성별=0] [머리=1] [외모=1] [직업=0] [레벨=1] [삭제예약시간=0]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UNUCharacterManager* Manager = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UNUCharacterManager>() : nullptr;
			if (Manager == nullptr)
			{
				return;
			}
			FNUCharacterSummary Character;
			Character.Name = Args.IsValidIndex(0) ? Args[0] : FString();
			Character.RaceId = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 1;
			Character.Gender = Args.IsValidIndex(2) ? FCString::Atoi(*Args[2]) : 0;
			Character.HairId = Args.IsValidIndex(3) ? FCString::Atoi(*Args[3]) : 1;
			Character.FaceId = Args.IsValidIndex(4) ? FCString::Atoi(*Args[4]) : 1;
			Character.JobId = Args.IsValidIndex(5) ? FCString::Atoi(*Args[5]) : 0;
			Character.Level = Args.IsValidIndex(6) ? FCString::Atoi(*Args[6]) : 1;
			const int32 PendingHours = Args.IsValidIndex(7) ? FCString::Atoi(*Args[7]) : 0;
			Character.bDeletePending = PendingHours > 0;
			Character.DeleteRemainSec = PendingHours * 3600;
			Manager->DebugAddDummyCharacter(Character);
		}),
		ECVF_Default));

	// 정령 붙이기/떼기 확인용 (0 = 없음)
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakeRemoteSpirit"),
		TEXT("[진단] 가짜 원격 정령 변경: ember.FakeRemoteSpirit [id=9001] [정령id=0]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UFieldSyncSubsystem* Sync = GetFieldSync();
			if (Sync == nullptr) return;

			const int64 FakeId = Args.IsValidIndex(0) ? FCString::Atoi64(*Args[0]) : NUNetDebugDefaultFakeId;
			const int32 SpiritId = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 0;

			UE_LOG(LogNUNetDebug, Warning, TEXT("가짜 원격 %lld 정령 -> %d"), FakeId, SpiritId);
			Sync->OnSpiritChanged(FakeId, SpiritId);
		}),
		ECVF_Default));

	// 타겟 추격 이동과 걷기 애니메이션 확인용
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakeRemoteMove"),
		TEXT("[진단] 가짜 원격 이동: ember.FakeRemoteMove [id=9001] [앞쪽거리cm=400]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UFieldSyncSubsystem* Sync = GetFieldSync();
			if (Sync == nullptr) return;

			const int64 FakeId = Args.IsValidIndex(0) ? FCString::Atoi64(*Args[0]) : NUNetDebugDefaultFakeId;
			const float Distance = Args.IsValidIndex(1) ? FCString::Atof(*Args[1]) : 400.f;

			Sync->OnMoved(FakeId, GetFakeRemoteLocation(Distance), EFieldFacing::Down);
		}),
		ECVF_Default));

	// 전투중, 전투 종료로 상태를 변경하는 용도
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakeRemoteActivity"),
		TEXT("[진단] 가짜 원격 상태 변경: ember.FakeRemoteActivity [id=9001] [0=Idle 1=Walk 2=Battle]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UFieldSyncSubsystem* Sync = GetFieldSync();
			if (Sync == nullptr) return;

			const int64 FakeId = Args.IsValidIndex(0) ? FCString::Atoi64(*Args[0]) : NUNetDebugDefaultFakeId;
			const int32 Value = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 2;

			UE_LOG(LogNUNetDebug, Warning, TEXT("가짜 원격 %lld 상태 -> %d"), FakeId, Value);
			Sync->OnActivityChanged(FakeId, static_cast<EFieldActivity>(Value));
		}),
		ECVF_Default));

	// 생성한 가짜 원격을 제거함
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakeRemoteRemove"),
		TEXT("[진단] 가짜 원격 제거: ember.FakeRemoteRemove [id=9001]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UFieldSyncSubsystem* Sync = GetFieldSync();
			if (Sync == nullptr) return;

			const int64 FakeId = Args.IsValidIndex(0) ? FCString::Atoi64(*Args[0]) : NUNetDebugDefaultFakeId;
			Sync->OnDisappeared(FakeId);
		}),
		ECVF_Default));

	// ── 서버 없이 파티를 확인하는 것 ───────────────────────────────────────
	// 서브시스템의 수신 함수를 JSON까지 만들어 직접 부른다.
	// 파서를 같이 통과시켜야 서버가 붙었을 때와 같은 경로를 탄다

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakeParty"),
		TEXT("[진단] 가짜 파티 구성: ember.FakeParty [인원=2] [내가리더 0|1=1]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			const int32 Count = FMath::Clamp(Args.IsValidIndex(0) ? FCString::Atoi(*Args[0]) : 2, 1, 4);
			const bool bMeLeader = !Args.IsValidIndex(1) || FCString::Atoi(*Args[1]) != 0;

			// 내가 목록에 없으면 파티창이 나를 못 찾는다. 실제 서버도 나를 포함해서 준다
			const int64 MyIdx = GetMyCharacterIdx();

			// 리더는 딱 한 명이어야 한다. 내가 아니면 그다음 사람이 맡는다
			const int32 LeaderIndex = bMeLeader ? 0 : FMath::Min(1, Count - 1);
			int64 LeaderIdx = 0;

			FString Members;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const bool bIsMe = (Index == 0 && MyIdx != 0);
				const int64 Idx = bIsMe ? MyIdx : NUField::CHARACTER_IDX_START + 500 + Index;
				const FString Name = bIsMe ? TEXT("나") : FString::Printf(TEXT("파티원%d"), Index);
				const bool bIsLeader = (Index == LeaderIndex);

				if (bIsLeader)
				{
					LeaderIdx = Idx;
				}

				if (Index > 0) Members.AppendChar(TEXT(','));
				Members += FString::Printf(TEXT("{\"Idx\":%lld,\"Name\":\"%s\",\"IsLeader\":%s}"),
					Idx, *Name, bIsLeader ? TEXT("true") : TEXT("false"));
			}

			const FString Json = FString::Printf(
				TEXT("{\"PartyId\":777,\"LeaderIdx\":%lld,\"MaxCount\":4,\"Members\":[%s]}"),
				LeaderIdx, *Members);

			UE_LOG(LogNUNetDebug, Warning, TEXT("가짜 파티 %d명 주입"), Count);
			PartySubsystem->HandleUpdated(777, static_cast<int32>(ENUPartyUpdateReason::Joined), LeaderIdx, Json);
		}),
		ECVF_Default));

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakePartyInvite"),
		TEXT("[진단] 가짜 파티 초대: ember.FakePartyInvite [이름=테스터] [초=10] [신청 0|1=0]"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			const FString Name = Args.IsValidIndex(0) ? Args[0] : TEXT("테스터");
			const int32 ExpireSec = Args.IsValidIndex(1) ? FCString::Atoi(*Args[1]) : 10;
			const bool bApply = Args.IsValidIndex(2) && FCString::Atoi(*Args[2]) != 0;

			const int64 FromIdx = NUField::CHARACTER_IDX_START + 999;

			UE_LOG(LogNUNetDebug, Warning, TEXT("가짜 파티 %s (%s, %d초)"),
				bApply ? TEXT("신청") : TEXT("초대"), *Name, ExpireSec);

			if (bApply)
			{
				PartySubsystem->HandleApplied(FromIdx, Name, ExpireSec);
			}
			else
			{
				PartySubsystem->HandleInvited(FromIdx, Name, ExpireSec);
			}
		}),
		ECVF_Default));

	// 해산·탈퇴 경로 — party_id 0에 빈 목록이 오는 것이 서버 동작이다
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.FakePartyLeave"),
		TEXT("[진단] 가짜 파티 해산: ember.FakePartyLeave"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			UE_LOG(LogNUNetDebug, Warning, TEXT("가짜 파티 해산"));
			PartySubsystem->HandleUpdated(0, static_cast<int32>(ENUPartyUpdateReason::Disbanded), 0,
				TEXT("{\"PartyId\":0,\"LeaderIdx\":0,\"MaxCount\":4,\"Members\":[]}"));
		}),
		ECVF_Default));

	// ── 실제 서버로 보내는 파티 조작 (UI가 생기기 전까지 유일한 조작 수단) ────

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.PartyAccept"),
		TEXT("[진단] 받아둔 파티 초대·신청 수락"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			UE_LOG(LogNUNetDebug, Warning, TEXT("파티 수락"));
			PartySubsystem->Respond(true);
		}),
		ECVF_Default));

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.PartyDecline"),
		TEXT("[진단] 받아둔 파티 초대·신청 거절"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			UE_LOG(LogNUNetDebug, Warning, TEXT("파티 거절"));
			PartySubsystem->Respond(false);
		}),
		ECVF_Default));

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.PartyInvite"),
		TEXT("[진단] 파티 초대 전송: ember.PartyInvite <캐릭터idx>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			if (!Args.IsValidIndex(0))
			{
				UE_LOG(LogNUNetDebug, Warning,
					TEXT("대상 캐릭터 idx가 필요합니다 — ember.FieldDump로 charIdx를 확인하세요"));
				return;
			}

			PartySubsystem->Invite(FCString::Atoi64(*Args[0]));
		}),
		ECVF_Default));

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.PartyLeave"),
		TEXT("[진단] 파티 탈퇴 전송"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			UE_LOG(LogNUNetDebug, Warning, TEXT("파티 탈퇴 전송"));
			PartySubsystem->Leave();
		}),
		ECVF_Default));

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.PartyKick"),
		TEXT("[진단] 파티 추방 전송 (리더만): ember.PartyKick <캐릭터idx>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			if (!Args.IsValidIndex(0))
			{
				UE_LOG(LogNUNetDebug, Warning,
					TEXT("대상 캐릭터 idx가 필요합니다 — ember.PartyDump로 확인하세요"));
				return;
			}

			PartySubsystem->Kick(FCString::Atoi64(*Args[0]));
		}),
		ECVF_Default));

	// 서버와 어긋났을 때 목록을 다시 받아 본다
	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.PartySync"),
		TEXT("[진단] PartyMemberList 재요청"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			PartySubsystem->RequestMemberList();
		}),
		ECVF_Default));

	RegisteredCommands.Add(Console.RegisterConsoleCommand(
		TEXT("ember.PartyDump"),
		TEXT("[진단] 현재 파티 스냅샷 출력"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			UNUPartySubsystem* PartySubsystem = GetParty();
			if (PartySubsystem == nullptr) return;

			const FNUPartySnapshot& Snapshot = PartySubsystem->GetParty();
			UE_LOG(LogNUNetDebug, Warning, TEXT("===== PartyDump ====="));
			UE_LOG(LogNUNetDebug, Warning, TEXT("  내 charIdx=%lld"), GetMyCharacterIdx());
			UE_LOG(LogNUNetDebug, Warning, TEXT("  partyId=%lld leaderIdx=%lld %d/%d명"),
				Snapshot.PartyId, Snapshot.LeaderIdx, Snapshot.Members.Num(), Snapshot.MaxCount);

			// 답할 것이 남아 있으면 ember.PartyAccept로 수락할 수 있다
			const int64 Pending = PartySubsystem->GetPendingFromIdx();
			UE_LOG(LogNUNetDebug, Warning, TEXT("  대기 중인 초대·신청: %s"),
				Pending != 0 ? *FString::Printf(TEXT("%lld (ember.PartyAccept)"), Pending) : TEXT("없음"));

			for (const FNUPartyMember& Member : Snapshot.Members)
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("    %lld [%s]%s"),
					Member.Idx, *Member.Name, Member.bIsLeader ? TEXT(" (리더)") : TEXT(""));
			}

			if (Snapshot.Members.Num() == 0)
			{
				UE_LOG(LogNUNetDebug, Warning, TEXT("    파티 없음"));
			}
		}),
		ECVF_Default));
}

UEmberServerManager* UNUNetDebugSubsystem::GetServer() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}

UFieldSyncSubsystem* UNUNetDebugSubsystem::GetFieldSync() const
{
	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UFieldSyncSubsystem>() : nullptr;
}

UNUPartySubsystem* UNUNetDebugSubsystem::GetParty() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
}

int64 UNUNetDebugSubsystem::GetMyCharacterIdx() const
{
	const UGameInstance* GI = GetGameInstance();
	const UNUCharacterManager* Characters = GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;

	return IsValid(Characters) ? Characters->GetSelectedCharacterIdx() : 0;
}

UNUChatWidget* UNUNetDebugSubsystem::GetChatWidget() const
{
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	ANUPlayerController* PC = World ? Cast<ANUPlayerController>(World->GetFirstPlayerController()) : nullptr;

	return IsValid(PC) ? PC->GetChatWidget() : nullptr;
}

FVector UNUNetDebugSubsystem::GetFakeRemoteLocation(float ForwardDistance) const
{
	// 내 캐릭터 기준 +X 방향, 월드기준
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* MyPawn = PC ? PC->GetPawn() : nullptr;

	return MyPawn
		? MyPawn->GetActorLocation() + FVector(ForwardDistance, 0.f, 0.f)
		: FVector::ZeroVector;
}
