// 외형(커스터마이징) 파츠 표. 프로젝트 세팅 > 게임 > NU Appearance Data.
//
// 생성창과 필드 캐릭터가 같은 표를 봐야 해서 여기 둔다 --- 위젯에만 두면
// 필드가 못 읽고, 양쪽에 따로 두면 언젠가 어긋난다 (NUNaiaDataSettings 선례).
#pragma once

#include "CoreMinimal.h"
#include "CharacterSelect/NUCharacterCreateTypes.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "NUAppearanceDataSettings.generated.h"

class UMaterialInterface;

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "NU Appearance Data"))
class PROJECTM_API UNUAppearanceDataSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UNUAppearanceDataSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	// 커마 베이스 몸 표 --- 종족·성별 조합마다 한 줄 (FNUBodyRow). 본체(레이어 0)를 그 줄의 스킨으로 덮는다.
	// 조합에 맞는 줄이 없으면 애님 시퀀스의 원래 몸. "모든 종족·모든 성별" 줄 하나면 몸이 하나뿐인 지금과 같고,
	// 성별·종족 몸이 오면 그 줄만 보태면 된다 (NUAppearance::FindBody가 고른다. 여성=1, 남성=0)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance Data", meta = (TitleProperty = "RaceId"))
	TArray<FNUBodyRow> Bodies;

	// 표의 행은 FNUAppearancePartRow. 미지정이면 생성창이 번호만 도는 더미를 띄운다
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance Data")
	TSoftObjectPtr<UDataTable> HairPartTable;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance Data")
	TSoftObjectPtr<UDataTable> FacePartTable;

	// 피부는 아직 형식이 안 정해졌다 (색으로 할지 그림으로 할지) --- 자리만 둔다
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Appearance Data")
	TSoftObjectPtr<UDataTable> SkinPartTable;

	// --- 머리 색 (10/13) ---
	// 서버에 머리 색 칸이 따로 없어 hair_id 한 숫자에 색 번호를 같이 싣는다 (NUAppearance::PackHairId).
	// **번호가 곧 저장되는 값이다 --- 순서를 바꾸거나 중간에 끼워 넣지 말고 뒤에만 보탠다.**
	// 0번은 "그림 원래 색"이라 물들이지 않는다. 0번에 적힌 색은 생성창 견본을 그리는 데만 쓴다.
	// ini에 없으면 생성자의 기본 견본 11색을 쓴다
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hair Color")
	TArray<FLinearColor> HairColors;

	// 물들인 머리 레이어에 쓰는 머티리얼 (필드·전투). 그림의 밝기만 남기고 정점 색(= 머리 색)을 입힌다.
	// 캐릭터 머티리얼(M_NUCharacterSprite)의 사본이라 그쪽 그래프를 고치면 이것도 다시 만들어야 한다
	// (Tools/CharacterCreateGlass/apply_hair.py). 비거나 못 찾으면 물들이지 않는다
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hair Color")
	TSoftObjectPtr<UMaterialInterface> HairTintMaterial;

	// 생성창 스탠드용 --- 스탠드의 몸과 같은 계열이어야 한다 (기본은 필드와 같은 것. 몸을 마스크드로 그리는 레벨이면 마스크드 사본으로 바꾼다)
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hair Color")
	TSoftObjectPtr<UMaterialInterface> HairTintStandMaterial;

	// UI용 (선택창 초상, 생성창 머리 썸네일). 파라미터: SpriteTexture · UVRect · HairTint
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Hair Color")
	TSoftObjectPtr<UMaterialInterface> HairTintUIMaterial;
};
