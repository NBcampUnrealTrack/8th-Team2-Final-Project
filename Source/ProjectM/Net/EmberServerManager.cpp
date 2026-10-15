#include "Net/EmberServerManager.h"
#include "Auth/NUGoogleAuthSubsystem.h"
#include "Net/EmberBearerToken.h"
#include "TurboLinkGrpcManager.h"
#include "TurboLinkGrpcUtilities.h"
#include "TurboLinkGrpcConfig.h"
#include "CharacterProgression/Component/NUProgressionComponent.h"
#include "CharacterProgression/Net/NUProgressionNetSubsystem.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "Chat/NUChatSubsystem.h"
#include "Net/EmberChatManager.h"
#include "Field/Net/FieldSyncSubsystem.h"
#include "Inventory/NUInventoryComponent.h"
#include "Party/NUPartySubsystem.h"
#include "Quest/Component/NUQuestComponent.h"
#include "Quest/Encounter/NUQuestEncounterSubsystem.h"
#include "Trade/NUTradeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"
#include "Battle/Net/NUBattleNetSubsystem.h"
#include "Battle/Net/NUBattleServerTypes.h"
#include "Naia/Net/NUNaiaNetSubsystem.h"
#include "Naia/Net/NUNaiaServerTypes.h"
#include "Recovery/Net/NURecoveryNetSubsystem.h"
#include "Field/Interior/NUInteriorParticipantComponent.h"
#include "WorldTransition/Net/NULocationRestoreSubsystem.h"
#include "WorldTransition/World/NUWorldTransitionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogEmberNet, Log, All);

namespace EmberServerCfg
{
	// ▼▼▼ 서버 주소/포트는 여기서 관리한다 (ini 아님) ▼▼▼
	// 로컬 개발 서버로 붙으려면 true, 원격(EC2)으로 붙으려면 false. (변경 시 재컴파일 필요)
	constexpr bool bUseLocalServer = false;
	const TCHAR* LocalHost  = TEXT("127.0.0.1");        // 로컬 개발 서버
	const TCHAR* RemoteHost = TEXT("43.202.139.187");   // EC2 서버
	constexpr int32 ServerPort = 7000;
	// ▲▲▲ ───────────────────────────────────── ▲▲▲

	// 서버 규약 상수 (CLIENT_INTEGRATION.md)
	const FString AUTH_KEY_TEMP = TEXT("google_or_apple_auth_key_abcd_1234");
	constexpr int32 PLATFORM_GOOGLE = 1;
	constexpr float PING_INTERVAL_SEC = 30.0f;

	// ResultCode (서버 GbEnum)
	constexpr int32 RC_SUCCESS = 200;
	constexpr int32 RC_LOGINUSER_NOT_FOUND = 1101;
	
	
	// ─── 지연 시간 ───
	// 왕복 시간 평균에 사용할 표본 개수
	// 지연 시간은 최근 평균 지연시간이기 때문에 많이 잡을 필요가 없음
	constexpr int32 MAX_RTT_SAMPLES = 10;
	
	// 이 값보다 크면 네트워크 지연이 아닌 프레임 정지 or 끊김
	constexpr int32 MAX_VALID_RTT_MS = 2000;
}

/////////////////////////////////////////////////////////////////
// 초기화 + 콘솔 명령
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 콘솔 명령: ember.Connect <userId>  (테스트/디버그용)
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ember.Connect"),
		TEXT("서버 접속: ember.Connect <userId>"),
		FConsoleCommandWithArgsDelegate::CreateWeakLambda(this, [this](const TArray<FString>& Args)
		{
			const FString Id = Args.Num() > 0 ? Args[0] : TEXT("uetester");
			Connect(Id);
		}),
		ECVF_Default);

	// 토큰 파생 self-test: 서버 테스트벡터 #1과 비교 (파생 알고리즘 검증용)
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("ember.TokenTest"),
		TEXT("토큰 파생 self-test (서버 테스트벡터 대조)"),
		FConsoleCommandDelegate::CreateWeakLambda(this, [this]()
		{
			const FString Got = FEmberBearerToken::GenerateToken(
				TEXT("aB3xY9kQ1mZ7pR2sT5vW8nD4gH6jL0cE"), TEXT("tester"), 1700000000, TEXT("IVIVIVIVIVIVIVIV"));
			const FString Expected = TEXT("SVZJVklWSVZJVklWSVZJVrqIB1lmIscWjDs8Fl/TG9RaCEocu8LCO164rfSK1nDwrMRe/A0KHJRhuBYfFIv9jQ==");
			UE_LOG(LogEmberNet, Warning, TEXT("[TokenTest] got=%s"), *Got);
			UE_LOG(LogEmberNet, Warning, TEXT("[TokenTest] exp=%s"), *Expected);
			UE_LOG(LogEmberNet, Warning, TEXT("[TokenTest] MATCH=%s"), Got == Expected ? TEXT("YES") : TEXT("NO"));
		}),
		ECVF_Default);
}

void UEmberServerManager::Deinitialize()
{
	Disconnect();
	Super::Deinitialize();
}

/////////////////////////////////////////////////////////////////
// 서버 주소/포트 (코드 관리)
/////////////////////////////////////////////////////////////////

FString UEmberServerManager::GetServerHost() const
{
	return EmberServerCfg::bUseLocalServer ? EmberServerCfg::LocalHost : EmberServerCfg::RemoteHost;
}

int32 UEmberServerManager::GetServerPort() const
{
	return EmberServerCfg::ServerPort;
}

FString UEmberServerManager::GetServerEndPoint() const
{
	return FString::Printf(TEXT("%s:%d"), *GetServerHost(), GetServerPort());
}

void UEmberServerManager::ApplyServerEndPoint()
{
	// 코드에서 정한 주소/포트를 TurboLink의 GameService 엔드포인트로 지정한다 (ini의 DefaultEndPoint보다 우선).
	// GameService::Connect가 접속 시점에 GetServiceEndPoint("GameService")를 읽으므로 접속 직전에 세팅한다.
	if (UTurboLinkGrpcConfig* Config = GetMutableDefault<UTurboLinkGrpcConfig>())
	{
		Config->ServiceEndPoint.Add(TEXT("GameService"), GetServerEndPoint());
	}
	UE_LOG(LogEmberNet, Log, TEXT("접속 대상 서버: %s (%s)"),
		*GetServerEndPoint(), EmberServerCfg::bUseLocalServer ? TEXT("Local") : TEXT("Remote"));
}

void UEmberServerManager::UpdateRtt()
{
	// 보낸적이 없다면 버림
	if (PingSentSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	
	double Elapsed = (FPlatformTime::Seconds() - PingSentSeconds) * 1000.0;
	PingSentSeconds = 0.0;
	
	// 차이가 0이하거나, 제한 시간을 넘었다면 표본으로 사용하기에 오염된 값
	if (Elapsed <= KINDA_SMALL_NUMBER || Elapsed > EmberServerCfg::MAX_VALID_RTT_MS)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("핑 왕복 %.0fms - 잘못된 값, 표본에서 제외 [%s]"), Elapsed, *UserId);
		return;
	}
	
	// 표본 개수가 상한에 닿기 전까진 계속 증가, 상한에 닿으면 고정
	// 이동 평균 구하기
	RttSampleCount = FMath::Min(RttSampleCount + 1, EmberServerCfg::MAX_RTT_SAMPLES);
	
	AverageRttMs += (Elapsed - AverageRttMs) / static_cast<double>(RttSampleCount);
}

/////////////////////////////////////////////////////////////////
// gRPC 연결 (TurboLink)
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Connect(const FString& InUserId)
{
	if (State != EEmberConnState::Idle && State != EEmberConnState::Failed)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("Connect 무시 — 이미 연결 진행 중 (State=%d)"), (int32)State);
		return;
	}

	ApplyServerEndPoint(); // 코드의 주소/포트를 TurboLink에 적용 (ini 대신)

	// 이름 로그인(또는 지금 서버의 구글 임시 방식)은 임시 키. 구글 토큰 검증 방식은 ConnectWithGoogle 이 미리 채워 둔다
	if (AuthKey.IsEmpty())
	{
		AuthKey = EmberServerCfg::AUTH_KEY_TEMP;
	}
	UserId = InUserId;
	SessionToken.Empty();
	bStreamStarted = false;
	SetState(EEmberConnState::Connecting);

	UTurboLinkGrpcManager* Manager = UTurboLinkGrpcUtilities::GetTurboLinkGrpcManager(this);
	if (Manager == nullptr)
	{
		UE_LOG(LogEmberNet, Error, TEXT("TurboLinkGrpcManager 없음"));
		SetState(EEmberConnState::Failed);
		return;
	}

	Service = Cast<UGameService>(Manager->MakeService(TEXT("GameService")));
	if (Service == nullptr)
	{
		UE_LOG(LogEmberNet, Error, TEXT("GameService 생성 실패 (endpoint 설정 확인)"));
		SetState(EEmberConnState::Failed);
		return;
	}

	// MakeService는 이름이 같으면 같은 객체를 돌려준다. 로그인이 실패해 Connect를 다시 부르면
	// 같은 델리게이트에 두 번 바인딩되어 ensure가 난다 (ScriptDelegates.h의 AddInternal)
	Service->OnServiceStateChanged.AddUniqueDynamic(this, &UEmberServerManager::HandleServiceStateChanged);
	Service->Connect();
	UE_LOG(LogEmberNet, Log, TEXT("서버 연결 시작 [%s]"), *UserId);

	// **채팅 서버도 같이 붙는다** — 클라는 스트림을 2개 연다(게임 · 채팅).
	// 원격은 주소가 같고 nginx 가 gRPC 경로로 갈라준다.
	// 채팅 스트림만 먼저 열어두고, ChatLogin 은 캐릭터 선택 뒤에 나간다(서버가 그 전엔 거부한다).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UEmberChatManager* ChatManager = GI->GetSubsystem<UEmberChatManager>())
		{
			ChatManager->Connect();
		}
	}
}

void UEmberServerManager::ConnectWithGoogle(const FString& InUserId, const FString& IdToken, bool bServerVerifiesToken)
{
	if (State != EEmberConnState::Idle && State != EEmberConnState::Failed)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("ConnectWithGoogle 무시 — 이미 연결 진행 중 (State=%d)"), (int32)State);
		return;
	}
	AuthKey = bServerVerifiesToken ? IdToken : EmberServerCfg::AUTH_KEY_TEMP;
	bAdoptServerUserId = bServerVerifiesToken;
	Connect(InUserId);
}

void UEmberServerManager::ReconnectWithFreshGoogleToken()
{
	UNUGoogleAuthSubsystem* Google = GetGameInstance() != nullptr ? GetGameInstance()->GetSubsystem<UNUGoogleAuthSubsystem>() : nullptr;
	if (Google == nullptr || !Google->IsSignedIn())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("재접속 불가 --- 구글 계정 정보가 없다 (다시 로그인 필요)"));
		SetState(EEmberConnState::Failed);
		return;
	}
	Google->EnsureFreshIdToken([WeakThis = TWeakObjectPtr<UEmberServerManager>(this)](bool bFresh)
	{
		UEmberServerManager* Self = WeakThis.Get();
		UNUGoogleAuthSubsystem* GoogleNow = Self != nullptr && Self->GetGameInstance() != nullptr
			? Self->GetGameInstance()->GetSubsystem<UNUGoogleAuthSubsystem>() : nullptr;
		if (Self == nullptr || GoogleNow == nullptr)
		{
			return;
		}
		if (!bFresh)
		{
			UE_LOG(LogEmberNet, Warning, TEXT("재접속 불가 --- 구글 ID 토큰을 새로 받지 못했다 (다시 로그인 필요)"));
			Self->SetState(EEmberConnState::Failed);
			return;
		}
		Self->AuthKey = GoogleNow->GetAccount().IdToken;
		Self->Connect(Self->UserId);
	});
}

