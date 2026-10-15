// Fill out your copyright notice in the Description page of Project Settings.


#include "NUNaiaServerTypes.h"

#include "Naia/Data/NUNaiaTypes.h"
#include "SRevGame/GameMessage.h"


bool NUNaiaServer::ConvertNaiaSlot(const FGrpcRevGameNaiaSlot& Source, FNUServerNaiaSlot& OutSlot, FString& OutError)
{
	if (Source.Container != NUNaiaContainer::Party && Source.Container != NUNaiaContainer::Storage)
	{
		OutError = FString::Printf(TEXT("Container=%d가 Party/Storage 범위가 아닙니다."), Source.Container);
		return false;
	}

	if (Source.SlotNo < 0)
	{
		OutError = FString::Printf(TEXT("Container=%d의 SlotNo가 음수입니다."), Source.Container);
		return false;
	}

	FNUServerNaiaSlot Parsed;
	Parsed.Container = Source.Container;
	Parsed.SlotNo = Source.SlotNo;

	// 빈 칸이면 instance 의 모든 필드가 0으로 옴 (NaiaId <= 0 이라 IsValid() == false)
	const FGrpcRevGameNaiaInstance& Instance = Source.Instance;

	if (Instance.NaiaId > 0)
	{
		if (Instance.NaiaInstanceId <= 0
			|| Instance.Level < 0
			|| Instance.Affection < 0
			|| Instance.PassiveSkillId < 0
			|| Instance.Exp < 0
			|| !FMath::IsFinite(Instance.HealthRatio.Value))
		{
			OutError = FString::Printf(TEXT("Container=%d Slot=%d의 나이아 개체 값이 유효하지 않습니다."),
				Source.Container, Source.SlotNo);
			return false;
		}

		Parsed.Instance.InstanceId = Instance.NaiaInstanceId;
		Parsed.Instance.NaiaId = Instance.NaiaId;
		Parsed.Instance.Level = Instance.Level;
		Parsed.Instance.HealthRatio = static_cast<float>(FMath::Clamp(Instance.HealthRatio.Value, 0.0, 1.0));
		Parsed.Instance.Experience = Instance.Exp;
		Parsed.Instance.Affection = Instance.Affection;
		Parsed.Instance.PassiveSkillId = Instance.PassiveSkillId;
		Parsed.Instance.ActiveSkillIds = Instance.ActiveSkillIds;
	}

	OutSlot = MoveTemp(Parsed);
	OutError.Reset();
	return true;
}

FText NUNaiaServer::GetResultMessage(int32 ResultCode)
{
	// gRPC 가이드 결과 코드표 중 나이아 로스터/스타팅 계약 패킷(150~152)이 실제로 받을 수 있는 값들
	switch (ResultCode)
	{
	case 1204: return NSLOCTEXT("NUNaia", "RC1204", "캐릭터를 먼저 선택해야 합니다.");
	case 2000: return NSLOCTEXT("NUNaia", "RC2000", "나이아 저장소 종류가 올바르지 않습니다.");
	case 2001: return NSLOCTEXT("NUNaia", "RC2001", "파티 칸 번호가 올바르지 않습니다.");
	case 2002: return NSLOCTEXT("NUNaia", "RC2002", "그 파티 칸이 비어 있습니다.");
	case 2003: return NSLOCTEXT("NUNaia", "RC2003", "같은 칸으로는 이동할 수 없습니다.");
	case 2004: return NSLOCTEXT("NUNaia", "RC2004", "파티와 보관함에 빈자리가 없습니다.");
	case 2005: return NSLOCTEXT("NUNaia", "RC2005", "보유하지 않은 나이아입니다.");
	case 2006: return NSLOCTEXT("NUNaia", "RC2006", "나이아 저장소 처리에 실패했습니다. 잠시 후 다시 시도하세요.");
	case 2007: return NSLOCTEXT("NUNaia", "RC2007", "파티의 마지막 나이아는 보관함으로 내릴 수 없습니다.");
	case 2008: return NSLOCTEXT("NUNaia", "RC2008", "이미 스타팅 나이아 계약을 완료했습니다.");
	case 2009: return NSLOCTEXT("NUNaia", "RC2009", "계약할 수 없는 스타팅 나이아입니다.");
	case 2010: return NSLOCTEXT("NUNaia", "RC2010", "직업을 먼저 선택해야 스타팅 나이아와 계약할 수 있습니다.");

	default:
		return FText::Format(
			NSLOCTEXT("NUNaia", "RCUnknown", "나이아 로스터 요청이 거부되었습니다. (코드 {0})"),
			FText::AsNumber(ResultCode));
	}
}
