#include "UI/Marker/NUWorldMarkerComponent.h"

#include "UI/Marker/NUWorldMarkerWidget.h"
#include "UI/NUUISettings.h"

#include "Engine/World.h"
#include "PaperFlipbookComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

UNUWorldMarkerComponent::UNUWorldMarkerComponent()
{
	SetWidgetSpace(EWidgetSpace::World);
	SetBlendMode(EWidgetBlendMode::Masked);
	SetTwoSided(true);
	SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Developers/zxcv1/UI/Materials/M_WorldMarker"));
	MarkerMaterial = Material.Object;
	SetDrawSize(FVector2D(48.0f, 48.0f));
	SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	SetTickMode(ETickMode::Automatic);
	SetTickWhenOffscreen(false);

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
}

FPrimitiveSceneProxy* UNUWorldMarkerComponent::CreateSceneProxy()
{
#if WITH_EDITOR
	// 플레이 전에는 마커 태그가 없으므로 에디터에 빈 위젯 면을 그리지 않는다.
	const UWorld* World = GetWorld();
	if (World == nullptr || !World->IsGameWorld())
	{
		return nullptr;
	}
#endif

	return Super::CreateSceneProxy();
}

void UNUWorldMarkerComponent::BeginPlay()
{
	// 기존 BP에 저장된 Screen 설정도 월드 가림을 따르도록 전환한다.
	SetWidgetSpace(EWidgetSpace::World);
	SetBlendMode(EWidgetBlendMode::Masked);
	SetTwoSided(true);
	SetCastShadow(false);
	ApplyHeadMarkerSettings();
	SetMaterial(0, MarkerMaterial);
	Super::BeginPlay();

	SetVisibility(false);
}

FBoxSphereBounds UNUWorldMarkerComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// 재질이 카메라 방향으로 회전시킨 면도 컬링 범위 안에 포함한다.
	const FBoxSphereBounds WidgetBounds = Super::CalcBounds(LocalToWorld);
	const double Radius = WidgetBounds.SphereRadius + FVector::Distance(WidgetBounds.Origin, LocalToWorld.GetLocation());
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(Radius), Radius);
}

void UNUWorldMarkerComponent::ApplyHeadMarkerSettings()
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	if (Settings == nullptr || Settings->HeadMarkerClass.IsNull())
	{
		return;
	}

	UClass* WidgetClassToUse = Settings->HeadMarkerClass.LoadSynchronous();
	if (WidgetClassToUse == nullptr)
	{
		return;
	}

	SetWidgetClass(WidgetClassToUse);
	if (UMaterialInterface* Material = Settings->HeadMarkerMaterial.LoadSynchronous())
	{
		MarkerMaterial = Material;
		// 반투명 재질은 렌더 타깃 바탕 알파가 0이어야 가장자리가 부드럽게 섞인다
		if (Material->GetBlendMode() != BLEND_Masked && Material->GetBlendMode() != BLEND_Opaque)
		{
			SetBlendMode(EWidgetBlendMode::Transparent);
		}
	}

	// 해상도는 크게, 월드 크기는 설정값으로 --- 위젯 1픽셀 = WorldSize / DrawSize cm
	const FIntPoint Pixels(FMath::Max(1, Settings->HeadMarkerDrawSize.X), FMath::Max(1, Settings->HeadMarkerDrawSize.Y));
	SetDrawSize(FVector2D(Pixels));
	SetPivot(Settings->HeadMarkerPivot);
	SetRelativeScale3D(FVector(Settings->HeadMarkerWorldSize / Pixels.Y));
}

void UNUWorldMarkerComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PlaceTimer += DeltaTime;
	if (PlaceTimer >= 0.5f && IsVisible())
	{
		PlaceTimer = 0.f;
		PlaceAboveOwnerSprite();
	}
}

void UNUWorldMarkerComponent::PlaceAboveOwnerSprite()
{
	const UNUUISettings* Settings = GetDefault<UNUUISettings>();
	const AActor* Owner = GetOwner();
	if (Settings == nullptr || Settings->HeadMarkerGap < 0.f || Settings->HeadMarkerClass.IsNull() || Owner == nullptr)
	{
		return;
	}
	UPaperFlipbookComponent* OwnerSprite = Owner->FindComponentByClass<UPaperFlipbookComponent>();
	if (OwnerSprite == nullptr || OwnerSprite->GetFlipbook() == nullptr)
	{
		return;
	}
	// 스프라이트 칸 꼭대기 = 그림 꼭대기 (NPC 그림은 칸 위쪽 0~5 도트 안에서 시작한다)
	const FBoxSphereBounds SpriteBounds = OwnerSprite->Bounds;
	const double Top = SpriteBounds.Origin.Z + SpriteBounds.BoxExtent.Z;
	FVector Location = GetComponentLocation();
	const double Target = Top + Settings->HeadMarkerGap;
	if (!FMath::IsNearlyEqual(Location.Z, Target, 0.5))
	{
		Location.Z = Target;
		SetWorldLocation(Location);
	}
}

bool UNUWorldMarkerComponent::IsMarkerEnabled() const
{
	return GetNetMode() != NM_DedicatedServer;
}

void UNUWorldMarkerComponent::SetMarkerTag(FGameplayTag NewTag)
{
	MarkerTag = NewTag;
	SetVisibility(MarkerTag.IsValid());
	if (!MarkerTag.IsValid())
	{
		UpdateWidget();
		return;
	}

	// 표시 전환 시 위젯 초기화와 태그 갱신을 보장한다.
	InitWidget();
	UNUWorldMarkerWidget* MarkerWidget = Cast<UNUWorldMarkerWidget>(GetUserWidgetObject());
	if (MarkerWidget == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("WorldMarker: 위젯이 없거나 NUWorldMarkerWidget 파생이 아닙니다 [%s]"),
			GetOwner() != nullptr ? *GetOwner()->GetName() : TEXT("Unknown")
		);

		return;
	}

	MarkerWidget->SetMarkerTag(MarkerTag);
	PlaceAboveOwnerSprite();

	SetComponentTickEnabled(true);
	UpdateWidget();
	RequestRedraw();
}