void UEmberServerManager::HandleServiceStateChanged(EGrpcServiceState NewServiceState)
{
	UE_LOG(LogEmberNet, Log, TEXT("ServiceState=%d"), (int32)NewServiceState);

	if (NewServiceState == EGrpcServiceState::Ready && !bStreamStarted)
	{
		bStreamStarted = true;

		Client = Service->MakeClient();
		if (Client == nullptr)
		{
			UE_LOG(LogEmberNet, Error, TEXT("GameServiceClient 생성 실패"));
			SetState(EEmberConnState::Failed);
			return;
		}

		Client->OnGameResponse.AddUniqueDynamic(this, &UEmberServerManager::HandleGameResponse);
		StreamHandle = Client->InitGame(); // 양방향 스트림 열기

		AuthBaseTimeStamp = NowUnixSeconds();
		SendAuthUser();
	}
	else if (NewServiceState == EGrpcServiceState::TransientFailure || NewServiceState == EGrpcServiceState::Shutdown)
	{
		SetState(EEmberConnState::Failed);
	}
}

bool UEmberServerManager::TryReconnect()
{
	if (UserId.IsEmpty())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("재접속 불가 --- 접속했던 계정이 없다"));
		return false;
	}
	if (State != EEmberConnState::Idle && State != EEmberConnState::Failed)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("재접속 무시 --- 이미 진행 중 (State=%d)"), (int32)State);
		return false;
	}

	// 이전 스트림·서비스를 확실히 내려놓고 시작한다. 남아 있으면 MakeService가 같은 객체를
	// 돌려주면서 델리게이트가 겹친다
	Disconnect();

	UE_LOG(LogEmberNet, Log, TEXT("재접속 시도 [%s]"), *UserId);
	if (bAdoptServerUserId)
	{
		// 서버가 구글 토큰을 검증하는 방식: 1시간짜리 ID 토큰이 만료됐을 수 있어 새로 받아 싣는다
		ReconnectWithFreshGoogleToken();
		return true;
	}
	Connect(UserId);
	return true;
}

void UEmberServerManager::Disconnect()
{
	if (UGameInstance* GI = GetGameInstance())
	{
		GI->GetTimerManager().ClearTimer(PingTimer);

		// **게임이 끊기면 채팅도 끊는다** (반대는 아니다 — 채팅만 끊겨도 게임은 계속된다).
		// 안 끊으면 세션 토큰이 사라진 뒤에도 채팅 스트림이 남아 서버가 핑에서 강퇴한다.
		if (UEmberChatManager* ChatManager = GI->GetSubsystem<UEmberChatManager>())
		{
			ChatManager->Disconnect();
		}
	}
	if (Client != nullptr && StreamHandle.Value != 0)
	{
		Client->TryCancel(StreamHandle);
	}
	StreamHandle = FGrpcContextHandle();
	Client = nullptr;
	Service = nullptr;
	bStreamStarted = false;
	PingSentSeconds = 0.0;
	AverageRttMs = 0.0;
	RttSampleCount = 0;
	if (State != EEmberConnState::Failed)
	{
		SetState(EEmberConnState::Idle);
	}
}

/////////////////////////////////////////////////////////////////
// 송신 (요청 빌드 + 전송)
/////////////////////////////////////////////////////////////////

FGrpcRevGameGameMessage UEmberServerManager::MakeEnvelope() const
{
	FGrpcRevGameGameMessage Msg;
	Msg.UserId = UserId;
	Msg.Timestamp = NowUnixSeconds();
	// token은 proto3 optional 필드 — 값(Token)뿐 아니라 _tokenCase도 세팅해야 마샬링이 set_token()을 호출한다.
	// (_tokenCase를 빼먹으면 서버에 토큰이 전송되지 않아 CheckUserSession이 "Invalid sessionToken" 401을 낸다)
	Msg._token.Token = SessionToken; // 인증 전엔 빈 문자열
	Msg._token._tokenCase = EGrpcRevGameGameMessage_token::Token;
	return Msg;
}

