// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Map/NU_MapImageViewWidget.h"

#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/LevelStreaming.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Field/FieldPlayerCharacter.h"
#include "NativeGameplayTags.h"
#include "NUGAS/NUGameplayTags.h"
#include "TimerManager.h"
#include "UI/Marker/NUWorldMarkerComponent.h"
#include "UI/Marker/NUWorldMarkerWidget.h"
#include "UObject/UObjectIterator.h"
#include "WorldTransition/World/NUWorldPortal.h"
#include "WorldTransition/World/NUWorldTransitionSubsystem.h"

// 지도 전용 아이콘 태그. 월드에는 포탈 마커가 없어서 지도만 쓴다
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NU_Marker_Map_Portal, "NU.Marker.Map.Portal");

namespace NUMapView
{
	// 공간 확인·아이콘 갱신 주기. 공간 이동은 암전 중이라 반 박자 늦어도 안 보인다
	constexpr float RefreshInterval = 0.5f;
}

UNU_MapImageViewWidget::UNU_MapImageViewWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EdgeTags.AddTag(NU::Marker::QuestTurnIn);
	EdgeTags.AddTag(NU::Marker::QuestTarget);
	EdgeTags.AddTag(NU::Marker::QuestOffer);
}

void UNU_MapImageViewWidget::InitializeWidget()
{
	Super::InitializeWidget();

	bHasEntry = false;
	bSpaceResolved = false;
	CachedLevel.Reset();
	if (Marker_Player != nullptr)
	{
		if (UCanvasPanelSlot* PlayerSlot = Cast<UCanvasPanelSlot>(Marker_Player->Slot))
		{
			PlayerSlot->SetAutoSize(true);
		}
	}

	// 첫 판정은 바로 한다 --- 창을 열자마자 그림이 있어야 한다
	HandleRefreshTimer();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(RefreshTimer, this, &ThisClass::HandleRefreshTimer, NUMapView::RefreshInterval, true);
	}
}

void UNU_MapImageViewWidget::ReleaseWidget()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimer);
	}
	Super::ReleaseWidget();
}

void UNU_MapImageViewWidget::HandleRefreshTimer()
{
	// 폰은 위젯 생성 시점에 한 번만 캐시된다 --- 빙의보다 먼저 만들어진 HUD 안에서도 맞도록 주기마다 다시 잡는다
	CacheNUPawn();
	RefreshSpace();
	RefreshIconMarkers();
}

void UNU_MapImageViewWidget::RefreshSpace()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// 옮겨 붙은 공간(사냥터)은 전환 시스템이 안다. 폰이 아직 없으면 월드의 기본 레벨로 본다
	APawn* Pawn = GetNUPawn();
	const UNUWorldTransitionSubsystem* Transition = World->GetSubsystem<UNUWorldTransitionSubsystem>();
	const bool bUseTransition = Transition != nullptr && Pawn != nullptr;
	const ULevel* Level = bUseTransition ? Transition->GetActiveLevel(Pawn) : World->PersistentLevel.Get();
	if (Level == nullptr)
	{
		return; // 목적 공간을 준비하는 중 --- 다음 주기에 다시 본다
	}
	if (bSpaceResolved && Level == CachedLevel.Get())
	{
		return;
	}
	bSpaceResolved = true;
	CachedLevel = Level;

	LevelToWorld = FTransform::Identity;
	const FString PackageName = bUseTransition
		? Transition->GetActiveMapPackageName(Pawn)
		: UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
	if (bUseTransition && Level != World->PersistentLevel)
	{
		if (const ULevelStreaming* Streaming = Transition->GetActiveStreamingLevel())
		{
			LevelToWorld = Streaming->LevelTransform;
		}
	}

	const bool bHadEntry = bHasEntry;
	const FNUMapImageEntry* Entry = GetDefault<UNUMapImageSettings>()->FindForPackage(PackageName);
	// 공간 이동 때만 불려서 동기 로드로 충분하다 (UI 그룹 텍스처 한 장)
	UTexture2D* Texture = Entry != nullptr ? Entry->Texture.LoadSynchronous() : nullptr;
	bHasEntry = Texture != nullptr;

	if (!bHasEntry)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		CachedEntry = *Entry;
		LastPlayerUV = FVector2D(-1.0f, -1.0f);
		Img_Map->SetBrushFromTexture(Texture);
		// 줌을 쓰는 인스턴스만 마우스를 받는다 --- 미니맵은 클릭이 그대로 게임으로 지나가야 한다
		SetVisibility(bEnableZoom ? ESlateVisibility::Visible : ESlateVisibility::SelfHitTestInvisible);
		if (bEnableZoom || bCenterOnPlayer)
		{
			SetRenderTransformPivot(FVector2D::ZeroVector);
			Zoom = bCenterOnPlayer ? FMath::Max(CenterZoom, 1.0f) : 1.0f;
			Pan = FVector2D::ZeroVector;
			SetRenderScale(FVector2D(Zoom, Zoom));
			SetRenderTranslation(FVector2D::ZeroVector);
		}
		ApplyMarkerScale();
	}

	if (bHadEntry != bHasEntry)
	{
		OnMapImageChanged.Broadcast();
	}
}

