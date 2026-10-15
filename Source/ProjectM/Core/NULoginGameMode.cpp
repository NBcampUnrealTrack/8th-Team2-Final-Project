#include "Core/NULoginGameMode.h"

#include "Auth/NUGoogleAuthSettings.h"
#include "Auth/NUGoogleAuthSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "WorldTransition/World/NUFieldLoadingSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNULogin, Log, All);

namespace
{
	// user_id는 2~64자, 영어 + 숫자 + `_` (서버 상한 64 --- 구글 계정 id "g_"+sub 가 23자라 10/14 에 20자에서 늘림)
	constexpr int32 USER_ID_MIN_LEN = 2;
	constexpr int32 USER_ID_MAX_LEN = 64;
}

ANULoginGameMode::ANULoginGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
	PlayerStateClass = APlayerState::StaticClass();
}

void ANULoginGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.AddDynamic(this, &ThisClass::HandleConnStateChanged);
	}
	else
	{
		UE_LOG(LogNULogin, Error, TEXT("EmberServerManager를 찾을 수 없습니다"));
	}
	if (UNUGoogleAuthSubsystem* Google = GetGoogleAuth())
	{
		Google->OnSignInProgress.AddUObject(this, &ThisClass::HandleGoogleProgress);
		Google->OnSignInFinished.AddUObject(this, &ThisClass::HandleGoogleFinished);
	}

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!IsValid(PC))
	{
		// 넷 모드가 Standalone이 아니면 여기는 서버 월드이고 로컬 PC가 없어서 위젯을 띄울 대상이 없음
		UE_LOG(LogNULogin, Error,
			TEXT("로컬 PlayerController가 없어 로그인 위젯을 만들 수 없습니다. PIE Net Mode를 Play Standalone으로 두세요"));
		return;
	}

	// UIOnly 모드로 변경, 커서 보이게
	PC->bShowMouseCursor = true;
	PC->SetInputMode(FInputModeUIOnly());

	// 프로젝트 세팅의 덮어쓰기(구글 로그인 화면)가 있으면 그것을 쓴다 --- BP 를 고치지 않고 ini 한 줄로 바꾸고 되돌리려고
	if (UClass* Override = GetDefault<UNUGoogleAuthSettings>()->LoginWidgetOverride.TryLoadClass<UUserWidget>())
	{
		LoginWidgetClass = Override;
	}
	if (!LoginWidgetClass)
	{
		UE_LOG(LogNULogin, Error, TEXT("LoginWidgetClass가 지정되지 않았습니다"));
		return;
	}

	LoginWidget = CreateWidget<UUserWidget>(PC, LoginWidgetClass);
	if (IsValid(LoginWidget))
	{
		LoginWidget->AddToViewport();
	}

	// 구글 로그인이 설정돼 있으면 그쪽으로 안내한다 (개발용 이름 접속은 아래에 따로 있다)
	SetStatus(IsGoogleLoginAvailable()
		? FText::FromString(TEXT("구글 계정으로 로그인하세요"))
		: FText::FromString(TEXT("ID를 입력하세요")), false);
}

void ANULoginGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEmberServerManager* Server = GetServerManager())
	{
		Server->OnStateChanged.RemoveDynamic(this, &ThisClass::HandleConnStateChanged);
	}
	if (UNUGoogleAuthSubsystem* Google = GetGoogleAuth())
	{
		Google->OnSignInProgress.RemoveAll(this);
		Google->OnSignInFinished.RemoveAll(this);
		Google->Cancel(FText::GetEmpty());          // 화면을 떠나면 브라우저 응답을 더 기다리지 않는다
	}

	Super::EndPlay(EndPlayReason);
}

void ANULoginGameMode::RequestLogin(const FString& InUserId)
{
	if (bLoginInProgress)
	{
		return;
	}

	// 시작과 끝에 있는 공백문자 제거
	FString Trimmed = InUserId.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		SetStatus(FText::FromString(TEXT("ID를 입력하세요")), false);
		return;
	}

	// 서버 규약 - user_id는 2~64자, 영어, 숫자와 `_` 만
	// 어차피 거부되기 때문에 접속 시도 전에 클라에서 막음
	if (Trimmed.Len() < USER_ID_MIN_LEN || Trimmed.Len() > USER_ID_MAX_LEN)
	{
		SetStatus(FText::Format(FText::FromString(TEXT("ID는 {0}~{1}자여야 합니다")), USER_ID_MIN_LEN, USER_ID_MAX_LEN), false);
		return;
	}

	for (const TCHAR Char : Trimmed)
	{
		if (!FChar::IsAlnum(Char) && Char != TEXT('_'))
		{
			SetStatus(FText::FromString(TEXT("ID는 영문, 숫자, _ 만 사용할 수 있습니다")), false);
			return;
		}
	}

	UEmberServerManager* Server = GetServerManager();
	if (!Server)
	{
		SetStatus(FText::FromString(TEXT("서버 매니저를 찾을 수 없습니다")), false);
		return;
	}

	// 접속 성공 후 이동할 레벨이 없으면 로그인 화면에 갇힌다. 미리 막는다
	if (FieldLevel.IsNull())
	{
		UE_LOG(LogNULogin, Error, TEXT("FieldLevel이 지정되지 않았습니다"));
		SetStatus(FText::FromString(TEXT("필드 레벨이 지정되지 않았습니다")), false);
		return;
	}

	bLoginInProgress = true;
	SetStatus(FText::FromString(TEXT("접속 중...")), true);

	UE_LOG(LogNULogin, Log, TEXT("로그인 요청 [%s]"), *Trimmed);

	// 서버로 접속 시도 (이전 구글 로그인 자격이 남아 있으면 지운다)
	Server->ResetCredential();
	Server->Connect(Trimmed);
}

