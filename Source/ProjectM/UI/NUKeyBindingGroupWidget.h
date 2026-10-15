// 키 설정 창의 묶음 제목 줄 ("필드", "창 열기" 등). 창이 묶음이 바뀔 때마다 하나씩 끼운다.
// 글자 하나뿐이지만 이름 조회 대신 BindWidget으로 잡으려고 클래스를 둔다 --- WBP가 이름을 바꾸면 컴파일이 막힌다.
//
// BindWidget: WBP에 같은 이름·타입의 위젯이 없으면 WBP 컴파일이 실패한다.
//   GroupText (TextBlock)  묶음 이름
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "NUKeyBindingGroupWidget.generated.h"

class UTextBlock;

UCLASS(Abstract)
class PROJECTM_API UNUKeyBindingGroupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetTitle(const FText& InTitle);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GroupText;
};
