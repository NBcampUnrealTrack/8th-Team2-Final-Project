// Fill out your copyright notice in the Description page of Project Settings.


#include "Party/NUPartyTypes.h"

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FString* FNUPartySnapshot::FindMemberName(int64 Idx) const
{
	const FNUPartyMember* Found = Members.FindByPredicate(
		[Idx](const FNUPartyMember& Member)
		{
			return Member.Idx == Idx;
		});

	return Found ? &Found->Name : nullptr;
}

namespace NUParty
{
	bool ParseMemberList(const FString& ResponseData, FNUPartySnapshot& Out)
	{
		if (ResponseData.IsEmpty())
		{
			// 거절 응답, 탈퇴·추방 응답에는 목록이 없으므로 false 리턴
			return false;
		}

		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		// PartyId가 없으면 파티 목록 JSON이 아님
		FNUPartySnapshot Parsed;
		if (!Root->TryGetNumberField(TEXT("PartyId"), Parsed.PartyId))
		{
			return false;
		}

		Root->TryGetNumberField(TEXT("LeaderIdx"), Parsed.LeaderIdx);
		Root->TryGetNumberField(TEXT("MaxCount"), Parsed.MaxCount);

		// Members가 없어도 성공해야 함
		// 나간·추방당한 사람이 받는 푸시가 Members가 없음
		// TryGetArrayField가 채워주므로 호출이 먼저, 순서가 바뀌면 항상 건너뜀
		const TArray<TSharedPtr<FJsonValue>>* JsonMembers = nullptr;
		if (Root->TryGetArrayField(TEXT("Members"), JsonMembers) && JsonMembers != nullptr)
		{
			for (const TSharedPtr<FJsonValue>& Value : *JsonMembers)
			{
				const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!Object.IsValid())
				{
					continue;
				}

				int64 MemberIdx = 0;
				if (!Object->TryGetNumberField(TEXT("Idx"), MemberIdx))
				{
					continue;   // 대상을 지목할 수 없는 멤버는 버린다
				}

				FNUPartyMember& Member = Parsed.Members.AddDefaulted_GetRef();
				Member.Idx = MemberIdx;
				Object->TryGetStringField(TEXT("Name"), Member.Name);
				Object->TryGetBoolField(TEXT("IsLeader"), Member.bIsLeader);
			}
		}

		Out = MoveTemp(Parsed);
		return true;
	}

	FText GetResultMessage(int32 ResultCode)
	{
		// gRPC 문서의 ResultCode 표에서 파티가 실제로 받는 값들
		switch (ResultCode)
		{
		case 1204: return NSLOCTEXT("NUParty", "RC1204", "캐릭터를 먼저 선택해야 합니다.");
		case 1600: return NSLOCTEXT("NUParty", "RC1600", "파티가 없습니다.");
		case 1601: return NSLOCTEXT("NUParty", "RC1601", "대상을 찾을 수 없습니다.");
		case 1602: return NSLOCTEXT("NUParty", "RC1602", "이미 같은 파티입니다.");
		case 1603: return NSLOCTEXT("NUParty", "RC1603", "이미 다른 파티에 속해 있습니다.");
		case 1604: return NSLOCTEXT("NUParty", "RC1604", "파티 인원이 가득 찼습니다.");
		case 1605: return NSLOCTEXT("NUParty", "RC1605", "파티장만 할 수 있습니다.");
		case 1606: return NSLOCTEXT("NUParty", "RC1606", "요청이 만료되었습니다.");
		case 1607: return NSLOCTEXT("NUParty", "RC1607", "자기 자신은 대상이 될 수 없습니다.");
		case 1608: return NSLOCTEXT("NUParty", "RC1608", "파티 처리에 실패했습니다. 잠시 후 다시 시도해 주세요.");

		default:
			return FText::Format(
				NSLOCTEXT("NUParty", "RCUnknown", "파티 요청이 거부되었습니다. (코드 {0})"),
				FText::AsNumber(ResultCode));
		}
	}

	FText GetUpdateMessage(ENUPartyUpdateReason Reason, const FString& TargetName)
	{
		// 이름을 모르는 경우가 있음
		// 나간 사람은 새 목록에 없고, 교체 전 목록에도 없을 수 있음
		bool bHasName = !TargetName.IsEmpty();
		FText Name = FText::FromString(TargetName);

		switch (Reason)
		{
		case ENUPartyUpdateReason::Joined:
			return bHasName
				? FText::Format(NSLOCTEXT("NUParty", "Joined", "{0}님이 파티에 참여했습니다."), Name)
				: NSLOCTEXT("NUParty", "JoinedNoName", "새 파티원이 참여했습니다.");

		case ENUPartyUpdateReason::Left:
			return bHasName
				? FText::Format(NSLOCTEXT("NUParty", "Left", "{0}님이 파티에서 나갔습니다."), Name)
				: NSLOCTEXT("NUParty", "LeftNoName", "파티원이 나갔습니다.");

		case ENUPartyUpdateReason::Kicked:
			return bHasName
				? FText::Format(NSLOCTEXT("NUParty", "Kicked", "{0}님이 파티에서 추방되었습니다."), Name)
				: NSLOCTEXT("NUParty", "KickedNoName", "파티원이 추방되었습니다.");

		case ENUPartyUpdateReason::LeaderChanged:
			return bHasName
				? FText::Format(NSLOCTEXT("NUParty", "LeaderChanged", "{0}님이 파티장이 되었습니다."), Name)
				: NSLOCTEXT("NUParty", "LeaderChangedNoName", "파티장이 변경되었습니다.");

		case ENUPartyUpdateReason::Disbanded:
			return NSLOCTEXT("NUParty", "Disbanded", "파티가 해산되었습니다.");

		default:
			return FText::GetEmpty();
		}
	}
}
