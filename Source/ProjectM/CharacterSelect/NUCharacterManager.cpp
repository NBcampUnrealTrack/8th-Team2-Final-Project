// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/NUCharacterManager.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Net/EmberServerManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "WorldTransition/World/NUFieldLoadingSubsystem.h"

DEFINE_LOG_CATEGORY(LogNUCharSelect);

namespace
{
	constexpr int32 RC_SUCCESS = 200;
	// 직업 기획 미확정 — 생성 시 보내는 더미 (서버는 저장·에코만 한다)
	// 전직은 10레벨이다 --- 생성 직후에는 직업이 없다.
	// 직업 id는 DT_JobDefinition이 정본이고 1001~1005를 쓴다 (0 = 미전직)
	constexpr int32 UNASSIGNED_JOB_ID = 0;
}

void UNUCharacterManager::RequestCharacterList()
{
	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		return; // 서버 없이 UI만 보는 경우 — 더미 목록을 그대로 둔다
	}
	Server->SendCharacterList();
}

void UNUCharacterManager::CreateCharacter(const FNUCharacterCreateParams& Params)
{
	// 종족·성별·외형은 보낼 곳이 없어 여기서 보관만 한다 --- 필드가 생기면 아래 송신에 얹으면 된다
	LastCreateParams = Params;
	PendingCreatedIdx = 0;

	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 생성 요청 [%s] 종족=%s 성별=%s 머리=%d 외모=%d 피부=%d"),
		*Params.Name,
		*Params.RaceRowName.ToString(),
		Params.Gender == ENUCharacterGender::Female ? TEXT("여성") : TEXT("남성"),
		Params.HairId, Params.FaceId, Params.SkinId);

	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		CreateDummyCharacter(Params.Name);
		return;
	}

	// 외형 4종은 서버가 저장·에코만 한다. 피부색은 아직 서버 필드가 없어 클라 보관만 (요청 예정)
	Server->SendCharacterCreate(Params.Name, UNASSIGNED_JOB_ID,
		Params.RaceId, static_cast<int32>(Params.Gender), Params.HairId, Params.FaceId);
}

void UNUCharacterManager::CreateCharacterAuto()
{
	// 서버 규칙은 2~14자 글자/숫자 (gRPC 가이드 §7)
	FNUCharacterCreateParams Params;
	Params.Name = FString::Printf(TEXT("Naia%lld"), FDateTime::UtcNow().ToUnixTimestamp() % 100000);
	CreateCharacter(Params);
}

void UNUCharacterManager::RestoreSessionAfterReconnect()
{
	if (SelectedCharacterIdx == 0)
	{
		RequestCharacterList(); // 선택창에 있었다 --- 목록만 다시 받으면 된다
		return;
	}

	UEmberServerManager* Server = GetServer();
	if (Server == nullptr)
	{
		return;
	}

	UE_LOG(LogNUCharSelect, Log, TEXT("재접속 복귀 --- 캐릭터 다시 선택 [Idx=%lld]"), SelectedCharacterIdx);
	Server->SendCharacterSelect(SelectedCharacterIdx);

	// 선택 응답을 기다렸다 레벨을 다시 연다. 지금 레벨을 그대로 열어서 있던 곳으로 돌아간다
	bRestoringAfterReconnect = true;
}

void UNUCharacterManager::SelectCharacter(int64 CharacterIdx)
{
	const FNUCharacterSummary* Found = Characters.FindByPredicate(
		[CharacterIdx](const FNUCharacterSummary& C) { return C.CharacterIdx == CharacterIdx; });
	if (Found == nullptr)
	{
		UE_LOG(LogNUCharSelect, Warning, TEXT("캐릭터 선택 실패 — 없는 Idx %lld"), CharacterIdx);
		return;
	}

	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		// 더미 경로: 서버 확인 없이 바로 확정
		SelectedCharacterIdx = CharacterIdx;
		UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 선택(더미) [%s] Idx=%lld"), *Found->Name, CharacterIdx);
		OnCharacterSelected.Broadcast(CharacterIdx);
		return;
	}

	Server->SendCharacterSelect(CharacterIdx); // 확정은 응답을 받고
}

