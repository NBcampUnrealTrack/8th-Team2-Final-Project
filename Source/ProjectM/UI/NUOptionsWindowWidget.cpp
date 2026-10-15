#include "UI/NUOptionsWindowWidget.h"

#include "Audio/NUSFXSubsystem.h"
#include "Audio/NUSFXTags.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Core/NUGameUserSettings.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "UI/Common/NU_CommonButtonWidget.h"
#include "UI/NUConfirmDialog.h"
#include "UI/NUKeyBindingsWindow.h"
#include "UI/NUOptionRadioGroupWidget.h"
#include "UI/NUUIManager.h"
#include "UI/NUUISettings.h"

// 유니티 빌드에서 익명 네임스페이스 충돌을 피하려고 static + 고유 접두어를 쓴다 (전체 메뉴와 같은 이유)

// 콤보 순서 = 엔진 EWindowMode 값 (Fullscreen 0 / WindowedFullscreen 1 / Windowed 2)
static const TCHAR* const NUOptions_WindowModeLabels[] = { TEXT("전체 화면"), TEXT("테두리 없는 창"), TEXT("창 모드") };

// 콤보 순서 = 엔진 스케일러빌리티 단계 0~3. 엔진의 4단계(시네마틱)는 내놓지 않는다 --- 저장돼 있으면 최상으로 보여준다
static const TCHAR* const NUOptions_QualityLabels[] = { TEXT("하"), TEXT("중"), TEXT("상"), TEXT("최상") };
static constexpr int32 NUOptions_QualityCount = UE_ARRAY_COUNT(NUOptions_QualityLabels);

// 프리셋 묶음의 마지막 버튼 --- 세부 7개가 서로 다를 때 켜진다 (번호 = NUOptions_QualityCount)
static const TCHAR* const NUOptions_CustomQualityLabel = TEXT("사용자 설정");

// 콤보 순서 = UNUGameUserSettings::UpscaleMode
static const TCHAR* const NUOptions_UpscaleLabels[UNUGameUserSettings::UpscaleModeCount] = { TEXT("끔"), TEXT("품질"), TEXT("균형"), TEXT("성능") };

// 0 = 제한 없음 (엔진 규약)
static constexpr float NUOptions_FrameLimits[] = { 30.0f, 60.0f, 120.0f, 144.0f, 240.0f, 0.0f };
static const TCHAR* const NUOptions_FrameLimitLabels[] = { TEXT("30"), TEXT("60"), TEXT("120"), TEXT("144"), TEXT("240"), TEXT("제한 없음") };

// 해상도를 바꾼 뒤 답이 없으면 되돌리기까지의 시간
static constexpr int32 NUOptions_VideoModeConfirmSeconds = 15;

// 볼륨 옆 숫자 "70%"
static const FNumberFormattingOptions NUOptions_PercentFormat = FNumberFormattingOptions().SetMaximumFractionalDigits(0);

// 탭 버튼 이름과 그 핸들러 --- 배열 순서 = TabSwitcher 페이지 순서
static const TCHAR* const NUOptions_TabHandlers[] = { TEXT("HandleDisplayTabClicked"), TEXT("HandleGraphicsTabClicked"), TEXT("HandleSoundTabClicked"), TEXT("HandleControlsTabClicked") };

static TArray<FString> NUOptions_ToStrings(const TCHAR* const* Labels, int32 Count)
{
	TArray<FString> Out;
	Out.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Out.Add(Labels[i]);
	}
	return Out;
}

