#include "UI/Common/NUPopupButtonHover.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUPopupButtonHoverLifecycleTest, "ProjectM.UI.PopupButtons.HoverLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUPopupButtonHoverLifecycleTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UOverlay> Root(NewObject<UOverlay>());
	UButton* Accept = NewObject<UButton>(Root.Get());
	UButton* Cancel = NewObject<UButton>(Root.Get());
	Root->AddChildToOverlay(Accept);
	Root->AddChildToOverlay(Cancel);
	FWidgetTransform Rest;
	Rest.Scale = FVector2D(0.9, 1.1);
	Rest.Translation = FVector2D(12, 7);
	Rest.Angle = 4;
	Accept->SetRenderTransform(Rest);
	// IsVisible() queries the live Slate widget, so keep the whole test tree built.
	const TSharedRef<SWidget> SlateRoot = Root->TakeWidget();
	FNUPopupButtonHover Motion;
	Motion.RegisterRoot(Root.Get());
	auto HoverAccept = [Accept](UButton& Button) { return &Button == Accept; };
	auto HoverNone = [](UButton&) { return false; };
	Motion.Advance(0.06f, HoverAccept);
	const FVector2D MidScale = Accept->GetRenderTransform().Scale;
	TestTrue(TEXT("Partial hover is between rest and target"), MidScale.X < Rest.Scale.X && MidScale.X > Rest.Scale.X * FNUPopupButtonHover::HoverScale);
	TestTrue(TEXT("Other button stays at rest"), Cancel->GetRenderTransform().Scale.Equals(FVector2D(1)));
	Motion.RegisterRoot(Root.Get());
	TestTrue(TEXT("Repeated registration does not resnapshot the shrunken scale"), Accept->GetRenderTransform().Scale.Equals(MidScale));
	Motion.Advance(0, HoverNone);
	TestTrue(TEXT("Changing direction does not jump"), Accept->GetRenderTransform().Scale.Equals(MidScale));
	Motion.Advance(0.03f, HoverNone);
	TestTrue(TEXT("Reverse moves smoothly toward rest"), Accept->GetRenderTransform().Scale.X > MidScale.X);
	Motion.Advance(1, HoverAccept);
	TestTrue(TEXT("Hover stops at 97 percent of authored scale"), Accept->GetRenderTransform().Scale.Equals(Rest.Scale * FNUPopupButtonHover::HoverScale, 0.00001));
	TestTrue(TEXT("Translation is preserved"), Accept->GetRenderTransform().Translation.Equals(Rest.Translation));
	TestEqual(TEXT("Rotation is preserved"), Accept->GetRenderTransform().Angle, Rest.Angle);
	Accept->SetIsEnabled(false);
	Motion.Advance(0.01f, HoverAccept);
	TestTrue(TEXT("Disabling restores immediately"), Accept->GetRenderTransform().Scale.Equals(Rest.Scale));
	Accept->SetIsEnabled(true);
	Motion.Advance(1, HoverAccept);
	Root->SetVisibility(ESlateVisibility::Collapsed);
	Motion.Advance(0.01f, HoverAccept);
	TestTrue(TEXT("Hidden parent resets hover"), Accept->GetRenderTransform().Scale.Equals(Rest.Scale));
	Root->SetVisibility(ESlateVisibility::Visible);
	Motion.Advance(1, HoverAccept);
	Motion.UnregisterRoot(Root.Get());
	TestTrue(TEXT("Closing restores authored scale"), Accept->GetRenderTransform().Scale.Equals(Rest.Scale));
	TestFalse(TEXT("No targets remain after closing"), Motion.HasTargets());
	Motion.RegisterRoot(Root.Get());
	// Use the real Slate mouse-enter/leave path too, without moving the OS cursor.
	const TSharedRef<SWidget> SlateButton = Accept->TakeWidget();
	SlateButton->OnMouseEnter(FGeometry(), FPointerEvent());
	Motion.Advance(1);
	TestTrue(TEXT("Native Slate hover triggers shrink"), Accept->GetRenderTransform().Scale.Equals(Rest.Scale * FNUPopupButtonHover::HoverScale, 0.00001));
	SlateButton->OnMouseLeave(FPointerEvent());
	Motion.Advance(1);
	TestTrue(TEXT("Native Slate leave restores scale"), Accept->GetRenderTransform().Scale.Equals(Rest.Scale));
	Motion.Reset();
	return true;
}
#endif
