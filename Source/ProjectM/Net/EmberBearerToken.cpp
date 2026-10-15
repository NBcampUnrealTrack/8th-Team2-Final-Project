#include "Net/EmberBearerToken.h"
#include "Misc/Base64.h"

// OpenSSL의 'UI' 타입이 UE의 'namespace UI'와 충돌 → OpenSSL 헤더 범위에서만 개명 (UI 타입은 사용 안 함)
#define UI OPENSSL_UI
THIRD_PARTY_INCLUDES_START
#include "openssl/sha.h"
#include "openssl/evp.h"
THIRD_PARTY_INCLUDES_END
#undef UI

FString FEmberBearerToken::GenerateToken(const FString& PassKey, const FString& UserId, int64 Timestamp, const FString& SubPassKey)
{
	// 평문 (UTF-8) — 서버와 동일: "PADDING_1.{userId}.{timestamp}.PADDING_2. " (끝의 ". " 포함)
	// 주의: FTCHARToUTF8.Length()는 null 종료자를 포함하므로, 실제 바이트 길이는 Strlen으로 구한다.
	const FString Plain = FString::Printf(TEXT("PADDING_1.%s.%lld.PADDING_2. "), *UserId, Timestamp);
	FTCHARToUTF8 PlainUtf8(*Plain);
	const uint8* PlainBytes = reinterpret_cast<const uint8*>(PlainUtf8.Get());
	const int32 PlainLen = FCStringAnsi::Strlen(reinterpret_cast<const ANSICHAR*>(PlainUtf8.Get()));

	// key = SHA256(UTF8(passKey)) → 32바이트 (null 제외 길이로 해싱)
	FTCHARToUTF8 PassUtf8(*PassKey);
	const int32 PassLen = FCStringAnsi::Strlen(reinterpret_cast<const ANSICHAR*>(PassUtf8.Get()));
	uint8 Key[SHA256_DIGEST_LENGTH];
	SHA256(reinterpret_cast<const unsigned char*>(PassUtf8.Get()), PassLen, Key);

	// iv = UTF8(subPassKey) → 16바이트 (부족하면 0 패딩, 서버는 16자 IV 발급)
	FTCHARToUTF8 IvUtf8(*SubPassKey);
	const int32 IvLen = FCStringAnsi::Strlen(reinterpret_cast<const ANSICHAR*>(IvUtf8.Get()));
	uint8 IV[16] = { 0 };
	FMemory::Memcpy(IV, IvUtf8.Get(), FMath::Min(16, IvLen));

	// AES-256-CBC + PKCS7 암호화
	EVP_CIPHER_CTX* Ctx = EVP_CIPHER_CTX_new();
	if (Ctx == nullptr)
	{
		return FString();
	}

	TArray<uint8> Cipher;
	Cipher.SetNumUninitialized(PlainLen + 16); // 블록 여유
	int OutLen1 = 0, OutLen2 = 0;

	bool bOk = EVP_EncryptInit_ex(Ctx, EVP_aes_256_cbc(), nullptr, Key, IV) == 1
		&& EVP_EncryptUpdate(Ctx, Cipher.GetData(), &OutLen1, PlainBytes, PlainLen) == 1
		&& EVP_EncryptFinal_ex(Ctx, Cipher.GetData() + OutLen1, &OutLen2) == 1;

	EVP_CIPHER_CTX_free(Ctx);

	if (!bOk)
	{
		return FString();
	}

	const int32 CipherLen = OutLen1 + OutLen2;

	// result = Base64(IV(16B) + 암호문)
	TArray<uint8> Combined;
	Combined.Append(IV, 16);
	Combined.Append(Cipher.GetData(), CipherLen);
	return FBase64::Encode(Combined.GetData(), Combined.Num());
}
