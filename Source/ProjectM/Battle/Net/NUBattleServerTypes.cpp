// Fill out your copyright notice in the Description page of Project Settings.


#include "NUBattleServerTypes.h"

#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUJobResourceViews.h"
#include "Battle/Data/NUStatusEffectDefinition.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SRevGame/GameMessage.h"


namespace
{
	bool TryReadBattleInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int32& OutValue)
	{
		double Number = 0.0;
		
		if (!Object.IsValid()
			|| !Object->TryGetNumberField(FieldName, Number)
			|| !FMath::IsFinite(Number)
			|| Number < static_cast<double>(MIN_int32)
			|| Number > static_cast<double>(MAX_int32)
			|| FMath::TruncToDouble(Number) != Number)
		{
			return false;
		}
		
		OutValue = static_cast<int32>(Number);
		return true;
	}
	
	bool TryReadBattleInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, int64& OutValue)
	{
		return Object.IsValid() && Object->TryGetNumberField(FieldName, OutValue);
	}

	bool ReadRequiredBattleInt32(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName, 
		int32& OutValue, FString& OutError)
	{
		if (TryReadBattleInt32(Object, FieldName, OutValue))
		{
			return true;
		}

		OutError = FString::Printf(TEXT("%s 필드가 없거나 int32 형식이 아닙니다."), FieldName);
		return false;
	}

	bool ReadRequiredBattleInt64(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName,
		int64& OutValue, FString& OutError)
	{
		if (TryReadBattleInt64(Object, FieldName, OutValue))
		{
			return true;
		}

		OutError = FString::Printf(TEXT("%s 필드가 없거나 int64 형식이 아닙니다."), FieldName);
		return false;
	}

	bool ReadBattleInt32Array(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName,
		TArray<int32>& OutValues, FString& OutError)
	{
		const TArray<TSharedPtr<FJsonValue>>* JsonValues = nullptr;

		if (!Object.IsValid()
			|| !Object->TryGetArrayField(FieldName, JsonValues)
			|| !JsonValues)
		{
			OutError = FString::Printf(TEXT("%s 배열이 없습니다."), FieldName);
			return false;
		}

		TArray<int32> ParsedValues;
		ParsedValues.Reserve(JsonValues->Num());

		for (const TSharedPtr<FJsonValue>& Value : *JsonValues)
		{
			double Number = 0.0;

			if (!Value.IsValid()
				|| !Value->TryGetNumber(Number)
				|| !FMath::IsFinite(Number)
				|| Number < static_cast<double>(MIN_int32)
				|| Number > static_cast<double>(MAX_int32)
				|| FMath::TruncToDouble(Number) != Number)
			{
				OutError = FString::Printf(TEXT("%s 배열에 int32가 아닌 값이 있습니다."), FieldName);
				return false;
			}

			ParsedValues.Add(static_cast<int32>(Number));
		}

		OutValues = MoveTemp(ParsedValues);
		return true;
	}

	ENUBattleEventType ConvertEventType(int32 RawType)
	{
		if (RawType <= static_cast<int32>(ENUBattleEventType::None)
			|| RawType >= static_cast<int32>(ENUBattleEventType::Count))
		{
			return ENUBattleEventType::None;
		}

		return static_cast<ENUBattleEventType>(RawType);
	}
}


