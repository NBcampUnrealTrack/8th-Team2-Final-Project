#include "Field/NUCharacterPreviewActor.h"

#include "CharacterProgression/Component/NUJobComponent.h"
#include "CharacterSelect/NUCharacterManager.h"
#include "CharacterState/NUPlayerState.h"
#include "CharacterSelect/NUCharacterTypes.h"
#include "Engine/GameInstance.h"

#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Field/NUSpriteVisualComponent.h"
#include "Field/NUSpriteVisualSettings.h"
#include "PaperFlipbookComponent.h"
#include "Inventory/NUEquipmentComponent.h"
#include "Inventory/NUInventoryTypes.h" // LogNUInventory
#include "PaperFlipbook.h"
#include "PaperZDAnimationComponent.h"

ANUCharacterPreviewActor::ANUCharacterPreviewActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// 루트를 따로 둔다 --- 공용 컴포넌트가 Sprite에 SpritePivotZ(발이 지면에 닿는 보정)를 넣는데,
	// Sprite가 루트면 그 보정이 액터 전체를 옮겨버려 스폰 위치가 무시된다
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Sprite = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Root);
	Sprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 겹침 렌더러 구성은 AFieldPlayerCharacter와 같은 값이어야 한다 --- 여기가 어긋나면
	// 미리보기와 필드가 다르게 보인다. Y는 겹치는 순서(깊이), Z는 파츠 그림의 몸 위치 보정이다
	const auto MakeLayer = [this](const TCHAR* Name, const FVector& Location) -> UPaperFlipbookComponent*
	{
		UPaperFlipbookComponent* Component = CreateDefaultSubobject<UPaperFlipbookComponent>(Name);
		Component->SetupAttachment(Sprite);
		Component->SetRelativeLocation(Location);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetVisibility(false); // 장착 전에는 숨김
		return Component;
	};
	EquipSprite         = MakeLayer(TEXT("EquipSprite"),         FVector(0.0f, 2.0f, -5.0f));
	EquipSpriteGloves   = MakeLayer(TEXT("EquipSpriteGloves"),   FVector(0.0f, 4.0f, -5.0f));
	EquipSpriteBoots    = MakeLayer(TEXT("EquipSpriteBoots"),    FVector(0.0f, 1.0f, -6.0f));
	EquipSpriteLegs     = MakeLayer(TEXT("EquipSpriteLegs"),     FVector(0.0f, 3.0f, -5.0f));
	EquipSpriteShoulder = MakeLayer(TEXT("EquipSpriteShoulder"), FVector(0.0f, 5.0f, -5.0f));
	// 커마 파츠는 Z 0 --- 몸과 같은 캔버스·피벗이라 원점만 맞추면 된다 (장비 -5는 옛 몸 보정).
	// Y는 필드와 같은 값(얼굴 1, 머리 2로 머리가 앞) --- 몸에 붙일수록 시차가 준다 (장비와는 그림이 안 겹쳐 깊이가 같아도 무방)
	HairSprite          = MakeLayer(TEXT("HairSprite"),          FVector(0.0f, 2.0f, 0.0f));
	FaceSprite          = MakeLayer(TEXT("FaceSprite"),          FVector(0.0f, 1.0f, 0.0f));

	Animation = CreateDefaultSubobject<UPaperZDAnimationComponent>(TEXT("Animation"));
	Animation->InitRenderComponent(Sprite);

	// 필드 팩토리에서 머티리얼만 Unlit으로 바꾼다.
	// 필드의 Lit 머티리얼은 빛 방향에 따라 어두워지는데, 미리보기는 자세가 고정이라
	// 면이 태양과 수직에 가까워 실루엣이 된다. 초상화는 그림 원본 색 그대로가 맞다
	FNUSpriteVisualSettings PreviewSettings = NUSpriteVisuals::MakeFieldCharacter();
	PreviewSettings.Material = TSoftObjectPtr<UMaterialInterface>(
		FSoftObjectPath(TEXT("/Paper2D/MaskedUnlitSpriteMaterial.MaskedUnlitSpriteMaterial")));
	PreviewSettings.bCastShadow = false; // 받을 바닥도 없다

	SpriteVisual = CreateDefaultSubobject<UNUSpriteVisualComponent>(TEXT("SpriteVisual"));
	SpriteVisual->Setup(Sprite, Animation, PreviewSettings);
	// 레이어 정의는 NUSpriteVisuals::GetCharacterLayers()가 유일한 출처다 ---
	// 액터마다 숫자를 적으면 한쪽만 고쳐져 어긋난다 (실제로 그런 적이 있다)
	const TArray<UPaperFlipbookComponent*> LayerComponents = {
		EquipSprite, EquipSpriteGloves, EquipSpriteBoots, EquipSpriteLegs, EquipSpriteShoulder,
		HairSprite, FaceSprite };
	const TArray<FNUSpriteLayerDef>& LayerDefs = NUSpriteVisuals::GetCharacterLayers();
	for (int32 i = 0; i < LayerDefs.Num() && i < LayerComponents.Num(); ++i)
	{
		SpriteVisual->RegisterLayerRenderComponent(
			LayerComponents[i], LayerDefs[i].Layer, LayerDefs[i].BounceScale, LayerDefs[i].FrameDelay);
	}

	// 카메라는 +Y 쪽에서 -Y를 바라본다 --- 필드와 같은 방향이다.
	// 장비 레이어의 깊이가 +Y(카메라 쪽)로 쌓이므로, 반대편(-Y)에서 찍으면
	// 장비가 전부 몸 뒤로 넘어간다 (스프라이트가 양면이라 몸은 뒤에서도 보여 헷갈린다)
	Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("Capture"));
	Capture->SetupAttachment(Sprite);

	// 필드 카메라와 같은 구도로 찍는다 --- 피치 -5도, 거리 1000 (SpringArm 값과 동일).
	// 거리가 중요하다: 장비 레이어가 카메라 쪽으로 2~5유닛 떠 있어서, 가까이서 찍으면
	// 앞 레이어가 원근 때문에 몸보다 몇 % 크게 나와 목 경계 같은 데가 어긋나 보인다
	// (거리 104에서 어깨가 4.8% 커졌던 실제 사례). 1000이면 필드처럼 0.5%라 안 보인다.
	// 크기는 FOV를 좁혀 맞춘다 (망원) --- 반폭을 줄일수록 크게 나온다
	Capture->ProjectionType = ECameraProjectionMode::Perspective;
	const float ViewDistance = 1000.0f;
	// 반폭은 "아트 1픽셀 = 렌더타깃 4픽셀"이 정확히 떨어지게 --- 384 / 4 / 2.
	// 새 몸(TestCHBody)이 세로 96픽셀이라 예전 배율(7배, 세로 73픽셀 시야)로는 머리가 잘렸다.
	// 배율 4면 세로 시야가 512/4 = 128픽셀 --- 몸 96픽셀이 위아래 16픽셀 여유를 두고 들어온다.
	// 비정수 배율로 찍으면 픽셀 줄 두께가 들쭉날쭉해져 머리가 커 보이거나
	// 경계선이 한 줄 밀려 보인다. 확대는 여기서 정수로 끝내고, 화면 축소만 아래 필터가 부드럽게 한다
	const float ViewHalfWidth = 384.0f / 4.0f / 2.0f;
	Capture->FOVAngle = 2.0f * FMath::RadiansToDegrees(FMath::Atan(ViewHalfWidth / ViewDistance));
	const FRotator ViewRotation(-5.0f, -90.0f, 0.0f);
	Capture->SetRelativeRotation(ViewRotation);
	// 바라보는 점은 그림 중심 --- 몸이 발(원점)에서 위로 96픽셀이라 절반인 +48
	Capture->SetRelativeLocation(FVector(0.0f, 0.0f, 48.0f) - ViewRotation.Vector() * ViewDistance);

	// 자기 자신만 찍는다 --- 미리보기가 여러 개여도 서로 안 찍히고, 배경도 안 들어온다.
	// 목록에 넣는 것은 BeginPlay에서 한다 (생성자에서 넣으면 CDO에 들어가 인스턴스에 안 붙는다)
	Capture->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;

	// 필요할 때만 찍는다 (장비 변경·애니 프레임). 매 프레임 찍으면 UI 하나에 렌더 패스가 붙는다
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// 조명은 켜 둔다 --- 머티리얼이 Lit이라 조명 플래그를 끄면 빛 0으로 계산돼 실루엣이 된다.
	// 액터가 게임 월드 안에 있으므로 월드 태양광을 그대로 받아 필드와 같은 톤이 된다
	// (ShowOnlyActors는 그리는 물체만 거르고 조명은 거르지 않는다)
	Capture->ShowFlags.SetAtmosphere(false);
	Capture->ShowFlags.SetFog(false);
	Capture->ShowFlags.SetVolumetricFog(false);
	// 픽셀 그림을 뭉개는 후처리 둘을 끈다 (9/29 렌더 타깃을 저장해 하나씩 꺼 보며 확인)
	//   피사계 심도 --- 레벨의 후처리 볼륨에 심도가 있으면(마을: 초점 1900 · 조리개 2.4) 캡처에도 걸린다.
	//     이 캡처는 시야 5.5도의 망원이라 심도가 아주 얕아 그림 전체가 흐려졌다 (흐림의 주원인)
	//   안티에일리어싱 --- TSR이 가장자리를 주변과 섞어 윤곽이 번진다. 끄면 알파가 0 아니면 255로 떨어진다
	Capture->ShowFlags.SetDepthOfField(false);
	Capture->ShowFlags.SetAntiAliasing(false);
	Capture->bAlwaysPersistRenderingState = true;

	// 노출은 자동에 맡긴다 --- 수동(AEM_Manual)은 물리 카메라 기본값 기준이라 설정 없이 켜면
	// 화면이 거의 검게 나온다 (실제로 당했다). 그레인·비네트·블룸만 끈다
	Capture->PostProcessSettings.bOverride_FilmGrainIntensity = true;
	Capture->PostProcessSettings.FilmGrainIntensity = 0.0f;
	Capture->PostProcessSettings.bOverride_VignetteIntensity = true;
	Capture->PostProcessSettings.VignetteIntensity = 0.0f;
	Capture->PostProcessSettings.bOverride_BloomIntensity = true;
	Capture->PostProcessSettings.BloomIntensity = 0.0f;
}