void UEmberServerManager::SendAuthUser()
{
	SetState(EEmberConnState::Authenticating);

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Timestamp = AuthBaseTimeStamp; // 인증 timestamp = 토큰 파생 기준

	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::AuthUser;
	Req.Message_type.AuthUser = MakeShared<FGrpcRevGameAuthUser>();
	Req.Message_type.AuthUser->PlatformType = EmberServerCfg::PLATFORM_GOOGLE;
	Req.Message_type.AuthUser->AuthKey = AuthKey.IsEmpty() ? EmberServerCfg::AUTH_KEY_TEMP : AuthKey;

	UE_LOG(LogEmberNet, Log, TEXT("[클라] AuthUser 전송 [%s]"), *UserId);
	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCreateUser()
{
	SetState(EEmberConnState::Creating);

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CreateUser;
	Req.Message_type.CreateUser = MakeShared<FGrpcRevGameCreateUser>();

	UE_LOG(LogEmberNet, Log, TEXT("[클라] CreateUser 전송 [%s]"), *UserId);
	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendLoginUser()
{
	SetState(EEmberConnState::LoggingIn);

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::LoginUser;
	Req.Message_type.LoginUser = MakeShared<FGrpcRevGameLoginUser>();
	Req.Message_type.LoginUser->LoginType = 0; // Normal

	UE_LOG(LogEmberNet, Log, TEXT("[클라] LoginUser 전송 [%s]"), *UserId);
	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendPing()
{
	if (State != EEmberConnState::LoggedIn || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::Ping;
	Req.Message_type.Ping = MakeShared<FGrpcRevGamePing>();
	Req.Message_type.Ping->ClientTime = FDateTime::UtcNow().ToUnixTimestampDecimal() * 1000.0; // ms

	// 지금까지 잰 평균 지연 시간을 보냄, 첫 핑은 0
	Req.Message_type.Ping->RttMs = GetAverageRttMs();
	
	// 보낸 시각 기록
	PingSentSeconds = FPlatformTime::Seconds();
	
	Client->Game(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 필드 위치 동기화 송신
//   oneof는 값(TSharedPtr)과 Message_typeCase를 **반드시 함께** 세팅해야 한다.
//   하나라도 빠지면 서버에 그 필드가 전송되지 않는다(과거 토큰 401의 원인과 동일한 함정).
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendFieldEnter(const FVector& Location, int32 Facing, int32 LocationId)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		// 2026.08.02 확인용으로 임시 로그 추가했습니다
		UE_LOG(LogEmberNet, Warning, TEXT("[진단] FieldEnter 미전송 — LoggedIn=%d Client=%d"),
			IsLoggedIn() ? 1 : 0, Client != nullptr ? 1 : 0);
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::FieldEnter;
	Req.Message_type.FieldEnter = MakeShared<FGrpcRevGameFieldEnter>();
	Req.Message_type.FieldEnter->ZoneId = 1; // 맵 1개 — 서버 Def.FIELD_DEFAULT_ZONE_ID
	Req.Message_type.FieldEnter->PosX = Location.X;
	Req.Message_type.FieldEnter->PosY = Location.Y;
	Req.Message_type.FieldEnter->PosZ = Location.Z;
	Req.Message_type.FieldEnter->Facing = Facing;
	Req.Message_type.FieldEnter->LocationId = LocationId;

	// 2026.08.02 확인용으로 임시 로그 추가했습니다
	UE_LOG(LogEmberNet, Log, TEXT("[진단] FieldEnter 전송 [%s] location=%d (%.0f, %.0f, %.0f) facing=%d"),
		*UserId, LocationId, Location.X, Location.Y, Location.Z, Facing);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendFieldMove(const FVector& Location, int32 Facing, int32 Reason)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::FieldMove;
	Req.Message_type.FieldMove = MakeShared<FGrpcRevGameFieldMove>();
	Req.Message_type.FieldMove->PosX = Location.X;
	Req.Message_type.FieldMove->PosY = Location.Y;
	Req.Message_type.FieldMove->PosZ = Location.Z;
	Req.Message_type.FieldMove->Facing = Facing;
	Req.Message_type.FieldMove->Reason = Reason;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendFieldLeave()
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::FieldLeave;
	Req.Message_type.FieldLeave = MakeShared<FGrpcRevGameFieldLeave>(); // 본문 없어도 반드시 생성

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendFieldActivity(int32 Activity)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::FieldActivity;
	Req.Message_type.FieldActivity = MakeShared<FGrpcRevGameFieldActivity>();
	Req.Message_type.FieldActivity->Activity = Activity;

	Client->Game(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 캐릭터 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendCharacterList()
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterList;
	Req.Message_type.CharacterList = MakeShared<FGrpcRevGameCharacterList>(); // 본문 없음

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterCreate(const FString& InName, int32 JobId, int32 RaceId, int32 Gender, int32 HairId, int32 FaceId)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterCreate;
	Req.Message_type.CharacterCreate = MakeShared<FGrpcRevGameCharacterCreate>();
	Req.Message_type.CharacterCreate->Name = InName;
	Req.Message_type.CharacterCreate->JobId = JobId;
	Req.Message_type.CharacterCreate->RaceId = RaceId;
	Req.Message_type.CharacterCreate->Gender = Gender;
	Req.Message_type.CharacterCreate->HairId = HairId;
	Req.Message_type.CharacterCreate->FaceId = FaceId;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterNameCheck(const FString& InName)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterNameCheck;
	Req.Message_type.CharacterNameCheck = MakeShared<FGrpcRevGameCharacterNameCheck>();
	Req.Message_type.CharacterNameCheck->Name = InName;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterDeleteCancel(int64 CharacterIdx)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterDeleteCancel;
	Req.Message_type.CharacterDeleteCancel = MakeShared<FGrpcRevGameCharacterDeleteCancel>();
	Req.Message_type.CharacterDeleteCancel->CharacterIdx = static_cast<uint64>(CharacterIdx);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterProfile(int64 CharacterIdx)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterProfile;
	Req.Message_type.CharacterProfile = MakeShared<FGrpcRevGameCharacterProfile>();
	Req.Message_type.CharacterProfile->CharacterIdx = static_cast<uint64>(CharacterIdx);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendItemDiscard(int32 Container, int32 SlotNo, int32 Count)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ItemDiscard;
	Req.Message_type.ItemDiscard = MakeShared<FGrpcRevGameItemDiscard>();
	Req.Message_type.ItemDiscard->Container = Container;
	Req.Message_type.ItemDiscard->SlotNo = SlotNo;
	Req.Message_type.ItemDiscard->Count = Count;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendShopBuy(int32 ShopId, int32 ItemId, int32 Count, int32 TotalPrice)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ShopBuy;
	Req.Message_type.ShopBuy = MakeShared<FGrpcRevGameShopBuy>();
	Req.Message_type.ShopBuy->ShopId = ShopId;
	Req.Message_type.ShopBuy->ItemId = ItemId;
	Req.Message_type.ShopBuy->Count = Count;
	Req.Message_type.ShopBuy->TotalPrice = TotalPrice;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendShopSell(int32 SlotNo, int32 Count, int32 TotalGain, int32 ShopId)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ShopSell;
	Req.Message_type.ShopSell = MakeShared<FGrpcRevGameShopSell>();
	Req.Message_type.ShopSell->SlotNo = SlotNo;
	Req.Message_type.ShopSell->Count = Count;
	Req.Message_type.ShopSell->TotalGain = TotalGain;
	Req.Message_type.ShopSell->ShopId = ShopId;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterSelect(int64 CharacterIdx)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterSelect;
	Req.Message_type.CharacterSelect = MakeShared<FGrpcRevGameCharacterSelect>();
	Req.Message_type.CharacterSelect->CharacterIdx = static_cast<uint64>(CharacterIdx);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterDelete(int64 CharacterIdx)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterDelete;
	Req.Message_type.CharacterDelete = MakeShared<FGrpcRevGameCharacterDelete>();
	Req.Message_type.CharacterDelete->CharacterIdx = static_cast<uint64>(CharacterIdx);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterStat()
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterStat;
	Req.Message_type.CharacterStat = MakeShared<FGrpcRevGameCharacterStat>();

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterStatAssign(int32 Strength, int32 Dexterity, int32 Intelligence, int32 Luck, int32 Vitality)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterStatAssign;
	Req.Message_type.CharacterStatAssign = MakeShared<FGrpcRevGameCharacterStatAssign>();
	Req.Message_type.CharacterStatAssign->Str = Strength;
	Req.Message_type.CharacterStatAssign->Dex = Dexterity;
	Req.Message_type.CharacterStatAssign->Int = Intelligence;
	Req.Message_type.CharacterStatAssign->Luk = Luck;
	Req.Message_type.CharacterStatAssign->Vit = Vitality;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterProgression(int32 GraphVersion)
{
	if (!IsLoggedIn() || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterProgression;
	Req.Message_type.CharacterProgression = MakeShared<FGrpcRevGameCharacterProgression>();
	Req.Message_type.CharacterProgression->GraphVersion = GraphVersion;
	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterJobSelect(int32 JobId, int32 GraphVersion, int64 ExpectedRevision, const FString& MutationId)
{
	if (!IsLoggedIn() || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterJobSelect;
	Req.Message_type.CharacterJobSelect = MakeShared<FGrpcRevGameCharacterJobSelect>();
	Req.Message_type.CharacterJobSelect->JobId = JobId;
	Req.Message_type.CharacterJobSelect->GraphVersion = GraphVersion;
	Req.Message_type.CharacterJobSelect->ExpectedRevision = ExpectedRevision;
	Req.Message_type.CharacterJobSelect->MutationId = MutationId;
	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendCharacterNodeAcquire(int32 NodeId, int32 ChoiceAdvancementId, int32 GraphVersion, int64 ExpectedRevision, const FString& MutationId)
{
	if (!IsLoggedIn() || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::CharacterNodeAcquire;
	Req.Message_type.CharacterNodeAcquire = MakeShared<FGrpcRevGameCharacterNodeAcquire>();
	Req.Message_type.CharacterNodeAcquire->NodeId = NodeId;
	Req.Message_type.CharacterNodeAcquire->ChoiceAdvancementId = ChoiceAdvancementId;
	Req.Message_type.CharacterNodeAcquire->GraphVersion = GraphVersion;
	Req.Message_type.CharacterNodeAcquire->ExpectedRevision = ExpectedRevision;
	Req.Message_type.CharacterNodeAcquire->MutationId = MutationId;
	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendFieldSpirit(int32 SpiritId)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::FieldSpirit;
	Req.Message_type.FieldSpirit = MakeShared<FGrpcRevGameFieldSpirit>();
	Req.Message_type.FieldSpirit->SpiritId = SpiritId;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendFieldChat(const FString& Text)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::FieldChat;
	Req.Message_type.FieldChat = MakeShared<FGrpcRevGameFieldChat>();
	Req.Message_type.FieldChat->Text = Text;

	Client->Game(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 파티 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendPartyInvite(int64 TargetIdx)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PartyInvite;
	Req.Message_type.PartyInvite = MakeShared<FGrpcRevGamePartyInvite>();
	Req.Message_type.PartyInvite->TargetIdx = static_cast<uint64>(TargetIdx);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendPartyApply(int64 TargetIdx)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PartyApply;
	Req.Message_type.PartyApply = MakeShared<FGrpcRevGamePartyApply>();
	Req.Message_type.PartyApply->TargetIdx = static_cast<uint64>(TargetIdx);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendPartyRespond(int64 TargetIdx, bool bAccept)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PartyRespond;
	Req.Message_type.PartyRespond = MakeShared<FGrpcRevGamePartyRespond>();

	// 초대면 초대한 사람, 신청이면 신청한 사람
	Req.Message_type.PartyRespond->TargetIdx = static_cast<uint64>(TargetIdx);
	Req.Message_type.PartyRespond->Accept = bAccept;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendPartyLeave()
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PartyLeave;
	Req.Message_type.PartyLeave = MakeShared<FGrpcRevGamePartyLeave>();

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendPartyKick(int64 TargetIdx)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PartyKick;
	Req.Message_type.PartyKick = MakeShared<FGrpcRevGamePartyKick>();
	Req.Message_type.PartyKick->TargetIdx = static_cast<uint64>(TargetIdx);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendPartyMemberList()
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PartyMemberList;
	Req.Message_type.PartyMemberList = MakeShared<FGrpcRevGamePartyMemberList>();

	Client->Game(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 전투 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendBattleStart(int32 NaiaId, int32 EncounterKind, int64 SymbolId)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleStart;
	Request.Message_type.BattleStart = MakeShared<FGrpcRevGameBattleStart>();

	Request.Message_type.BattleStart->NaiaId = NaiaId;
	Request.Message_type.BattleStart->EncounterKind = EncounterKind;
	Request.Message_type.BattleStart->SymbolId = SymbolId;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendQuestBattleStart(int32 NaiaId, int32 QuestId, int32 ObjectiveId, int32 BattleMode)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleStart;
	Request.Message_type.BattleStart = MakeShared<FGrpcRevGameBattleStart>();

	Request.Message_type.BattleStart->NaiaId = NaiaId;
	Request.Message_type.BattleStart->QuestId = QuestId;
	Request.Message_type.BattleStart->ObjectiveId = ObjectiveId;
	Request.Message_type.BattleStart->BattleMode = BattleMode;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendQuestCalmStart(int32 QuestId, int32 ObjectiveId)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::QuestCalmStart;
	Request.Message_type.QuestCalmStart = MakeShared<FGrpcRevGameQuestCalmStart>();
	Request.Message_type.QuestCalmStart->QuestId = QuestId;
	Request.Message_type.QuestCalmStart->ObjectiveId = ObjectiveId;

	Client->Game(StreamHandle, Request);
	UE_LOG(LogEmberNet, Log, TEXT("QuestCalmStart 전송 QuestId=%d ObjectiveId=%d"), QuestId, ObjectiveId);
}

void UEmberServerManager::SendQuestCalmResult(int32 QuestId, int32 ObjectiveId, const FString& AttemptId, bool bSuccess)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::QuestCalmResult;
	Request.Message_type.QuestCalmResult = MakeShared<FGrpcRevGameQuestCalmResult>();
	Request.Message_type.QuestCalmResult->QuestId = QuestId;
	Request.Message_type.QuestCalmResult->ObjectiveId = ObjectiveId;
	Request.Message_type.QuestCalmResult->AttemptId = AttemptId;
	Request.Message_type.QuestCalmResult->Success = bSuccess;

	Client->Game(StreamHandle, Request);
	UE_LOG(LogEmberNet, Log, TEXT("QuestCalmResult 전송 QuestId=%d ObjectiveId=%d Success=%d"), QuestId, ObjectiveId, bSuccess ? 1 : 0);
}

void UEmberServerManager::SendBattleCommand(const FNUServerBattleCommandRequest& Command)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}
	
	if (!Command.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("유효하지 않은 BattleCommand 요청"));
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleCommand;
	Request.Message_type.BattleCommand = MakeShared<FGrpcRevGameBattleCommand>();

	FGrpcRevGameBattleCommand& Payload = *Request.Message_type.BattleCommand;

	Payload.BattleId	= Command.BattleId;
	Payload.CombatantId = Command.CombatantId;
	Payload.Action		= Command.Action;
	Payload.SkillId		= Command.SkillId;
	Payload.TargetId	= Command.TargetId;
	Payload.InvestSp	= Command.InvestSP;
	Payload.ItemId		= Command.ItemId;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendBattleReaction(const FNUServerBattleReactionRequest& Reaction)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	if (!Reaction.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("유효하지 않은 BattleReaction 요청"));
		return;
	}
	
	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleReaction;
	Request.Message_type.BattleReaction = MakeShared<FGrpcRevGameBattleReaction>();

	FGrpcRevGameBattleReaction& Payload = *Request.Message_type.BattleReaction;

	Payload.BattleId    = Reaction.BattleId;
	Payload.CombatantId = Reaction.CombatantId;
	Payload.PressTimeMs = Reaction.PressTimeMs;
	Payload.ReactionId  = Reaction.ReactionId;
	Payload.Success     = Reaction.bSuccess;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendBattleNaiaSwap(int64 BattleId, int32 PartySlot)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleNaiaSwap;
	Request.Message_type.BattleNaiaSwap = MakeShared<FGrpcRevGameBattleNaiaSwap>();

	Request.Message_type.BattleNaiaSwap->BattleId = BattleId;
	Request.Message_type.BattleNaiaSwap->PartySlot = PartySlot;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendBattleSync(int64 BattleId)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleSync;
	Request.Message_type.BattleSync = MakeShared<FGrpcRevGameBattleSync>();

	// 0이면 서버가 현재 로그인 캐릭터의 진행 중인 전투를 찾음
	Request.Message_type.BattleSync->BattleId = BattleId;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendBattleLeave(int64 BattleId)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleLeave;
	Request.Message_type.BattleLeave = MakeShared<FGrpcRevGameBattleLeave>();

	Request.Message_type.BattleLeave->BattleId = BattleId;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendBattleActionAck(int32 Turn)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::BattleActionAck;
	Request.Message_type.BattleActionAck = MakeShared<FGrpcRevGameBattleActionAck>();

	Request.Message_type.BattleActionAck->Turn = Turn;

	Client->Game(StreamHandle, Request);
}

/////////////////////////////////////////////////////////////////
// 회복 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendNpcHeal(int32 NpcId, const FString& MutationId)
{
	if (!IsLoggedIn() || !IsValid(Client) || NpcId <= 0 || MutationId.IsEmpty())
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::NpcHeal;
	Request.Message_type.NpcHeal = MakeShared<FGrpcRevGameNpcHeal>();
	Request.Message_type.NpcHeal->NpcId = NpcId;
	Request.Message_type.NpcHeal->MutationId = MutationId;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendNpcRest(int32 NpcId, const FString& MutationId)
{
	if (!IsLoggedIn() || !IsValid(Client) || NpcId <= 0 || MutationId.IsEmpty())
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::NpcRest;
	Request.Message_type.NpcRest = MakeShared<FGrpcRevGameNpcRest>();
	Request.Message_type.NpcRest->NpcId = NpcId;
	Request.Message_type.NpcRest->MutationId = MutationId;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendRecoveryStatus()
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::RecoveryStatus;
	Request.Message_type.RecoveryStatus = MakeShared<FGrpcRevGameRecoveryStatus>();

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendRecoveryPointRegister(int32 LocationId, const FString& MutationId)
{
	if (!IsLoggedIn() || !IsValid(Client) || LocationId <= 0 || MutationId.IsEmpty()) { return; }

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::RecoveryPointRegister;
	Request.Message_type.RecoveryPointRegister = MakeShared<FGrpcRevGameRecoveryPointRegister>();
	Request.Message_type.RecoveryPointRegister->LocationId = LocationId;
	Request.Message_type.RecoveryPointRegister->MutationId = MutationId;
	Client->Game(StreamHandle, Request);
}

/////////////////////////////////////////////////////////////////
// 공간 이동 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendPortalMove(int32 PortalId, int32 SourceSide, const FString& MutationId,
	int32 ToLocationId, int32 ToAreaId, const FString& EntryPointId)
{
	if (!IsLoggedIn() || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PortalMove;
	Request.Message_type.PortalMove = MakeShared<FGrpcRevGamePortalMove>();
	Request.Message_type.PortalMove->PortalId = PortalId;
	Request.Message_type.PortalMove->SourceSide = SourceSide;
	Request.Message_type.PortalMove->MutationId = MutationId;
	Request.Message_type.PortalMove->ToLocationId = ToLocationId;
	Request.Message_type.PortalMove->ToAreaId = ToAreaId;
	Request.Message_type.PortalMove->EntryPointId = EntryPointId;
	UE_LOG(LogEmberNet, Log,
		TEXT("[공간이동][송신] PortalMove portal=%d source_side=%d mutation=%s to_area=%d to_location=%d entry=%s"),
		PortalId, SourceSide, *MutationId, ToAreaId, ToLocationId, *EntryPointId);
	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendPortalArrive(const FString& TransitionId, bool bSuccess, int32 Failure,
	const FVector& Location, int32 Facing)
{
	if (!IsLoggedIn() || Client == nullptr || TransitionId.IsEmpty())
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::PortalArrive;
	Request.Message_type.PortalArrive = MakeShared<FGrpcRevGamePortalArrive>();
	Request.Message_type.PortalArrive->TransitionId = TransitionId;
	Request.Message_type.PortalArrive->Success = bSuccess;
	Request.Message_type.PortalArrive->Failure = Failure;
	Request.Message_type.PortalArrive->PosX = Location.X;
	Request.Message_type.PortalArrive->PosY = Location.Y;
	Request.Message_type.PortalArrive->PosZ = Location.Z;
	Request.Message_type.PortalArrive->Facing = Facing;
	UE_LOG(LogEmberNet, Log,
		TEXT("[공간이동][송신] PortalArrive transition=%s success=%d failure=%d pos=(%.1f, %.1f, %.1f) facing=%d"),
		*TransitionId, bSuccess ? 1 : 0, Failure, Location.X, Location.Y, Location.Z, Facing);
	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendLocationMove(int32 FromLocationId, int32 ToLocationId, int32 GateId,
	const FString& MutationId, const FVector& Location, int32 Facing)
{
	if (!IsLoggedIn() || Client == nullptr || ToLocationId <= 0 || MutationId.IsEmpty())
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::LocationMove;
	Request.Message_type.LocationMove = MakeShared<FGrpcRevGameLocationMove>();
	Request.Message_type.LocationMove->FromLocationId = FromLocationId;
	Request.Message_type.LocationMove->ToLocationId = ToLocationId;
	Request.Message_type.LocationMove->GateId = GateId;
	Request.Message_type.LocationMove->MutationId = MutationId;
	Request.Message_type.LocationMove->PosX = Location.X;
	Request.Message_type.LocationMove->PosY = Location.Y;
	Request.Message_type.LocationMove->PosZ = Location.Z;
	Request.Message_type.LocationMove->Facing = Facing;

	UE_LOG(LogEmberNet, Log,
		TEXT("[공간이동][송신] LocationMove from=%d to=%d gate=%d mutation=%s pos=(%.1f, %.1f, %.1f) facing=%d"),
		FromLocationId, ToLocationId, GateId, *MutationId, Location.X, Location.Y, Location.Z, Facing);

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendLocationRestore(const FString& TransitionId)
{
	if (!IsLoggedIn() || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::LocationRestore;
	Request.Message_type.LocationRestore = MakeShared<FGrpcRevGameLocationRestore>();
	Request.Message_type.LocationRestore->TransitionId = TransitionId;

	UE_LOG(LogEmberNet, Log, TEXT("[공간이동][송신] LocationRestore transition=%s"),
		TransitionId.IsEmpty() ? TEXT("<none>") : *TransitionId);

	Client->Game(StreamHandle, Request);
}

/////////////////////////////////////////////////////////////////
// 나이아 로스터 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendNaiaList()
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::NaiaList;
	Request.Message_type.NaiaList = MakeShared<FGrpcRevGameNaiaList>(); // 본문 없음

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendNaiaMove(const FNUServerNaiaMoveRequest& Move)
{
	if (!IsLoggedIn() || !IsValid(Client))
	{
		return;
	}

	if (!Move.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("유효하지 않은 NaiaMove 요청"));
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::NaiaMove;
	Request.Message_type.NaiaMove = MakeShared<FGrpcRevGameNaiaMove>();

	FGrpcRevGameNaiaMove& Payload = *Request.Message_type.NaiaMove;
	Payload.FromContainer = Move.FromContainer;
	Payload.FromSlot	  = Move.FromSlot;
	Payload.ToContainer   = Move.ToContainer;
	Payload.ToSlot		  = Move.ToSlot;

	Client->Game(StreamHandle, Request);
}

void UEmberServerManager::SendNaiaStarterSelect(int32 NaiaId, const FString& MutationId)
{
	if (!IsLoggedIn() || !IsValid(Client) || NaiaId <= 0 || MutationId.IsEmpty())
	{
		return;
	}

	FGrpcRevGameGameMessage Request = MakeEnvelope();
	Request.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::NaiaStarterSelect;
	Request.Message_type.NaiaStarterSelect = MakeShared<FGrpcRevGameNaiaStarterSelect>();
	Request.Message_type.NaiaStarterSelect->NaiaId = NaiaId;
	Request.Message_type.NaiaStarterSelect->MutationId = MutationId;

	Client->Game(StreamHandle, Request);
}

/////////////////////////////////////////////////////////////////
// 인벤토리 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendInventoryList(int32 Container)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::InventoryList;
	Req.Message_type.InventoryList = MakeShared<FGrpcRevGameInventoryList>();
	Req.Message_type.InventoryList->Container = Container;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendItemMove(int32 FromContainer, int32 FromSlot, int32 ToContainer, int32 ToSlot)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ItemMove;
	Req.Message_type.ItemMove = MakeShared<FGrpcRevGameItemMove>();
	Req.Message_type.ItemMove->FromContainer = FromContainer;
	Req.Message_type.ItemMove->FromSlot = FromSlot;
	Req.Message_type.ItemMove->ToContainer = ToContainer;
	Req.Message_type.ItemMove->ToSlot = ToSlot;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendItemEquip(int32 InvSlot, int32 EquipSlot)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ItemEquip;
	Req.Message_type.ItemEquip = MakeShared<FGrpcRevGameItemEquip>();
	Req.Message_type.ItemEquip->InvSlot = InvSlot;
	Req.Message_type.ItemEquip->EquipSlot = EquipSlot;
	UE_LOG(LogEmberNet, Log, TEXT("[장착 요청] 인벤 칸 %d => 부위 %d"), InvSlot, EquipSlot);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendItemUnequip(int32 EquipSlot)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ItemUnequip;
	Req.Message_type.ItemUnequip = MakeShared<FGrpcRevGameItemUnequip>();
	Req.Message_type.ItemUnequip->EquipSlot = EquipSlot;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendItemSort(int32 Container, const TArray<FNUServerSlotEntry>& Slots)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ItemSort;
	Req.Message_type.ItemSort = MakeShared<FGrpcRevGameItemSort>();
	Req.Message_type.ItemSort->Container = Container;

	// 진단: 서버가 1312(총량 불일치)로 거절하면 이 줄과 서버 기대치를 대조한다
	TMap<int32, int32> Totals;
	for (const FNUServerSlotEntry& Entry : Slots)
	{
		Totals.FindOrAdd(Entry.ItemId) += Entry.ItemCount;
	}
	FString TotalsText;
	for (const TPair<int32, int32>& Pair : Totals)
	{
		TotalsText += FString::Printf(TEXT("%d x%d, "), Pair.Key, Pair.Value);
	}
	UE_LOG(LogEmberNet, Log, TEXT("[정렬 요청] %d칸 — 총량: %s"), Slots.Num(), *TotalsText);

	// 목록 필드는 항목마다 공유 포인터를 만들어 채운다 (TurboLink가 생성한 형태)
	Req.Message_type.ItemSort->Slots.Reserve(Slots.Num());
	for (const FNUServerSlotEntry& Entry : Slots)
	{
		TSharedPtr<FGrpcRevGameItemSlotEntry> Slot = MakeShared<FGrpcRevGameItemSlotEntry>();
		Slot->SlotNo = Entry.SlotNo;
		Slot->ItemId = Entry.ItemId;
		Slot->ItemCount = Entry.ItemCount;
		Req.Message_type.ItemSort->Slots.Add(MoveTemp(Slot));
	}

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendItemSplit(int32 Container, int32 FromSlot, int32 ToSlot, int32 Count)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::ItemSplit;
	Req.Message_type.ItemSplit = MakeShared<FGrpcRevGameItemSplit>();
	Req.Message_type.ItemSplit->Container = Container;
	Req.Message_type.ItemSplit->FromSlot = FromSlot;
	Req.Message_type.ItemSplit->ToSlot = ToSlot;
	Req.Message_type.ItemSplit->Count = Count;

	Client->Game(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 퀘스트 송신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendQuestList()
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::QuestList;
	Req.Message_type.QuestList = MakeShared<FGrpcRevGameQuestList>();

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendQuestAccept(int32 QuestId)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::QuestAccept;
	Req.Message_type.QuestAccept = MakeShared<FGrpcRevGameQuestAccept>();
	Req.Message_type.QuestAccept->QuestId = QuestId;
	Req.Message_type.QuestAccept->MutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendQuestObjective(int32 QuestId, int32 ObjectiveId, int32 CurrentCount)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::QuestObjective;
	Req.Message_type.QuestObjective = MakeShared<FGrpcRevGameQuestObjective>();
	Req.Message_type.QuestObjective->QuestId = QuestId;
	Req.Message_type.QuestObjective->MutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Req.Message_type.QuestObjective->ObjectiveId = ObjectiveId;
	Req.Message_type.QuestObjective->CurrentCount = CurrentCount;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendQuestComplete(int32 QuestId, int32 NpcId)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::QuestComplete;
	Req.Message_type.QuestComplete = MakeShared<FGrpcRevGameQuestComplete>();
	Req.Message_type.QuestComplete->QuestId = QuestId;
	Req.Message_type.QuestComplete->MutationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	Req.Message_type.QuestComplete->NpcId = NpcId;

	Client->Game(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 1:1 거래 송신
//   응답은 ResultCode뿐이고, 제안·잠금·종료는 전부 SVR_Trade_* 푸시로 온다
/////////////////////////////////////////////////////////////////

void UEmberServerManager::SendTradeRequest(int64 TargetIdx)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::TradeRequest;
	Req.Message_type.TradeRequest = MakeShared<FGrpcRevGameTradeRequest>();
	Req.Message_type.TradeRequest->TargetIdx = TargetIdx;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendTradeRespond(bool bAccept, int64 FromIdx)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::TradeRespond;
	Req.Message_type.TradeRespond = MakeShared<FGrpcRevGameTradeRespond>();
	Req.Message_type.TradeRespond->Accept = bAccept;
	Req.Message_type.TradeRespond->FromIdx = FromIdx;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendTradeSetItem(int32 TradeSlot, int32 InvSlotNo)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::TradeSetItem;
	Req.Message_type.TradeSetItem = MakeShared<FGrpcRevGameTradeSetItem>();
	Req.Message_type.TradeSetItem->TradeSlot = TradeSlot;
	Req.Message_type.TradeSetItem->InvSlotNo = InvSlotNo; // -1이면 그 칸을 비운다

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendTradeLock(bool bLock)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::TradeLock;
	Req.Message_type.TradeLock = MakeShared<FGrpcRevGameTradeLock>();
	Req.Message_type.TradeLock->Lock = bLock;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendTradeConfirm(int32 OfferVer)
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::TradeConfirm;
	Req.Message_type.TradeConfirm = MakeShared<FGrpcRevGameTradeConfirm>();
	Req.Message_type.TradeConfirm->OfferVer = OfferVer;

	Client->Game(StreamHandle, Req);
}

void UEmberServerManager::SendTradeCancel()
{
	if (IsLoggedIn() == false || Client == nullptr)
	{
		return;
	}

	FGrpcRevGameGameMessage Req = MakeEnvelope();
	Req.Message_type.Message_typeCase = EGrpcRevGameGameMessageMessage_type::TradeCancel;
	Req.Message_type.TradeCancel = MakeShared<FGrpcRevGameTradeCancel>();

	Client->Game(StreamHandle, Req);
}

/////////////////////////////////////////////////////////////////
// 1:1 거래 수신 → UNUTradeSubsystem으로 전달
/////////////////////////////////////////////////////////////////

UNUTradeSubsystem* UEmberServerManager::GetTradeSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUTradeSubsystem>() : nullptr;
}

void UEmberServerManager::Handle_TradeAck(int32 ResultCode)
{
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->HandleTradeAck(ResultCode);
	}
}

void UEmberServerManager::Handle_SVR_Trade_Requested(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrTradeRequested.IsValid() == false)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Trade_Requested 페이로드 없음"));
		return;
	}
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		const FGrpcRevGameSVR_Trade_Requested& Msg = *Response.Message_type.SvrTradeRequested;
		Trade->HandleTradeRequested(static_cast<int64>(Msg.FromIdx.Value), Msg.FromName, Msg.ExpireSec);
	}
}

void UEmberServerManager::Handle_SVR_Trade_Started(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrTradeStarted.IsValid() == false)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Trade_Started 페이로드 없음"));
		return;
	}
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		const FGrpcRevGameSVR_Trade_Started& Msg = *Response.Message_type.SvrTradeStarted;
		Trade->HandleTradeStarted(static_cast<int64>(Msg.TradeId.Value), Msg.Me, Msg.Other);
	}
}

void UEmberServerManager::Handle_SVR_Trade_Updated(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrTradeUpdated.IsValid() == false)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Trade_Updated 페이로드 없음"));
		return;
	}
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		const FGrpcRevGameSVR_Trade_Updated& Msg = *Response.Message_type.SvrTradeUpdated;
		Trade->HandleTradeUpdated(Msg.Me, Msg.Other, Msg.OfferVer);
	}
}

void UEmberServerManager::Handle_SVR_Trade_Ended(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrTradeEnded.IsValid() == false)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Trade_Ended 페이로드 없음"));
		return;
	}
	if (UNUTradeSubsystem* Trade = GetTradeSubsystem())
	{
		Trade->HandleTradeEnded(Response.Message_type.SvrTradeEnded->Reason);
	}
}

/////////////////////////////////////////////////////////////////
// 전투 수신 -> UNUBattleNetSubsystem으로 전달
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_BattleCommand(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleCommandAck(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_BattleReaction(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	Handle_BattleAck(ResultCode, TEXT("BattleReaction"));

	if (ResultCode != 200 || !Response.Message_type.BattleReaction.IsValid())
	{
		return;
	}

	const FGrpcRevGameBattleReaction& Reaction = *Response.Message_type.BattleReaction;
	if (Reaction._practice_count._practice_countCase == EGrpcRevGameBattleReaction_practice_count::PracticeCount)
	{
		if (UNUQuestEncounterSubsystem* QuestEncounter = GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>())
		{
			QuestEncounter->HandlePracticeCount(Reaction.BattleId, Reaction._practice_count.PracticeCount);
		}
	}
}

void UEmberServerManager::Handle_BattleAck(int32 ResultCode, const TCHAR* RequestName)
{
	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleAck(ResultCode, RequestName);
	}
}

void UEmberServerManager::Handle_SVR_Battle_Started(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleStarted.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_Started 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleStarted(*Response.Message_type.SvrBattleStarted);
		GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>()->HandleStarted(*Response.Message_type.SvrBattleStarted);

		if (BattleNet->IsInBattle())
		{
			GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>()->HandleBattleStarted();
		}
	}
}

void UEmberServerManager::Handle_SVR_Battle_TurnBegin(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleTurnBegin.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_TurnBegin 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleTurnBegin(*Response.Message_type.SvrBattleTurnBegin);
	}
}

void UEmberServerManager::Handle_SVR_Battle_Submit(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleSubmit.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_Submit 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleSubmit(*Response.Message_type.SvrBattleSubmit);
	}
}

void UEmberServerManager::Handle_SVR_Battle_Order(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleOrder.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_Order 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleOrder(*Response.Message_type.SvrBattleOrder);
	}
}

void UEmberServerManager::Handle_SVR_Battle_Reaction(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleReaction.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_Reaction 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleReaction(*Response.Message_type.SvrBattleReaction);
	}
}

