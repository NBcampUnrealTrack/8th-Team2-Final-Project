// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "NUCharacterManager.generated.h"

class UEmberServerManager;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnNUCharacterListChanged);
// 실패 사유를 UI에 띄우기 위한 통지 (성공 시에는 목록 변경 델리게이트가 대신한다)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNUCharacterError, int32, ResultCode, const FString&, Message);
// 선택 확정 — 선택창 게임모드가 이걸 받고 필드로 이동한다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUCharacterSelected, int64, CharacterIdx);
// 생성 결과 — 성공이면 만들어진 Idx(0일 수 있음), 실패면 결과 코드.
// 생성창이 이걸 보고 창을 닫거나 이름 오류를 그 자리에 띄운다 (입력값을 날리지 않으려고)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNUCharacterCreateResult, bool, bSuccess, int32, ResultCode);
// 이름 확인(140) 결과 — 200/1202/1210/1211. 생성창 [중복확인]이 구독한다
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNUCharacterNameChecked, int32, ResultCode);
// 남의 캐릭터 정보(142) 결과 — 200이 아니면 프로필은 비어 있다 (1200 없음 / 1205 DB 오류).
// C++ UI 컨트롤러만 구독해서 동적 델리게이트가 아니다
DECLARE_MULTICAST_DELEGATE_TwoParams(FNUOnCharacterProfile, int32 /*ResultCode*/, const FNUCharacterProfile&);

// 내 캐릭터 목록·선택 상태를 들고 있는 서브시스템.
//
// GameInstance 수명이라 레벨 전환(선택창 => 필드)에도 선택 정보가 살아남는다.
// 서버에 로그인돼 있으면 캐릭터 패킷(14~16)으로 처리하고, 아니면 클라 더미로 동작한다
// (선택창 레벨만 단독 PIE로 띄워 UI를 확인할 수 있게 남겨둔 경로).
UCLASS()
class PROJECTM_API UNUCharacterManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "NU|Character")
	const TArray<FNUCharacterSummary>& GetCharacters() const { return Characters; }

	// 서버에 목록을 요청한다 (로그인 상태가 아니면 아무 것도 하지 않는다)
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void RequestCharacterList();

	// 생성창에서 고른 값으로 만든다. 서버로 가는 건 이름뿐이고
	// 종족·성별·외형은 저장할 필드가 아직 없어 여기서 들고만 있는다 (서버에 요청해 둔 상태)
	void CreateCharacter(const FNUCharacterCreateParams& Params);

	// 서버 없이 UI만 볼 때 쓰는 경로 (이름을 자동으로 지어 만든다)
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void CreateCharacterAuto();

	// 생성 요청에 실려 있던 값 --- 서버에 종족·성별 필드가 생기면 여기서 꺼내 보내면 된다
	const FNUCharacterCreateParams& GetLastCreateParams() const { return LastCreateParams; }

	// 방금 만든 캐릭터 Idx (없으면 0). 선택창이 목록 갱신 후 이 칸을 골라 둔다
	int64 ConsumePendingCreatedIdx();

	// --- 생성 창에서 고르는 중인 값 (초안) ---
	// GameInstance 수명이라 선택 <=> 생성 레벨을 오가도 남는다.
	// 커마 항목이 늘어나도 여기 한 곳만 커지면 된다
	const FNUCharacterCreateParams& GetDraftParams() const { return DraftParams; }
	void SetDraftParams(const FNUCharacterCreateParams& InParams) { DraftParams = InParams; }
	void ClearDraftParams() { DraftParams = FNUCharacterCreateParams(); }

	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void SelectCharacter(int64 CharacterIdx);

	/**
	 * 재접속 뒤 원래 있던 곳으로 되돌린다. 새 세션은 캐릭터가 안 골라진 상태라
	 * 골라 두었던 캐릭터를 다시 고르고 지금 레벨을 다시 연다.
	 * 레벨을 다시 여는 이유: 인벤·퀘스트 등이 로그인 신호에 요청을 날리는데,
	 * 캐릭터가 정해지기 전이라 전부 거절당한다 --- 처음부터 태우는 편이 확실하다.
	 * 고른 캐릭터가 없었으면(선택창에 있었다) 목록만 다시 받는다.
	 */
	void RestoreSessionAfterReconnect();

	// 24시간 뒤 삭제로 예약된다 (즉시 삭제 아님). 확인 창은 호출부(UI) 책임. 필드에서는 411로 거절
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void DeleteCharacter(int64 CharacterIdx);

	// 삭제 예약 취소 (141). 24시간이 지나기 전이면 되돌릴 수 있다
	UFUNCTION(BlueprintCallable, Category = "NU|Character")
	void CancelDeleteCharacter(int64 CharacterIdx);

	// 이름만 확인 (140) --- 캐릭터를 만들지 않는다. 결과는 OnCharacterNameChecked로
	void RequestNameCheck(const FString& InName);

	// 남의 캐릭터 정보 조회 (142) --- 내 캐릭터도 조회된다 (소유 검사 없음). 결과는 OnCharacterProfile로
	void RequestCharacterProfile(int64 CharacterIdx);

	// 선택된 캐릭터가 없으면 false (0 = 미선택)
	UFUNCTION(BlueprintPure, Category = "NU|Character")
	bool GetSelectedCharacter(FNUCharacterSummary& OutCharacter) const;

	// 고른 캐릭터, 없으면(서버 없이 맵을 바로 열어 시험 플레이) 기본 노비스 외형(종족 1, 머리·얼굴 1, 노비스, 레벨 1) ---
	// 필드·전투 캐릭터와 HUD 초상이 옛 단일 스프라이트 대신 지금 외형으로 그려지게 한다. 시험 성별은 nu.Char.TestGender
	bool GetSelectedOrTestCharacter(FNUCharacterSummary& OutCharacter) const;

	UFUNCTION(BlueprintPure, Category = "NU|Character")
	int64 GetSelectedCharacterIdx() const { return SelectedCharacterIdx; }

	UFUNCTION(BlueprintPure, Category = "NU|Character")
	int32 GetMaxCharacterCount() const { return MaxCharacterCount; }

	// --- 서버 응답 (UEmberServerManager가 호출) ---
	void HandleListResponse(int32 ResultCode, const FString& ResponseData);
	void HandleCreateResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData);
	void HandleSelectResponse(int32 ResultCode, const FString& ResponseData);
	// LocationRestore가 끝난 뒤에만 선택 완료 이벤트 또는 재접속 레벨 재진입을 실행한다.
	void HandleLocationRestoreCompleted(bool bSuccess, int32 ResultCode, const FString& ResultMessage);
	bool IsSelectionAwaitingLocationRestore() const { return bAwaitingLocationRestore; }
	void HandleDeleteResponse(int32 ResultCode, const FString& ResponseData);
	void HandleDeleteCancelResponse(int32 ResultCode, const FString& ResponseData);
	void HandleNameCheckResponse(int32 ResultCode);
	void HandleProfileResponse(int32 ResultCode, const FString& ResponseData);

	UPROPERTY(BlueprintAssignable, Category = "NU|Character")
	FOnNUCharacterListChanged OnCharacterListChanged;

	UPROPERTY(BlueprintAssignable, Category = "NU|Character")
	FOnNUCharacterError OnCharacterError;

	UPROPERTY(BlueprintAssignable, Category = "NU|Character")
	FOnNUCharacterSelected OnCharacterSelected;

	UPROPERTY(BlueprintAssignable, Category = "NU|Character")
	FOnNUCharacterCreateResult OnCharacterCreateResult;

	UPROPERTY(BlueprintAssignable, Category = "NU|Character")
	FOnNUCharacterNameChecked OnCharacterNameChecked;

	FNUOnCharacterProfile OnCharacterProfile;