bool NUBattleServer::ConvertCombatant(const FGrpcRevGameBattleCombatant& Source, FNUServerCombatant& OutCombatant,
	FString& OutError)
{
	if (Source.CombatantId < 0)
	{
		OutError = TEXT("CombatantId가 음수입니다.");
		return false;
	}

	if (Source.Side < 0 || Source.Side > 1)
	{
		OutError = FString::Printf(
			TEXT("CombatantId=%d의 Side가 0~1 범위가 아닙니다."),
			Source.CombatantId);

		return false;
	}

	if (Source.Kind < 0 || Source.Kind > 2)
	{
		OutError = FString::Printf(
			TEXT("CombatantId=%d의 Kind가 0~2 범위가 아닙니다."),
			Source.CombatantId);

		return false;
	}

	if (Source.OwnerCharacterIdx < 0
		|| Source.NaiaInstanceId < 0
		|| Source.NaiaId < 0
		|| Source.Level < 0
		|| Source.Hp < 0
		|| Source.MaxHp < 0
		|| Source.Sp < 0
		|| Source.MaxSp < 0
		|| !FMath::IsFinite(Source.Speed.Value))
	{
		OutError = FString::Printf(
			TEXT("CombatantId=%d의 스냅샷 값이 유효하지 않습니다."),
			Source.CombatantId);

		return false;
	}

	FNUServerCombatant Parsed;
	Parsed.CombatantId		 = Source.CombatantId;
	Parsed.Side				 = Source.Side;
	Parsed.Kind				 = Source.Kind;
	Parsed.OwnerCharacterIdx = Source.OwnerCharacterIdx;
	Parsed.NaiaInstanceId	 = Source.NaiaInstanceId;
	Parsed.NaiaId			 = Source.NaiaId;
	Parsed.Name				 = Source.Name;
	Parsed.Level			 = Source.Level;
	Parsed.Health			 = Source.Hp;
	Parsed.MaxHealth		 = Source.MaxHp;
	Parsed.SP				 = Source.Sp;
	Parsed.MaxSP			 = Source.MaxSp;
	Parsed.Speed			 = Source.Speed.Value;
	Parsed.Elements			 = Source.Elements;
	Parsed.RaceId			 = Source.RaceId;
	Parsed.Gender			 = Source.Gender;
	Parsed.HairId			 = Source.HairId;
	Parsed.FaceId			 = Source.FaceId;
	Parsed.JobId			 = Source.JobId;

	Parsed.Statuses.Reserve(Source.Statuses.Num());

	for (const TSharedPtr<FGrpcRevGameBattleStatus>& SourceStatus : Source.Statuses)
	{
		if (!SourceStatus.IsValid())
		{
			OutError = FString::Printf(
				TEXT("CombatantId=%d의 Statuses에 빈 항목이 있습니다."),
				Source.CombatantId);

			return false;
		}

		if (SourceStatus->StatusId <= 0
			|| SourceStatus->Stacks < 0
			|| SourceStatus->RemainingTurns < 0)
		{
			OutError = FString::Printf(
				TEXT("CombatantId=%d의 상태이상 값이 유효하지 않습니다."),
				Source.CombatantId);

			return false;
		}

		FNUServerBattleStatus& Status = Parsed.Statuses.AddDefaulted_GetRef();

		Status.StatusId = SourceStatus->StatusId;
		Status.Stacks = SourceStatus->Stacks;
		Status.RemainingTurns = SourceStatus->RemainingTurns;
	}

	Parsed.ActiveSkillIds = Source.ActiveSkillIds;
	Parsed.PassiveSkillIds = Source.PassiveSkillIds;

	// 번호가 없는 칸은 어느 스킬인지 알 수 없어 뺌
	Parsed.ActiveSkillIds.RemoveAll([](int32 SkillId)
	{
		return SkillId <= 0;
	});

	Parsed.PassiveSkillIds.RemoveAll([](int32 SkillId)
	{
		return SkillId <= 0;
	});

	Parsed.Resources.Reserve(Source.Resources.Num());

	for (const TSharedPtr<FGrpcRevGameBattleResource>& SourceResource : Source.Resources)
	{
		// 칸 하나가 이상하다고 스냅샷을 통째로 버리면 전투가 멈추므로 그 칸만 건너뜀
		if (!SourceResource.IsValid()
			|| SourceResource->ResourceId <= 0
			|| SourceResource->Value < 0
			|| SourceResource->Max < 0
			|| SourceResource->RemainingTurns < 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("CombatantId=%d의 직업 자원 칸을 건너뜀: 번호나 값이 유효하지 않습니다."),
				Source.CombatantId);
			continue;
		}

		FNUServerJobResource& Resource = Parsed.Resources.AddDefaulted_GetRef();

		Resource.ResourceId = SourceResource->ResourceId;
		Resource.Value = SourceResource->Value;
		Resource.Max = SourceResource->Max;
		Resource.RemainingTurns = SourceResource->RemainingTurns;
	}

	Parsed.JobStates.Reserve(Source.JobStates.Num());

	for (const TSharedPtr<FGrpcRevGameBattleJobState>& SourceState : Source.JobStates)
	{
		if (!SourceState.IsValid()
			|| SourceState->Name.IsEmpty()
			|| SourceState->StartTurn < 0
			|| SourceState->EndTurn < SourceState->StartTurn)
		{
			UE_LOG(LogTemp, Warning, TEXT("CombatantId=%d의 직업 상태 칸을 건너뜀: 이름이나 턴이 유효하지 않습니다."),
				Source.CombatantId);
			continue;
		}

		FNUServerJobState& State = Parsed.JobStates.AddDefaulted_GetRef();

		State.StateName = FName(*SourceState->Name);
		State.StartTurn = SourceState->StartTurn;
		State.EndTurn = SourceState->EndTurn;
		State.ResourceId = SourceState->ResourceId;
	}

	OutCombatant = MoveTemp(Parsed);
	OutError.Reset();
	return true;
}

