#include "ProjectMCombatArtLibrary.h"

#include "AnimSequences/PaperZDAnimSequence_Flipbook.h"
#include "AnimSequences/PaperZDFlipbookAnimDataSource.h"
#include "AnimSequences/Skins/PaperZDAnimationSkin_Flipbook.h"
#include "AnimSequences/Sources/PaperZDAnimationSource.h"
#include "Battle/Avatar/NUBattleMotionMarkerNotify.h"
#include "Notifies/PaperZDAnimNotify_Base.h"
#include "PaperFlipbook.h"
#include "UObject/UnrealType.h"

namespace ProjectMCombatArt
{
	using FSkinMap = TMap<TSoftObjectPtr<UPaperZDAnimSequence>, FPaperZDFlipbookSkinData>;

	// "/Game/A/B" 와 "/Game/A/B.B" 둘 다 받음
	static FString ToObjectPath(const FString& Path)
	{
		if (Path.Contains(TEXT(".")))
		{
			return Path;
		}

		return Path + TEXT(".") + FPackageName::GetLongPackageAssetName(Path);
	}

	template <typename T>
	static T* Load(const FString& Path)
	{
		return Path.IsEmpty() ? nullptr : LoadObject<T>(nullptr, *ToObjectPath(Path));
	}

	// 알림 배열은 private 이라 리플렉션으로 꺼냄
	static TArray<TObjectPtr<UPaperZDAnimNotify_Base>>* GetNotifies(UPaperZDAnimSequence* Sequence)
	{
		FArrayProperty* Property = FindFProperty<FArrayProperty>(UPaperZDAnimSequence::StaticClass(), TEXT("AnimNotifies"));
		return Property ? Property->ContainerPtrToValuePtr<TArray<TObjectPtr<UPaperZDAnimNotify_Base>>>(Sequence) : nullptr;
	}

	// 스킨 표도 private 이라 리플렉션으로 꺼냄
	static FSkinMap* GetSkinMap(UPaperZDAnimationSkin_Flipbook* Skin)
	{
		FMapProperty* Property = FindFProperty<FMapProperty>(UPaperZDAnimationSkin_Flipbook::StaticClass(), TEXT("SkinsPerAnimation"));
		return Property ? Property->ContainerPtrToValuePtr<FSkinMap>(Skin) : nullptr;
	}

	static UPaperFlipbook* GetSourceFlipbook(const UPaperZDAnimSequence_Flipbook* Sequence)
	{
		FArrayProperty* ArrayProperty = Sequence ? Sequence->GetAnimDataSourceProperty() : nullptr;
		if (!ArrayProperty)
		{
			return nullptr;
		}

		FScriptArrayHelper Helper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(const_cast<UPaperZDAnimSequence_Flipbook*>(Sequence)));
		if (Helper.Num() == 0)
		{
			return nullptr;
		}

		const FPaperZDFlipbookAnimDataSource* Entry = reinterpret_cast<const FPaperZDFlipbookAnimDataSource*>(Helper.GetRawPtr(0));
		return Entry->Animation;
	}

	static float FindMarker(const UPaperZDAnimSequence* Sequence, ENUBattleMotionMarker Marker)
	{
		float Time = -1.f;
		return UNUBattleMotionMarkerNotify::FindMarkerTime(Sequence, Marker, Time) ? Time : -1.f;
	}
}

FString UProjectMCombatArtLibrary::DescribeCombatSequence(const FString& SequencePath)
{
	using namespace ProjectMCombatArt;

	const UPaperZDAnimSequence_Flipbook* Sequence = Load<UPaperZDAnimSequence_Flipbook>(SequencePath);
	if (!Sequence)
	{
		return FString::Printf(TEXT("ERROR: 시퀀스를 못 찾음 %s"), *SequencePath);
	}

	const UPaperFlipbook* Source = GetSourceFlipbook(Sequence);
	return FString::Printf(TEXT("Duration=%.3f Hit=%.3f Cast=%.3f Notifies=%d Source=%s"),
		Sequence->GetTotalDuration(),
		FindMarker(Sequence, ENUBattleMotionMarker::Hit),
		FindMarker(Sequence, ENUBattleMotionMarker::Cast),
		Sequence->GetAnimNotifies().Num(),
		Source ? *Source->GetPathName() : TEXT("None"));
}