void ANUCharacterPreviewActor::BeginPlay()
{
	Super::BeginPlay();

	// 세로로 긴 칸이라 3:4. 픽셀아트라 필터는 최근접으로 둬야 확대해도 뭉개지지 않는다
	RenderTarget = NewObject<UTextureRenderTarget2D>(this);
	RenderTarget->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor::Transparent;
	// 캡처가 이미 정수 배율(7배)의 또렷한 픽셀을 담고 있으므로, 화면에 맞춰 줄일 때만
	// 부드럽게 섞는다 (픽셀아트 축소의 정석) --- 최근접으로 줄이면 줄이 불규칙하게 사라진다
	RenderTarget->Filter = TF_Bilinear;
	RenderTarget->bAutoGenerateMips = false;
	RenderTarget->InitAutoFormat(384, 512);
	RenderTarget->UpdateResourceImmediate(true);
	Capture->TextureTarget = RenderTarget;

	// 이 액터만 찍는다 (생성자가 아니라 여기서 --- 인스턴스 기준으로 넣어야 한다)
	Capture->ShowOnlyActors.Reset();
	Capture->ShowOnlyActors.Add(this);

	// 정면 Idle 고정 --- 미리보기는 걷지 않는다
	if (IsValid(SpriteVisual))
	{
		SpriteVisual->SetLocomotion(EFieldFacing::Down, false);
	}

	UE_LOG(LogNUInventory, Log,
		TEXT("미리보기 액터 준비 --- 위치 %s / 캡처 상대 %s / OrthoWidth %.0f / 렌더타깃 %dx%d"),
		*GetActorLocation().ToCompactString(), *Capture->GetRelativeLocation().ToCompactString(),
		Capture->OrthoWidth, RenderTarget->SizeX, RenderTarget->SizeY);
	UE_LOG(LogNUInventory, Log,
		TEXT("미리보기 배치 --- 스프라이트 월드 %s / 캡처 월드 %s 방향 %s / 스프라이트 바운드 %s"),
		*Sprite->GetComponentLocation().ToCompactString(),
		*Capture->GetComponentLocation().ToCompactString(),
		*Capture->GetForwardVector().ToCompactString(),
		*Sprite->Bounds.GetBox().ToString());
}

void ANUCharacterPreviewActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsValid(SpriteVisual) || !IsValid(Capture) || !Capture->TextureTarget)
	{
		return;
	}

	// 프레임이 바뀔 때만 찍는다 --- 정지 화면을 초당 60번 다시 그릴 이유가 없다
	const UPaperFlipbook* Flipbook = Sprite != nullptr ? Sprite->GetFlipbook() : nullptr;
	// 첫 틱에는 애님이 아직 안 붙어 있다 --- 잠깐 기다렸다가도 없으면 그때 알린다
	if (Flipbook == nullptr)
	{
		TimeWithoutFlipbook += DeltaSeconds;
		if (TimeWithoutFlipbook > 1.0f && !bLoggedMissingFlipbook)
		{
			bLoggedMissingFlipbook = true;
			UE_LOG(LogNUInventory, Warning,
				TEXT("미리보기 몸통 플립북 없음 --- 애님 BP가 안 붙었을 수 있습니다"));
		}
	}
	else if (TimeWithoutFlipbook > 0.f)
	{
		TimeWithoutFlipbook = 0.f;
		UE_LOG(LogNUInventory, Log, TEXT("미리보기 몸통 플립북 연결됨 [%s]"), *GetNameSafe(Flipbook));
	}
	const int32 Frame = (Flipbook != nullptr && Sprite != nullptr)
		? Flipbook->GetKeyFrameIndexAtTime(Sprite->GetPlaybackPosition(), true)
		: 0;

	// 지연 캡처 카운트다운 (RefreshEquipment가 건다) --- 스킨 적용 뒤에 찍기 위함
	const bool bPendingDue = PendingCaptureTicks > 0 && --PendingCaptureTicks == 0;

	// 프레임이 바뀌었거나, 스킨이 몸 플립북을 갈아끼웠거나, 지연 캡처 차례면 찍는다
	if (Frame != LastCapturedFrame || Flipbook != LastCapturedFlipbook.Get() || bPendingDue)
	{
		LastCapturedFrame = Frame;
		LastCapturedFlipbook = Flipbook;
		Capture->CaptureScene();
	}
}