bool NUBattleServer::ConvertEvent(const FGrpcRevGameBattleEvent& Source, FNUServerBattleEvent& OutEvent,
	FString& OutError)
{
	if (!FMath::IsFinite(Source.ElementMultiplier.Value))
	{
		OutError = TEXT("BattleEvent의 ElementMultiplier가 유효하지 않습니다.");
		return false;
	}

	FNUServerBattleEvent Parsed;

	Parsed.RawType = Source.Type;
	Parsed.Type				 = ConvertEventType(Source.Type);
	Parsed.ActorId			 = Source.ActorId;
	Parsed.TargetId			 = Source.TargetId;
	Parsed.Amount			 = Source.Amount;
	Parsed.bCritical		 = Source.IsCritical;
	Parsed.ElementMultiplier = Source.ElementMultiplier.Value;
	Parsed.StatusId	 		 = Source.StatusId;
	Parsed.Stacks			 = Source.Stacks;
	Parsed.SkillId			 = Source.SkillId;
	Parsed.ItemId			 = Source.ItemId;
	Parsed.HealthAfter		 = Source.HpAfter;
	Parsed.SPAfter			 = Source.SpAfter;
	Parsed.bSuccess			 = Source.Success;
	Parsed.CaptureShakes	 = Source.Shakes;
	Parsed.RemainingTurns	 = Source.RemainingTurns;
	Parsed.ResourceId		 = Source.ResourceId;
	Parsed.ResourceMax		 = Source.ResourceMax;
	Parsed.StateName		 = Source.StateName.IsEmpty() ? NAME_None : FName(*Source.StateName);
	Parsed.StartTurn		 = Source.StartTurn;
	Parsed.EndTurn			 = Source.EndTurn;

	OutEvent = MoveTemp(Parsed);
	OutError.Reset();
	return true;
}

void NUBattleServer::ApplyStatusEvent(const FNUServerBattleEvent& Event, TArray<FNUServerBattleStatus>& InOutStatuses)
{
	if (Event.StatusId <= 0)
	{
		return;
	}

	const int32 ExistingIndex = InOutStatuses.IndexOfByPredicate(
		[&Event](const FNUServerBattleStatus& Status)
		{
			return Status.StatusId == Event.StatusId;
		});

	if (Event.Type == ENUBattleEventType::StatusRemoved)
	{
		if (ExistingIndex != INDEX_NONE)
		{
			InOutStatuses.RemoveAt(ExistingIndex);
		}
		return;
	}

	FNUServerBattleStatus& Status = ExistingIndex != INDEX_NONE
		? InOutStatuses[ExistingIndex]
		: InOutStatuses.AddDefaulted_GetRef();

	Status.StatusId = Event.StatusId;
	Status.Stacks = Event.Stacks;

	// 0 은 전투 끝까지 라는 뜻이라 그대로 받아야 함
	// 남은 턴이 안 바뀌는 이벤트에서도 서버가 현재 값을 실어 보내는 것이 계약임
	Status.RemainingTurns = Event.RemainingTurns;
}

int32 NUBattleServer::ResolveResourceEventOwner(const FNUServerBattleEvent& Event)
{
	return Event.ActorId;
}

