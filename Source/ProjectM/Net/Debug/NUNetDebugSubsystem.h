#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "NUNetDebugSubsystem.generated.h"

class UEmberServerManager;
class UFieldSyncSubsystem;
class UNUChatWidget;
class UNUPartySubsystem;
struct IConsoleCommand;

// 확인용 디버그 시스템
//
//   ember.FieldEnter                           FieldEnter 재전송 (강제 재동기화)
//   ember.FieldActivity      [0|2]             FieldActivity 단독 전송
//   ember.FakeRemote         [id] [거리cm]     가짜 원격 캐릭터 스폰
//   ember.FakeRemoteMove     [id] [거리cm]     가짜 원격 이동
//   ember.FakeRemoteActivity [id] [0|1|2]      가짜 원격 상태 변경
//   ember.FakeRemoteRemove   [id]              가짜 원격 제거
//   ember.FakeChat           [채널] [본문...]   가짜 채팅 (로그·필터·스크롤 검증)
//   ember.FakeParty          [인원] [내가리더]  가짜 파티 구성 (푸시 흉내)
//   ember.FakePartyInvite    [이름] [초] [신청] 가짜 초대·신청 팝업
//   ember.FakePartyLeave                      가짜 파티 해산 (party_id 0)
//
// 아래 파티 명령은 실제 서버로 전송됩니다 (파티 UI가 생기기 전까지의 조작 수단)
//   ember.PartyInvite        [캐릭터idx]       파티 초대 전송
//   ember.PartyAccept                         받아둔 초대·신청 수락
//   ember.PartyDecline                        받아둔 초대·신청 거절
//   ember.PartyLeave                          파티 탈퇴
//   ember.PartyKick          [캐릭터idx]       파티 추방 (리더만)
//   ember.PartySync                           PartyMemberList 재요청
//   ember.PartyDump                           현재 파티 스냅샷 출력
//
// 가짜 원격 계열은 서버 푸시(SVR_Field_*)를 흉내내서 수신 함수를 직접 호출합니다
// 위치 동기화를 검증하는 용도입니다
UCLASS()
class PROJECTM_API UNUNetDebugSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void RegisterCommands();

	UEmberServerManager* GetServer() const;
	UFieldSyncSubsystem* GetFieldSync() const;
	UNUChatWidget* GetChatWidget() const;
	UNUPartySubsystem* GetParty() const;

	// 가짜 파티 목록에 나를 넣을 때 사용
	int64 GetMyCharacterIdx() const;

	// 가짜 원격 캐릭터를 놓을 위치 (내 캐릭터 앞쪽)
	FVector GetFakeRemoteLocation(float ForwardDistance) const;

	// PIE를 반복 실행하면 Initialize가 다시 도므로 해제해야 중복 등록 경고가 안 뜸
	TArray<IConsoleCommand*> RegisteredCommands;
};