void UNUCharacterManager::DeleteCharacter(int64 CharacterIdx)
{
	const FNUCharacterSummary* Found = Characters.FindByPredicate(
		[CharacterIdx](const FNUCharacterSummary& C) { return C.CharacterIdx == CharacterIdx; });
	if (Found == nullptr)
	{
		UE_LOG(LogNUCharSelect, Warning, TEXT("캐릭터 삭제 실패 — 없는 Idx %lld"), CharacterIdx);
		return;
	}

	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		// 더미 경로: 즉시 제거
		Characters.RemoveAll([CharacterIdx](const FNUCharacterSummary& C) { return C.CharacterIdx == CharacterIdx; });
		if (SelectedCharacterIdx == CharacterIdx)
		{
			SelectedCharacterIdx = 0;
		}
		UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 삭제(더미) Idx=%lld"), CharacterIdx);
		OnCharacterListChanged.Broadcast();
		return;
	}

	Server->SendCharacterDelete(CharacterIdx); // 성공 응답에 남은 목록이 실려 온다
}

static TAutoConsoleVariable<int32> CVarNUTestGender(
	TEXT("nu.Char.TestGender"), 0,
	TEXT("Gender of the default novice used when no character is selected (test play straight from a map): 0 = male, 1 = female."));

bool UNUCharacterManager::GetSelectedOrTestCharacter(FNUCharacterSummary& OutCharacter) const
{
	if (GetSelectedCharacter(OutCharacter))
	{
		return true;
	}
	OutCharacter = FNUCharacterSummary();
	OutCharacter.RaceId = 1;
	OutCharacter.Gender = FMath::Clamp(CVarNUTestGender.GetValueOnGameThread(), 0, 1);
	OutCharacter.HairId = 1;
	OutCharacter.FaceId = 1;
	OutCharacter.JobId = 0;
	OutCharacter.Level = 1;
	return true;
}

bool UNUCharacterManager::GetSelectedCharacter(FNUCharacterSummary& OutCharacter) const
{
	const FNUCharacterSummary* Found = Characters.FindByPredicate(
		[this](const FNUCharacterSummary& C) { return C.CharacterIdx == SelectedCharacterIdx; });
	if (Found == nullptr)
	{
		return false;
	}
	OutCharacter = *Found;
	return true;
}

/////////////////////////////////////////////////////////////////
// 서버 응답
/////////////////////////////////////////////////////////////////

void UNUCharacterManager::HandleListResponse(int32 ResultCode, const FString& ResponseData)
{
	if (ResultCode != RC_SUCCESS)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("캐릭터 목록 실패 (%d)"), ResultCode);
		OnCharacterError.Broadcast(ResultCode, TEXT("캐릭터 목록을 불러오지 못했습니다"));
		return;
	}

	if (!ApplyListJson(ResponseData))
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("캐릭터 목록 JSON 파싱 실패: %s"), *ResponseData);
	}
}

void UNUCharacterManager::HandleDeleteResponse(int32 ResultCode, const FString& ResponseData)
{
	if (ResultCode != RC_SUCCESS)
	{
		// 1200=없는 캐릭터/내 소유 아님(이미 지운 idx 포함), 411=필드에서는 삭제 불가, 1205=DB 오류
		UE_LOG(LogNUCharSelect, Error, TEXT("캐릭터 삭제 실패 (%d)"), ResultCode);
		OnCharacterError.Broadcast(ResultCode, TEXT("캐릭터를 삭제하지 못했습니다"));
		return;
	}

	// 성공 응답에 남은 목록이 실려 온다 (CharacterList와 같은 형식 — 재요청 불필요).
	// 드물게 response_data가 {}여도 삭제는 성공 — 그때만 목록을 다시 받는다 (gRPC_help §CharacterDelete)
	if (!ApplyListJson(ResponseData))
	{
		RequestCharacterList();
	}
}

