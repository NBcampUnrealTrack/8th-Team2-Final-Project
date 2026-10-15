#include "UI/HUD/NUFieldHUDMotion.h"
#include "UI/HUD/NUFieldHUDVisibility.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/Border.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUFieldHUDMotionReversalTest, "ProjectM.UI.FieldHUD.Motion.Reversal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUFieldHUDMotionReversalTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UOverlay> Layout(NewObject<UOverlay>());
	UBorder* Left = NewObject<UBorder>(Layout.Get());
	UBorder* Right = NewObject<UBorder>(Layout.Get());
	Layout->AddChildToOverlay(Left)->SetHorizontalAlignment(HAlign_Left);
	Layout->AddChildToOverlay(Right)->SetHorizontalAlignment(HAlign_Right);
	FWidgetTransform Rest;
	Rest.Translation = FVector2D(7.0, 3.0);
	Rest.Scale = FVector2D(0.9, 1.1);
	Rest.Angle = 2.0f;
	Left->SetRenderTransform(Rest);
	Left->SetRenderOpacity(0.8f);
	FNUFieldHUDMotion Motion;
	Motion.Present(Left, ESlateVisibility::SelfHitTestInvisible);
	Motion.Present(Right, ESlateVisibility::Visible);
	Motion.SetVisible(false);
	TestEqual(TEXT("Outgoing panels immediately stop receiving input"), Left->GetVisibility(), ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("Hide starts at the resting opacity"), Left->GetRenderOpacity(), 0.8f);
	TestTrue(TEXT("Hide is still drawing halfway through"), Motion.Advance(0.12f));
	TestTrue(TEXT("Left moves left"), Left->GetRenderTransform().Translation.X < Rest.Translation.X);
	TestTrue(TEXT("Right moves right"), Right->GetRenderTransform().Translation.X > 0.0);
	TestTrue(TEXT("Opacity is partial during the motion"), Left->GetRenderOpacity() > 0.0f && Left->GetRenderOpacity() < 0.8f);
	const FVector2D HalfPosition = Left->GetRenderTransform().Translation;
	const float HalfOpacity = Left->GetRenderOpacity();
	Motion.SetVisible(true);
	TestTrue(TEXT("Reversal does not jump position"), Left->GetRenderTransform().Translation.Equals(HalfPosition));
	TestEqual(TEXT("Reversal does not jump opacity"), Left->GetRenderOpacity(), HalfOpacity);
	Motion.Advance(0.06f);
	TestTrue(TEXT("Reversed panel returns toward rest"), Left->GetRenderTransform().Translation.X > HalfPosition.X);
	TestTrue(TEXT("Reversed panel fades back in"), Left->GetRenderOpacity() > HalfOpacity);
	TestFalse(TEXT("Show finishes without a permanent tick"), Motion.Advance(1.0f));
	TestTrue(TEXT("Original translation restored"), Left->GetRenderTransform().Translation.Equals(Rest.Translation));
	TestTrue(TEXT("Original scale preserved"), Left->GetRenderTransform().Scale.Equals(Rest.Scale));
	TestEqual(TEXT("Original angle preserved"), Left->GetRenderTransform().Angle, Rest.Angle);
	TestEqual(TEXT("Original opacity restored"), Left->GetRenderOpacity(), 0.8f);
	TestEqual(TEXT("Original input visibility restored"), Right->GetVisibility(), ESlateVisibility::Visible);

	for (int32 Index = 0; Index < 6; ++Index)
	{
		Motion.SetVisible(false);
		Motion.Advance(0.03f);
		Motion.SetVisible(true);
		Motion.Advance(0.03f);
	}
	Motion.Finish();
	TestTrue(TEXT("Rapid toggles do not accumulate offset"), Left->GetRenderTransform().Translation.Equals(Rest.Translation));
	Motion.SetVisible(false);
	TestFalse(TEXT("A long frame completes hide"), Motion.Advance(1.0f));
	TestEqual(TEXT("Collapse only after fade has finished"), Left->GetVisibility(), ESlateVisibility::Collapsed);
	Motion.SetVisible(true);
	TestEqual(TEXT("Show uncovers the panel before the fade"), Left->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Show begins fully transparent"), Left->GetRenderOpacity(), 0.0f);
	Motion.Advance(0.12f);
	TestTrue(TEXT("Show follows the same path backwards"), Left->GetRenderTransform().Translation.Equals(HalfPosition, 0.001));
	Motion.Forget(Left);
	TestEqual(TEXT("Removing a panel restores its opacity for reuse"), Left->GetRenderOpacity(), 0.8f);
	Motion.Finish();
	TestTrue(TEXT("Removed panels are no longer driven"), Left->GetRenderTransform().Translation.Equals(Rest.Translation));
	Motion.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUFieldHUDMotionVisibilityTest, "ProjectM.UI.FieldHUD.Motion.Visibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUFieldHUDMotionVisibilityTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UOverlay> Group(NewObject<UOverlay>());
	UBorder* Map = NewObject<UBorder>(Group.Get());
	Group->AddChildToOverlay(Map);
	TStrongObjectPtr<UBorder> ClosedTracker(NewObject<UBorder>());
	TStrongObjectPtr<UBorder> ActiveTracker(NewObject<UBorder>());
	TStrongObjectPtr<UBorder> LateChat(NewObject<UBorder>());
	TStrongObjectPtr<UBorder> Toggle(NewObject<UBorder>());
	Toggle->SetVisibility(ESlateVisibility::Visible);
	FNUFieldHUDMotion Motion;
	Motion.Present(Group.Get(), ESlateVisibility::SelfHitTestInvisible, ENUFieldHUDMotionRole::Container);
	Motion.Present(Map, ESlateVisibility::SelfHitTestInvisible, ENUFieldHUDMotionRole::Content);
	Motion.Present(ClosedTracker.Get(), ESlateVisibility::Collapsed);
	Motion.Present(ActiveTracker.Get(), ESlateVisibility::SelfHitTestInvisible);
	Motion.SetVisible(false);
	Motion.Advance(0.06f);
	Map->SetVisibility(ESlateVisibility::Collapsed); // E.g. map image disappears mid-transition.
	Motion.Present(ClosedTracker.Get(), ESlateVisibility::Collapsed); // A quest update must not reveal it.
	Motion.Present(ActiveTracker.Get(), ESlateVisibility::Collapsed); // Last tracked quest removed mid-motion.
	Motion.Advance(0.06f);
	TestEqual(TEXT("Container does not multiply its children's opacity"), Group->GetRenderOpacity(), 1.0f);
	TestTrue(TEXT("Child fades independently"), Map->GetRenderOpacity() < 1.0f);
	TestEqual(TEXT("Missing map stays collapsed during animation"), Map->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Closed tracker stays closed during animation"), ClosedTracker->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Empty tracker stays hidden during animation"), ActiveTracker->GetVisibility(), ESlateVisibility::Collapsed);
	Motion.Finish();
	Motion.Present(LateChat.Get(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Late panel inherits hidden preference without flashing"), LateChat->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Late panel is also transparent"), LateChat->GetRenderOpacity(), 0.0f);
	Motion.SetVisible(true);
	Motion.Finish();
	TestEqual(TEXT("HUD show does not reveal a missing map"), Map->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("HUD show preserves closed tracker state"), ClosedTracker->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("HUD show does not restore an empty tracker"), ActiveTracker->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("Late panel returns normally"), LateChat->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Persistent toggle is untouched"), Toggle->GetVisibility(), ESlateVisibility::Visible);
	Motion.Reset();
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUFieldHUDRequestsTest, "ProjectM.UI.FieldHUD.Requests.Ownership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUFieldHUDRequestsTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UObject> Menu(NewObject<UObject>());
	TStrongObjectPtr<UObject> Cinematic(NewObject<UObject>());
	FNUFieldHUDVisibility State;
	const FName Reason(TEXT("Preview"));
	State.RequestHidden(Menu.Get(), Reason, true);
	State.RequestHidden(Menu.Get(), Reason, true);
	State.RequestHidden(Cinematic.Get(), Reason, true);
	State.RequestHidden(Menu.Get(), Reason, false);
	TestFalse(TEXT("Closing a menu cannot reveal HUD during another owner's cinematic"), State.IsVisible());
	State.RequestHidden(Cinematic.Get(), Reason, false);
	TestTrue(TEXT("Repeated acquire is idempotent; a single release is enough"), State.IsVisible());
	State.RequestHidden(Menu.Get(), Reason, true);
	State.RequestHidden(Menu.Get(), TEXT("SecondReason"), true);
	State.RequestHidden(Menu.Get(), Reason, false);
	TestFalse(TEXT("One owner can hold independent reasons"), State.IsVisible());
	State.RequestHidden(Cinematic.Get(), TEXT("SecondReason"), false);
	TestFalse(TEXT("An unrelated owner cannot release another owner's reason"), State.IsVisible());
	State.ReleaseOwner(Menu.Get());
	TestTrue(TEXT("Owner cleanup releases all its reasons"), State.IsVisible());
	State.RequestHidden(nullptr, Reason, true);
	State.RequestHidden(Menu.Get(), NAME_None, true);
	TestFalse(TEXT("Invalid requests do not leave permanent suppression"), State.HasRequests());
	State.SetUserVisible(false);
	State.RequestHidden(Menu.Get(), Reason, true);
	State.RequestHidden(Menu.Get(), Reason, false);
	TestFalse(TEXT("Closing a temporary feature preserves manual hide"), State.IsVisible());
	State.RequestHidden(Menu.Get(), Reason, true);
	State.SetUserVisible(true);
	TestFalse(TEXT("Manual show cannot override an active feature"), State.IsVisible());
	State.ClearRequests();
	TestTrue(TEXT("Travel removes temporary requests"), State.IsVisible());
	State.SetUserVisible(false);
	State.ClearRequests();
	TestFalse(TEXT("Travel preserves the manual preference"), State.IsUserVisible());
	State.SetUserVisible(true);
	State.RequestHidden(Menu.Get(), Reason, true);
	Menu->MarkAsGarbage();
	State.PruneInvalidOwners();
	TestTrue(TEXT("Destroyed owners cannot strand the HUD hidden"), State.IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUFieldHUDRequestsMotionTest, "ProjectM.UI.FieldHUD.Requests.Motion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUFieldHUDRequestsMotionTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UBorder> Panel(NewObject<UBorder>());
	TStrongObjectPtr<UObject> Menu(NewObject<UObject>());
	TStrongObjectPtr<UObject> Cinematic(NewObject<UObject>());
	FNUFieldHUDVisibility State;
	FNUFieldHUDMotion Motion;
	Motion.Present(Panel.Get(), ESlateVisibility::SelfHitTestInvisible);
	State.RequestHidden(Menu.Get(), TEXT("Menu"), true);
	Motion.SetVisible(State.IsVisible());
	Motion.Advance(0.06f);
	const float OpacityBeforeOverlap = Panel->GetRenderOpacity();
	State.RequestHidden(Cinematic.Get(), TEXT("Cinematic"), true);
	Motion.SetVisible(State.IsVisible());
	TestEqual(TEXT("Overlapping request does not restart fade"), Panel->GetRenderOpacity(), OpacityBeforeOverlap);
	State.ReleaseOwner(Menu.Get());
	Motion.SetVisible(State.IsVisible());
	Motion.Advance(0.06f);
	TestTrue(TEXT("Remaining request continues hiding"), Panel->GetRenderOpacity() < OpacityBeforeOverlap);
	const FVector2D BeforeRelease = Panel->GetRenderTransform().Translation;
	State.ReleaseOwner(Cinematic.Get());
	Motion.SetVisible(State.IsVisible());
	TestTrue(TEXT("Last release reverses without jumping"), Panel->GetRenderTransform().Translation.Equals(BeforeRelease));
	Motion.Finish();
	TestEqual(TEXT("All requests released restores opacity"), Panel->GetRenderOpacity(), 1.0f);
	TestEqual(TEXT("All requests released restores input"), Panel->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	State.SetUserVisible(false);
	Motion.SetVisible(State.IsVisible());
	Motion.Finish();
	State.RequestHidden(Menu.Get(), TEXT("Menu"), true);
	State.ReleaseOwner(Menu.Get());
	Motion.SetVisible(State.IsVisible());
	Motion.Advance(1.0f);
	TestEqual(TEXT("Temporary feature ending does not flash a manually hidden HUD"), Panel->GetVisibility(), ESlateVisibility::Collapsed);
	Motion.Reset();
	return true;
}
#endif
