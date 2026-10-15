// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/VFXViewer/NUVFXViewerPlayerController.h"

#include "Framework/Application/SlateApplication.h"
#include "InputKeyEventArgs.h"
#include "Tools/VFXViewer/NUVFXViewerDirector.h"
#include "Widgets/SWidget.h"

ANUVFXViewerPlayerController::ANUVFXViewerPlayerController()
{
	bShowMouseCursor = true;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ANUVFXViewerPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// 패널은 마우스로, 단축키는 게임 쪽으로 받음
	FInputModeGameAndUI InputMode;
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void ANUVFXViewerPlayerController::SetDirector(ANUVFXViewerDirector* InDirector)
{
	Director = InDirector;
}

bool ANUVFXViewerPlayerController::IsTypingText()
{
	if (!FSlateApplication::IsInitialized())
	{
		return false;
	}

	const TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
	return Focused.IsValid() && Focused->GetType() == FName(TEXT("SEditableText"));
}

bool ANUVFXViewerPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	// 진행바: ← → 한 프레임, Shift 를 같이 누르면 0.1초 (누르고 있으면 계속 넘김)
	const bool bPressedOrRepeat = Params.Event == IE_Pressed || Params.Event == IE_Repeat;
	if (bPressedOrRepeat && IsValid(Director) && !IsTypingText()
		&& (Params.Key == EKeys::Left || Params.Key == EKeys::Right))
	{
		const int32 Direction = Params.Key == EKeys::Right ? 1 : -1;
		const bool bShift = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
		if (bShift)
		{
			Director->StepSeconds(Direction * 0.1f);
		}
		else
		{
			Director->StepFrames(Direction);
		}
		return true;
	}

	if (Params.Event == IE_Pressed && IsValid(Director) && !IsTypingText())
	{
		const FKey& Key = Params.Key;
		if (Key == EKeys::R)
		{
			Director->Replay();
			return true;
		}

		if (Key == EKeys::Home)
		{
			Director->SeekTo(0.f);
			return true;
		}

		if (Key == EKeys::SpaceBar)
		{
			Director->TogglePause();
			return true;
		}

		if (Key == EKeys::F5)
		{
			Director->RequestReload();
			return true;
		}

		if (Key == EKeys::Tab)
		{
			Director->TogglePanel();
			return true;
		}

		// 후보 비교 (-VFXCompare): [ ] 후보, Shift+[ ] 후보가 있는 스킬, Enter 고름
		// ↑ ↓ · PgUp PgDn 은 스킬 목록이 씀 (목록에 포커스가 있으면 스킬을 옮기며 틂)
		if (Director->HasCandidates())
		{
			if (Key == EKeys::LeftBracket || Key == EKeys::RightBracket)
			{
				const int32 Direction = Key == EKeys::RightBracket ? 1 : -1;
				const bool bShift = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();
				if (bShift)
				{
					Director->StepCandidateSkill(Direction);
				}
				else
				{
					Director->StepCandidate(Direction);
				}
				return true;
			}

			if (Key == EKeys::Enter)
			{
				Director->ChooseCandidate();
				return true;
			}
		}

		// 숫자 키로 속도, 폴리싱은 느리게 보는 일이 많음
		static const TPair<FKey, float> SpeedKeys[] =
		{
			{ EKeys::One, 0.1f },
			{ EKeys::Two, 0.25f },
			{ EKeys::Three, 0.5f },
			{ EKeys::Four, 1.f }
		};

		for (const TPair<FKey, float>& SpeedKey : SpeedKeys)
		{
			if (Key == SpeedKey.Key)
			{
				Director->SetSpeed(SpeedKey.Value);
				return true;
			}
		}
	}

	return Super::InputKey(Params);
}