bool UNUCharacterManager::ApplyListJson(const FString& ResponseData)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* JsonCharacters = nullptr;
	if (!Root->TryGetArrayField(TEXT("Characters"), JsonCharacters) || JsonCharacters == nullptr)
	{
		return false;
	}

	Characters.Reset();
	Root->TryGetNumberField(TEXT("MaxCount"), MaxCharacterCount);
	for (const TSharedPtr<FJsonValue>& Value : *JsonCharacters)
	{
		FNUCharacterSummary Summary;
		if (Value.IsValid() && ParseCharacterInfo(Value->AsObject(), Summary))
		{
			Characters.Add(Summary);
		}
	}

	// 방금 만든 캐릭터를 골라 둔다 (기획서 5.1: 선택 화면 복귀 시 신규 캐릭터 자동 선택).
	// 서버 선택 패킷을 보내는 것이 아니라 선택창의 강조만 옮기는 것이라 여기서 끝난다
	if (PendingCreatedIdx != 0)
	{
		const bool bFound = Characters.ContainsByPredicate(
			[this](const FNUCharacterSummary& C) { return C.CharacterIdx == PendingCreatedIdx; });
		if (bFound)
		{
			UE_LOG(LogNUCharSelect, Log, TEXT("신규 캐릭터 자동 선택 [Idx=%lld]"), PendingCreatedIdx);
		}
		else
		{
			PendingCreatedIdx = 0;
		}
	}

	// 선택해뒀던 캐릭터가 목록에서 사라졌으면(삭제 등) 선택 해제
	if (SelectedCharacterIdx != 0)
	{
		FNUCharacterSummary Unused;
		if (!GetSelectedCharacter(Unused))
		{
			SelectedCharacterIdx = 0;
		}
	}

	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 목록 %d개 (최대 %d)"), Characters.Num(), MaxCharacterCount);
	OnCharacterListChanged.Broadcast();
	return true;
}

void UNUCharacterManager::HandleCreateResponse(int32 ResultCode, const FString& ResultMessage, const FString& ResponseData)
{
	if (ResultCode != RC_SUCCESS)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("캐릭터 생성 실패 (%d) %s"), ResultCode, *ResultMessage);

		// 이름 문제(1201·1202)는 생성창이 그 자리에 띄운다 --- 창을 닫으면 입력한 것이 날아간다
		OnCharacterCreateResult.Broadcast(false, ResultCode);
		OnCharacterError.Broadcast(ResultCode, ResultMessage.IsEmpty()
			? TEXT("캐릭터를 만들지 못했습니다") : ResultMessage);
		return;
	}

	// 생성 응답에도 캐릭터 정보가 오지만, 목록을 다시 받아 서버를 진실의 원천으로 둔다.
	// 새로 만든 Idx는 여기서만 알 수 있으므로 기억해뒀다가 목록이 오면 그 칸을 골라 준다
	PendingCreatedIdx = 0;
	if (!ResponseData.IsEmpty())
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			FNUCharacterSummary Created;
			if (ParseCharacterInfo(Root, Created))
			{
				PendingCreatedIdx = Created.CharacterIdx;
			}
		}
	}

	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 생성 성공 [Idx=%lld] — 목록 재요청"), PendingCreatedIdx);
	ClearDraftParams(); // 다음에 생성 창을 열면 빈 상태로 시작한다
	UNUSFXSubsystem::Play(GetGameInstance(), NU::SFX::Char::Create);
	OnCharacterCreateResult.Broadcast(true, ResultCode);
	RequestCharacterList();
}

void UNUCharacterManager::HandleSelectResponse(int32 ResultCode, const FString& ResponseData)
{
	bAwaitingLocationRestore = false;

	if (ResultCode != RC_SUCCESS)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("캐릭터 선택 실패 (%d)"), ResultCode);
		// 재접속 복귀 중이었다면 있던 곳으로 못 돌아간다 --- 선택창에서 다시 고르게 한다
		if (bRestoringAfterReconnect)
		{
			bRestoringAfterReconnect = false;
			SelectedCharacterIdx = 0;
			RequestCharacterList();
			return;
		}
		OnCharacterError.Broadcast(ResultCode, TEXT("캐릭터를 선택하지 못했습니다"));
		return;
	}

	if (!bRestoringAfterReconnect)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
		FNUCharacterSummary Selected;

		if (FJsonSerializer::Deserialize(Reader, Root) && ParseCharacterInfo(Root, Selected))
		{
			SelectedCharacterIdx = Selected.CharacterIdx;
		}
	}

	if (SelectedCharacterIdx == 0)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("선택 응답에서 캐릭터 Idx를 읽지 못했습니다: %s"), *ResponseData);
		OnCharacterError.Broadcast(ResultCode, TEXT("캐릭터 정보를 읽지 못했습니다"));
		return;
	}

	// 필드 레벨을 여는 기존 선택 완료 이벤트는 LocationRestore가 확정될 때까지 보류한다.
	bAwaitingLocationRestore = true;
	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 선택 확정 Idx=%lld — 위치 복원 대기"), SelectedCharacterIdx);
}

