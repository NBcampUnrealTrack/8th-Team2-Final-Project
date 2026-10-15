#include "Auth/NUGoogleAuthSubsystem.h"

#include "Auth/NUGoogleAuthSettings.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HttpModule.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/Base64.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SWindow.h"

// OpenSSL의 'UI' 타입이 UE의 'namespace UI'와 충돌 → OpenSSL 헤더 범위에서만 개명 (EmberBearerToken.cpp 와 같다)
#define UI OPENSSL_UI
THIRD_PARTY_INCLUDES_START
#include "openssl/rand.h"
#include "openssl/sha.h"
THIRD_PARTY_INCLUDES_END
#undef UI

DEFINE_LOG_CATEGORY_STATIC(LogNUGoogle, Log, All);

namespace NUGoogle
{
	const TCHAR* AuthEndpoint = TEXT("https://accounts.google.com/o/oauth2/v2/auth");
	const TCHAR* TokenEndpoint = TEXT("https://oauth2.googleapis.com/token");

	// RFC 4648 base64url, 끝의 '=' 없음 (PKCE·JWT 공통)
	FString Base64Url(const uint8* Data, int32 Length)
	{
		FString Out = FBase64::Encode(Data, Length);
		Out.ReplaceInline(TEXT("+"), TEXT("-"));
		Out.ReplaceInline(TEXT("/"), TEXT("_"));
		Out.RemoveFromEnd(TEXT("=="));
		Out.RemoveFromEnd(TEXT("="));
		return Out;
	}

	bool DecodeBase64Url(FString In, TArray<uint8>& Out)
	{
		In.ReplaceInline(TEXT("-"), TEXT("+"));
		In.ReplaceInline(TEXT("_"), TEXT("/"));
		while (In.Len() % 4 != 0)
		{
			In.AppendChar(TEXT('='));
		}
		return FBase64::Decode(In, Out);
	}

	FString RandomUrlToken(int32 Bytes)
	{
		TArray<uint8> Buffer;
		Buffer.SetNumUninitialized(Bytes);
		if (RAND_bytes(Buffer.GetData(), Bytes) != 1)
		{
			// OpenSSL 난수가 실패하는 일은 없지만, 실패하면 GUID 두 개로 대신한다 (verifier 길이 규약 43~128자는 지킨다)
			return FGuid::NewGuid().ToString(EGuidFormats::Digits) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		}
		return Base64Url(Buffer.GetData(), Buffer.Num());
	}

	TArray<uint8> Sha256(const FString& Text)
	{
		// 길이는 Strlen 으로 (EmberBearerToken.cpp 주석: 변환 길이에 끝의 null 이 섞이는 경우가 있다) --- PKCE challenge 가 한 바이트라도 다르면 구글이 거절한다
		FTCHARToUTF8 Utf8(*Text);
		const int32 Length = FCStringAnsi::Strlen(reinterpret_cast<const ANSICHAR*>(Utf8.Get()));
		TArray<uint8> Digest;
		Digest.SetNumUninitialized(SHA256_DIGEST_LENGTH);
		SHA256(reinterpret_cast<const unsigned char*>(Utf8.Get()), Length, Digest.GetData());
		return Digest;
	}

	FString FormEncode(const TArray<TPair<FString, FString>>& Fields)
	{
		TArray<FString> Parts;
		for (const TPair<FString, FString>& Field : Fields)
		{
			Parts.Add(FGenericPlatformHttp::UrlEncode(Field.Key) + TEXT("=") + FGenericPlatformHttp::UrlEncode(Field.Value));
		}
		return FString::Join(Parts, TEXT("&"));
	}

	// 브라우저에 보여 줄 마지막 페이지 (게임 HUD 와 같은 남색·금색)
	FString ResultPage(bool bSuccess, const FString& Message)
	{
		return FString::Printf(TEXT(
			"<!doctype html><html lang=\"ko\"><head><meta charset=\"utf-8\"><title>Nia Unbound</title>"
			"<style>body{margin:0;height:100vh;display:grid;place-items:center;background:#0e1a28;color:#f1ede6;"
			"font-family:'Malgun Gothic','Apple SD Gothic Neo',sans-serif}"
			".card{padding:40px 56px;border-radius:20px;background:#16293d;box-shadow:inset 0 0 0 1px rgba(214,190,140,.45);text-align:center}"
			"h1{margin:0 0 12px;font-size:24px;color:%s}p{margin:0;color:#aab6bf;font-size:16px}</style></head>"
			"<body><div class=\"card\"><h1>%s</h1><p>%s</p></div></body></html>"),
			bSuccess ? TEXT("#f0d79d") : TEXT("#e3705d"),
			bSuccess ? TEXT("로그인되었습니다") : TEXT("로그인하지 못했습니다"),
			*Message);
	}
}