void UNU_MapImageViewWidget::RefreshIconMarkers()
{
	int32 Used = 0;
	UWorld* World = GetWorld();
	if (bHasEntry && IconMarkerClass != nullptr && World != nullptr)
	{
		const auto Place = [this, &Used](const FVector& WorldLocation, const FGameplayTag& Tag)
		{
			const FVector2D UV = WorldToUV(WorldLocation);
			if (UV.X < 0.0f || UV.X > 1.0f || UV.Y < 0.0f || UV.Y > 1.0f)
			{
				return; // 다른 공간의 것 (사냥터에서 본 마을 NPC 등) --- 자리를 쓰지 않는다
			}

			if (!IconMarkers.IsValidIndex(Used))
			{
				UNUWorldMarkerWidget* NewIcon = CreateWidget<UNUWorldMarkerWidget>(this, IconMarkerClass);
				UCanvasPanelSlot* IconSlot = NewIcon != nullptr ? Canvas_Markers->AddChildToCanvas(NewIcon) : nullptr;
				if (IconSlot == nullptr)
				{
					return;
				}
				IconSlot->SetAutoSize(false);
				IconSlot->SetSize(IconMarkerSize);
				IconSlot->SetZOrder(-1); // 플레이어 마커가 늘 위에 보이게
				NewIcon->SetRenderScale(FVector2D(1.0f / Zoom));
				IconMarkers.Add(NewIcon);
				IconUVs.Add(UV);
			}

			const int32 Index = Used++;
			UNUWorldMarkerWidget* Icon = IconMarkers[Index];
			if (Icon->GetMarkerTag() != Tag)
			{
				Icon->SetMarkerTag(Tag);
			}
			IconUVs[Index] = UV;
			PlaceIcon(Index);
		};

		// 지역 이동 포탈만 --- 건물 출입구(장소 이동)는 지도에 그려진 건물로 충분하다
		for (TActorIterator<ANUWorldPortal> It(World); It; ++It)
		{
			if (!It->UsesLocationMove())
			{
				Place(It->GetActorLocation(), TAG_NU_Marker_Map_Portal);
			}
		}

		// 월드 마커(NPC 머리 위 퀘스트 표시)가 정한 태그를 그대로 비춘다
		for (TObjectIterator<UNUWorldMarkerComponent> It; It; ++It)
		{
			const UNUWorldMarkerComponent* WorldMarker = *It;
			if (!IsValid(WorldMarker) || WorldMarker->GetWorld() != World || WorldMarker->GetOwner() == nullptr)
			{
				continue;
			}
			const FGameplayTag Tag = WorldMarker->GetMarkerTag();
			if (Tag.IsValid())
			{
				Place(WorldMarker->GetOwner()->GetActorLocation(), Tag);
			}
		}
	}

	for (int32 Index = Used; Index < IconMarkers.Num(); ++Index)
	{
		IconMarkers[Index]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UNU_MapImageViewWidget::PlaceIcon(int32 Index)
{
	UNUWorldMarkerWidget* Icon = IconMarkers.IsValidIndex(Index) ? IconMarkers[Index].Get() : nullptr;
	if (Icon == nullptr || !IconUVs.IsValidIndex(Index))
	{
		return;
	}

	FVector2D UV = IconUVs[Index];
	bool bOnEdge = false;
	float Angle = 0.0f;
	if (bCenterOnPlayer && LastLocalSize.X > 1.0f && LastLocalSize.Y > 1.0f && EdgeTags.HasTagExact(Icon->GetMarkerTag()))
	{
		// 창에 보이는 지도 범위(UV). 화면점 = Pan + Zoom * 콘텐츠점, 콘텐츠점 = UV * 조각 크기
		const FVector2D Span = LastLocalSize * Zoom;
		const FVector2D Inset(EdgeInset / Span.X, EdgeInset / Span.Y);
		const FVector2D Min = -Pan / Span + Inset;
		const FVector2D Max = (LastLocalSize - Pan) / Span - Inset;
		if (Min.X < Max.X && Min.Y < Max.Y && (UV.X < Min.X || UV.X > Max.X || UV.Y < Min.Y || UV.Y > Max.Y))
		{
			// 플레이어에서 아이콘 쪽으로 그은 선이 테두리와 만나는 자리
			const FVector2D Center = LastPlayerUV.X >= 0.0f ? LastPlayerUV : (Min + Max) * 0.5f;
			const FVector2D From(FMath::Clamp(Center.X, Min.X, Max.X), FMath::Clamp(Center.Y, Min.Y, Max.Y));
			const FVector2D Dir = UV - From;
			float T = 1.0f;
			if (Dir.X > KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Max.X - From.X) / Dir.X); }
			if (Dir.X < -KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Min.X - From.X) / Dir.X); }
			if (Dir.Y > KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Max.Y - From.Y) / Dir.Y); }
			if (Dir.Y < -KINDA_SMALL_NUMBER) { T = FMath::Min(T, (Min.Y - From.Y) / Dir.Y); }
			UV = From + Dir * FMath::Max(T, 0.0f);
			bOnEdge = true;

			// 화면 방향 (오른쪽 +, 아래 +) --- 위 = 0도, 시계 방향
			const FVector2D ScreenDir = Dir * LastLocalSize;
			Angle = FMath::RadiansToDegrees(FMath::Atan2(ScreenDir.X, -ScreenDir.Y));
		}
	}

	Icon->SetEdgeDirection(bOnEdge, Angle);
	PlaceMarkerAtUV(Icon, UV);
}