void ANUCharacterPreviewActor::RefreshEquipment(const UNUEquipmentComponent* Equipment,
	UNUInventoryComponent* Inventory)
{
	if (!IsValid(SpriteVisual))
	{
		return;
	}
	SpriteVisual->RefreshEquipmentSkins(Equipment, Inventory);

	// 커마도 같이 --- 초상화가 필드의 내 모습과 달라 보이면 안 된다.
	// 장비가 바뀔 때만 불리지만 커마는 그 사이 안 바뀌므로 같이 갱신해도 손해가 없다
	const UGameInstance* GI = GetGameInstance();
	const UNUCharacterManager* Characters = GI != nullptr ? GI->GetSubsystem<UNUCharacterManager>() : nullptr;
	FNUCharacterSummary Selected;
	if (Characters != nullptr && Characters->GetSelectedOrTestCharacter(Selected))
	{
		// 직업 몸은 직업 컴포넌트가 정본(전직하면 바뀐다) --- 장비 컴포넌트의 주인이 곧 PlayerState다. 아직 안 받았으면 목록 값
		const ANUPlayerState* NUState = Equipment != nullptr ? Cast<ANUPlayerState>(Equipment->GetOwner()) : nullptr;
		const UNUJobComponent* Job = NUState != nullptr ? NUState->GetJobComponent() : nullptr;
		const int32 JobId = Job != nullptr && Job->HasJob() ? Job->GetJobId() : Selected.JobId;
		SpriteVisual->RefreshAppearanceSkins(Selected.RaceId, Selected.Gender, Selected.HairId, Selected.FaceId, JobId);
	}

	// 장비·커마 스킨은 다음 애님 틱에 렌더러로 들어간다 --- 즉시 찍으면 적용 전 그림이 담기므로
	// 두 틱 기다렸다 찍는다 (Tick의 카운트다운). 몸이 정지 그림이라 프레임 변화로는 재촬영이 안 온다
	PendingCaptureTicks = 2;
}

#if WITH_EDITOR
static FAutoConsoleCommand GDumpPreviewCommand(
	TEXT("nu.Preview.Dump"),
	TEXT("미리보기 렌더 타깃을 Saved/preview_dump.png로 저장한다 (캡처 내용 확인용)"),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		for (TActorIterator<ANUCharacterPreviewActor> It(World); It; ++It)
		{
			UTextureRenderTarget2D* Target = It->GetRenderTarget();
			if (Target == nullptr)
			{
				continue;
			}
			FImage Image;
			if (!FImageUtils::GetRenderTargetImage(Target, Image))
			{
				UE_LOG(LogNUInventory, Warning, TEXT("미리보기 덤프 실패 --- 렌더 타깃을 읽을 수 없음"));
				continue;
			}
			TArray64<uint8> Png;
			if (FImageUtils::CompressImage(Png, TEXT("png"), Image))
			{
				const FString Path = FPaths::ProjectSavedDir() / TEXT("preview_dump.png");
				FFileHelper::SaveArrayToFile(Png, *Path);
				UE_LOG(LogNUInventory, Log, TEXT("미리보기 덤프 저장 --- %s"), *Path);
			}
			return; // 첫 액터만
		}
		UE_LOG(LogNUInventory, Warning, TEXT("미리보기 덤프 실패 --- 액터가 없음 (정보창을 먼저 여세요)"));
	}));
#endif

void ANUCharacterPreviewActor::SetCaptureEnabled(bool bEnabled)
{
	SetActorTickEnabled(bEnabled);
	if (bEnabled && IsValid(Capture) && Capture->TextureTarget != nullptr)
	{
		LastCapturedFrame = INDEX_NONE; // 다시 열 때 한 번은 찍게 한다
		Capture->CaptureScene();
	}
}