void UNUCharacterManager::HandleLocationRestoreCompleted(bool bSuccess, int32 ResultCode, const FString& ResultMessage)
{
	if (!bAwaitingLocationRestore)
	{
		return;
	}

	bAwaitingLocationRestore = false;

	if (!bSuccess)
	{
		UE_LOG(LogNUCharSelect, Error, TEXT("캐릭터 위치 복원 실패 (%d): %s"), ResultCode, *ResultMessage);

		if (bRestoringAfterReconnect)
		{
			bRestoringAfterReconnect = false;
			SelectedCharacterIdx = 0;
			RequestCharacterList();
			return;
		}

		OnCharacterError.Broadcast(ResultCode, ResultMessage.IsEmpty()
			? TEXT("캐릭터 위치를 복원하지 못했습니다") : ResultMessage);
		return;
	}

	// 재접속은 현재 월드를 다시 열어 모든 월드 서브시스템이 복원 스냅샷을 기준으로 시작하게 한다.
	if (bRestoringAfterReconnect)
	{
		bRestoringAfterReconnect = false;

		if (const UWorld* World = GetWorld())
		{
			UE_LOG(LogNUCharSelect, Log, TEXT("재접속 위치 복원 완료 — 레벨 재진입 [%s]"), *World->GetName());
			if (UNUFieldLoadingSubsystem* Loading = GetGameInstance()->GetSubsystem<UNUFieldLoadingSubsystem>())
			{
				Loading->BeginLoading(UGameplayStatics::GetCurrentLevelName(World, true));
			}
			UGameplayStatics::OpenLevel(this, FName(*World->GetName()));
		}

		return;
	}

	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 위치 복원 완료 Idx=%lld"), SelectedCharacterIdx);
	OnCharacterSelected.Broadcast(SelectedCharacterIdx);
}

/////////////////////////////////////////////////////////////////
// 내부
/////////////////////////////////////////////////////////////////

bool UNUCharacterManager::ParseCharacterInfo(const TSharedPtr<FJsonObject>& Obj, FNUCharacterSummary& Out) const
{
	if (!Obj.IsValid())
	{
		return false;
	}

	// PascalCase — 서버 JSON 규약 (gRPC_help §4.4)
	double IdxValue = 0.0;
	if (!Obj->TryGetNumberField(TEXT("Idx"), IdxValue) || IdxValue <= 0.0)
	{
		return false;
	}
	Out.CharacterIdx = static_cast<int64>(IdxValue);
	Obj->TryGetStringField(TEXT("Name"), Out.Name);
	Obj->TryGetNumberField(TEXT("JobId"), Out.JobId);
	Obj->TryGetNumberField(TEXT("Level"), Out.Level);
	if (Out.Level <= 0)
	{
		Out.Level = 1;
	}

	double GoldValue = 0.0;
	Obj->TryGetNumberField(TEXT("Gold"), GoldValue);
	Out.Gold = static_cast<int64>(GoldValue);

	// 외형 --- 생성 때 클라가 보낸 값이 그대로 돌아온다
	Obj->TryGetNumberField(TEXT("RaceId"), Out.RaceId);
	Obj->TryGetNumberField(TEXT("Gender"), Out.Gender);
	Obj->TryGetNumberField(TEXT("HairId"), Out.HairId);
	Obj->TryGetNumberField(TEXT("FaceId"), Out.FaceId);

	// 삭제 예약 --- 예약 중에도 목록에 계속 나온다
	Obj->TryGetBoolField(TEXT("DeletePending"), Out.bDeletePending);
	Obj->TryGetNumberField(TEXT("DeleteRemainSec"), Out.DeleteRemainSec);
	return true;
}