void UEmberServerManager::Handle_SVR_Battle_Action(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleAction.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_Action 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleAction(*Response.Message_type.SvrBattleAction);
	}
}

void UEmberServerManager::Handle_SVR_Battle_TurnEnd(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleTurnEnd.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_TurnEnd 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		BattleNet->HandleTurnEnd(*Response.Message_type.SvrBattleTurnEnd);
		GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>()->HandlePracticeCount(Response.Message_type.SvrBattleTurnEnd->BattleId, Response.Message_type.SvrBattleTurnEnd->PracticeCount);
	}
}

void UEmberServerManager::Handle_SVR_Battle_End(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrBattleEnd.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Battle_End 페이로드 없음"));
		return;
	}

	if (UNUBattleNetSubsystem* BattleNet = GetBattleNetSubsystem())
	{
		if (Response.Message_type.SvrBattleEnd->BattleId == BattleNet->GetBattleId())
		{
			GetGameInstance()->GetSubsystem<UNURecoveryNetSubsystem>()->HandleBattleEnded(Response.Message_type.SvrBattleEnd->Reason);
		}

		GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>()->HandleEnd(*Response.Message_type.SvrBattleEnd);
		// 승리 보상은 response_data 에 있음
		BattleNet->HandleEnd(*Response.Message_type.SvrBattleEnd, Response._response_data.ResponseData);
	}
}