void UNU_MapImageViewWidget::UpdatePlayerHeading(const APawn* Pawn, float DeltaTime)
{
	float Target = PlayerHeading;
	FVector Velocity = Pawn->GetVelocity();
	Velocity.Z = 0.0;
	if (Velocity.SizeSquared() > FMath::Square(20.0))
	{
		// 지도 오른쪽 = -X, 위 = +Y (WorldToMapUV 와 같은 축). 사냥터는 레벨 배치를 되돌린 축으로 본다
		const FVector Local = LevelToWorld.InverseTransformVectorNoScale(Velocity);
		Target = FMath::RadiansToDegrees(FMath::Atan2(-Local.X, Local.Y));
	}
	else if (!bPlayerHeadingSet)
	{
		// 서 있을 때 처음 방향은 캐릭터가 바라보는 쪽 (카메라가 고정이라 화면 방향 = 지도 방향)
		if (const AFieldPlayerCharacter* Field = Cast<AFieldPlayerCharacter>(Pawn))
		{
			switch (Field->Facing)
			{
			case EFieldFacing::Up: Target = 0.0f; break;
			case EFieldFacing::Right: Target = 90.0f; break;
			case EFieldFacing::Left: Target = -90.0f; break;
			default: Target = 180.0f; break;
			}
		}
	}

	if (!bPlayerHeadingSet)
	{
		PlayerHeading = Target;
		bPlayerHeadingSet = true;
	}
	else
	{
		PlayerHeading += FMath::FindDeltaAngleDegrees(PlayerHeading, Target) * FMath::Min(1.0f, DeltaTime * PlayerTurnSpeed);
		PlayerHeading = FMath::UnwindDegrees(PlayerHeading);
	}

	if (FMath::Abs(FMath::FindDeltaAngleDegrees(AppliedPlayerHeading, PlayerHeading)) > 0.25f)
	{
		AppliedPlayerHeading = PlayerHeading;
		Marker_Player->SetRenderTransformAngle(PlayerHeading);
	}
}

FVector2D UNU_MapImageViewWidget::WorldToUV(const FVector& WorldLocation) const
{
	return UNUMapImageSettings::WorldToMapUV(CachedEntry, LevelToWorld.InverseTransformPosition(WorldLocation));
}

void UNU_MapImageViewWidget::ApplyMarkerScale()
{
	const FVector2D MarkerScale(1.0f / Zoom);
	if (Marker_Player != nullptr)
	{
		Marker_Player->SetRenderScale(MarkerScale);
	}
	for (UNUWorldMarkerWidget* Icon : IconMarkers)
	{
		Icon->SetRenderScale(MarkerScale);
	}
}