bool UNUGoogleAuthSubsystem::IsConfigured()
{
	return !GetDefault<UNUGoogleAuthSettings>()->ClientId.IsEmpty();
}

void UNUGoogleAuthSubsystem::Deinitialize()
{
	if (bSigningIn)
	{
		Cancel(FText::GetEmpty());
	}
	CloseCallbackListener();
	Super::Deinitialize();
}

bool UNUGoogleAuthSubsystem::SignIn()
{
	if (bSigningIn)
	{
		return false;
	}

	const UNUGoogleAuthSettings* Settings = GetDefault<UNUGoogleAuthSettings>();
	if (Settings->ClientId.IsEmpty())
	{
		Finish(false, NSLOCTEXT("NUGoogle", "NotConfigured", "구글 로그인이 설정되지 않았습니다 (프로젝트 세팅 > NU Google Login > Client Id)"));
		return false;
	}

	if (!OpenCallbackListener())
	{
		Finish(false, NSLOCTEXT("NUGoogle", "NoPort", "로그인 응답을 받을 포트를 열지 못했습니다"));
		return false;
	}

	// PKCE: verifier 는 43~128자, challenge = base64url(SHA-256(verifier))
	CodeVerifier = NUGoogle::RandomUrlToken(48);
	const TArray<uint8> Challenge = NUGoogle::Sha256(CodeVerifier);
	State = NUGoogle::RandomUrlToken(24);
	Nonce = NUGoogle::RandomUrlToken(24);

	const FString Url = FString::Printf(TEXT("%s?%s"), NUGoogle::AuthEndpoint, *NUGoogle::FormEncode({
		{TEXT("client_id"), Settings->ClientId},
		{TEXT("redirect_uri"), RedirectUri},
		{TEXT("response_type"), TEXT("code")},
		{TEXT("scope"), Settings->Scopes},
		{TEXT("code_challenge"), NUGoogle::Base64Url(Challenge.GetData(), Challenge.Num())},
		{TEXT("code_challenge_method"), TEXT("S256")},
		{TEXT("state"), State},
		{TEXT("nonce"), Nonce},
		{TEXT("access_type"), TEXT("offline")},          // 재접속 때 ID 토큰을 새로 받을 refresh token
		{TEXT("prompt"), TEXT("select_account")},
	}));

	FString LaunchError;
	FPlatformProcess::LaunchURL(*Url, nullptr, &LaunchError);
	if (!LaunchError.IsEmpty())
	{
		CloseCallbackListener();
		Finish(false, FText::Format(NSLOCTEXT("NUGoogle", "NoBrowser", "브라우저를 열지 못했습니다: {0}"), FText::FromString(LaunchError)));
		return false;
	}

	bSigningIn = true;
	TimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::HandleTimeout), Settings->TimeoutSeconds);
	UE_LOG(LogNUGoogle, Log, TEXT("구글 로그인 시작 (콜백 %s)"), *RedirectUri);
	OnSignInProgress.Broadcast(NSLOCTEXT("NUGoogle", "WaitingBrowser", "브라우저에서 구글 계정을 선택하세요"));
	return true;
}

bool UNUGoogleAuthSubsystem::OpenCallbackListener()
{
	CloseCallbackListener();
	FHttpServerModule& HttpServer = FHttpServerModule::Get();
	for (const int32 Port : GetDefault<UNUGoogleAuthSettings>()->CallbackPorts)
	{
		if (Port <= 0 || Port > 65535)
		{
			continue;
		}
		// bFailOnBindFailure: 다른 프로그램이 쓰는 포트면 null --- 다음 후보로
		TSharedPtr<IHttpRouter> Candidate = HttpServer.GetHttpRouter(static_cast<uint32>(Port), true);
		if (!Candidate.IsValid())
		{
			continue;
		}
		FHttpRouteHandle Handle = Candidate->BindRoute(FHttpPath(TEXT("/")), EHttpServerRequestVerbs::VERB_GET,
			FHttpRequestHandler::CreateUObject(this, &ThisClass::HandleCallback));
		if (!Handle.IsValid())
		{
			continue; // 이 포트의 "/" 를 다른 기능이 이미 쓰고 있다
		}
		Router = Candidate;
		RouteHandle = Handle;
		RedirectUri = FString::Printf(TEXT("http://127.0.0.1:%d"), Port);
		// 리스너 주소는 엔진 기본값 localhost (HTTPServer.Listeners DefaultBindAddress) --- 바깥에서는 접근할 수 없다
		HttpServer.StartAllListeners();
		return true;
	}
	return false;
}

