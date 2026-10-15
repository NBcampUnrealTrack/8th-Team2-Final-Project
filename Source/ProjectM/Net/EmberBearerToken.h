// 세션 토큰(Bearer) 파생 — 서버 BearerTokenGenerator와 바이트 단위로 동일해야 한다.
// key=SHA256(passKey), iv=subPassKey(16B), plaintext="PADDING_1.{userId}.{timestamp}.PADDING_2. ",
// AES-256-CBC + PKCS7 → Base64(IV(16B) + 암호문)
#pragma once

#include "CoreMinimal.h"

class PROJECTM_API FEmberBearerToken
{
public:
	/** 인증 응답의 passKey/subPassKey + userId + 인증 timestamp로 세션 토큰을 파생한다. */
	static FString GenerateToken(const FString& PassKey, const FString& UserId, int64 Timestamp, const FString& SubPassKey);
};