void UNU_MapImageViewWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bHasEntry || Marker_Player == nullptr)
	{
		return;
	}

	APawn* Pawn = GetNUPawn();
	if (Pawn == nullptr)
	{
		return;
	}

	LastLocalSize = MyGeometry.GetLocalSize();
	if (bRotatePlayerMarker)
	{
		UpdatePlayerHeading(Pawn, InDeltaTime);
	}

	const FVector2D UV = WorldToUV(Pawn->GetActorLocation());
	// 0.0005 는 1024 텍스처에서 지도 반 픽셀 --- 그보다 작은 이동은 화면에 안 보인다
	if (!UV.Equals(LastPlayerUV, 0.0005f))
	{
		LastPlayerUV = UV;
		PlaceMarkerAtUV(Marker_Player, UV);

		// 미니맵 모드: 플레이어 지점이 창 중앙에 오도록 지도를 민다. 지도 끝에서는 가장자리에 붙는다
		if (bCenterOnPlayer)
		{
			const FVector2D LocalSize = MyGeometry.GetLocalSize();
			Pan = LocalSize * 0.5f - UV * LocalSize * Zoom;
			ClampPan(LocalSize);
			SetRenderTranslation(Pan);

			// 창이 움직였으니 가장자리에 붙는 아이콘을 다시 놓는다 (몇 개뿐이라 매번 해도 싸다)
			if (!EdgeTags.IsEmpty())
			{
				for (int32 Index = 0; Index < IconMarkers.Num(); ++Index)
				{
					if (IconMarkers[Index]->GetVisibility() != ESlateVisibility::Collapsed && EdgeTags.HasTagExact(IconMarkers[Index]->GetMarkerTag()))
					{
						PlaceIcon(Index);
					}
				}
			}
		}
	}
}

void UNU_MapImageViewWidget::PlaceMarkerAtWorld(UWidget* Marker, const FVector& WorldLocation)
{
	if (!bHasEntry || Marker == nullptr)
	{
		return;
	}
	PlaceMarkerAtUV(Marker, WorldToUV(WorldLocation));
}

void UNU_MapImageViewWidget::PlaceMarkerAtUV(UWidget* Marker, const FVector2D& UV)
{
	const bool bInside = UV.X >= 0.0f && UV.X <= 1.0f && UV.Y >= 0.0f && UV.Y <= 1.0f;
	Marker->SetVisibility(bInside ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bInside)
	{
		return;
	}

	// 앵커를 UV 로 쓰면 캔버스(=지도) 크기가 바뀌어도 마커가 같은 지점에 붙는다. 크기는 슬롯에 둔 값 그대로
	if (UCanvasPanelSlot* MarkerSlot = Cast<UCanvasPanelSlot>(Marker->Slot))
	{
		MarkerSlot->SetAnchors(FAnchors(UV.X, UV.Y));
		MarkerSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		MarkerSlot->SetPosition(FVector2D::ZeroVector);
	}
}

FReply UNU_MapImageViewWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bEnableZoom || !bHasEntry)
	{
		return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
	}

	const float NewZoom = FMath::Clamp(Zoom * (InMouseEvent.GetWheelDelta() > 0.0f ? 1.2f : 1.0f / 1.2f), 1.0f, MaxZoom);
	// AbsoluteToLocal 은 렌더 변환까지 되돌리므로 결과는 콘텐츠(1배) 좌표다
	ApplyZoom(InGeometry, NewZoom, InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition()));
	return FReply::Handled();
}

FReply UNU_MapImageViewWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 확대 중일 때만 팬을 잡는다. 1배에서는 창 드래그(BaseSheet)가 받도록 흘려보낸다
	if (bEnableZoom && Zoom > 1.0f && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPanning = true;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UNU_MapImageViewWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bPanning)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	// 스크린 델타를 콘텐츠 좌표로 바꾼 뒤 화면(레이아웃) 단위로 팬에 싣는다
	const FVector2D ContentDelta = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())
		- InGeometry.AbsoluteToLocal(InMouseEvent.GetLastScreenSpacePosition());
	Pan += ContentDelta * Zoom;
	ClampPan(InGeometry.GetLocalSize());
	SetRenderTranslation(Pan);
	return FReply::Handled();
}

FReply UNU_MapImageViewWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bPanning && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

void UNU_MapImageViewWidget::ApplyZoom(const FGeometry& Geometry, float NewZoom, const FVector2D& PivotLocal)
{
	const FVector2D ScreenPoint = Pan + PivotLocal * Zoom;
	Zoom = NewZoom;
	Pan = ScreenPoint - PivotLocal * Zoom;
	ClampPan(Geometry.GetLocalSize());
	SetRenderScale(FVector2D(Zoom, Zoom));
	SetRenderTranslation(Pan);
	ApplyMarkerScale();
}

void UNU_MapImageViewWidget::ClampPan(const FVector2D& LocalSize)
{
	Pan.X = FMath::Clamp(Pan.X, LocalSize.X * (1.0f - Zoom), 0.0f);
	Pan.Y = FMath::Clamp(Pan.Y, LocalSize.Y * (1.0f - Zoom), 0.0f);
}