private:
	UEmberServerManager* GetServer() const;

	// 서버 없이 UI만 확인할 때 쓰는 경로
	void CreateDummyCharacter(const FString& InName);

public:
	// [개발용] 서버 없이 선택창을 볼 때 목록에 한 줄을 넣는다 (콘솔 nu.Char.Dummy). 서버에 붙어 있으면 아무것도 하지 않는다
	void DebugAddDummyCharacter(const FNUCharacterSummary& InCharacter);

private:

	// 서버 JSON은 PascalCase다 (소문자로 파싱하면 전부 0/null — gRPC_help §4.4)
	bool ParseCharacterInfo(const TSharedPtr<class FJsonObject>& Obj, FNUCharacterSummary& Out) const;

	// 목록 JSON({"MaxCount","Characters"}) 적용 + 브로드캐스트 — 목록/삭제 응답이 같은 형식이라 공용.
	// "Characters" 배열이 없으면 false (호출부가 재요청 등으로 처리)
	bool ApplyListJson(const FString& ResponseData);

	UPROPERTY()
	TArray<FNUCharacterSummary> Characters;

	int64 SelectedCharacterIdx = 0;

	// 재접속 뒤 캐릭터 재선택 응답을 기다리는 중. 성공하면 지금 레벨을 다시 연다
	bool bRestoringAfterReconnect = false;
	bool bAwaitingLocationRestore = false;
	int32 MaxCharacterCount = 0;

	// 더미 Idx 발급기 — 서버 경로에서는 쓰지 않는다
	int64 NextDummyIdx = 1;

	// 마지막 생성 요청에 실려 있던 값 (종족·성별·외형 보관용)
	FNUCharacterCreateParams LastCreateParams;

	// 생성 창에서 고르는 중인 값. 창을 닫았다 열어도, 레벨을 오가도 남는다
	FNUCharacterCreateParams DraftParams;

	// 방금 만든 캐릭터의 Idx. 목록이 갱신되면 이 칸을 골라 준다 (기획서 5.1: 신규 캐릭터 자동 선택)
	int64 PendingCreatedIdx = 0;
};