FString UProjectMCombatArtLibrary::DescribeCombatSkin(const FString& SkinPath)
{
	using namespace ProjectMCombatArt;

	UPaperZDAnimationSkin_Flipbook* Skin = Load<UPaperZDAnimationSkin_Flipbook>(SkinPath);
	FSkinMap* Map = Skin ? GetSkinMap(Skin) : nullptr;
	if (!Map)
	{
		return FString::Printf(TEXT("ERROR: 스킨을 못 찾음 %s"), *SkinPath);
	}

	TArray<FString> Lines;
	for (const TPair<TSoftObjectPtr<UPaperZDAnimSequence>, FPaperZDFlipbookSkinData>& Pair : *Map)
	{
		TArray<FString> Flipbooks;
		for (const UPaperFlipbook* Flipbook : Pair.Value.AnimationDirections)
		{
			Flipbooks.Add(Flipbook ? Flipbook->GetName() : TEXT("None"));
		}

		TArray<FString> Mirrored;
		for (const bool bMirrored : Pair.Value.MirroredDirections)
		{
			Mirrored.Add(bMirrored ? TEXT("1") : TEXT("0"));
		}

		Lines.Add(FString::Printf(TEXT("%s=[%s] mirror=[%s]"), *Pair.Key.GetAssetName(),
			*FString::Join(Flipbooks, TEXT(",")), *FString::Join(Mirrored, TEXT(","))));
	}

	const UPaperZDAnimationSource* Source = Skin->GetAnimationSource();
	return FString::Printf(TEXT("Source=%s | %s"), Source ? *Source->GetName() : TEXT("None"), *FString::Join(Lines, TEXT(" | ")));
}

FString UProjectMCombatArtLibrary::SetupCombatAttackSequence(const FString& SequencePath, const FString& BodyFlipbookPath,
	float HitTime, float CastTime)
{
	using namespace ProjectMCombatArt;

	UPaperZDAnimSequence_Flipbook* Sequence = Load<UPaperZDAnimSequence_Flipbook>(SequencePath);
	UPaperFlipbook* Body = Load<UPaperFlipbook>(BodyFlipbookPath);
	if (!Sequence || !Body)
	{
		return FString::Printf(TEXT("ERROR: 시퀀스 또는 플립북을 못 찾음 %s / %s"), *SequencePath, *BodyFlipbookPath);
	}

	FArrayProperty* DataProperty = Sequence->GetAnimDataSourceProperty();
	TArray<TObjectPtr<UPaperZDAnimNotify_Base>>* Notifies = GetNotifies(Sequence);
	if (!DataProperty || !Notifies)
	{
		return TEXT("ERROR: 시퀀스의 원본 · 알림 프로퍼티를 못 찾음");
	}

	Sequence->Modify();

	// 원본 플립북(레이어 0) 교체 — 남캐 직업 공격 시퀀스를 복제해 쓰므로 한 칸만 있음
	FScriptArrayHelper Helper(DataProperty, DataProperty->ContainerPtrToValuePtr<void>(Sequence));
	if (Helper.Num() == 0)
	{
		Helper.AddValue();
	}
	reinterpret_cast<FPaperZDFlipbookAnimDataSource*>(Helper.GetRawPtr(0))->Animation = Body;

	const float Duration = Sequence->GetTotalDuration();
	if (HitTime < 0.f || HitTime > Duration)
	{
		return FString::Printf(TEXT("ERROR: Hit 시각 %.3f 이 시퀀스 길이 %.3f 밖"), HitTime, Duration);
	}

	// 표시를 종류별로 하나씩만 남기고 시각을 옮김, 없으면 새로 만듦
	UNUBattleMotionMarkerNotify* HitMarker = nullptr;
	UNUBattleMotionMarkerNotify* CastMarker = nullptr;
	int32 TrackIndex = 0;
	for (int32 Index = Notifies->Num() - 1; Index >= 0; --Index)
	{
		UNUBattleMotionMarkerNotify* Marker = Cast<UNUBattleMotionMarkerNotify>((*Notifies)[Index].Get());
		if (!Marker)
		{
			continue;
		}

		TrackIndex = Marker->TrackIndex;
		UNUBattleMotionMarkerNotify*& Slot = Marker->Marker == ENUBattleMotionMarker::Hit ? HitMarker : CastMarker;
		if (Slot)
		{
			Notifies->RemoveAt(Index);
			continue;
		}
		Slot = Marker;
	}

	auto MakeMarker = [Sequence, Notifies, TrackIndex](ENUBattleMotionMarker Kind)
	{
		UNUBattleMotionMarkerNotify* Marker = NewObject<UNUBattleMotionMarkerNotify>(Sequence, NAME_None, RF_Transactional);
		Marker->Marker = Kind;
		Marker->TrackIndex = TrackIndex;
		Notifies->Add(Marker);
		return Marker;
	};

	if (!HitMarker)
	{
		HitMarker = MakeMarker(ENUBattleMotionMarker::Hit);
	}
	HitMarker->Time = HitTime;

	if (CastTime >= 0.f)
	{
		if (!CastMarker)
		{
			CastMarker = MakeMarker(ENUBattleMotionMarker::Cast);
		}
		CastMarker->Time = FMath::Min(CastTime, HitTime);
	}
	else if (CastMarker)
	{
		Notifies->Remove(CastMarker);
	}

	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();

	return DescribeCombatSequence(SequencePath);
}

