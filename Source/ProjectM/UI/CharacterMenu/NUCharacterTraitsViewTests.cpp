#include "UI/CharacterMenu/NUCharacterTraitsView.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UI/CharacterMenu/NUCharacterTraitsPage.h"
#include "Components/CanvasPanel.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Slate/WidgetRenderer.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUCharacterTraitsViewTest, "ProjectM.UI.CharacterTraits.View",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUCharacterTraitsViewTest::RunTest(const FString& Parameters)
{
	FNUCharacterTraitsView View;
	View.Offset = FVector2D(-240.f, -100.f);
	View.Zoom = 2.f;
	const FVector2D Anchor(310.f, 180.f);
	const FVector2D PointUnderCursor = (Anchor - View.Offset) / View.Zoom;
	View.ZoomAt(4.f, Anchor);
	TestTrue(TEXT("Cursor keeps the same graph point"), ((Anchor - View.Offset) / View.Zoom).Equals(PointUnderCursor));
	View.ZoomAt(100.f, Anchor);
	TestEqual(TEXT("Maximum zoom"), View.Zoom, 8.f);
	View.ZoomAt(.1f, Anchor);
	TestEqual(TEXT("Minimum zoom"), View.Zoom, 1.f);

	const FVector2D Size(768.f, 420.f);
	const FBox2D Bounds(FVector2D(180.f, 0.f), FVector2D(588.f, 420.f));
	View.Offset = FVector2D(99999.f, -99999.f);
	View.Constrain(Size, Bounds);
	TestTrue(TEXT("Overview stays centered"), View.Offset.Equals(FVector2D::ZeroVector));
	View.Zoom = 4.f;
	View.Offset = FVector2D(99999.f);
	View.Constrain(Size, Bounds);
	TestTrue(TEXT("Pan stops at near edges"), View.Offset.Equals(-Bounds.Min * 4.f));
	View.Offset = FVector2D(-99999.f);
	View.Constrain(Size, Bounds);
	TestTrue(TEXT("Pan stops at far edges"), View.Offset.Equals(Size - Bounds.Max * 4.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNUCharacterTraitsInputTest, "ProjectM.UI.CharacterTraits.Input",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FNUCharacterTraitsInputTest::RunTest(const FString& Parameters)
{
	UClass* PageClass = LoadClass<UNUCharacterTraitsPage>(nullptr,
		TEXT("/Game/Developers/MinkiCho/UI/CharacterMenu/WBP_CharacterTraitsPage.WBP_CharacterTraitsPage_C"));
	if (!TestNotNull(TEXT("Traits WBP uses native presentation controls"), PageClass)) return false;
	TStrongObjectPtr<UNUCharacterTraitsPage> Page(NewObject<UNUCharacterTraitsPage>(GetTransientPackage(), PageClass));
	Page->Initialize();
	const TSharedRef<SWidget> SlatePage = Page->TakeWidget();
	FWidgetRenderer Renderer(true);
	TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(1800, 900), TF_Bilinear, true));
	if (!TestNotNull(TEXT("Offscreen render target"), Target.Get())) return false;
	Renderer.DrawWidget(Target.Get(), SlatePage, 1.5f, FVector2D(1800, 900), 0.f);
	TArray<FColor> Pixels;
	if (!TestTrue(TEXT("Offscreen draw completed"), Target->GameThread_GetRenderTargetResource()->ReadPixels(Pixels))) return false;
	const FGeometry Clip = Page->GraphClip->GetCachedGeometry();
	if (!TestTrue(TEXT("Graph laid out at 150% DPI"), FVector2D(Clip.GetLocalSize()).Equals(FVector2D(768, 420), .1))) return false;
	const FVector2D Cursor = Clip.LocalToAbsolute(FVector2D(384, 210));
	const TSet<FKey> NoButtons;
	const TSet<FKey> RightButton{EKeys::RightMouseButton};
	const FModifierKeysState Modifiers;
	Page->ResetView();
	TestTrue(TEXT("Wheel over graph handled"), Page->NativeOnMouseWheel(Clip,
		FPointerEvent(0, Cursor, Cursor, NoButtons, FKey(), 1.f, Modifiers)).IsEventHandled());
	TestEqual(TEXT("Wheel changes zoom"), Page->GetZoom(), 1.25f);
	const FVector2D Outside = Clip.LocalToAbsolute(FVector2D(900, 210));
	Page->NativeOnMouseWheel(Clip, FPointerEvent(0, Outside, Outside, NoButtons, FKey(), 1.f, Modifiers));
	TestEqual(TEXT("Detail panel wheel leaves graph alone"), Page->GetZoom(), 1.25f);
	for (int32 Index = 0; Index < 4; ++Index) Page->ZoomIn();
	const FVector2D Before = Page->GetViewOffset();
	TestTrue(TEXT("Right drag begins"), Page->NativeOnMouseButtonDown(Clip,
		FPointerEvent(0, Cursor, Cursor, RightButton, EKeys::RightMouseButton, 0.f, Modifiers)).IsEventHandled());
	const FVector2D MovedCursor = Cursor + FVector2D(60, 30);
	Page->NativeOnMouseMove(Clip, FPointerEvent(0, MovedCursor, Cursor, RightButton, FKey(), 0.f, Modifiers));
	TestTrue(TEXT("Drag converts 60/30 screen pixels to 40/20 local units"),
		(Page->GetViewOffset() - Before).Equals(FVector2D(40, 20), .1));
	TestTrue(TEXT("Right release handled"), Page->NativeOnMouseButtonUp(Clip,
		FPointerEvent(0, MovedCursor, MovedCursor, NoButtons, EKeys::RightMouseButton, 0.f, Modifiers)).IsEventHandled());
	TestFalse(TEXT("Release ends dragging"), Page->bPanning);
	const FVector2D After = Page->GetViewOffset();
	Page->NativeOnMouseMove(Clip, FPointerEvent(0, Cursor, MovedCursor, NoButtons, FKey(), 0.f, Modifiers));
	TestTrue(TEXT("Moving after release does not pan"), Page->GetViewOffset().Equals(After));
	return true;
}
#endif