bool ANULoginGameMode::IsGoogleLoginAvailable() const
{
	return UNUGoogleAuthSubsystem::IsConfigured();
}

bool ANULoginGameMode::IsDevNameLoginVisible() const
{
#if UE_BUILD_SHIPPING
	return false;
#else
	return GetDefault<UNUGoogleAuthSettings>()->bShowDevNameLogin;
#endif
}

bool ANULoginGameMode::IsGoogleSignInPending() const
{
	const UNUGoogleAuthSubsystem* Google = GetGoogleAuth();
	return Google != nullptr && Google->IsSigningIn();
}

void ANULoginGameMode::RequestGoogleLogin()
{
	if (bLoginInProgress)
	{
		return;
	}
	UNUGoogleAuthSubsystem* Google = GetGoogleAuth();
	if (Google == nullptr)
	{
		SetStatus(FText::FromString(TEXT("구글 로그인을 사용할 수 없습니다")), false);
		return;
	}
	if (FieldLevel.IsNull())
	{
		UE_LOG(LogNULogin, Error, TEXT("FieldLevel이 지정되지 않았습니다"));
		SetStatus(FText::FromString(TEXT("필드 레벨이 지정되지 않았습니다")), false);
		return;
	}

	bLoginInProgress = true;
	if (!Google->SignIn())
	{
		// 실패 사유는 HandleGoogleFinished 가 이미 띄웠다 (설정 없음, 포트, 브라우저)
		bLoginInProgress = false;
	}
}

void ANULoginGameMode::CancelGoogleLogin()
{
	if (UNUGoogleAuthSubsystem* Google = GetGoogleAuth(); Google != nullptr && Google->IsSigningIn())
	{
		Google->Cancel(FText::GetEmpty());
	}
}

void ANULoginGameMode::HandleGoogleProgress(const FText& Message)
{
	SetStatus(Message, true);
}

void ANULoginGameMode::HandleGoogleFinished(bool bSuccess, const FText& Message)
{
	if (!bSuccess)
	{
		bLoginInProgress = false;
		SetStatus(Message, false);
		return;
	}

	UNUGoogleAuthSubsystem* Google = GetGoogleAuth();
	UEmberServerManager* Server = GetServerManager();
	if (Google == nullptr || Server == nullptr)
	{
		bLoginInProgress = false;
		SetStatus(FText::FromString(TEXT("서버 매니저를 찾을 수 없습니다")), false);
		return;
	}

	// 토큰 검증 서버: ID 토큰 + "g_"+sub / 옛 임시 방식: sub 해시 id + 임시 키
	const bool bVerify = GetDefault<UNUGoogleAuthSettings>()->bServerVerifiesIdToken;
	const FString UserId = Google->MakeGameUserId();
	UE_LOG(LogNULogin, Log, TEXT("구글 로그인 → 서버 접속 [%s] %s (토큰 검증 %s)"), *UserId, *Google->GetAccount().Email, bVerify ? TEXT("켬") : TEXT("끔"));
	SetStatus(Message, true);
	Server->ConnectWithGoogle(UserId, Google->GetAccount().IdToken, bVerify);
}

UNUGoogleAuthSubsystem* ANULoginGameMode::GetGoogleAuth() const
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNUGoogleAuthSubsystem>() : nullptr;
}

void ANULoginGameMode::HandleConnStateChanged(EEmberConnState NewState)
{
	switch (NewState)
	{
	case EEmberConnState::Connecting:
		SetStatus(FText::FromString(TEXT("서버 연결 중...")), true);
		break;

	case EEmberConnState::Authenticating:
		SetStatus(FText::FromString(TEXT("인증 중...")), true);
		break;

	case EEmberConnState::Creating:
		SetStatus(FText::FromString(TEXT("계정 생성 중...")), true);
		break;

	case EEmberConnState::LoggingIn:
		SetStatus(FText::FromString(TEXT("로그인 중...")), true);
		break;

	case EEmberConnState::LoggedIn:
		SetStatus(FText::FromString(TEXT("접속 완료")), true);
		UE_LOG(LogNULogin, Log, TEXT("로그인 성공 — 필드로 이동 [%s]"), *FieldLevel.ToString());

		// 필드 진입(EnterField)은 폰이 준비된 뒤 ANUPlayerController가 처리할 것
		if (UNUFieldLoadingSubsystem* Loading = GetGameInstance()->GetSubsystem<UNUFieldLoadingSubsystem>())
		{
			Loading->BeginLoading(FieldLevel.ToSoftObjectPath().GetAssetName());
		}
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, FieldLevel);
		break;

	case EEmberConnState::Failed:
		bLoginInProgress = false;
		SetStatus(FText::FromString(TEXT("접속 실패 — 다시 시도하세요")), false);
		break;

	default:
		break;
	}
}

void ANULoginGameMode::SetStatus(const FText& StatusText, bool bBusy)
{
	OnLoginStatusChanged.Broadcast(StatusText, bBusy);
}

UEmberServerManager* ANULoginGameMode::GetServerManager() const
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}