void NUBattleServer::ApplyResourceEvent(const FNUServerBattleEvent& Event, TArray<FNUServerJobResource>& InOutResources)
{
	if (Event.ResourceId <= 0)
	{
		return;
	}

	const int32 ExistingIndex = InOutResources.IndexOfByPredicate(
		[&Event](const FNUServerJobResource& Resource)
		{
			return Resource.ResourceId == Event.ResourceId;
		});

	FNUServerJobResource& Resource = ExistingIndex != INDEX_NONE
		? InOutResources[ExistingIndex]
		: InOutResources.AddDefaulted_GetRef();

	Resource.ResourceId = Event.ResourceId;

	// 새 수치는 Stacks 칸으로 읽음, Amount 는 변화량이라 쓰지 않음
	Resource.Value = FMath::Max(0, Event.Stacks);
	Resource.Max = FMath::Max(0, Event.ResourceMax);

	// 0 은 셀 턴이 없다는 뜻이라 그대로 받아야 함
	Resource.RemainingTurns = FMath::Max(0, Event.RemainingTurns);
}

void NUBattleServer::ApplyJobStateEvent(const FNUServerBattleEvent& Event, TArray<FNUServerJobState>& InOutStates)
{
	if (Event.StateName.IsNone())
	{
		return;
	}

	const int32 ExistingIndex = InOutStates.IndexOfByPredicate(
		[&Event](const FNUServerJobState& State)
		{
			return State.StateName == Event.StateName;
		});

	// 닫힘(success = false)은 새 턴이 열릴 때 TurnEnd 에 실려 옴
	if (!Event.bSuccess)
	{
		if (ExistingIndex != INDEX_NONE)
		{
			InOutStates.RemoveAt(ExistingIndex);
		}
		return;
	}

	FNUServerJobState& State = ExistingIndex != INDEX_NONE
		? InOutStates[ExistingIndex]
		: InOutStates.AddDefaulted_GetRef();

	State.StateName = Event.StateName;
	State.StartTurn = Event.StartTurn;
	State.EndTurn = Event.EndTurn;
	State.ResourceId = Event.ResourceId;
}

void NUBattleServer::BuildJobResourceViews(const UNUBattleDataSubsystem* BattleData,
	const TArray<FNUServerJobResource>& Resources, const TArray<FNUServerJobState>& JobStates, int32 Turn,
	TArray<FNUJobResourceView>& OutGauges, TArray<FNUJobResourceView>& OutBadges)
{
	OutGauges.Reset();
	OutBadges.Reset();

	if (!IsValid(BattleData))
	{
		return;
	}

	for (const FNUServerJobResource& Resource : Resources)
	{
		const FNUJobResourceDefinition* Definition = BattleData->FindJobResourceById(Resource.ResourceId);

		// 표에 없는 번호는 아이콘도 이름도 없어서 그릴 것이 없음
		if (!Definition)
		{
			static TSet<int32> LoggedResourceMisses;
			if (!LoggedResourceMisses.Contains(Resource.ResourceId))
			{
				LoggedResourceMisses.Add(Resource.ResourceId);
				UE_LOG(LogTemp, Warning,
					TEXT("서버가 보낸 직업 자원 %d 의 정의가 표에 없습니다"), Resource.ResourceId);
			}
			continue;
		}

		if (!NUJobResourceView::IsShownResource(*Definition))
		{
			continue;
		}

		if (Definition->OwnerScope == ENUJobResourceOwner::Self)
		{
			// 서버가 칸을 보냈다는 것이 곧 이 전투원이 그 자원을 연다는 뜻, 노드를 가졌는지는 서버가 이미 가름
			OutGauges.Add(NUJobResourceView::MakeResourceView(*Definition, Resource.Value, Resource.Max, 0));
			continue;
		}

		if (NUJobResourceView::ShouldShowBadge(Resource.Value))
		{
			OutBadges.Add(NUJobResourceView::MakeResourceView(
				*Definition, Resource.Value, Resource.Max, Resource.RemainingTurns));
		}
	}

	// 상태는 자원 뒤에, 끝난 것은 만들어지지 않고 대기 중인 것은 기간 전체를 보여 줌
	TArray<FNUJobResourceView> States;

	for (const FNUServerJobState& State : JobStates)
	{
		const FNUJobResourceDefinition* Definition = BattleData->FindJobResourceById(State.ResourceId);

		// 자원이 없는 상태(도발 · 사망 방지)는 아이콘을 찾을 열쇠가 없어 띄우지 않음
		if (!Definition)
		{
			static TSet<FName> LoggedStateMisses;
			if (!LoggedStateMisses.Contains(State.StateName))
			{
				LoggedStateMisses.Add(State.StateName);
				UE_LOG(LogTemp, Log,
					TEXT("직업 상태 %s 는 딸린 자원이 없어 체력바에 띄우지 않습니다"), *State.StateName.ToString());
			}
			continue;
		}

		FNUJobResourceView View;
		if (NUJobResourceView::MakeStateView(Definition, Definition->KoreanName, State.StateName,
			State.StartTurn, State.EndTurn, Turn, View))
		{
			States.Add(View);
		}
	}

	NUJobResourceView::SortViews(OutGauges, OutBadges, States);
}