void UNUCharacterManager::DebugAddDummyCharacter(const FNUCharacterSummary& InCharacter)
{
	const UEmberServerManager* Server = GetServer();
	if (Server != nullptr && Server->IsLoggedIn())
	{
		UE_LOG(LogNUCharSelect, Warning, TEXT("서버에 붙어 있는 동안에는 더미 캐릭터를 넣지 않습니다"));
		return;
	}

	FNUCharacterSummary& NewCharacter = Characters.Add_GetRef(InCharacter);
	NewCharacter.CharacterIdx = NextDummyIdx++;
	if (NewCharacter.Name.IsEmpty())
	{
		NewCharacter.Name = FString::Printf(TEXT("모험가%lld"), NewCharacter.CharacterIdx);
	}
	NewCharacter.Level = FMath::Max(NewCharacter.Level, 1);
	OnCharacterListChanged.Broadcast();
}

void UNUCharacterManager::CreateDummyCharacter(const FString& InName)
{
	// 더미 경로에도 중복 이름은 막는다 --- 서버 1202를 흉내내야 UI를 그대로 확인할 수 있다
	const bool bDuplicate = Characters.ContainsByPredicate(
		[&InName](const FNUCharacterSummary& C) { return C.Name.Equals(InName, ESearchCase::IgnoreCase); });
	if (bDuplicate)
	{
		UE_LOG(LogNUCharSelect, Warning, TEXT("캐릭터 생성(더미) 거절 — 이름 중복 [%s]"), *InName);
		OnCharacterCreateResult.Broadcast(false, 1202);
		OnCharacterError.Broadcast(1202, TEXT("이미 사용 중인 이름입니다"));
		return;
	}

	FNUCharacterSummary& NewCharacter = Characters.AddDefaulted_GetRef();
	NewCharacter.CharacterIdx = NextDummyIdx++;
	NewCharacter.Name = InName.IsEmpty()
		? FString::Printf(TEXT("모험가%lld"), NewCharacter.CharacterIdx)
		: InName;
	NewCharacter.JobId = UNASSIGNED_JOB_ID;
	NewCharacter.Level = 1;
	// 서버는 외형을 저장해 그대로 돌려준다 --- 더미도 고른 모습을 들고 있어야 선택창에서 같은 캐릭터로 보인다
	NewCharacter.RaceId = LastCreateParams.RaceId;
	NewCharacter.Gender = static_cast<int32>(LastCreateParams.Gender);
	NewCharacter.HairId = LastCreateParams.HairId;
	NewCharacter.FaceId = LastCreateParams.FaceId;

	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 생성(더미 — 서버 미연결) [%s] Idx=%lld"),
		*NewCharacter.Name, NewCharacter.CharacterIdx);

	PendingCreatedIdx = NewCharacter.CharacterIdx;
	ClearDraftParams();
	UNUSFXSubsystem::Play(GetGameInstance(), NU::SFX::Char::Create);
	OnCharacterCreateResult.Broadcast(true, 200);
	OnCharacterListChanged.Broadcast();
}

void UNUCharacterManager::RequestNameCheck(const FString& InName)
{
	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		// 서버 없는 개발 경로 --- 더미 목록과 대조해 중복만 흉내낸다
		const bool bDuplicate = Characters.ContainsByPredicate(
			[&InName](const FNUCharacterSummary& C) { return C.Name.Equals(InName, ESearchCase::IgnoreCase); });
		OnCharacterNameChecked.Broadcast(bDuplicate ? 1202 : 200);
		return;
	}
	Server->SendCharacterNameCheck(InName);
}

void UNUCharacterManager::HandleNameCheckResponse(int32 ResultCode)
{
	UE_LOG(LogNUCharSelect, Log, TEXT("이름 확인 응답 (%d)"), ResultCode);
	OnCharacterNameChecked.Broadcast(ResultCode);
}

void UNUCharacterManager::RequestCharacterProfile(int64 CharacterIdx)
{
	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		// 남의 정보는 더미로 흉내낼 것이 없다 --- 못 찾음(1200)으로 알린다
		OnCharacterProfile.Broadcast(1200, FNUCharacterProfile());
		return;
	}
	Server->SendCharacterProfile(CharacterIdx);
}