void UNUGoogleAuthSubsystem::CloseCallbackListener()
{
	// 리스너 자체는 엔진 모듈이 들고 있어 포트 단위로 닫을 수 없다 --- 경로만 풀어 더는 응답하지 않게 한다
	if (Router.IsValid() && RouteHandle.IsValid())
	{
		Router->UnbindRoute(RouteHandle);
	}
	Router.Reset();
	RouteHandle.Reset();
}

bool UNUGoogleAuthSubsystem::HandleCallback(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const auto Respond = [&OnComplete](bool bSuccess, const FString& Message)
	{
		TUniquePtr<FHttpServerResponse> Response = FHttpServerResponse::Create(NUGoogle::ResultPage(bSuccess, Message), TEXT("text/html; charset=utf-8"));
		OnComplete(MoveTemp(Response));
	};

	if (!bSigningIn)
	{
		Respond(false, TEXT("이미 끝난 로그인 요청입니다. 게임에서 다시 시도하세요."));
		return true;
	}

	const FString* ReturnedState = Request.QueryParams.Find(TEXT("state"));
	if (ReturnedState == nullptr || *ReturnedState != State)
	{
		// 다른 페이지가 이 포트를 두드린 것 --- 무시하고 진짜 응답을 계속 기다린다
		Respond(false, TEXT("잘못된 요청입니다."));
		return true;
	}

	if (const FString* Error = Request.QueryParams.Find(TEXT("error")))
	{
		Respond(false, TEXT("게임으로 돌아가 다시 시도하세요."));
		CloseCallbackListener();
		Finish(false, *Error == TEXT("access_denied")
			? NSLOCTEXT("NUGoogle", "Denied", "구글 로그인을 취소했습니다")
			: FText::Format(NSLOCTEXT("NUGoogle", "AuthError", "구글 로그인 오류: {0}"), FText::FromString(*Error)));
		return true;
	}

	const FString* Code = Request.QueryParams.Find(TEXT("code"));
	if (Code == nullptr || Code->IsEmpty())
	{
		Respond(false, TEXT("게임으로 돌아가 다시 시도하세요."));
		return true;
	}

	Respond(true, TEXT("이 창을 닫고 게임으로 돌아가세요."));
	CloseCallbackListener();
	BringGameWindowToFront();
	OnSignInProgress.Broadcast(NSLOCTEXT("NUGoogle", "Exchanging", "구글 계정 확인 중..."));
	ExchangeCode(*Code);
	return true;
}

void UNUGoogleAuthSubsystem::ExchangeCode(const FString& Code)
{
	const UNUGoogleAuthSettings* Settings = GetDefault<UNUGoogleAuthSettings>();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(NUGoogle::TokenEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Request->SetContentAsString(NUGoogle::FormEncode({
		{TEXT("code"), Code},
		{TEXT("client_id"), Settings->ClientId},
		{TEXT("client_secret"), Settings->ClientSecret},
		{TEXT("redirect_uri"), RedirectUri},
		{TEXT("grant_type"), TEXT("authorization_code")},
		{TEXT("code_verifier"), CodeVerifier},
	}));
	Request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		if (!bSigningIn)
		{
			return; // 그 사이 취소됐다
		}
		if (!bConnected || !Response.IsValid())
		{
			Finish(false, NSLOCTEXT("NUGoogle", "TokenNetwork", "구글 서버에 연결하지 못했습니다"));
			return;
		}
		FText Error;
		if (Response->GetResponseCode() != 200 || !ApplyTokenResponse(Response->GetContentAsString(), Error))
		{
			UE_LOG(LogNUGoogle, Warning, TEXT("토큰 교환 실패 %d: %s"), Response->GetResponseCode(), *Response->GetContentAsString().Left(300));
			Finish(false, Error.IsEmpty() ? NSLOCTEXT("NUGoogle", "TokenFailed", "구글 계정 확인에 실패했습니다") : Error);
			return;
		}
		UE_LOG(LogNUGoogle, Log, TEXT("구글 로그인 완료 %s"), *Account.Email);
		Finish(true, FText::Format(NSLOCTEXT("NUGoogle", "SignedIn", "{0} 계정으로 로그인했습니다"), FText::FromString(Account.Email)));
	});
	Request->ProcessRequest();
}

