// 외형 설정의 기본값 --- 머리 색 견본과 물들이는 머티리얼.
#include "CharacterSelect/Data/NUAppearanceDataSettings.h"

#include "Materials/MaterialInterface.h"

UNUAppearanceDataSettings::UNUAppearanceDataSettings()
{
	// 번호가 서버에 저장된다 --- 순서를 바꾸지 말고 뒤에만 보탤 것.
	// 0 원래 색(견본 표시용) / 1 흑 / 2 갈 / 3 적 / 4 금 / 5 은 / 6 백 / 7 청 / 8 녹 / 9 분홍 / 10 보라
	static const TCHAR* const DefaultHairColors[] = {
		TEXT("E8C2A6"), TEXT("2B2A33"), TEXT("6B4A32"), TEXT("B4412F"), TEXT("E0B86A"), TEXT("C9CDD6"),
		TEXT("F2EEE6"), TEXT("3F6FB5"), TEXT("4E8F5A"), TEXT("E58AA8"), TEXT("7A56B0") };
	for (const TCHAR* Hex : DefaultHairColors)
	{
		HairColors.Add(FLinearColor::FromSRGBColor(FColor::FromHex(Hex)));
	}

	HairTintMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/HairTint/M_NUCharacterHairTint.M_NUCharacterHairTint")));
	HairTintStandMaterial = HairTintMaterial; // 스탠드도 필드와 같은 캐릭터 머티리얼로 그린다
	HairTintUIMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Art/Materials/HairTint/M_NUHairTint_UI.M_NUHairTint_UI")));
}
