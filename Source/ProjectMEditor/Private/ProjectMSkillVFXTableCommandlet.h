#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ProjectMSkillVFXTableCommandlet.generated.h"

// 스킬 연출 표(DT_SkillVFX)를 원본 CSV 로 다시 채우고 저장함, VFX 뷰어 [적용] 이 화면 없는 에디터로 부름
//   UnrealEditor-Cmd.exe ProjectM.uproject -run=ProjectMSkillVFXTable [-Csv=<경로>]
// 경로를 안 주면 DT 가 기억하는 원본 파일(AssetImportData), 에디터의 "다시 가져오기"와 같은 결과
// 끝 코드: 0 성공 · 1 표를 못 찾음 · 2 CSV 를 못 읽음 · 3 CSV 문제 · 4 저장 실패
UCLASS()
class UProjectMSkillVFXTableCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UProjectMSkillVFXTableCommandlet();

	virtual int32 Main(const FString& Params) override;
};