bool UNUGoogleAuthSubsystem::ApplyTokenResponse(const FString& Json, FText& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		return false;
	}

	FString IdToken;
	if (!Root->TryGetStringField(TEXT("id_token"), IdToken))
	{
		return false;
	}

	// JWT = header.payload.signature --- 내용만 읽는다. 서명 검증은 서버가 구글 공개키로 한다
	TArray<FString> Parts;
	IdToken.ParseIntoArray(Parts, TEXT("."));
	TArray<uint8> PayloadBytes;
	if (Parts.Num() != 3 || !NUGoogle::DecodeBase64Url(Parts[1], PayloadBytes))
	{
		return false;
	}
	FUTF8ToTCHAR PayloadText(reinterpret_cast<const ANSICHAR*>(PayloadBytes.GetData()), PayloadBytes.Num());
	TSharedPtr<FJsonObject> Claims;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(FString(PayloadText.Length(), PayloadText.Get())), Claims) || !Claims.IsValid())
	{
		return false;
	}

	// 다른 앱용 토큰·오래된 토큰·바꿔치기를 걸러낸다 (서버 검증과 별개의 클라 확인)
	const FString Iss = Claims->GetStringField(TEXT("iss"));
	const FString Aud = Claims->GetStringField(TEXT("aud"));
	const int64 Exp = static_cast<int64>(Claims->GetNumberField(TEXT("exp")));
	FString TokenNonce;
	Claims->TryGetStringField(TEXT("nonce"), TokenNonce);
	if ((Iss != TEXT("https://accounts.google.com") && Iss != TEXT("accounts.google.com"))
		|| Aud != GetDefault<UNUGoogleAuthSettings>()->ClientId
		|| Exp <= FDateTime::UtcNow().ToUnixTimestamp()
		|| (!Nonce.IsEmpty() && TokenNonce != Nonce))
	{
		OutError = NSLOCTEXT("NUGoogle", "BadToken", "구글 응답을 확인하지 못했습니다 (토큰 불일치)");
		return false;
	}

	FNUGoogleAccount NewAccount;
	NewAccount.Sub = Claims->GetStringField(TEXT("sub"));
	Claims->TryGetStringField(TEXT("email"), NewAccount.Email);
	Claims->TryGetStringField(TEXT("name"), NewAccount.Name);
	NewAccount.IdToken = IdToken;
	NewAccount.IdTokenExpiry = FDateTime::FromUnixTimestamp(Exp);
	// refresh token 은 처음 동의할 때만 온다 --- 재로그인으로 안 왔으면 이전 것을 유지한다
	if (!Root->TryGetStringField(TEXT("refresh_token"), NewAccount.RefreshToken) && NewAccount.Sub == Account.Sub)
	{
		NewAccount.RefreshToken = Account.RefreshToken;
	}
	if (NewAccount.Sub.IsEmpty())
	{
		return false;
	}
	Account = MoveTemp(NewAccount);
	return true;
}