/////////////////////////////////////////////////////////////////
// 나이아 로스터 수신 -> UNUNaiaNetSubsystem으로 전달
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_NaiaList(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	// response_data JSON이 아니라 타입 있는 NaiaList
	if (!Response.Message_type.NaiaList.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("NaiaList 페이로드 없음"));
		return;
	}

	if (UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem())
	{
		NaiaNet->HandleListResponse(ResultCode, *Response.Message_type.NaiaList);
	}
}

void UEmberServerManager::Handle_NaiaAck(int32 ResultCode, const TCHAR* RequestName)
{
	if (UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem())
	{
		NaiaNet->HandleAck(ResultCode, RequestName);
	}
}

void UEmberServerManager::Handle_NaiaStarterSelect(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem())
	{
		const FGrpcRevGameNaiaStarterSelect EmptyPayload;
		const FGrpcRevGameNaiaStarterSelect& Payload = Response.Message_type.NaiaStarterSelect.IsValid()
			? *Response.Message_type.NaiaStarterSelect
			: EmptyPayload;
		NaiaNet->HandleStarterSelectResponse(ResultCode, Response._result_message.ResultMessage, Payload);
	}
}

void UEmberServerManager::Handle_SVR_Naia_Updated(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrNaiaUpdated.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Naia_Updated 페이로드 없음"));
		return;
	}

	if (UNUNaiaNetSubsystem* NaiaNet = GetNaiaNetSubsystem())
	{
		NaiaNet->HandleUpdated(*Response.Message_type.SvrNaiaUpdated);
	}
}

UFieldSyncSubsystem* UEmberServerManager::GetFieldSync() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			return World->GetSubsystem<UFieldSyncSubsystem>();
		}
	}
	return nullptr;
}

UNUChatSubsystem* UEmberServerManager::GetChatSubsystem() const
{
	// 채팅은 레벨이 변해도 살아있으므로 GameInstance 수명과 같음
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUChatSubsystem>() : nullptr;
}

UNUPartySubsystem* UEmberServerManager::GetPartySubsystem() const
{
	// 파티도 필드-전투 레벨 전환을 넘어 유지됨
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUPartySubsystem>() : nullptr;
}

UNUBattleNetSubsystem* UEmberServerManager::GetBattleNetSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUBattleNetSubsystem>() : nullptr;
}

UNUNaiaNetSubsystem* UEmberServerManager::GetNaiaNetSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUNaiaNetSubsystem>() : nullptr;
}

