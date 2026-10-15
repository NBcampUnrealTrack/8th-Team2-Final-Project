#include "Field/NUCameraOcclusionFadeComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Field/FieldPlayerCharacter.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"

namespace
{
	// MPC 파라미터 이름, MF_CameraOcclusionFade 안의 Collection Parameter 와 같아야 함
	const FName PARAM_CHARACTER_OFFSET = TEXT("CharacterOffset");
	const FName PARAM_FADE_RADIUS = TEXT("FadeRadius");
	const FName PARAM_FADE_STRENGTH = TEXT("FadeStrength");
}

UNUCameraOcclusionFadeComponent::UNUCameraOcclusionFadeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UNUCameraOcclusionFadeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (FadeCollection.IsNull())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] 카메라 페이드: MPC 가 지정되지 않음 - 페이드가 동작하지 않음"));
		return;
	}

	LoadedCollection = FadeCollection.LoadSynchronous();
	if (!IsValid(LoadedCollection))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[NU] 카메라 페이드: MPC 를 불러오지 못함 (%s)"), *FadeCollection.ToString());
		return;
	}

	ValidateCollection();
}

void UNUCameraOcclusionFadeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 폰이 사라진 뒤에도 값이 남아 소품이 계속 뚫려 있지 않게 세기를 0 으로 내려 둠
	if (IsValid(LoadedCollection))
	{
		PushFadeParameters(FVector::ZeroVector, 0.f, 0.f);
	}

	Super::EndPlay(EndPlayReason);
}

void UNUCameraOcclusionFadeComponent::SetFadeEnabled(bool bEnabled)
{
	bFadeEnabled = bEnabled;
}

void UNUCameraOcclusionFadeComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsValid(LoadedCollection))
	{
		return;
	}

	const AFieldPlayerCharacter* OwnerCharacter = Cast<AFieldPlayerCharacter>(GetOwner());
	if (!IsValid(OwnerCharacter) || !IsValid(OwnerCharacter->Camera))
	{
		return;
	}

	// 전투에 들어가면 필드 폰이 숨겨잠, 그동안 카메라는 전투 쪽에
	// 있는데 여기 값은 필드 것이라 켜 둘 이유가 없음
	const bool bActive = bFadeEnabled && !OwnerCharacter->IsHidden();
	const float TargetStrength = bActive ? FMath::Clamp(1.f - MinFade, 0.f, 1.f) : 0.f;
	CurrentStrength = FMath::FInterpTo(CurrentStrength, TargetStrength, DeltaTime, EnableInterpSpeed);

	const FVector CameraPosition = OwnerCharacter->Camera->GetComponentLocation();
	const FVector CharacterPosition = OwnerCharacter->GetActorLocation() + FVector(0.f, 0.f, ChestHeightOffset);

	// 딛고 선 바닥의 높이, 머티리얼이 이보다 낮은 픽셀은 걷어내지 않음
	// 이 값만 카메라를 빼지 않고 월드 높이로 넘김, 여기서 읽는 카메라 위치는 스프링 암의 지연이 들어가기 전이라
	// 계단을 오르는 프레임에는 화면의 카메라와 한 단만큼 어긋나고 바닥 판정은 그 차이를 못 견딤
	const float FloorZ = static_cast<float>(OwnerCharacter->GetActorLocation().Z)
		- OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + FloorMargin;

	// 머티리얼이 픽셀 위치를 카메라 기준으로 읽으므로 여기서도 카메라를 뺀 값을 넘김
	PushFadeParameters(CharacterPosition - CameraPosition, FloorZ, CurrentStrength);

#if ENABLE_DRAW_DEBUG
	if (bDebugDrawCone)
	{
		// 머티리얼이 실제로 마스크하는 모양이 이 원뿔임
		// 카메라에서 반경 0 으로 시작해 캐릭터 자리에서 FadeRadius 가 됨
		const FVector ToCharacter = CharacterPosition - CameraPosition;
		const double Length = ToCharacter.Size();
		if (Length > KINDA_SMALL_NUMBER)
		{
			const float Angle = FMath::Atan2(FadeRadius, static_cast<float>(Length));
			DrawDebugCone(
				GetWorld(),
				CameraPosition,
				ToCharacter / Length, 
				static_cast<float>(Length),
				Angle,
				Angle, 
				16, 
				FColor::Cyan, 
				false,
				-1.f,
				0, 
				1.f
				);
		}
	}
#endif
}

void UNUCameraOcclusionFadeComponent::PushFadeParameters(const FVector& CharacterOffset, float FloorZ, float Strength)
{
	UKismetMaterialLibrary::SetVectorParameterValue(this, LoadedCollection, PARAM_CHARACTER_OFFSET,
		FLinearColor(
			static_cast<float>(CharacterOffset.X),
			static_cast<float>(CharacterOffset.Y),
			static_cast<float>(CharacterOffset.Z),
			FloorZ)
			);

	UKismetMaterialLibrary::SetScalarParameterValue(this, LoadedCollection, PARAM_FADE_RADIUS, FadeRadius);
	UKismetMaterialLibrary::SetScalarParameterValue(this, LoadedCollection, PARAM_FADE_STRENGTH, Strength);
}

void UNUCameraOcclusionFadeComponent::ValidateCollection() const
{
	const TArray<FName> VectorNames = LoadedCollection->GetVectorParameterNames();
	const TArray<FName> ScalarNames = LoadedCollection->GetScalarParameterNames();

	for (const FName& Required : { PARAM_CHARACTER_OFFSET })
	{
		if (!VectorNames.Contains(Required))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[NU] 카메라 페이드: %s 에 Vector 파라미터 %s 가 없음 - 페이드가 동작하지 않음"),
				*GetNameSafe(LoadedCollection), *Required.ToString());
		}
	}

	for (const FName& Required : { PARAM_FADE_RADIUS, PARAM_FADE_STRENGTH })
	{
		if (!ScalarNames.Contains(Required))
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[NU] 카메라 페이드: %s 에 Scalar 파라미터 %s 가 없음 - 페이드가 동작하지 않음"),
				*GetNameSafe(LoadedCollection), *Required.ToString());
		}
	}
}
