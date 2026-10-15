#include "Field/NUSpriteFrameOffset.h"

#include "Engine/Texture2D.h"
#include "Field/NUFrameBounceSettings.h"
#include "PaperFlipbook.h"
#include "PaperSprite.h"

#if WITH_EDITOR
#include "Engine/ObjectLibrary.h"
#include "ImageCore.h"
#endif

namespace
{
	// 플립북별 측정 결과. 같은 플립북을 매 프레임 다시 재지 않게 들고 있는다
	TMap<FObjectKey, TArray<float>> GVerticalOffsetCache;

	// 알파가 이 값보다 크면 그림으로 친다 (픽셀아트는 0 아니면 255라 경계값은 넉넉해도 된다)
	constexpr uint8 GOpaqueAlphaThreshold = 8;

	// 스프라이트가 쓰는 텍스처 영역에서 그림이 처음 나타나는 행(위에서부터, 영역 기준).
	// 소스 텍스처를 못 읽거나 전부 투명이면 false
	bool FindTopOpaqueRow(const UPaperSprite* Sprite, int32& OutRow)
	{
#if WITH_EDITOR
		UTexture2D* Texture = Sprite->GetSourceTexture();
		if (!IsValid(Texture) || !Texture->Source.IsValid())
		{
			return false;
		}

		FImage Mip;
		if (!Texture->Source.GetMipImage(Mip, 0))
		{
			return false;
		}

		// 임포트 포맷이 무엇이든 BGRA8로 맞춘다 --- 여기서 보는 건 알파뿐이라 감마는 무관
		FImage Converted;
		if (Mip.Format != ERawImageFormat::BGRA8)
		{
			Mip.CopyTo(Converted, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
		}
		const FImage& Image = Mip.Format == ERawImageFormat::BGRA8 ? Mip : Converted;

		const TArrayView64<const FColor> Pixels = Image.AsBGRA8();
		if (Pixels.IsEmpty() || Image.SizeX <= 0 || Image.SizeY <= 0)
		{
			return false;
		}

		// 아틀라스 일부만 쓰는 스프라이트가 있어 자기 영역만 훑는다
		const FVector2D SourceUV = Sprite->GetSourceUV();
		const FVector2D SourceSize = Sprite->GetSourceSize();
		const int32 MinX = FMath::Clamp(FMath::RoundToInt(SourceUV.X), 0, Image.SizeX);
		const int32 MinY = FMath::Clamp(FMath::RoundToInt(SourceUV.Y), 0, Image.SizeY);
		const int32 MaxX = FMath::Clamp(MinX + FMath::RoundToInt(SourceSize.X), MinX, Image.SizeX);
		const int32 MaxY = FMath::Clamp(MinY + FMath::RoundToInt(SourceSize.Y), MinY, Image.SizeY);

		for (int32 Y = MinY; Y < MaxY; ++Y)
		{
			const int64 RowStart = static_cast<int64>(Y) * Image.SizeX;
			for (int32 X = MinX; X < MaxX; ++X)
			{
				if (Pixels[RowStart + X].A > GOpaqueAlphaThreshold)
				{
					OutRow = Y - MinY;
					return true;
				}
			}
		}
#endif
		return false;
	}

	// 측정 결과를 사람이 읽을 수 있게 --- 값이 전부 0이면 이 애니메이션은 몸이 안 움직인다는 뜻
	FString DescribeOffsets(const TArray<float>& Offsets)
	{
		TArray<FString> Parts;
		Parts.Reserve(Offsets.Num());
		for (const float Offset : Offsets)
		{
			Parts.Add(FString::Printf(TEXT("%.2f"), Offset));
		}
		return FString::Join(Parts, TEXT(", "));
	}

#if WITH_EDITOR
	// 잰 값을 설정 표와 DefaultGame.ini에 남긴다.
	// 패키징 빌드는 소스 텍스처가 없어 다시 잴 수 없으므로, 여기 적힌 값이 그대로 출하된다
	void RecordTrack(const UPaperFlipbook* Flipbook, const TArray<float>& Offsets)
	{
		UNUFrameBounceSettings* Settings = GetMutableDefault<UNUFrameBounceSettings>();
		if (Settings == nullptr)
		{
			return;
		}

		const FSoftObjectPath Path(Flipbook);
		FNUFrameBounceEntry* Entry = Settings->Tracks.FindByPredicate([&Path](const FNUFrameBounceEntry& Item)
		{
			return Item.Flipbook.ToSoftObjectPath() == Path;
		});
		if (Entry == nullptr)
		{
			Entry = &Settings->Tracks.AddDefaulted_GetRef();
			Entry->Flipbook = Path; // 포인터 대입은 deprecated --- 경로로 넣는다
		}
		Entry->Offsets = Offsets;
		Settings->TryUpdateDefaultConfigFile();

		UE_LOG(LogTemp, Log, TEXT("[NU] 프레임 바운스를 표에 적었습니다 %s --- DefaultGame.ini를 커밋해야 패키징 빌드에서도 흔들립니다"),
			*GetNameSafe(Flipbook));
	}
#endif
}

const TArray<float>& NUSpriteFrameOffset::GetVerticalOffsets(const UPaperFlipbook* Flipbook)
{
	static const TArray<float> EmptyOffsets;

	if (!IsValid(Flipbook))
	{
		return EmptyOffsets;
	}

	const FObjectKey Key(Flipbook);
	if (const TArray<float>* Cached = GVerticalOffsetCache.Find(Key))
	{
		return *Cached;
	}

	// 표가 먼저다 --- 패키징 빌드에는 소스 텍스처가 없어 이 길밖에 없다.
	// 아트가 바뀌어 다시 재야 하면 nu.ClearFrameBounce로 표를 비운다
	if (const UNUFrameBounceSettings* Settings = GetDefault<UNUFrameBounceSettings>())
	{
		if (const FNUFrameBounceEntry* Track = Settings->FindTrack(Flipbook))
		{
			return GVerticalOffsetCache.Add(Key, Track->Offsets);
		}
	}

	// 프레임별 그림 최상단 --- 행 번호가 작을수록 위에 그려져 있다는 뜻
	const int32 NumFrames = Flipbook->GetNumKeyFrames();
	TArray<int32> TopRows;
	TopRows.Reserve(NumFrames);
	float PixelsPerUnit = 1.0f;
	for (int32 Index = 0; Index < NumFrames; ++Index)
	{
		const UPaperSprite* Sprite = Flipbook->GetKeyFrameChecked(Index).Sprite;
		int32 TopRow = INDEX_NONE;
		if (IsValid(Sprite) && FindTopOpaqueRow(Sprite, TopRow))
		{
			PixelsPerUnit = Sprite->GetPixelsPerUnrealUnit();
		}
		else
		{
			TopRow = INDEX_NONE;
		}
		TopRows.Add(TopRow);
	}

	// 기준은 측정에 성공한 첫 프레임. 장비 그림도 그 프레임 자세에 맞춰 그려져 있다고 본다
	const int32 BaseIndex = TopRows.IndexOfByPredicate([](int32 Row) { return Row != INDEX_NONE; });

	TArray<float> Offsets;
	if (BaseIndex != INDEX_NONE)
	{
		const float UnitsPerPixel = 1.0f / FMath::Max(PixelsPerUnit, UE_KINDA_SMALL_NUMBER);
		Offsets.SetNumZeroed(NumFrames);
		for (int32 Index = 0; Index < NumFrames; ++Index)
		{
			if (TopRows[Index] != INDEX_NONE)
			{
				// 위로 올라갈수록 행 번호가 작아지므로 부호를 뒤집어 +Z 방향으로 만든다
				Offsets[Index] = (TopRows[BaseIndex] - TopRows[Index]) * UnitsPerPixel;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("[NU] 프레임 바운스 측정 %s: [%s]"),
			*GetNameSafe(Flipbook), *DescribeOffsets(Offsets));
#if WITH_EDITOR
		RecordTrack(Flipbook, Offsets);
#endif
	}
	else
	{
#if WITH_EDITOR
		UE_LOG(LogTemp, Log, TEXT("[NU] 프레임 바운스 측정 실패 %s --- 소스 텍스처를 읽을 수 없음"),
			*GetNameSafe(Flipbook));
#else
		// 패키징 빌드는 픽셀을 못 읽는다. 표에 없다는 것은 에디터에서 한 번도 재지 않았다는 뜻이다
		UE_LOG(LogTemp, Warning, TEXT("[NU] 프레임 바운스 표에 없음 %s --- 흔들림 없이 그립니다 (에디터에서 재생 후 DefaultGame.ini 커밋 필요)"),
			*GetNameSafe(Flipbook));
#endif
	}

	return GVerticalOffsetCache.Add(Key, MoveTemp(Offsets));
}

void NUSpriteFrameOffset::ClearCache()
{
	GVerticalOffsetCache.Empty();

#if WITH_EDITOR
	// 표까지 비워야 다시 잰다 --- 표가 남아 있으면 그 값이 계속 이긴다
	if (UNUFrameBounceSettings* Settings = GetMutableDefault<UNUFrameBounceSettings>())
	{
		Settings->Tracks.Empty();
		Settings->TryUpdateDefaultConfigFile();
	}
#endif

	UE_LOG(LogTemp, Log, TEXT("[NU] 프레임 바운스 측정 캐시와 표를 비웠습니다 --- 다시 재려면 애니메이션을 한 번씩 재생하세요"));
}

static FAutoConsoleCommand GClearFrameBounceCommand(
	TEXT("nu.ClearFrameBounce"),
	TEXT("프레임 바운스 측정 캐시와 표를 비운다 (스프라이트를 다시 임포트했을 때)"),
	FConsoleCommandDelegate::CreateStatic(&NUSpriteFrameOffset::ClearCache));

#if WITH_EDITOR
namespace
{
	// 폴더 안의 플립북을 전부 재서 표에 넣는다.
	// 게임을 돌리며 애니메이션이 한 번씩 재생되기를 기다리지 않아도 되게 하는 용도다 ---
	// 걸어보지 않은 방향이 표에서 빠지면 패키징 빌드에서 그 방향만 안 흔들린다
	void BakeFrameBounce(const TArray<FString>& Args)
	{
		TArray<FString> Paths = Args;
		if (Paths.IsEmpty())
		{
			// PZA_Player_Idle / PZA_Player_Walk이 참조하는 폴더 --- 실제로 재생되는 것만 표에 넣는다.
			// 위 폴더를 넓히면 안 쓰는 옛 스프라이트까지 표에 실려 커밋 파일이 지저분해진다
			Paths.Add(TEXT("/Game/Art/Sprites/Player/Player"));
		}

		UObjectLibrary* Library = UObjectLibrary::CreateLibrary(UPaperFlipbook::StaticClass(), false, true);
		Library->AddToRoot();
		Library->LoadAssetsFromPaths(Paths);

		TArray<UObject*> Objects;
		Library->GetObjects(Objects);

		int32 Measured = 0;
		for (UObject* Object : Objects)
		{
			const UPaperFlipbook* Flipbook = Cast<UPaperFlipbook>(Object);
			if (Flipbook == nullptr)
			{
				continue;
			}
			// 표에 적는 것은 GetVerticalOffsets가 한다 --- 여기서는 재게 만들기만 하면 된다
			if (!NUSpriteFrameOffset::GetVerticalOffsets(Flipbook).IsEmpty())
			{
				++Measured;
			}
		}

		Library->RemoveFromRoot();
		UE_LOG(LogTemp, Log, TEXT("[NU] 프레임 바운스 굽기 완료 --- 플립북 %d개 중 %d개 측정 (경로: %s)"),
			Objects.Num(), Measured, *FString::Join(Paths, TEXT(", ")));
	}
}

static FAutoConsoleCommand GBakeFrameBounceCommand(
	TEXT("nu.BakeFrameBounce"),
	TEXT("폴더 안의 플립북을 전부 재서 표에 넣는다. 인자로 경로를 주면 그 경로를 쓴다"),
	FConsoleCommandWithArgsDelegate::CreateStatic(&BakeFrameBounce));
#endif