/////////////////////////////////////////////////////////////////
// 회복 응답/푸시 → UNURecoveryNetSubsystem으로 전달
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_NpcHeal(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNURecoveryNetSubsystem* RecoveryNet = GetRecoveryNetSubsystem())
	{
		RecoveryNet->HandleResponse(ENURecoveryRequest::Heal, ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_NpcRest(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNURecoveryNetSubsystem* RecoveryNet = GetRecoveryNetSubsystem())
	{
		RecoveryNet->HandleResponse(ENURecoveryRequest::Rest, ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_RecoveryStatus(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNURecoveryNetSubsystem* RecoveryNet = GetRecoveryNetSubsystem())
	{
		RecoveryNet->HandleResponse(ENURecoveryRequest::Status, ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_RecoveryPointRegister(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNURecoveryNetSubsystem* RecoveryNet = GetRecoveryNetSubsystem())
	{
		RecoveryNet->HandleResponse(ENURecoveryRequest::RegisterPoint, ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_SVR_Respawn_Result(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrRespawnResult.IsValid())
	{
		return;
	}

	if (UNURecoveryNetSubsystem* RecoveryNet = GetRecoveryNetSubsystem())
	{
		RecoveryNet->HandleRespawn(*Response.Message_type.SvrRespawnResult);
	}
}

UNURecoveryNetSubsystem* UEmberServerManager::GetRecoveryNetSubsystem() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNURecoveryNetSubsystem>() : nullptr;
}

/////////////////////////////////////////////////////////////////
// 공간 이동 응답 → 현재 월드의 UNUWorldTransitionSubsystem으로 전달
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_PortalMove(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	const FGrpcRevGamePortalMove* PortalMove = Response.Message_type.PortalMove.Get();
	if (PortalMove != nullptr)
	{
		UE_LOG(LogEmberNet, Log,
			TEXT("[공간이동][수신] PortalMove code=%d mutation=%s transition=%s state=%d to_area=%d to_location=%d entry=%s message=%s"),
			ResultCode, *PortalMove->MutationId, *PortalMove->_transition_id.TransitionId,
			PortalMove->_transition_state.TransitionState, PortalMove->ToAreaId, PortalMove->ToLocationId,
			*PortalMove->EntryPointId, *Response._result_message.ResultMessage);
	}
	else
	{
		UE_LOG(LogEmberNet, Warning, TEXT("[공간이동][수신] PortalMove code=%d 본문 없음 message=%s"),
			ResultCode, *Response._result_message.ResultMessage);
	}

	if (UNUWorldTransitionSubsystem* Transition = GetWorldTransitionSubsystem())
	{
		Transition->HandlePortalMoveResponse(ResultCode, Response._result_message.ResultMessage,
			PortalMove);
	}
}

void UEmberServerManager::Handle_PortalArrive(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	const FGrpcRevGamePortalArrive* PortalArrive = Response.Message_type.PortalArrive.Get();
	if (PortalArrive != nullptr)
	{
		UE_LOG(LogEmberNet, Log,
			TEXT("[공간이동][수신] PortalArrive code=%d transition=%s state=%d area=%d location=%d pos=(%.1f, %.1f, %.1f) facing=%d message=%s"),
			ResultCode, *PortalArrive->TransitionId, PortalArrive->_transition_state.TransitionState,
			PortalArrive->_area_id.AreaId, PortalArrive->_location_id.LocationId,
			PortalArrive->PosX, PortalArrive->PosY, PortalArrive->PosZ, PortalArrive->Facing,
			*Response._result_message.ResultMessage);
	}
	else
	{
		UE_LOG(LogEmberNet, Warning, TEXT("[공간이동][수신] PortalArrive code=%d 본문 없음 message=%s"),
			ResultCode, *Response._result_message.ResultMessage);
	}

	UNULocationRestoreSubsystem* Restore = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UNULocationRestoreSubsystem>() : nullptr;

	// 복원 중 보낸 취소 응답이면 일반 포탈 전환 서브시스템이 중복 처리하지 않게 여기서 소비한다.
	if (Restore != nullptr
		&& Restore->HandlePortalArriveResponse(ResultCode, Response._result_message.ResultMessage, PortalArrive))
	{
		return;
	}

	if (UNUWorldTransitionSubsystem* Transition = GetWorldTransitionSubsystem())
	{
		Transition->HandlePortalArriveResponse(ResultCode, Response._result_message.ResultMessage,
			PortalArrive);
	}
}

void UEmberServerManager::Handle_LocationMove(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	const FGrpcRevGameLocationMove* LocationMove = Response.Message_type.LocationMove.Get();

	if (LocationMove != nullptr)
	{
		UE_LOG(LogEmberNet, Log,
			TEXT("[공간이동][수신] LocationMove code=%d from=%d to=%d gate=%d mutation=%s area=%d location=%d indoor=%d pos=(%.1f, %.1f, %.1f) facing=%d message=%s"),
			ResultCode, LocationMove->FromLocationId, LocationMove->ToLocationId, LocationMove->GateId,
			*LocationMove->MutationId, LocationMove->_area_id.AreaId, LocationMove->_location_id.LocationId,
			LocationMove->_is_indoor.IsIndoor ? 1 : 0, LocationMove->PosX, LocationMove->PosY,
			LocationMove->PosZ, LocationMove->Facing, *Response._result_message.ResultMessage);
	}
	else
	{
		UE_LOG(LogEmberNet, Warning, TEXT("[공간이동][수신] LocationMove code=%d 본문 없음 message=%s"),
			ResultCode, *Response._result_message.ResultMessage);
	}

	UWorld* World = GetGameInstance() != nullptr ? GetGameInstance()->GetWorld() : nullptr;

	if (UNUWorldTransitionSubsystem* Transition = World != nullptr ? World->GetSubsystem<UNUWorldTransitionSubsystem>() : nullptr)
	{
		if (Transition->HandleLocationMoveResponse(ResultCode, Response._result_message.ResultMessage, LocationMove)) { return; }
	}

	if (UFieldSyncSubsystem* Sync = World != nullptr ? World->GetSubsystem<UFieldSyncSubsystem>() : nullptr)
	{
		if (Sync->HandleLocationMoveResponse(ResultCode, Response._result_message.ResultMessage, LocationMove))
		{
			return;
		}
	}

	APlayerController* Controller = World != nullptr ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = Controller != nullptr ? Controller->GetPawn() : nullptr;

	if (UNUInteriorParticipantComponent* Participant = Pawn != nullptr
		? Pawn->FindComponentByClass<UNUInteriorParticipantComponent>() : nullptr)
	{
		Participant->HandleLocationMoveResponse(ResultCode, Response._result_message.ResultMessage, LocationMove);
	}
}

void UEmberServerManager::Handle_LocationRestore(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	const FGrpcRevGameLocationRestore* LocationRestore = Response.Message_type.LocationRestore.Get();

	if (LocationRestore != nullptr)
	{
		const FVector Location(LocationRestore->_pos_x.PosX,
			LocationRestore->_pos_y.PosY, LocationRestore->_pos_z.PosZ);

		UE_LOG(LogEmberNet, Log,
			TEXT("[공간이동][수신] LocationRestore code=%d character=%lld transition=%s state=%d area=%d location=%d indoor=%d pos=%s facing=%d message=%s"),
			ResultCode, static_cast<int64>(LocationRestore->_character_idx.CharacterIdx), *LocationRestore->TransitionId,
			LocationRestore->_transition_state.TransitionState, LocationRestore->_area_id.AreaId,
			LocationRestore->_location_id.LocationId, LocationRestore->_is_indoor.IsIndoor ? 1 : 0,
			*Location.ToCompactString(), LocationRestore->_facing.Facing,
			*Response._result_message.ResultMessage);
	}
	else
	{
		UE_LOG(LogEmberNet, Warning, TEXT("[공간이동][수신] LocationRestore code=%d 본문 없음 message=%s"),
			ResultCode, *Response._result_message.ResultMessage);
	}

	if (UNULocationRestoreSubsystem* Restore = GetGameInstance() != nullptr
		? GetGameInstance()->GetSubsystem<UNULocationRestoreSubsystem>() : nullptr)
	{
		Restore->HandleLocationRestoreResponse(ResultCode, Response._result_message.ResultMessage, LocationRestore);
	}
}

UNUWorldTransitionSubsystem* UEmberServerManager::GetWorldTransitionSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	UWorld* World = GI != nullptr ? GI->GetWorld() : nullptr;
	return World != nullptr ? World->GetSubsystem<UNUWorldTransitionSubsystem>() : nullptr;
}

/////////////////////////////////////////////////////////////////
// 응답 수신 → 디스패치 → 타입별 핸들러 호출
/////////////////////////////////////////////////////////////////

void UEmberServerManager::HandleGameResponse(FGrpcContextHandle Handle, const FGrpcResult& Result, const FGrpcRevGameGameMessage& Response)
{
	// **다른(옛) 스트림에서 온 응답은 버린다.**
	// Disconnect()는 구 Client의 델리게이트를 풀지 않으므로, 재접속 중이면
	// 옛 스트림의 늦은 Kick이 갓 살아난 새 세션을 죽일 수 있다.
	if (Handle.Value != StreamHandle.Value)
	{
		// 2026.08.02 확인용으로 임시 로그 추가했습니다
		UE_LOG(LogEmberNet, Warning, TEXT("[진단][수신] 다른 스트림 응답 폐기 (handle %lld != %lld)"),
			Handle.Value, StreamHandle.Value);
		return;
	}

	// 2026.08.02 확인용으로 임시 로그 추가했습니다
	UE_LOG(LogEmberNet, Log, TEXT("[진단][수신] 타입=%d code=%d"),
		(int32)Response.Message_type.Message_typeCase, Response._result_code.ResultCode);

	if (Result.Code != EGrpcResultCode::Ok)
	{
		UE_LOG(LogEmberNet, Error, TEXT("gRPC 오류: %s (%s)"), *Result.GetCodeString(), *Result.Message);
		SetState(EEmberConnState::Failed);
		return;
	}

	const int32 ResultCode = Response._result_code.ResultCode;

	switch (Response.Message_type.Message_typeCase)
	{
	case EGrpcRevGameGameMessageMessage_type::AuthUser:   Handle_AuthUser(ResultCode, Response);   break;
	case EGrpcRevGameGameMessageMessage_type::CreateUser: Handle_CreateUser(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::LoginUser:  Handle_LoginUser(ResultCode, Response);  break;
	case EGrpcRevGameGameMessageMessage_type::Ping:       Handle_Ping(ResultCode, Response);       break;
	case EGrpcRevGameGameMessageMessage_type::Kick:       Handle_Kick(ResultCode, Response);       break;

	// 캐릭터 (선택창)
	case EGrpcRevGameGameMessageMessage_type::CharacterList:   Handle_CharacterList(ResultCode, Response);   break;
	case EGrpcRevGameGameMessageMessage_type::CharacterCreate: Handle_CharacterCreate(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::CharacterSelect: Handle_CharacterSelect(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::CharacterDelete: Handle_CharacterDelete(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::CharacterNameCheck:    Handle_CharacterNameCheck(ResultCode, Response);    break;
	case EGrpcRevGameGameMessageMessage_type::CharacterDeleteCancel: Handle_CharacterDeleteCancel(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::CharacterProfile:      Handle_CharacterProfile(ResultCode, Response);      break;
	case EGrpcRevGameGameMessageMessage_type::CharacterStat:   Handle_CharacterStat(ResultCode, Response);   break;
	case EGrpcRevGameGameMessageMessage_type::CharacterStatAssign: Handle_CharacterStatAssign(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::CharacterProgression: Handle_CharacterProgression(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::CharacterJobSelect: Handle_CharacterJobSelect(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::CharacterNodeAcquire: Handle_CharacterNodeAcquire(ResultCode, Response); break;

	// 인벤토리
	case EGrpcRevGameGameMessageMessage_type::InventoryList: Handle_InventoryList(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::ItemMove:      Handle_ItemMove(ResultCode, Response);      break;
	case EGrpcRevGameGameMessageMessage_type::ItemSort:      Handle_ItemSort(ResultCode, Response);      break;
	case EGrpcRevGameGameMessageMessage_type::ItemEquip:    Handle_ItemEquip(ResultCode, Response);     break;
	case EGrpcRevGameGameMessageMessage_type::ItemUnequip:  Handle_ItemUnequip(ResultCode, Response);   break;
	case EGrpcRevGameGameMessageMessage_type::ItemSplit:    Handle_ItemSplit(ResultCode, Response);     break;
	case EGrpcRevGameGameMessageMessage_type::ItemDiscard:  Handle_ItemDiscard(ResultCode, Response);   break;

	// 상점
	case EGrpcRevGameGameMessageMessage_type::ShopBuy:  Handle_ShopBuy(ResultCode, Response);  break;
	case EGrpcRevGameGameMessageMessage_type::ShopSell: Handle_ShopSell(ResultCode, Response); break;

	// 퀘스트
	case EGrpcRevGameGameMessageMessage_type::SvrQuestUpdated: Handle_SVR_Quest_Updated(Response); break;
	case EGrpcRevGameGameMessageMessage_type::QuestList:      Handle_QuestList(ResultCode, Response);      break;
	case EGrpcRevGameGameMessageMessage_type::QuestAccept:    Handle_QuestAccept(ResultCode, Response);    break;
	case EGrpcRevGameGameMessageMessage_type::QuestObjective: Handle_QuestObjective(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::QuestComplete:  Handle_QuestComplete(ResultCode, Response);  break;
	case EGrpcRevGameGameMessageMessage_type::QuestCalmStart: Handle_QuestCalmStart(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::QuestCalmResult: Handle_QuestCalmResult(ResultCode, Response); break;

	// 서버 푸시 (요청에 대한 응답이 아니다 — 남이 움직여서 서버가 밀어준 것)
	case EGrpcRevGameGameMessageMessage_type::SvrFieldAppeared:    Handle_SVR_Field_Appeared(Response);    break;
	case EGrpcRevGameGameMessageMessage_type::SvrFieldDisappeared: Handle_SVR_Field_Disappeared(Response); break;
	case EGrpcRevGameGameMessageMessage_type::SvrFieldMoved:       Handle_SVR_Field_Moved(Response);       break;
	case EGrpcRevGameGameMessageMessage_type::SvrFieldActivity:    Handle_SVR_Field_Activity(Response);    break;
	case EGrpcRevGameGameMessageMessage_type::SvrFieldSpirit:      Handle_SVR_Field_Spirit(Response);      break;
	case EGrpcRevGameGameMessageMessage_type::SvrItemSlot:         Handle_SVR_Item_Slot(Response);         break;
	case EGrpcRevGameGameMessageMessage_type::FieldEnter:          Handle_FieldEnter(ResultCode, Response); break;

	// 근접 채팅. FieldChat은 요청에 대한 응답인데 본문이 실려 온다 —
	// 내 말은 푸시로 오지 않고 이 응답으로만 온다
	case EGrpcRevGameGameMessageMessage_type::FieldChat:    Handle_FieldChat(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::SvrFieldChat: Handle_SVR_Field_Chat(Response);        break;

	// 파티. 초대·신청·탈퇴·추방은 응답이 ResultCode뿐이라 하나로 받음
	// 목록은 SVR_Party_Updated 푸시로 오므로 여기선 실패만 알리면 된다
	case EGrpcRevGameGameMessageMessage_type::PartyInvite:
	case EGrpcRevGameGameMessageMessage_type::PartyApply:
	case EGrpcRevGameGameMessageMessage_type::PartyLeave:
	case EGrpcRevGameGameMessageMessage_type::PartyKick:            Handle_PartyAck(ResultCode);                      break;
	case EGrpcRevGameGameMessageMessage_type::PartyRespond:         Handle_PartyRespond(ResultCode, Response);        break;
	case EGrpcRevGameGameMessageMessage_type::PartyMemberList:      Handle_PartyMemberList(ResultCode, Response);     break;

	case EGrpcRevGameGameMessageMessage_type::SvrPartyInvited:      Handle_SVR_Party_Invited(Response);               break;
	case EGrpcRevGameGameMessageMessage_type::SvrPartyApplied:      Handle_SVR_Party_Applied(Response);               break;
	case EGrpcRevGameGameMessageMessage_type::SvrPartyUpdated:      Handle_SVR_Party_Updated(Response);               break;
	case EGrpcRevGameGameMessageMessage_type::SvrPartyRejected:     Handle_SVR_Party_Rejected(Response);              break;

	// 1:1 거래. 요청 6종은 응답이 ResultCode뿐이라 하나로 받음
	// 제안·잠금·종료는 SVR_Trade_* 푸시로 오므로 여기선 실패만 알리면 된다
	case EGrpcRevGameGameMessageMessage_type::TradeRequest:
	case EGrpcRevGameGameMessageMessage_type::TradeRespond:
	case EGrpcRevGameGameMessageMessage_type::TradeSetItem:
	case EGrpcRevGameGameMessageMessage_type::TradeLock:
	case EGrpcRevGameGameMessageMessage_type::TradeConfirm:
	case EGrpcRevGameGameMessageMessage_type::TradeCancel:          Handle_TradeAck(ResultCode);                      break;

	case EGrpcRevGameGameMessageMessage_type::SvrTradeRequested:    Handle_SVR_Trade_Requested(Response);             break;
	case EGrpcRevGameGameMessageMessage_type::SvrTradeStarted:      Handle_SVR_Trade_Started(Response);               break;
	case EGrpcRevGameGameMessageMessage_type::SvrTradeUpdated:      Handle_SVR_Trade_Updated(Response);               break;
	case EGrpcRevGameGameMessageMessage_type::SvrTradeEnded:        Handle_SVR_Trade_Ended(Response);                 break;
		
	// 전투 요청 응답
	case EGrpcRevGameGameMessageMessage_type::BattleStart:	  Handle_BattleAck(ResultCode, TEXT("BattleStart"));		break;
	case EGrpcRevGameGameMessageMessage_type::BattleCommand:  Handle_BattleCommand(ResultCode, Response);						break;
	case EGrpcRevGameGameMessageMessage_type::BattleReaction: Handle_BattleReaction(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::BattleNaiaSwap: Handle_BattleAck(ResultCode, TEXT("BattleNaiaSwap"));	break;
	case EGrpcRevGameGameMessageMessage_type::BattleSync:	  Handle_BattleAck(ResultCode, TEXT("BattleSync"));		break;
	case EGrpcRevGameGameMessageMessage_type::BattleLeave:	  Handle_BattleAck(ResultCode, TEXT("BattleLeave"));		break;
	case EGrpcRevGameGameMessageMessage_type::BattleActionAck: Handle_BattleAck(ResultCode, TEXT("BattleActionAck")); break;
		
	// 전투 서버 푸시
	case EGrpcRevGameGameMessageMessage_type::SvrBattleStarted:	  Handle_SVR_Battle_Started(Response);	 break;
	case EGrpcRevGameGameMessageMessage_type::SvrBattleTurnBegin: Handle_SVR_Battle_TurnBegin(Response); break;
	case EGrpcRevGameGameMessageMessage_type::SvrBattleSubmit:	  Handle_SVR_Battle_Submit(Response);	 break;
	case EGrpcRevGameGameMessageMessage_type::SvrBattleOrder:	  Handle_SVR_Battle_Order(Response);	 break;
	case EGrpcRevGameGameMessageMessage_type::SvrBattleReaction:  Handle_SVR_Battle_Reaction(Response);	 break;
	case EGrpcRevGameGameMessageMessage_type::SvrBattleAction:	  Handle_SVR_Battle_Action(Response);	 break;
	case EGrpcRevGameGameMessageMessage_type::SvrBattleTurnEnd:	  Handle_SVR_Battle_TurnEnd(Response);	 break;
	case EGrpcRevGameGameMessageMessage_type::SvrBattleEnd:		  Handle_SVR_Battle_End(Response);		 break;

	// 회복 요청 응답/서버 푸시
	case EGrpcRevGameGameMessageMessage_type::NpcHeal: Handle_NpcHeal(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::NpcRest: Handle_NpcRest(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::RecoveryStatus: Handle_RecoveryStatus(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::RecoveryPointRegister: Handle_RecoveryPointRegister(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::SvrRespawnResult: Handle_SVR_Respawn_Result(Response); break;

	// 포탈 이동 승인과 목적지 준비 완료 확정
	case EGrpcRevGameGameMessageMessage_type::PortalMove:   Handle_PortalMove(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::PortalArrive: Handle_PortalArrive(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::LocationMove: Handle_LocationMove(ResultCode, Response); break;
	case EGrpcRevGameGameMessageMessage_type::LocationRestore: Handle_LocationRestore(ResultCode, Response); break;

	// 나이아 로스터 요청 응답
	case EGrpcRevGameGameMessageMessage_type::NaiaList:          Handle_NaiaList(ResultCode, Response);          break;
	case EGrpcRevGameGameMessageMessage_type::NaiaMove:          Handle_NaiaAck(ResultCode, TEXT("NaiaMove"));  break;
	case EGrpcRevGameGameMessageMessage_type::NaiaStarterSelect: Handle_NaiaStarterSelect(ResultCode, Response); break;

	// 나이아 로스터 서버 푸시
	case EGrpcRevGameGameMessageMessage_type::SvrNaiaUpdated: Handle_SVR_Naia_Updated(Response); break;

	default:
		UE_LOG(LogEmberNet, Warning, TEXT("처리하지 않는 메시지 타입 (%d)"), (int32)Response.Message_type.Message_typeCase);
		break;
	}
}

/////////////////////////////////////////////////////////////////
// 필드 푸시 수신 → UFieldSyncSubsystem 으로 전달
//   페이로드 IsValid() 검사 필수 — oneof가 설정되지 않은 메시지는
//   기본값(LoginUser) + null 포인터로 도착하므로 그냥 역참조하면 크래시난다.
/////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////
// 캐릭터 응답 → UNUCharacterManager로 전달
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_CharacterList(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUCharacterManager* Manager = GetCharacterManager())
	{
		Manager->HandleListResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterCreate(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUCharacterManager* Manager = GetCharacterManager())
	{
		// 생성 응답 JSON에 만들어진 캐릭터 1건이 실려 온다 --- 새 Idx를 알 수 있는 유일한 곳이다
		Manager->HandleCreateResponse(ResultCode, Response._result_message.ResultMessage,
			Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterSelect(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUCharacterManager* Manager = GetCharacterManager())
	{
		Manager->HandleSelectResponse(ResultCode, Response._response_data.ResponseData);
	}

	// 채팅 스트림은 Connect 때 이미 열렸다. 여기서는 **ChatLogin 을 보낼 때가 됐다**고만 알린다 —
	// 채팅 서버는 게임 서버가 Redis 에 써둔 이름·idx 로 신원을 잡는데 캐릭터 선택 전에는
	// 그 값이 비어 있어 거부(1405)당하기 때문이다.
	// 캐릭터를 **바꿔서** 다시 선택한 경우에도 불러야 한다(안 하면 옛 이름으로 계속 발화한다).
	if (ResultCode == EmberServerCfg::RC_SUCCESS)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UNUCharacterManager* CharacterManager = GI->GetSubsystem<UNUCharacterManager>())
			{
				// 캐릭터 선택 완료 직후 요청해 FieldEnter보다 먼저 서버 위치를 확보한다.
				if (CharacterManager->IsSelectionAwaitingLocationRestore())
				{
					GI->GetSubsystem<UNULocationRestoreSubsystem>()->BeginCharacterRestore(
						CharacterManager->GetSelectedCharacterIdx());
				}

				if (UNUProgressionNetSubsystem* ProgressionNet = GI->GetSubsystem<UNUProgressionNetSubsystem>())
				{
					ProgressionNet->BeginCharacterSession(CharacterManager->GetSelectedCharacterIdx());
				}
			}

			if (UEmberChatManager* ChatManager = GI->GetSubsystem<UEmberChatManager>())
			{
				ChatManager->NotifyCharacterSelected();
			}

			// 회복 정산(192) 응답 뒤 NaiaList를 조회해야 미접속 HP가 반영된다.
			if (UNUCharacterManager* Characters = GI->GetSubsystem<UNUCharacterManager>())
			{
				GI->GetSubsystem<UNURecoveryNetSubsystem>()->BeginCharacterSession(Characters->GetSelectedCharacterIdx());
			}
		}
	}
}

void UEmberServerManager::Handle_CharacterNameCheck(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	// 응답은 result_code 하나뿐이다 (response_data 없음)
	if (UNUCharacterManager* Manager = GetCharacterManager())
	{
		Manager->HandleNameCheckResponse(ResultCode);
	}
}

void UEmberServerManager::Handle_CharacterDeleteCancel(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	// 성공 응답 JSON = 삭제 응답과 같은 목록 형식이라 같은 흐름을 태운다
	if (UNUCharacterManager* Manager = GetCharacterManager())
	{
		Manager->HandleDeleteCancelResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterProfile(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUCharacterManager* Manager = GetCharacterManager())
	{
		Manager->HandleProfileResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_ItemDiscard(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	// 200이면 바뀐 칸이 SVR_Item_Slot으로 따로 온다 --- 여기서는 거절만 알리면 된다
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		Inventory->HandleServerDiscardResponse(ResultCode);
	}
}

// 상점 구매/판매 --- 바뀐 칸은 SVR_Item_Slot으로 따로 오고, 여기서는 골드 잔액(response_data)과 거절 사유(result_message)만 넘긴다
void UEmberServerManager::Handle_ShopBuy(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		Inventory->HandleServerShopResponse(true, ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_ShopSell(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		Inventory->HandleServerShopResponse(false, ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterDelete(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUCharacterManager* Manager = GetCharacterManager())
	{
		Manager->HandleDeleteResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterStat(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUProgressionComponent* Progression = GetLocalProgressionComponent())
	{
		Progression->HandleServerCharacterStatResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterStatAssign(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUProgressionComponent* Progression = GetLocalProgressionComponent())
	{
		Progression->HandleServerCharacterStatResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterProgression(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem())
	{
		ProgressionNet->HandleProgressionResponse(ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterJobSelect(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem())
	{
		ProgressionNet->HandleJobSelectResponse(ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_CharacterNodeAcquire(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUProgressionNetSubsystem* ProgressionNet = GetProgressionNetSubsystem())
	{
		ProgressionNet->HandleNodeAcquireResponse(ResultCode, Response._result_message.ResultMessage, Response._response_data.ResponseData);
	}
}

UNUCharacterManager* UEmberServerManager::GetCharacterManager() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
}

UNUProgressionNetSubsystem* UEmberServerManager::GetProgressionNetSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI != nullptr ? GI->GetSubsystem<UNUProgressionNetSubsystem>() : nullptr;
}

UNUProgressionComponent* UEmberServerManager::GetLocalProgressionComponent() const
{
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerState* PS = PC ? PC->PlayerState.Get() : nullptr;
	return PS ? PS->FindComponentByClass<UNUProgressionComponent>() : nullptr;
}

/////////////////////////////////////////////////////////////////
// 인벤토리 응답/푸시 → 로컬 UNUInventoryComponent로 전달
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_InventoryList(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		Inventory->HandleServerListResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_ItemMove(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		Inventory->HandleServerMoveResponse(ResultCode);
	}
}

void UEmberServerManager::Handle_ItemSort(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		Inventory->HandleServerSortResponse(ResultCode);
	}
}

void UEmberServerManager::Handle_ItemEquip(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	// 200 = 접수 (바뀐 칸은 SVR_Item_Slot 푸시로 온다)
	if (ResultCode != 200)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("장착 거절 (%d)"), ResultCode);
		if (UNUInventoryComponent* Inventory = GetLocalInventory())
		{
			Inventory->ShowEquipRejectNotice(ResultCode, false);
		}
	}
}

void UEmberServerManager::Handle_ItemUnequip(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (ResultCode != 200)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("해제 거절 (%d)"), ResultCode);
		if (UNUInventoryComponent* Inventory = GetLocalInventory())
		{
			Inventory->ShowEquipRejectNotice(ResultCode, true);
		}
	}
}

void UEmberServerManager::Handle_ItemSplit(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		Inventory->HandleServerSplitResponse(ResultCode);
	}
}

void UEmberServerManager::Handle_SVR_Item_Slot(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrItemSlot.IsValid() == false)
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Item_Slot 페이로드 없음"));
		return;
	}
	if (UNUInventoryComponent* Inventory = GetLocalInventory())
	{
		const FGrpcRevGameSVR_Item_Slot& Slot = *Response.Message_type.SvrItemSlot;
		UE_LOG(LogEmberNet, Log, TEXT("[칸 푸시] 저장소 %d 칸 %d = 아이템 %d x%d"),
			Slot.Container, Slot.SlotNo, Slot.ItemId, Slot.ItemCount);
		Inventory->HandleServerSlotPush(Slot.Container, Slot.SlotNo, Slot.ItemId, Slot.ItemCount);
	}
}

UNUInventoryComponent* UEmberServerManager::GetLocalInventory() const
{
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerState* PS = PC ? PC->PlayerState.Get() : nullptr;
	return PS ? PS->FindComponentByClass<UNUInventoryComponent>() : nullptr;
}

/////////////////////////////////////////////////////////////////
// 퀘스트 응답 -> 로컬 UNUQuestComponent로 전달
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_QuestList(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUQuestComponent* QuestComponent = GetLocalQuestComponent())
	{
		QuestComponent->HandleServerListResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_QuestAccept(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUQuestComponent* QuestComponent = GetLocalQuestComponent())
	{
		QuestComponent->HandleServerAcceptResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_QuestObjective(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUQuestComponent* QuestComponent = GetLocalQuestComponent())
	{
		QuestComponent->HandleServerObjectiveResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_QuestComplete(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUQuestComponent* QuestComponent = GetLocalQuestComponent())
	{
		QuestComponent->HandleServerCompleteResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_QuestCalmStart(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	const TSharedPtr<FGrpcRevGameQuestCalmStart>& Message = Response.Message_type.QuestCalmStart;
	const bool bHasAttempt = Message.IsValid() && Message->_attempt_id._attempt_idCase == EGrpcRevGameQuestCalmStart_attempt_id::AttemptId;
	GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>()->HandleCalmStartResponse(ResultCode, bHasAttempt ? Message->_attempt_id.AttemptId : FString());
}

void UEmberServerManager::Handle_QuestCalmResult(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	GetGameInstance()->GetSubsystem<UNUQuestEncounterSubsystem>()->HandleCalmResultResponse(ResultCode);
	if (UNUQuestComponent* QuestComponent = GetLocalQuestComponent())
	{
		QuestComponent->HandleServerCalmResultResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_SVR_Quest_Updated(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrQuestUpdated.IsValid())
	{
		UE_LOG(LogEmberNet, Warning, TEXT("SVR_Quest_Updated 페이로드 없음"));
		return;
	}

	if (UNUQuestComponent* QuestComponent = GetLocalQuestComponent())
	{
		const FGrpcRevGameSVR_Quest_Updated& Message = *Response.Message_type.SvrQuestUpdated;
		QuestComponent->HandleServerQuestUpdated(Message.QuestId, Message.Reason, Response._response_data.ResponseData);
	}
}

UNUQuestComponent* UEmberServerManager::GetLocalQuestComponent() const
{
	const UGameInstance* GI = GetGameInstance();
	const UWorld* World = GI ? GI->GetWorld() : nullptr;
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerState* PS = PC ? PC->PlayerState.Get() : nullptr;
	return PS ? PS->FindComponentByClass<UNUQuestComponent>() : nullptr;
}

void UEmberServerManager::Handle_FieldEnter(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		const FGrpcRevGameFieldEnter* FieldEnter = Response.Message_type.FieldEnter.Get();
		if (FieldEnter == nullptr)
		{
			Sync->HandleEnterResponse(0, TEXT("FieldEnter 응답 본문이 없습니다."), 0, 0, false);
			return;
		}
		Sync->HandleEnterResponse(ResultCode, Response._result_message.ResultMessage,
			FieldEnter->_area_id.AreaId, FieldEnter->LocationId, FieldEnter->_is_indoor.IsIndoor);
	}
}

void UEmberServerManager::Handle_SVR_Field_Appeared(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrFieldAppeared.IsValid() == false)
	{
		// 2026.08.02 확인용으로 임시 로그 추가했습니다
		UE_LOG(LogEmberNet, Warning, TEXT("[진단] SVR_Field_Appeared 페이로드 없음"));
		return;
	}
	UFieldSyncSubsystem* Sync = GetFieldSync();
	if (Sync == nullptr)
	{
		// 2026.08.02 확인용으로 임시 로그 추가했습니다
		UE_LOG(LogEmberNet, Warning, TEXT("[진단] SVR_Field_Appeared — FieldSyncSubsystem 없음"));
		return;
	}

	const FGrpcRevGameSVR_Field_Appeared& D = *Response.Message_type.SvrFieldAppeared;

	FNUFieldAppearance Data;
	Data.Location      = FVector(D.PosX, D.PosY, D.PosZ);
	Data.Facing        = static_cast<EFieldFacing>(D.Facing);
	Data.Activity      = static_cast<EFieldActivity>(D.Activity);
	Data.SpiritId      = D.SpiritId;
	Data.CharacterIdx  = static_cast<int64>(D.CharacterIdx.Value);
	Data.CharacterName = D.CharacterName;
	Data.RaceId        = D.RaceId;
	Data.Gender        = D.Gender;
	Data.HairId        = D.HairId;
	Data.FaceId        = D.FaceId;
	Data.JobId         = D.JobId;
	Data.AreaId        = D.AreaId;
	Data.LocationId    = D.LocationId;
	Data.bIndoor       = D.IsIndoor;

	Sync->OnAppeared(static_cast<int64>(D.PlayerId.Value), Data);
}

void UEmberServerManager::Handle_SVR_Field_Disappeared(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrFieldDisappeared.IsValid() == false)
	{
		return;
	}
	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		const FGrpcRevGameSVR_Field_Disappeared& D = *Response.Message_type.SvrFieldDisappeared;
		Sync->OnDisappeared(static_cast<int64>(D.PlayerId.Value));
	}
}

void UEmberServerManager::Handle_SVR_Field_Moved(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrFieldMoved.IsValid() == false)
	{
		return;
	}
	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		const FGrpcRevGameSVR_Field_Moved& D = *Response.Message_type.SvrFieldMoved;
		Sync->OnMoved(
			static_cast<int64>(D.PlayerId.Value),
			FVector(D.PosX, D.PosY, D.PosZ),
			static_cast<EFieldFacing>(D.Facing));
	}
}

void UEmberServerManager::Handle_SVR_Field_Activity(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrFieldActivity.IsValid() == false)
	{
		return;
	}
	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		const FGrpcRevGameSVR_Field_Activity& D = *Response.Message_type.SvrFieldActivity;
		Sync->OnActivityChanged(
			static_cast<int64>(D.PlayerId.Value),
			static_cast<EFieldActivity>(D.Activity));
	}
}

void UEmberServerManager::Handle_SVR_Field_Spirit(const FGrpcRevGameGameMessage& Response)
{
	if (Response.Message_type.SvrFieldSpirit.IsValid() == false)
	{
		return;
	}
	if (UFieldSyncSubsystem* Sync = GetFieldSync())
	{
		const FGrpcRevGameSVR_Field_Spirit& D = *Response.Message_type.SvrFieldSpirit;
		Sync->OnSpiritChanged(
			static_cast<int64>(D.PlayerId.Value),
			D.SpiritId);
	}
}

void UEmberServerManager::Handle_FieldChat(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	UNUChatSubsystem* Chat = GetChatSubsystem();
	if (!IsValid(Chat))
	{
		return;
	}

	// 실패면 본문이 비어 있을 수 있지만 코드는 넘겨야 거부를 알릴 수 있음
	const FString Text = Response.Message_type.FieldChat.IsValid()
		? Response.Message_type.FieldChat->Text
		: FString();

	Chat->HandleFieldChatEcho(ResultCode, Text);
}

void UEmberServerManager::Handle_SVR_Field_Chat(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrFieldChat.IsValid())
	{
		return;
	}
	
	if (UNUChatSubsystem* Chat = GetChatSubsystem())
	{
		const FGrpcRevGameSVR_Field_Chat& D = *Response.Message_type.SvrFieldChat;
		Chat->HandleFieldChatReceived(
			static_cast<int64>(D.PlayerId.Value),
			D.Text);
	}
}

/////////////////////////////////////////////////////////////////
// 파티 수신
/////////////////////////////////////////////////////////////////

void UEmberServerManager::Handle_PartyAck(int32 ResultCode)
{
	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		PartySubsystem->HandleAck(ResultCode);
	}
}

void UEmberServerManager::Handle_PartyRespond(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		// 수락이면 파티원 목록이, 거절이면 ResultCode만 온다
		PartySubsystem->HandleRespondResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_PartyMemberList(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		PartySubsystem->HandleMemberListResponse(ResultCode, Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_SVR_Party_Invited(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrPartyInvited.IsValid())
	{
		return;
	}

	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		const FGrpcRevGameSVR_Party_Invited& D = *Response.Message_type.SvrPartyInvited;
		PartySubsystem->HandleInvited(static_cast<int64>(D.FromIdx.Value), D.FromName, D.ExpireSec);
	}
}

void UEmberServerManager::Handle_SVR_Party_Applied(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrPartyApplied.IsValid())
	{
		return;
	}

	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		const FGrpcRevGameSVR_Party_Applied& D = *Response.Message_type.SvrPartyApplied;
		PartySubsystem->HandleApplied(static_cast<int64>(D.FromIdx.Value), D.FromName, D.ExpireSec);
	}
}

void UEmberServerManager::Handle_SVR_Party_Updated(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrPartyUpdated.IsValid())
	{
		return;
	}

	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		// 파티는 푸시에도 response_data가 실림, 다른 SVR_* 와 다른 예외
		// 변동마다 목록을 다시 요청할 필요가 없음
		const FGrpcRevGameSVR_Party_Updated& D = *Response.Message_type.SvrPartyUpdated;
		PartySubsystem->HandleUpdated(
			static_cast<int64>(D.PartyId.Value),
			D.Reason,
			static_cast<int64>(D.TargetIdx.Value),
			Response._response_data.ResponseData);
	}
}

void UEmberServerManager::Handle_SVR_Party_Rejected(const FGrpcRevGameGameMessage& Response)
{
	if (!Response.Message_type.SvrPartyRejected.IsValid())
	{
		return;
	}

	if (UNUPartySubsystem* PartySubsystem = GetPartySubsystem())
	{
		const FGrpcRevGameSVR_Party_Rejected& D = *Response.Message_type.SvrPartyRejected;
		PartySubsystem->HandleRejected(D.FromName, D.WasInvite);
	}
}

void UEmberServerManager::Handle_AuthUser(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (ResultCode == EmberServerCfg::RC_SUCCESS && Response.Message_type.AuthUser.IsValid())
	{
		const FString PassKey = Response.Message_type.AuthUser->_ret_pass_key.RetPassKey;
		const FString SubPassKey = Response.Message_type.AuthUser->_ret_sub_pass_key.RetSubPassKey;
		// 구글 계정 id 는 보낸 "g_"+sub 그대로다 (서버 10/14). 혹시 응답이 다른 id 를 주면 세션 토큰과 이후 요청이 그 id 를 쓴다
		if (bAdoptServerUserId && !Response.UserId.IsEmpty() && Response.UserId != UserId)
		{
			UE_LOG(LogEmberNet, Log, TEXT("서버가 정한 계정 id 사용 [%s] → [%s]"), *UserId, *Response.UserId);
			UserId = Response.UserId;
		}
		// pass key/IV + 인증 timestamp로 세션 토큰(Bearer) 파생 — 이후 요청 token에 실어 서버가 검증
		SessionToken = FEmberBearerToken::GenerateToken(PassKey, UserId, AuthBaseTimeStamp, SubPassKey);
		UE_LOG(LogEmberNet, Log, TEXT("--- 인증 성공 [%s]"), *UserId);
		SendLoginUser();
	}
	else
	{
		UE_LOG(LogEmberNet, Error, TEXT("인증 실패 (%d) [%s]"), ResultCode, *UserId);
		SetState(EEmberConnState::Failed);
	}
}

void UEmberServerManager::Handle_CreateUser(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (ResultCode == EmberServerCfg::RC_SUCCESS)
	{
		UE_LOG(LogEmberNet, Log, TEXT("--- 유저 생성 성공 [%s] %s"), *UserId, *Response._response_data.ResponseData);
		SendLoginUser();
	}
	else
	{
		UE_LOG(LogEmberNet, Error, TEXT("유저 생성 실패 (%d) [%s]"), ResultCode, *UserId);
		SetState(EEmberConnState::Failed);
	}
}

void UEmberServerManager::Handle_LoginUser(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	if (ResultCode == EmberServerCfg::RC_SUCCESS)
	{
		UE_LOG(LogEmberNet, Log, TEXT("--- 로그인 성공 [%s] %s"), *UserId, *Response._response_data.ResponseData);
		SetState(EEmberConnState::LoggedIn);
		// 핑 주기 시작 (세션 유지)
		if (UGameInstance* GI = GetGameInstance())
		{
			// 맨처음 핑만 빠르게 시작, 
			// 평균 지연 시간을 구하기 위해 첫 Ping을 앞당김
			GI->GetTimerManager().SetTimer(
				PingTimer, 
				this,
				&UEmberServerManager::SendPing, 
				EmberServerCfg::PING_INTERVAL_SEC, 
				true,
				1.f
				);
		}
	}
	else if (ResultCode == EmberServerCfg::RC_LOGINUSER_NOT_FOUND)
	{
		UE_LOG(LogEmberNet, Log, TEXT("유저 없음 → 생성 [%s]"), *UserId);
		SendCreateUser();
	}
	else
	{
		UE_LOG(LogEmberNet, Error, TEXT("로그인 실패 (%d) [%s]"), ResultCode, *UserId);
		SetState(EEmberConnState::Failed);
	}
}

void UEmberServerManager::Handle_Ping(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	UpdateRtt();
	
	UE_LOG(LogEmberNet, Verbose, TEXT("Pong [%s] 평균 왕복시간 %dms (표본 %d개)"),
		*UserId, GetAverageRttMs(), RttSampleCount);
}

void UEmberServerManager::Handle_Kick(int32 ResultCode, const FGrpcRevGameGameMessage& Response)
{
	UE_LOG(LogEmberNet, Warning, TEXT("--- 강퇴됨 (사유 %d) [%s]"), ResultCode, *UserId);
	SetState(EEmberConnState::Failed);
	Disconnect();
}

/////////////////////////////////////////////////////////////////
// 내부 유틸
/////////////////////////////////////////////////////////////////

int64 UEmberServerManager::NowUnixSeconds() const
{
	return FDateTime::UtcNow().ToUnixTimestamp();
}

void UEmberServerManager::SetState(EEmberConnState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		OnStateChanged.Broadcast(NewState);
	}
}