void NUBattleServer::BuildStatusViews(const UNUBattleDataSubsystem* BattleData,
	const TArray<FNUServerBattleStatus>& Statuses, TArray<FNUStatusEffectView>& OutViews)
{
	OutViews.Reset();

	if (!IsValid(BattleData))
	{
		return;
	}

	for (const FNUServerBattleStatus& Status : Statuses)
	{
		const FNUStatusEffectDefinition* Definition =
			BattleData->FindStatusEffectDefinition(Status.StatusId);

		// 표에 없는 번호는 아이콘도 이름도 없어서 그릴 것이 없음
		if (!Definition)
		{
			// 이 함수는 상태이상이 바뀔 때마다 불려서, 같은 번호를 매번 찍으면 로그가 묻힘
			// NU_StatusIconWidget::ResolveStatusIcon 의 아이콘 누락 경고와 같은 방식
			static TSet<int32> LoggedMisses;
			if (!LoggedMisses.Contains(Status.StatusId))
			{
				LoggedMisses.Add(Status.StatusId);
				UE_LOG(LogTemp, Warning,
					TEXT("서버가 보낸 상태이상 %d 의 정의가 표에 없습니다"), Status.StatusId);
			}
			continue;
		}

		FNUStatusEffectView& View = OutViews.AddDefaulted_GetRef();
		View.StatusEffectId = Status.StatusId;
		View.StatusEffectTag = Definition->StatusEffectTag;
		View.KoreanName = Definition->KoreanName;
		View.Description = Definition->Description;
		View.StatusCategory = Definition->StatusCategory;
		View.MaxStack = Definition->MaxStack;

		// 스택형은 지속 턴 없이 스택만 쌓이므로 스택 수를, 나머지는 남은 턴을 띄움
		View.bBadgeIsStack = Definition->MaxStack > 1;
		View.BadgeNumber = View.bBadgeIsStack ? Status.Stacks : Status.RemainingTurns;
	}

	// 정렬하지 않으면 아이콘 자리가 전투 중에 뒤바뀜, ASC 경로와 같은 이유
	OutViews.Sort([](const FNUStatusEffectView& Left, const FNUStatusEffectView& Right)
		{
			return Left.StatusEffectId < Right.StatusEffectId;
		}
		);
}

bool NUBattleServer::ParseCommandAck(const FString& ResponseData, FNUServerCommandAck& OutAck, FString& OutError)
{
	if (ResponseData.IsEmpty())
	{
		OutError = TEXT("BattleCommand response_data가 비어 있습니다.");
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseData);

	if (!FJsonSerializer::Deserialize(Reader, Root)
		|| !Root.IsValid())
	{
		OutError = TEXT("BattleCommand response_data JSON을 읽지 못했습니다.");
		return false;
	}

	FNUServerCommandAck Parsed;

	if (!ReadRequiredBattleInt64(Root, TEXT("BattleId"), Parsed.BattleId, OutError)
		|| !ReadRequiredBattleInt32(Root, TEXT("CombatantId"), Parsed.CombatantId, OutError)
		|| !ReadRequiredBattleInt32(Root, TEXT("SpentSp"), Parsed.SpentSP, OutError)
		|| !ReadRequiredBattleInt32(Root, TEXT("HitCount"), Parsed.HitCount, OutError)
		|| !ReadRequiredBattleInt32( Root, TEXT("TargetId"), Parsed.TargetId, OutError)
		|| !ReadRequiredBattleInt32( Root, TEXT("Sp"), Parsed.RemainingSP, OutError)
		|| !ReadBattleInt32Array( Root, TEXT("Pending"), Parsed.PendingCombatantIds, OutError))
	{
		return false;
	}

	if (Parsed.BattleId <= 0
		|| Parsed.CombatantId < 0
		|| Parsed.SpentSP < 0
		|| Parsed.HitCount < 0
		|| Parsed.TargetId < INDEX_NONE
		|| Parsed.RemainingSP < 0)
	{
		OutError = TEXT("BattleCommand 확정값이 유효 범위를 벗어났습니다.");
		return false;
	}

	for (int32 PendingCombatantId : Parsed.PendingCombatantIds)
	{
		if (PendingCombatantId < 0)
		{
			OutError = TEXT("Pending 배열에 음수 CombatantId가 있습니다.");
			return false;
		}
	}

	OutAck = MoveTemp(Parsed);
	OutError.Reset();
	return true;
}