UNUOptionsWindowWidget* UNUOptionsWindowWidget::Show(APlayerController* OwningPlayer)
{
	TSubclassOf<UNUOptionsWindowWidget> WindowClass = GetDefault<UNUUISettings>()->OptionsWindowClass.LoadSynchronous();
	if (OwningPlayer == nullptr || WindowClass == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[옵션] OptionsWindowClass 미지정 --- 프로젝트 세팅 > 게임 > NU UI"));
		return nullptr;
	}

	UNUOptionsWindowWidget* Window = CreateWidget<UNUOptionsWindowWidget>(OwningPlayer, WindowClass);
	if (Window == nullptr)
	{
		return nullptr;
	}

	if (UNUUIManager* UI = UNUUIManager::Get(OwningPlayer))
	{
		FNUUIWindowParams Params;
		Params.Layer = NUUILayer::Window;
		Params.bDimBackground = true;
		Params.bBlockGameInput = true; // 화면을 덮는 창 --- 뒤에서 캐릭터가 걷지 않게
		UI->OpenWindow(Window, Params, FSimpleDelegate::CreateUObject(Window, &UNUOptionsWindowWidget::Close));
	}
	else
	{
		Window->AddToViewport(NUUILayer::Window);
	}
	return Window;
}

/////////////////////////////////////////////////////////////////
// 수명
/////////////////////////////////////////////////////////////////

void UNUOptionsWindowWidget::BindButtonClicked(UWidget* Widget, UObject* Target, FName FunctionName)
{
	FScriptDelegate Handler;
	Handler.BindUFunction(Target, FunctionName);
	if (UButton* AsButton = Cast<UButton>(Widget))
	{
		AsButton->OnClicked.AddUnique(Handler);
	}
	else if (UNU_CommonButtonWidget* AsCommon = Cast<UNU_CommonButtonWidget>(Widget))
	{
		AsCommon->OnCommonClicked.AddUnique(Handler);
	}
}

void UNUOptionsWindowWidget::UnbindButtonClicked(UWidget* Widget, UObject* Target, FName FunctionName)
{
	FScriptDelegate Handler;
	Handler.BindUFunction(Target, FunctionName);
	if (UButton* AsButton = Cast<UButton>(Widget))
	{
		AsButton->OnClicked.Remove(Handler);
	}
	else if (UNU_CommonButtonWidget* AsCommon = Cast<UNU_CommonButtonWidget>(Widget))
	{
		AsCommon->OnCommonClicked.Remove(Handler);
	}
}

void UNUOptionsWindowWidget::SetButtonBackgroundColor(UWidget* Widget, const FLinearColor& Color)
{
	if (UButton* AsButton = Cast<UButton>(Widget))
	{
		AsButton->SetBackgroundColor(Color);
	}
	else if (UNU_CommonButtonWidget* AsCommon = Cast<UNU_CommonButtonWidget>(Widget))
	{
		AsCommon->SetBackgroundColor(Color);
	}
}

void UNUOptionsWindowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 재오픈 중복 구독 방지 --- 해제 선행 후 구독
	UnbindButtonClicked(ApplyButton, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, Apply));
	BindButtonClicked(ApplyButton, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, Apply));
	UnbindButtonClicked(DefaultsButton, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, ResetToDefaults));
	BindButtonClicked(DefaultsButton, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, ResetToDefaults));
	UnbindButtonClicked(CloseButton, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, Close));
	BindButtonClicked(CloseButton, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, Close));
	if (UWidget* HeaderClose = GetWidgetFromName(TEXT("HeaderCloseButton")))
	{
		UnbindButtonClicked(HeaderClose, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, Close));
		BindButtonClicked(HeaderClose, this, GET_FUNCTION_NAME_CHECKED(UNUOptionsWindowWidget, Close));
	}
	WindowModeCombo->OnSelectionChanged.RemoveDynamic(this, &UNUOptionsWindowWidget::HandleWindowModeChanged);
	WindowModeCombo->OnSelectionChanged.AddDynamic(this, &UNUOptionsWindowWidget::HandleWindowModeChanged);

	// 그래픽 --- 프리셋과 세부 7개가 서로를 갱신한다 (묶음의 알림은 사용자 클릭에만 오므로 되돌아 걸리지 않는다)
	QualityGroup->OnSelected.BindUObject(this, &UNUOptionsWindowWidget::HandlePresetSelected);
	for (const FQualityRow& Row : GetQualityRows())
	{
		Row.Group->OnSelected.BindUObject(this, &UNUOptionsWindowWidget::HandleCategorySelected);
	}

	// 사운드 슬라이더 --- 움직이는 동안 바로 들리게
	MasterVolumeSlider->OnValueChanged.RemoveDynamic(this, &UNUOptionsWindowWidget::HandleMasterVolumeChanged);
	MasterVolumeSlider->OnValueChanged.AddDynamic(this, &UNUOptionsWindowWidget::HandleMasterVolumeChanged);
	BgmVolumeSlider->OnValueChanged.RemoveDynamic(this, &UNUOptionsWindowWidget::HandleBgmVolumeChanged);
	BgmVolumeSlider->OnValueChanged.AddDynamic(this, &UNUOptionsWindowWidget::HandleBgmVolumeChanged);
	SfxVolumeSlider->OnValueChanged.RemoveDynamic(this, &UNUOptionsWindowWidget::HandleSfxVolumeChanged);
	SfxVolumeSlider->OnValueChanged.AddDynamic(this, &UNUOptionsWindowWidget::HandleSfxVolumeChanged);

	// 탭 버튼은 이름표로 핸들러를 묶는다 (AddDynamic은 매크로라 표로 돌릴 수 없다)
	UWidget* const TabButtons[] = { DisplayTabButton, GraphicsTabButton, SoundTabButton, ControlsTabButton };
	for (int32 i = 0; i < UE_ARRAY_COUNT(TabButtons); ++i)
	{
		BindButtonClicked(TabButtons[i], this, NUOptions_TabHandlers[i]);
	}

	// 키 설정 버튼은 WBP마다 타입이 다르다 (순정 Button / 공용 버튼)
	if (UButton* KeyBindingsPure = Cast<UButton>(KeyBindingsButton))
	{
		KeyBindingsPure->OnClicked.RemoveDynamic(this, &UNUOptionsWindowWidget::HandleKeyBindingsClicked);
		KeyBindingsPure->OnClicked.AddDynamic(this, &UNUOptionsWindowWidget::HandleKeyBindingsClicked);
	}
	else if (UNU_CommonButtonWidget* KeyBindingsCommon = Cast<UNU_CommonButtonWidget>(KeyBindingsButton))
	{
		KeyBindingsCommon->OnCommonClicked.RemoveDynamic(this, &UNUOptionsWindowWidget::HandleKeyBindingsClicked);
		KeyBindingsCommon->OnCommonClicked.AddDynamic(this, &UNUOptionsWindowWidget::HandleKeyBindingsClicked);
	}

	LoadFromSettings();
	SelectTab(ActiveTab);
	// Settings uses the Window layer, outside the automatic popup hover range.
	// Register after constructing the option controls, including nested buttons.
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->SetPopupButtonHover(this, true);
	}
}

void UNUOptionsWindowWidget::HandleKeyBindingsClicked()
{
	UNUKeyBindingsWindow::Show(GetOwningPlayer());
}

void UNUOptionsWindowWidget::SelectTab(int32 Index)
{
	if (ActiveTab != Index)
	{
		UNUSFXSubsystem::Play(this, NU::SFX::UI::Tab);
	}
	ActiveTab = Index;
	TabSwitcher->SetActiveWidgetIndex(Index);
	UpdateTabVisual();
}

