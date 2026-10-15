#include "Field/NUFrameBounceSettings.h"

#include "PaperFlipbook.h"

const FNUFrameBounceEntry* UNUFrameBounceSettings::FindTrack(const UPaperFlipbook* Flipbook) const
{
	if (!IsValid(Flipbook))
	{
		return nullptr;
	}

	// 소프트 참조를 로드하지 않고 경로만 비교한다 --- 표에 실린 다른 플립북까지 끌어올 이유가 없다
	const FSoftObjectPath Path(Flipbook);
	return Tracks.FindByPredicate([&Path](const FNUFrameBounceEntry& Entry)
	{
		return Entry.Flipbook.ToSoftObjectPath() == Path;
	});
}
