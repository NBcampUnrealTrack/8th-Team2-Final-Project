#pragma once

#include "CoreMinimal.h"

class UPaperFlipbook;

/**
 * 플립북 프레임별 세로 흔들림 측정.
 *
 * Idle 바운스처럼 몸 전체가 위아래로 오르내리는 애니메이션은, 프레임마다 그림이 통째로
 * 몇 픽셀씩 옮겨 그려져 있다. 장비 그림이 부위당 한 장뿐이어도 그 이동량만큼 겹침 레이어를
 * 같이 올렸다 내리면 몸에 붙어 움직이는 것처럼 보인다.
 *
 * 이동량은 프레임 스프라이트에서 그림이 시작되는 최상단 행을 프레임끼리 비교해 얻는다.
 * 소스 텍스처 픽셀을 읽어야 해서 측정은 에디터에서만 된다 --- 패키징 빌드는 소스가 없다.
 * 그래서 잰 값은 UNUFrameBounceSettings 표(DefaultGame.ini)에 적히고, 실제 사용은 항상 표에서 읽는다.
 * 표가 곧 출하되는 값이므로 DefaultGame.ini를 커밋해야 다른 사람 빌드에서도 흔들린다.
 *
 * 스프라이트를 다시 임포트했다면 nu.ClearFrameBounce로 캐시와 표를 비우고 애니메이션을 한 번씩 재생한다.
 */
namespace NUSpriteFrameOffset
{
	// 첫 프레임 대비 각 키프레임이 위로 올라간 양(스프라이트 로컬 유닛).
	// 측정할 수 없으면 빈 배열 --- 호출부는 보정을 건너뛰면 된다
	const TArray<float>& GetVerticalOffsets(const UPaperFlipbook* Flipbook);

	// 측정 캐시를 비운다
	void ClearCache();
}