ENUBattleCommandSubmitResult NUBattleServer::MapCommandSubmitResult(int32 ResultCode)
{
	switch (ResultCode)
	{
	case 200:
		return ENUBattleCommandSubmitResult::Accepted;
	
	case 1901:	// 진행중인 전투가 없다(끝났거나 만료)
		return ENUBattleCommandSubmitResult::WrongEncounter;

	case 1903:	// 없는 행동값
	case 1910:	// 없는 스킬이거나 그 전투원이 가진 스킬이 아님 (스냅샷 active_skill_ids 기준)
	case 1911:	// 서버가 아직 실행 못 하는 스킬
	case 1912:	// 실명 등으로 액티브 스킬이 막힘
		return ENUBattleCommandSubmitResult::InvalidAbility;

	case 1905:	// 같은 턴이 두번 들어옴
		return ENUBattleCommandSubmitResult::WrongTurn;

	case 1906:	// SP 부족
		return ENUBattleCommandSubmitResult::NotEnoughSP;

	case 1907:	// 없는 대상, 죽은 대상 등 잘못된 타겟
		return ENUBattleCommandSubmitResult::InvalidTarget;

	case 1908:	// 죽었거나 기절해서 명령 낼 수없음 / 내 전투원이 아님
		return ENUBattleCommandSubmitResult::SourceDead;

	case 1913:	// 전투에서 쓸 수 없는 아이템 (없는 id · 효과 없음 · 교감석을 action 4로 보냄)
	case 1914:	// 그 아이템을 가지고 있지 않음
	case 2013:	// 교감석이 없음 / 교감석이 아닌 아이템을 action 3으로 보냄
		// 표의 bUsableInBattle 과 서버 판정이 어긋나면 여기로 떨어짐, 사유를 잃지 않게 갈라둠
		return ENUBattleCommandSubmitResult::InvalidItem;

	case 2014:	// 퀘스트 전투에서는 포획할 수 없음
		return ENUBattleCommandSubmitResult::CannotCommune;

	default:
		return ENUBattleCommandSubmitResult::Rejected;
	}
}

const TCHAR* NUBattleServer::DescribeCommandResult(int32 ResultCode)
{
	switch (ResultCode)
	{
	case 1901:
		return TEXT("진행 중인 전투가 없음 (끝났거나 만료)");

	case 1903:
		return TEXT("없는 행동 값");

	case 1905:
		return TEXT("같은 턴이 두 번 들어옴");

	case 1906:
		return TEXT("SP 부족");

	case 1907:
		return TEXT("없는 대상 · 죽은 대상 · 같은 진영");

	case 1908:
		return TEXT("죽었거나 기절해서 명령을 낼 수 없음 / 내 전투원이 아님");

	case 1909:
		return TEXT("이번 턴에 이미 교체함");

	case 1910:
		return TEXT("없는 스킬이거나 그 전투원이 가진 스킬이 아님");

	case 1911:
		return TEXT("서버가 아직 실행하지 못하는 스킬");

	case 1912:
		return TEXT("실명 등으로 액티브 스킬이 막혀 있음");

	case 1913:
		return TEXT("전투에서 쓸 수 없는 아이템");

	case 1914:
		return TEXT("그 아이템을 가지고 있지 않음");

	case 1915:
		return TEXT("없거나 지났거나 이미 처리된 반응 창");

	case 2014:
		return TEXT("퀘스트 전투에서는 포획할 수 없음");

	default:
		return TEXT("표에 없는 코드");
	}
}