void UNUGoogleAuthSubsystem::EnsureFreshIdToken(TFunction<void(bool)> Done)
{
	if (!Account.IsValid())
	{
		Done(false);
		return;
	}
	if (Account.IdTokenExpiry - FDateTime::UtcNow() > FTimespan::FromMinutes(5.0))
	{
		Done(true);
		return;
	}
	if (Account.RefreshToken.IsEmpty())
	{
		Done(false);
		return;
	}

	const UNUGoogleAuthSettings* Settings = GetDefault<UNUGoogleAuthSettings>();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(NUGoogle::TokenEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	Request->SetContentAsString(NUGoogle::FormEncode({
		{TEXT("client_id"), Settings->ClientId},
		{TEXT("client_secret"), Settings->ClientSecret},
		{TEXT("refresh_token"), Account.RefreshToken},
		{TEXT("grant_type"), TEXT("refresh_token")},
	}));
	Request->OnProcessRequestComplete().BindWeakLambda(this, [this, Done = MoveTemp(Done)](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		FText Error;
		const FString Previous = Nonce;
		Nonce.Empty(); // 갱신된 ID 토큰에는 nonce 가 없다
		const bool bOk = bConnected && Response.IsValid() && Response->GetResponseCode() == 200
			&& ApplyTokenResponse(Response->GetContentAsString(), Error);
		Nonce = Previous;
		UE_LOG(LogNUGoogle, Log, TEXT("ID 토큰 갱신 %s"), bOk ? TEXT("성공") : TEXT("실패"));
		Done(bOk);
	});
	Request->ProcessRequest();
}

FString UNUGoogleAuthSubsystem::MakeGameUserId() const
{
	if (Account.Sub.IsEmpty())
	{
		return FString();
	}
	// 토큰 검증 서버(10/14 반영): 계정 키가 "g_" + sub 로 정해져 있다 (예 g_110123456789012345678, 23자, 서버 상한 64)
	if (GetDefault<UNUGoogleAuthSettings>()->bServerVerifiesIdToken)
	{
		return TEXT("g_") + Account.Sub;
	}
	// 임시 방식 (서버가 토큰을 안 볼 때): 옛 규약 20자에 맞춰 sub 해시를 줄여 쓴다
	const FString Prefix = GetDefault<UNUGoogleAuthSettings>()->UserIdPrefix;
	const int32 HashChars = FMath::Clamp(20 - Prefix.Len(), 8, 20);
	const TArray<uint8> Digest = NUGoogle::Sha256(TEXT("google:") + Account.Sub);
	return (Prefix + BytesToHex(Digest.GetData(), Digest.Num()).ToLower()).Left(Prefix.Len() + HashChars);
}

void UNUGoogleAuthSubsystem::Cancel(const FText& Reason)
{
	if (!bSigningIn)
	{
		return;
	}
	CloseCallbackListener();
	Finish(false, Reason.IsEmpty() ? NSLOCTEXT("NUGoogle", "Cancelled", "구글 로그인을 취소했습니다") : Reason);
}

void UNUGoogleAuthSubsystem::SignOut()
{
	Account = FNUGoogleAccount();
}

bool UNUGoogleAuthSubsystem::HandleTimeout(float DeltaTime)
{
	TimeoutHandle.Reset();
	Cancel(NSLOCTEXT("NUGoogle", "Timeout", "구글 로그인 시간이 지났습니다. 다시 시도하세요"));
	return false; // 한 번만
}

void UNUGoogleAuthSubsystem::Finish(bool bSuccess, const FText& Message)
{
	bSigningIn = false;
	CodeVerifier.Empty();
	State.Empty();
	if (TimeoutHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TimeoutHandle);
		TimeoutHandle.Reset();
	}
	if (!bSuccess)
	{
		UE_LOG(LogNUGoogle, Warning, TEXT("구글 로그인 실패: %s"), *Message.ToString());
	}
	OnSignInFinished.Broadcast(bSuccess, Message);
}

void UNUGoogleAuthSubsystem::BringGameWindowToFront()
{
	// 브라우저에서 계정을 고른 뒤 게임 창이 뒤에 남지 않게
	if (GEngine != nullptr && GEngine->GameViewport != nullptr)
	{
		if (TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow())
		{
			Window->BringToFront(true);
			Window->HACK_ForceToFront();
		}
	}
}

#if !UE_BUILD_SHIPPING
namespace NUGoogleDebug
{
	UNUGoogleAuthSubsystem* Find(UWorld* World)
	{
		UGameInstance* GameInstance = World != nullptr ? World->GetGameInstance() : nullptr;
		return GameInstance != nullptr ? GameInstance->GetSubsystem<UNUGoogleAuthSubsystem>() : nullptr;
	}

	static FAutoConsoleCommandWithWorld SignInCommand(TEXT("nu.Google.SignIn"), TEXT("구글 로그인만 해 본다 (서버 접속 없음)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UNUGoogleAuthSubsystem* Google = Find(World))
			{
				Google->SignIn();
			}
		}));

	static FAutoConsoleCommandWithWorld CancelCommand(TEXT("nu.Google.Cancel"), TEXT("진행 중인 구글 로그인 취소"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UNUGoogleAuthSubsystem* Google = Find(World))
			{
				Google->Cancel(FText::GetEmpty());
			}
		}));

	static FAutoConsoleCommandWithWorld StatusCommand(TEXT("nu.Google.Status"), TEXT("구글 로그인 상태와 게임 user_id 출력"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UNUGoogleAuthSubsystem* Google = Find(World))
			{
				const FNUGoogleAccount& Account = Google->GetAccount();
				UE_LOG(LogNUGoogle, Log, TEXT("[nu.Google.Status] configured=%d signingIn=%d signedIn=%d email=%s userId=%s tokenExpiry=%s refresh=%d"),
					UNUGoogleAuthSubsystem::IsConfigured(), Google->IsSigningIn(), Google->IsSignedIn(), *Account.Email,
					*Google->MakeGameUserId(), *Account.IdTokenExpiry.ToString(), !Account.RefreshToken.IsEmpty());
			}
		}));
}
#endif