void UNUOptionsWindowWidget::UpdateTabVisual()
{
	const UNUUISettings* Palette = GetDefault<UNUUISettings>();
	UWidget* const TabButtons[] = { DisplayTabButton, GraphicsTabButton, SoundTabButton, ControlsTabButton };
	for (int32 i = 0; i < UE_ARRAY_COUNT(TabButtons); ++i)
	{
		SetButtonBackgroundColor(TabButtons[i], i == ActiveTab ? Palette->TabSelectedColor : Palette->SlotColor);
		// WBP authors the gold underline and selected label; C++ only selects their state.
		if (UWidget* Mark = GetWidgetFromName(*FString::Printf(TEXT("SettingsTabMark%d"), i)))
		{
			Mark->SetVisibility(i == ActiveTab ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
		if (UWidget* SelectedLabel = GetWidgetFromName(*FString::Printf(TEXT("SettingsTabLabel%d"), i)))
		{
			SelectedLabel->SetVisibility(i == ActiveTab ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			if (UWidget* Label = GetWidgetFromName(*FString::Printf(TEXT("%sLabel"), *TabButtons[i]->GetName())))
			{
				Label->SetVisibility(i == ActiveTab ? ESlateVisibility::Hidden : ESlateVisibility::HitTestInvisible);
			}
		}
	}
}

void UNUOptionsWindowWidget::NativeDestruct()
{
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->SetPopupButtonHover(this, false);
	}
	// 확인을 기다리는 중에 창이 사라지면(레벨 전환 등) 답이 없는 것으로 보고 되돌린다
	if (VideoModeConfirmTimer.IsValid())
	{
		FinishVideoModeConfirm(false);
	}
	if (bSettingsTouched)
	{
		// [기본값]과 볼륨 슬라이더는 엔진 설정 객체를 바로 바꾼다 --- 적용 없이 사라지면 디스크 값으로 되돌리고, 들리던 볼륨도 그 값으로
		UNUGameUserSettings* Settings = UNUGameUserSettings::Get();
		Settings->LoadSettings(true);
		Settings->ApplyAudioVolumes();
		bSettingsTouched = false;
	}
	Super::NativeDestruct();
}

void UNUOptionsWindowWidget::Close()
{
	// 적용하지 않은 변경은 NativeDestruct가 되돌린다 (레벨 전환으로 사라질 때와 같은 길)
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->CloseWindow(this); // 미등록이면 무시 --- 아래 RemoveFromParent와 겹쳐도 안전
	}
	RemoveFromParent();
}

/////////////////////////////////////////////////////////////////
// 설정 <-> UI
/////////////////////////////////////////////////////////////////

void UNUOptionsWindowWidget::LoadFromSettings()
{
	const UNUGameUserSettings* Settings = UNUGameUserSettings::Get();

	const int32 WindowMode = static_cast<int32>(Settings->GetFullscreenMode());
	FillCombo(WindowModeCombo,
		NUOptions_ToStrings(NUOptions_WindowModeLabels, UE_ARRAY_COUNT(NUOptions_WindowModeLabels)), WindowMode);
	RefreshResolutionList(WindowMode, Settings->GetScreenResolution());

	VSyncCheck->SetIsChecked(Settings->IsVSyncEnabled());

	int32 FrameLimitIndex = INDEX_NONE;
	for (int32 i = 0; i < UE_ARRAY_COUNT(NUOptions_FrameLimits); ++i)
	{
		if (FMath::IsNearlyEqual(Settings->GetFrameRateLimit(), NUOptions_FrameLimits[i]))
		{
			FrameLimitIndex = i;
			break;
		}
	}
	FillCombo(FrameLimitCombo,
		NUOptions_ToStrings(NUOptions_FrameLimitLabels, UE_ARRAY_COUNT(NUOptions_FrameLimitLabels)), FrameLimitIndex);

	// 세부 7개를 먼저 채우고, 프리셋은 그 결과로 정한다 (전부 같으면 그 단계, 아니면 사용자 설정)
	const TArray<FString> QualityOptions = NUOptions_ToStrings(NUOptions_QualityLabels, NUOptions_QualityCount);
	for (const FQualityRow& Row : GetQualityRows())
	{
		Row.Group->SetOptions(QualityOptions);
		Row.Group->SetSelectedIndex(FMath::Min((Settings->*Row.Get)(), NUOptions_QualityCount - 1));
	}
	TArray<FString> PresetOptions = QualityOptions;
	PresetOptions.Add(NUOptions_CustomQualityLabel);
	QualityGroup->SetOptions(PresetOptions);
	QualityGroup->SetSelectedIndex(ComputePresetIndex());

	UpscaleGroup->SetOptions(NUOptions_ToStrings(NUOptions_UpscaleLabels, UNUGameUserSettings::UpscaleModeCount));
	UpscaleGroup->SetSelectedIndex(Settings->GetUpscaleMode());

	// 사운드 --- 슬라이더는 저장값으로, 옆 숫자는 WBP에 있으면 채운다
	SetVolumeRow(MasterVolumeSlider, MasterVolumeValueText, Settings->GetMasterVolume());
	SetVolumeRow(BgmVolumeSlider, BgmVolumeValueText, Settings->GetBgmVolume());
	SetVolumeRow(SfxVolumeSlider, SfxVolumeValueText, Settings->GetSfxVolume());
}

void UNUOptionsWindowWidget::RefreshResolutionList(int32 WindowMode, FIntPoint Preferred)
{
	const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	const FIntPoint Desktop = Settings->GetDesktopResolution();

	Resolutions.Reset();
	switch (EWindowMode::ConvertIntToWindowMode(WindowMode))
	{
	case EWindowMode::Fullscreen:
		// 모니터가 지원하는 모드만 (에디터 PIE처럼 목록을 못 받으면 바탕화면 하나)
		UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);
		break;
	case EWindowMode::Windowed:
		UKismetSystemLibrary::GetConvenientWindowedResolutions(Resolutions);
		Resolutions.AddUnique(Preferred); // 지금 크기는 목록에 없어도 고를 수 있게
		break;
	default:
		// 테두리 없는 창은 엔진이 바탕화면 크기로 맞춘다 --- 고를 것이 없다
		break;
	}
	Resolutions.AddUnique(Desktop);
	Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
	{
		return A.X != B.X ? A.X < B.X : A.Y < B.Y;
	});

	TArray<FString> Labels;
	Labels.Reserve(Resolutions.Num());
	for (const FIntPoint& Res : Resolutions)
	{
		Labels.Add(FString::Printf(TEXT("%d x %d"), Res.X, Res.Y));
	}
	int32 Selected = Resolutions.IndexOfByKey(Preferred);
	if (Selected == INDEX_NONE)
	{
		Selected = Resolutions.Num() - 1; // 정렬돼 있으니 마지막 = 가장 큰 것
	}
	FillCombo(ResolutionCombo, Labels, Selected);
	ResolutionCombo->SetIsEnabled(Resolutions.Num() > 1);
}

