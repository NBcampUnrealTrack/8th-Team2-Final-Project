// Fill out your copyright notice in the Description page of Project Settings.

#include "CharacterSelect/UI/NUCharacterProfileUIController.h"

#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterSelect/UI/NUCharacterProfileWidget.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

void UNUCharacterProfileUIController::OnInitialize()
{
	if (UNUCharacterManager* Characters = GetCharacterManager())
	{
		ProfileHandle = Characters->OnCharacterProfile.AddUObject(
			this, &UNUCharacterProfileUIController::HandleProfileResponse);
	}
}

void UNUCharacterProfileUIController::OnDeinitialize()
{
	if (UNUCharacterManager* Characters = GetCharacterManager())
	{
		Characters->OnCharacterProfile.Remove(ProfileHandle);
	}
	ProfileHandle.Reset();
}

void UNUCharacterProfileUIController::ShowProfile(int64 CharacterIdx)
{
	UNUCharacterManager* Characters = GetCharacterManager();
	if (Characters == nullptr)
	{
		return;
	}

	// 캐릭터를 고르지 않고 접속한 대상은 조회할 것이 없다 (서버도 1200으로 거절한다)
	if (CharacterIdx == 0)
	{
		if (UNUUIManager* UI = GetManager())
		{
			UI->ShowNotice(NSLOCTEXT("NUCharacterProfile", "NoCharacter", "캐릭터 정보를 찾을 수 없습니다"));
		}
		return;
	}

	PendingIdx = CharacterIdx;
	Characters->RequestCharacterProfile(CharacterIdx);
}

void UNUCharacterProfileUIController::HandleProfileResponse(int32 ResultCode, const FNUCharacterProfile& Profile)
{
	// 내가 요청해 둔 응답이 아니면 창을 열지 않는다 (앞으로 다른 사용처가 생겨도 안전)
	if (PendingIdx == 0)
	{
		return;
	}
	PendingIdx = 0;

	UNUUIManager* UI = GetManager();
	if (UI == nullptr)
	{
		return;
	}

	if (ResultCode != 200)
	{
		UI->ShowNotice(ResultCode == 1200
			? NSLOCTEXT("NUCharacterProfile", "NotFound", "캐릭터 정보를 찾을 수 없습니다")
			: NSLOCTEXT("NUCharacterProfile", "LoadFailed", "캐릭터 정보를 불러오지 못했습니다"));
		return;
	}

	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	TSubclassOf<UNUCharacterProfileWidget> WindowClass =
		Settings ? Settings->CharacterProfileWindowClass.LoadSynchronous() : nullptr;
	if (WindowClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("캐릭터 정보 창 WBP 미지정 --- 프로젝트 세팅 > 게임 > NU UI"));
		return;
	}

	// 레벨이 바뀌었으면 이전 월드의 위젯은 버리고 새로 만든다
	GetOrCreateWidget(ProfileWindow, WindowClass);
	if (ProfileWindow == nullptr)
	{
		return;
	}
	ProfileWindow->SetProfile(Profile);

	// 이미 열려 있으면 OpenWindow는 무시되고 내용만 갈린다 (다른 대상을 연달아 조회하는 경우)
	FNUUIWindowParams Params;
	Params.bCloseOnSessionEnd = true; // 서버 데이터를 그리는 창이다
	UI->OpenWindow(ProfileWindow, Params,
		FSimpleDelegate::CreateUObject(this, &UNUCharacterProfileUIController::CloseProfileWindow));
}

void UNUCharacterProfileUIController::CloseProfileWindow()
{
	if (ProfileWindow == nullptr)
	{
		return;
	}
	if (UNUUIManager* UI = GetManager())
	{
		UI->CloseWindow(ProfileWindow);
	}
}

void UNUCharacterProfileUIController::OnServerSessionEnded()
{
	// 창은 bCloseOnSessionEnd로 매니저가 이미 닫았다 --- 대기 상태만 정리한다
	PendingIdx = 0;
}

UNUCharacterManager* UNUCharacterProfileUIController::GetCharacterManager() const
{
	const APlayerController* PC = GetOwningPlayerController();
	const UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
}