bool NUBattleServer::MapRootActionToServerAction(ENUBattleRootAction RootAction, bool bCommuneItem, int32& OutAction)
{
	switch (RootAction)
	{
	case ENUBattleRootAction::Attack:
	case ENUBattleRootAction::Skill:
		// 스킬도 기본 공격과 같은 action, 스킬은 skill id로 구분함
		OutAction = 0;
		return true;

	case ENUBattleRootAction::Flee:
		OutAction = 2;
		return true;

	case ENUBattleRootAction::Item:
		// 포획(교감석)과 아이템 사용이 서버에서 서로 다른 행동임
		// 잘못 보내면 거절로 돌아옴
		// 교감석을 4로 보내면 1913, 회복 아이템을 3으로 보내면 2013
		OutAction = bCommuneItem ? 3 : 4;
		return true;

	case ENUBattleRootAction::NaiaSwap:
		// 턴을 소비하지 않는 별도 패킷(BattleNaiaSwap)으로 감, BattleCommand로 보내지 않음
		return false;

	default:
		return false;
	}
}


FNUParryTimingSpec FNUServerBattleReactionWindow::ToTimingSpec() const
{
	FNUParryTimingSpec Spec;
	if (ChallengeDurationMs <= 0)
	{
		// 서버가 창 길이를 안 채워 보낸 경우, 기본값 그대로 두면 판정이 돌기는 함
		return Spec;
	}

	Spec.ChallengeDuration = ChallengeDurationMs / 1000.f;

	const float StartSeconds = SuccessStartMs / 1000.f;
	const float EndSeconds = SuccessEndMs / 1000.f;

	// 판정기는 기준점 하나와 그 앞뒤 여유로 판정하므로 구간의 가운데와 반폭으로 변경함
	Spec.SuccessAnchor = FMath::Clamp(
		(StartSeconds + EndSeconds) * 0.5f / Spec.ChallengeDuration, 0.f, 1.f);
	Spec.SuccessWindow = FMath::Max((EndSeconds - StartSeconds) * 0.5f, 0.f);

	return Spec;
}

bool NUBattleServer::ConvertReactionWindow(const FGrpcRevGameSVR_Battle_Reaction& Source,
	FNUServerBattleReactionWindow& OutWindow, FString& OutError)
{
	if (Source.ReactionId <= 0)
	{
		OutError = TEXT("반응 창의 ReactionId 가 없습니다.");
		return false;
	}

	if (Source.AttackerId < 0 || Source.DefenderId < 0)
	{
		OutError = FString::Printf(
			TEXT("반응 창의 전투원 번호가 음수입니다. Attacker=%d Defender=%d"),
			Source.AttackerId, Source.DefenderId);

		return false;
	}

	if (Source.ChallengeDurationMs < 0
		|| Source.SuccessStartMs < 0
		|| Source.SuccessEndMs < Source.SuccessStartMs
		|| Source.SuccessEndMs > Source.ChallengeDurationMs
		|| Source.SpCost < 0)
	{
		OutError = FString::Printf(
			TEXT("반응 창의 구간이 유효하지 않습니다. Duration=%d Start=%d End=%d SP=%d"),
			Source.ChallengeDurationMs, Source.SuccessStartMs, Source.SuccessEndMs, Source.SpCost);

		return false;
	}

	OutWindow.ReactionId          = Source.ReactionId;
	OutWindow.AttackerId          = Source.AttackerId;
	OutWindow.DefenderId          = Source.DefenderId;
	OutWindow.ActionSeq           = Source.ActionSeq;
	OutWindow.HitIndex            = Source.HitIndex;
	OutWindow.SkillId             = Source.SkillId;
	OutWindow.bParryable          = Source.Parryable;
	OutWindow.bMustReport         = Source.MustReport;
	OutWindow.ChallengeDurationMs = Source.ChallengeDurationMs;
	OutWindow.SuccessStartMs      = Source.SuccessStartMs;
	OutWindow.SuccessEndMs        = Source.SuccessEndMs;
	OutWindow.SpCost              = Source.SpCost;

	return true;
}