void UNUOptionsWindowWidget::HandleWindowModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// 프로그램이 채울 때(Direct)는 LoadFromSettings가 목록까지 맡는다 --- 사용자가 골랐을 때만
	if (SelectionType == ESelectInfo::Direct)
	{
		return;
	}
	const int32 WindowMode = GetComboIndex(WindowModeCombo);
	const UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	// 전체 화면 계열로 바꾸는 사람의 의도는 "화면을 꽉 채우기"다 --- 바탕화면 해상도를 기본으로, 창 모드는 지금 크기 유지
	const FIntPoint Preferred = WindowMode == static_cast<int32>(EWindowMode::Windowed)
		? Settings->GetScreenResolution()
		: Settings->GetDesktopResolution();
	RefreshResolutionList(WindowMode, Preferred);
}

void UNUOptionsWindowWidget::FillCombo(UComboBoxString* Combo, const TArray<FString>& Options, int32 SelectedIndex)
{
	Combo->ClearOptions();
	for (const FString& Option : Options)
	{
		Combo->AddOption(Option);
	}
	if (Options.IsValidIndex(SelectedIndex))
	{
		Combo->SetSelectedIndex(SelectedIndex);
	}
}

int32 UNUOptionsWindowWidget::GetComboIndex(const UComboBoxString* Combo)
{
	return Combo->GetSelectedIndex();
}