void UNUCharacterManager::HandleProfileResponse(int32 ResultCode, const FString& ResponseData)
{
	FNUCharacterProfile Profile;
	if (ResultCode == 200)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			// 성공 코드인데 본문이 깨졌다 --- UI가 빈 창을 열지 않게 실패로 바꿔 알린다
			UE_LOG(LogNUCharSelect, Warning, TEXT("캐릭터 정보 응답 JSON 파싱 실패: %s"), *ResponseData);
			OnCharacterProfile.Broadcast(0, Profile);
			return;
		}

		// PascalCase — 서버 JSON 규약 (gRPC_help §4.4)
		double IdxValue = 0.0;
		Root->TryGetNumberField(TEXT("Idx"), IdxValue);
		Profile.CharacterIdx = static_cast<int64>(IdxValue);
		Root->TryGetStringField(TEXT("Name"), Profile.Name);
		Root->TryGetNumberField(TEXT("Level"), Profile.Level);
		Root->TryGetNumberField(TEXT("JobId"), Profile.JobId);
		Profile.Level = FMath::Max(Profile.Level, 1);

		// 빈 부위는 배열에 없다 --- 온 것만 담는다
		const TArray<TSharedPtr<FJsonValue>>* JsonEquipment = nullptr;
		if (Root->TryGetArrayField(TEXT("Equipment"), JsonEquipment) && JsonEquipment != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Value : *JsonEquipment)
			{
				const TSharedPtr<FJsonObject> Entry = Value.IsValid() ? Value->AsObject() : nullptr;
				int32 SlotNo = -1;
				int32 ItemId = 0;
				if (Entry.IsValid() && Entry->TryGetNumberField(TEXT("Slot"), SlotNo)
					&& Entry->TryGetNumberField(TEXT("ItemId"), ItemId))
				{
					Profile.Equipment.Add(SlotNo, ItemId);
				}
			}
		}
	}

	UE_LOG(LogNUCharSelect, Log, TEXT("캐릭터 정보 응답 (%d) Idx=%lld 장비 %d부위"),
		ResultCode, Profile.CharacterIdx, Profile.Equipment.Num());
	OnCharacterProfile.Broadcast(ResultCode, Profile);
}

void UNUCharacterManager::CancelDeleteCharacter(int64 CharacterIdx)
{
	UEmberServerManager* Server = GetServer();
	if (Server == nullptr || !Server->IsLoggedIn())
	{
		UE_LOG(LogNUCharSelect, Warning, TEXT("삭제 예약 취소 실패 --- 서버 미접속"));
		return;
	}
	UE_LOG(LogNUCharSelect, Log, TEXT("삭제 예약 취소 요청 Idx=%lld"), CharacterIdx);
	Server->SendCharacterDeleteCancel(CharacterIdx);
}

void UNUCharacterManager::HandleDeleteCancelResponse(int32 ResultCode, const FString& ResponseData)
{
	if (ResultCode != RC_SUCCESS)
	{
		// 1200 없는 캐릭터(만기 지나 이미 지워짐 포함) / 1213 예약이 없음 / 1205 DB 오류
		UE_LOG(LogNUCharSelect, Error, TEXT("삭제 예약 취소 실패 (%d)"), ResultCode);
		OnCharacterError.Broadcast(ResultCode, TEXT("삭제 예약을 취소하지 못했습니다"));
		return;
	}

	// 응답에 삭제 응답과 같은 형식의 목록이 실려 온다 --- 그대로 갱신하면 배지가 내려간다
	UE_LOG(LogNUCharSelect, Log, TEXT("삭제 예약 취소 완료"));
	if (!ApplyListJson(ResponseData))
	{
		RequestCharacterList();
	}
}

int64 UNUCharacterManager::ConsumePendingCreatedIdx()
{
	// 한 번만 쓰고 지운다 --- 다음 목록 갱신(삭제 등)에서 다시 고르면 안 된다
	const int64 Idx = PendingCreatedIdx;
	PendingCreatedIdx = 0;
	return Idx;
}

UEmberServerManager* UNUCharacterManager::GetServer() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEmberServerManager>() : nullptr;
}