FString UProjectMCombatArtLibrary::SetCombatSkinEntries(const FString& SkinPath, const TArray<FString>& SequencePaths,
	const TArray<FString>& FlipbookPaths)
{
	using namespace ProjectMCombatArt;

	UPaperZDAnimationSkin_Flipbook* Skin = Load<UPaperZDAnimationSkin_Flipbook>(SkinPath);
	FSkinMap* Map = Skin ? GetSkinMap(Skin) : nullptr;
	if (!Map)
	{
		return FString::Printf(TEXT("ERROR: 스킨을 못 찾음 %s"), *SkinPath);
	}

	if (SequencePaths.Num() != FlipbookPaths.Num())
	{
		return TEXT("ERROR: 시퀀스와 플립북 개수가 다름");
	}

	// 좌우반전은 기존 표의 첫 줄을 따름 (전투는 한 방향이라 보통 비어 있음)
	TArray<bool> Mirrored;
	for (const TPair<TSoftObjectPtr<UPaperZDAnimSequence>, FPaperZDFlipbookSkinData>& Pair : *Map)
	{
		Mirrored = Pair.Value.MirroredDirections;
		break;
	}

	FSkinMap NewMap;
	for (int32 Index = 0; Index < SequencePaths.Num(); ++Index)
	{
		UPaperZDAnimSequence* Sequence = Load<UPaperZDAnimSequence>(SequencePaths[Index]);
		if (!Sequence)
		{
			return FString::Printf(TEXT("ERROR: 시퀀스를 못 찾음 %s"), *SequencePaths[Index]);
		}

		// 빈 칸은 키를 남기고 플립북만 비움 — 키가 없으면 시퀀스 원본(몸 그림)이 그 레이어에 그려짐
		UPaperFlipbook* Flipbook = Load<UPaperFlipbook>(FlipbookPaths[Index]);
		if (!Flipbook && !FlipbookPaths[Index].IsEmpty())
		{
			return FString::Printf(TEXT("ERROR: 플립북을 못 찾음 %s"), *FlipbookPaths[Index]);
		}

		FPaperZDFlipbookSkinData Data;
		Data.AnimationDirections.Add(Flipbook);
		Data.MirroredDirections = Mirrored;
		NewMap.Add(TSoftObjectPtr<UPaperZDAnimSequence>(Sequence), Data);
	}

	Skin->Modify();
	*Map = MoveTemp(NewMap);
	Skin->PostEditChange();
	Skin->MarkPackageDirty();

	return DescribeCombatSkin(SkinPath);
}