/////////////////////////////////////////////////////////////////
// 그래픽
/////////////////////////////////////////////////////////////////

TArray<UNUOptionsWindowWidget::FQualityRow, TInlineAllocator<7>> UNUOptionsWindowWidget::GetQualityRows() const
{
	return {
		{ ShadowQualityGroup, &UGameUserSettings::GetShadowQuality, &UGameUserSettings::SetShadowQuality },
		{ GlobalIlluminationQualityGroup, &UGameUserSettings::GetGlobalIlluminationQuality, &UGameUserSettings::SetGlobalIlluminationQuality },
		{ ReflectionQualityGroup, &UGameUserSettings::GetReflectionQuality, &UGameUserSettings::SetReflectionQuality },
		{ PostProcessQualityGroup, &UGameUserSettings::GetPostProcessingQuality, &UGameUserSettings::SetPostProcessingQuality },
		{ EffectsQualityGroup, &UGameUserSettings::GetVisualEffectQuality, &UGameUserSettings::SetVisualEffectQuality },
		{ TextureQualityGroup, &UGameUserSettings::GetTextureQuality, &UGameUserSettings::SetTextureQuality },
		{ ViewDistanceQualityGroup, &UGameUserSettings::GetViewDistanceQuality, &UGameUserSettings::SetViewDistanceQuality },
	};
}

int32 UNUOptionsWindowWidget::ComputePresetIndex() const
{
	int32 Common = INDEX_NONE;
	for (const FQualityRow& Row : GetQualityRows())
	{
		const int32 Index = Row.Group->GetSelectedIndex();
		if (Index == INDEX_NONE || (Common != INDEX_NONE && Index != Common))
		{
			return NUOptions_QualityCount; // 사용자 설정
		}
		Common = Index;
	}
	return Common;
}

void UNUOptionsWindowWidget::HandlePresetSelected(int32 Preset)
{
	if (Preset >= NUOptions_QualityCount)
	{
		return; // "사용자 설정"을 직접 누른 것 --- 세부는 그대로
	}
	for (const FQualityRow& Row : GetQualityRows())
	{
		Row.Group->SetSelectedIndex(Preset);
	}
}

void UNUOptionsWindowWidget::HandleCategorySelected(int32 Index)
{
	QualityGroup->SetSelectedIndex(ComputePresetIndex());
}

/////////////////////////////////////////////////////////////////
// 사운드
/////////////////////////////////////////////////////////////////

void UNUOptionsWindowWidget::HandleMasterVolumeChanged(float Value)
{
	UNUGameUserSettings::Get()->SetMasterVolume(Value);
	PreviewVolume(MasterVolumeValueText, Value);
}

void UNUOptionsWindowWidget::HandleBgmVolumeChanged(float Value)
{
	UNUGameUserSettings::Get()->SetBgmVolume(Value);
	PreviewVolume(BgmVolumeValueText, Value);
}

void UNUOptionsWindowWidget::HandleSfxVolumeChanged(float Value)
{
	UNUGameUserSettings::Get()->SetSfxVolume(Value);
	PreviewVolume(SfxVolumeValueText, Value);

	// BGM과 달리 나고 있는 소리가 없어 크기를 가늠할 소리를 낸다 (간격은 효과음 표가 제한)
	UNUSFXSubsystem::Play(this, NU::SFX::UI::Step);
}

void UNUOptionsWindowWidget::PreviewVolume(UTextBlock* ValueText, float Value)
{
	// 들으면서 고르게 바로 건다. 저장은 [적용], 그 전에 창이 사라지면 NativeDestruct가 디스크 값으로 되돌린다
	UNUGameUserSettings::Get()->ApplyAudioVolumes();
	bSettingsTouched = true;
	SetVolumeText(ValueText, Value);
}

void UNUOptionsWindowWidget::SetVolumeRow(USlider* Slider, UTextBlock* ValueText, float Value)
{
	Slider->SetValue(Value); // OnValueChanged는 사용자 조작에만 오므로 미리듣기가 되돌아 걸리지 않는다
	SetVolumeText(ValueText, Value);
}

void UNUOptionsWindowWidget::SetVolumeText(UTextBlock* ValueText, float Value)
{
	ValueText->SetText(FText::AsPercent(Value, &NUOptions_PercentFormat));
}

void UNUOptionsWindowWidget::Apply()
{
	UNUGameUserSettings* Settings = UNUGameUserSettings::Get();

	const int32 WindowMode = GetComboIndex(WindowModeCombo);
	if (WindowMode >= 0 && WindowMode < UE_ARRAY_COUNT(NUOptions_WindowModeLabels))
	{
		Settings->SetFullscreenMode(EWindowMode::ConvertIntToWindowMode(WindowMode));
	}
	const int32 Resolution = GetComboIndex(ResolutionCombo);
	if (Resolutions.IsValidIndex(Resolution))
	{
		Settings->SetScreenResolution(Resolutions[Resolution]);
	}
	Settings->SetVSyncEnabled(VSyncCheck->IsChecked());
	const int32 FrameLimit = GetComboIndex(FrameLimitCombo);
	if (FrameLimit >= 0 && FrameLimit < UE_ARRAY_COUNT(NUOptions_FrameLimits))
	{
		Settings->SetFrameRateLimit(NUOptions_FrameLimits[FrameLimit]);
	}
	// 세부 7개가 한 단계로 모이면 노출하지 않은 항목(식생·셰이딩·안티에일리어싱 품질)도 같은 단계로 맞춘 뒤 7개를 쓴다
	const int32 Preset = ComputePresetIndex();
	if (Preset < NUOptions_QualityCount)
	{
		Settings->SetOverallScalabilityLevel(Preset);
	}
	for (const FQualityRow& Row : GetQualityRows())
	{
		const int32 Index = Row.Group->GetSelectedIndex();
		if (Index != INDEX_NONE)
		{
			(Settings->*Row.Set)(Index);
		}
	}
	if (UpscaleGroup->GetSelectedIndex() != INDEX_NONE)
	{
		Settings->SetUpscaleMode(UpscaleGroup->GetSelectedIndex());
	}

	// 창 모드/해상도는 잘못 고르면 화면을 못 볼 수 있어, 적용한 뒤 확인을 받고서야 확정한다
	const bool bVideoModeChanged =
		Settings->GetFullscreenMode() != Settings->GetLastConfirmedFullscreenMode() ||
		Settings->GetScreenResolution() != Settings->GetLastConfirmedScreenResolution();

	Settings->ApplySettings(false); // 적용 + 저장 (볼륨도 UNUGameUserSettings가 같이 건다)
	bSettingsTouched = false;

	if (bVideoModeChanged)
	{
		BeginVideoModeConfirm();
	}
	else if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(NSLOCTEXT("NUOptions", "Applied", "설정을 적용했습니다."));
	}
}

void UNUOptionsWindowWidget::ResetToDefaults()
{
	// 엔진 기본값(바탕화면 해상도, 기본 창 모드, 프레임 무제한, 기본 품질)을 객체에 쓰고 UI에 비춘다.
	// 화면 설정은 [적용] 전까지 그대로, 볼륨은 슬라이더처럼 바로 들리게. 적용 없이 닫히면 NativeDestruct가 디스크 값으로 되돌린다
	UNUGameUserSettings* Settings = UNUGameUserSettings::Get();
	Settings->SetToDefaults();
	Settings->ApplyAudioVolumes();
	bSettingsTouched = true;
	LoadFromSettings();
}

/////////////////////////////////////////////////////////////////
// 해상도 변경 확인
/////////////////////////////////////////////////////////////////

FText UNUOptionsWindowWidget::GetVideoModeCountdownText() const
{
	return FText::Format(
		NSLOCTEXT("NUOptions", "KeepVideoMode", "이 화면 설정을 유지할까요?\n{0}초 뒤 이전 설정으로 돌아갑니다."),
		FText::AsNumber(VideoModeSecondsLeft));
}

void UNUOptionsWindowWidget::BeginVideoModeConfirm()
{
	VideoModeSecondsLeft = NUOptions_VideoModeConfirmSeconds;

	UNUConfirmDialog* Dialog = UNUConfirmDialog::Show(
		GetOwningPlayer(),
		GetDefault<UNUUISettings>()->ConfirmDialogClass.LoadSynchronous(),
		GetVideoModeCountdownText(),
		NSLOCTEXT("NUOptions", "KeepVideoModeConfirm", "유지"),
		FSimpleDelegate::CreateUObject(this, &UNUOptionsWindowWidget::HandleVideoModeKept),
		FSimpleDelegate::CreateUObject(this, &UNUOptionsWindowWidget::HandleVideoModeReverted),
		NSLOCTEXT("NUOptions", "KeepVideoModeCancel", "되돌리기"),
		ENUConfirmIntent::Positive);

	// 확인 창이 없으면 물을 수단이 없다 --- 그대로 확정한다
	if (Dialog == nullptr)
	{
		FinishVideoModeConfirm(true);
		return;
	}

	VideoModeDialog = Dialog;
	GetWorld()->GetTimerManager().SetTimer(
		VideoModeConfirmTimer, this, &UNUOptionsWindowWidget::TickVideoModeCountdown, 1.0f, true);
}

void UNUOptionsWindowWidget::TickVideoModeCountdown()
{
	--VideoModeSecondsLeft;
	if (VideoModeSecondsLeft > 0)
	{
		if (VideoModeDialog.IsValid())
		{
			VideoModeDialog->SetMessage(GetVideoModeCountdownText());
		}
		return;
	}

	// 시간 초과 = 되돌리기. 창의 Cancel이 우리 콜백(FinishVideoModeConfirm(false))까지 태운다
	if (VideoModeDialog.IsValid())
	{
		VideoModeDialog->Cancel();
	}
	else
	{
		FinishVideoModeConfirm(false);
	}
}

void UNUOptionsWindowWidget::FinishVideoModeConfirm(bool bKeep)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(VideoModeConfirmTimer);
	}
	VideoModeConfirmTimer.Invalidate();
	if (VideoModeDialog.IsValid())
	{
		VideoModeDialog->Dismiss(); // 콜백 경로에서는 이미 닫혀 있다 --- 겹쳐도 안전
		VideoModeDialog.Reset();
	}

	UGameUserSettings* Settings = UGameUserSettings::GetGameUserSettings();
	if (bKeep)
	{
		Settings->ConfirmVideoMode();
		Settings->SaveSettings();
	}
	else
	{
		Settings->RevertVideoMode();
		Settings->ApplySettings(false);
	}

	if (IsConstructed())
	{
		LoadFromSettings();
	}
	if (UNUUIManager* UI = UNUUIManager::Get(GetOwningPlayer()))
	{
		UI->ShowNotice(bKeep
			? NSLOCTEXT("NUOptions", "Applied", "설정을 적용했습니다.")
			: NSLOCTEXT("NUOptions", "VideoModeReverted", "이전 화면 설정으로 되돌렸습니다."));
	}
}

#if !UE_BUILD_SHIPPING
// PIE는 ESC를 에디터가 재생 중지로 가져가 전체 메뉴를 못 연다 --- 옵션창을 바로 여는 테스트 경로.
// 탭은 창에서 누르면 되므로 인자는 두지 않는다. 사용: nu.Options.Open
static FAutoConsoleCommandWithWorld GNUOptionsOpenCommand(
	TEXT("nu.Options.Open"),
	TEXT("옵션 창 열기 (PIE에서 ESC 대신)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		UNUOptionsWindowWidget::Show(World != nullptr ? World->GetFirstPlayerController() : nullptr);
	}));
#endif
