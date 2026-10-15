// Fill out your copyright notice in the Description page of Project Settings.

#include "Field/NUSpriteVisualSettings.h"

#include "UObject/SoftObjectPtr.h"
#include "AnimSequences/PaperZDAnimSequence.h"
#include "Materials/MaterialInterface.h"
#include "PaperZDAnimInstance.h"

namespace
{
	// 기본으로 사용할 애셋 경로들
	// 스프라이트 머티리얼은 비워 둔다 (필드 나이아만 예외 --- 아래 MATERIAL_FIELD_SPIRIT).
	// FNUSpriteVisualSettings::Material 이 null 이면 UNUSpriteVisualComponent::RefreshAppearance() 가
	// 컴포넌트 오버라이드를 걸지 않고 PaperSprite/PaperFlipbook 애셋에 지정된 머티리얼을 그대로 쓴다.
	// 그래야 노멀맵이 붙은 스프라이트는 라이팅을 받고(MI_PixelSprite_Lit), 아직 준비되지 않은
	// 스프라이트는 종전대로 Paper2D 언릿 머티리얼로 남는다. 여기에 경로를 하드코딩하면
	// 노멀맵이 없는 스프라이트까지 머티리얼 기본 노멀맵을 뒤집어쓴다.
	const TCHAR* ANIMBP_PLAYER         = TEXT("/Game/Art/Animations/PaperZD/Player/ABPZD_Player.ABPZD_Player_C");
	const TCHAR* ANIMBP_PLAYER_COMBAT  = TEXT("/Game/Art/Animations/PaperZD/Player/ABPZD_Player_Combat.ABPZD_Player_Combat_C");
	const TCHAR* ANIMSEQ_PLAYER_COMBAT_ATTACK = TEXT("/Game/Art/Animations/PaperZD/Player/PZA_Player_Combat_Attack.PZA_Player_Combat_Attack");
	const TCHAR* ANIMBP_SLIME          = TEXT("/Game/Art/Animations/PaperZD/Naia/ABPZD_Slime_Fallback.ABPZD_Slime_Fallback_C");

	// 필드 나이아만 머티리얼을 지정한다 --- 캐릭터와 같은 M_NUCharacterSprite.
	// 안티앨리어싱(TSR)을 거치지 않아 도트가 또렷하고, 언릿 색이 맵의 고정 노출만큼 어두워지는 것도 되돌린다.
	// TSR 을 우회하므로 그림 1칸이 화면 1픽셀보다 작으면 칸이 빠지며 깜빡인다 --- 그래서 MakeFieldSpirit 의 배율을 0.5 로 둔다
	// (2560 폭에서 1칸이 약 1픽셀. 그보다 좁은 화면에서는 1칸이 1픽셀보다 작아져 다시 칸이 빠진다)
	const TCHAR* MATERIAL_FIELD_SPIRIT = TEXT("/Game/Art/Materials/M_NUCharacterSprite.M_NUCharacterSprite");
}

namespace NUSpriteVisuals
{

	const TArray<FNUSpriteLayerDef>& GetCharacterLayers()
	{
		// 바운스: 갑옷·장갑·어깨·머리·외모는 몸을 따라가고, 하의·신발은 땅에 붙어 0.
		// 지연: 팔은 몸통보다 한 박자 늦어 장갑에만 1프레임
		static const TArray<FNUSpriteLayerDef> Layers = {
			{ NUSpriteLayer::EquipChest,    1.0f, 0 },
			{ NUSpriteLayer::EquipGloves,   1.0f, 1 },
			{ NUSpriteLayer::EquipBoots,    0.0f, 0 },
			{ NUSpriteLayer::EquipLegs,     0.0f, 0 },
			{ NUSpriteLayer::EquipShoulder, 1.0f, 0 },
			{ NUSpriteLayer::Hair,          1.0f, 0 },
			{ NUSpriteLayer::Face,          1.0f, 0 },
		};
		return Layers;
	}
	FNUSpriteVisualSettings MakeNPC()
	{
		FNUSpriteVisualSettings Settings;
		Settings.BlobShadowRadius = 26.0f;
		Settings.BillboardMode = ENUBillboardMode::CameraYaw;
		Settings.BillboardYawOffset = 90.0f;
		Settings.SpritePivotZ =  0.0f;
		Settings.bCastShadow = true;
		return Settings;
	}

	FNUSpriteVisualSettings MakeFieldCharacter()
	{
		FNUSpriteVisualSettings Settings;
		Settings.BlobShadowRadius = 26.0f;
		Settings.AnimBlueprint = TSoftClassPtr<UPaperZDAnimInstance>(FSoftObjectPath(ANIMBP_PLAYER));
		Settings.BillboardMode = ENUBillboardMode::CameraYaw;
		Settings.BillboardYawOffset = 90.0f;
		Settings.SpritePivotZ = -49.0f; // 캡슐 높이가 48, (-48 ~ -49)로 두면 발이 지면에 닿음
		Settings.bCastShadow = true;
		return Settings;
	}

	FNUSpriteVisualSettings MakeFieldSpirit()
	{
		FNUSpriteVisualSettings Settings;
		Settings.Material = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MATERIAL_FIELD_SPIRIT));
		Settings.AnimBlueprint = TSoftClassPtr<UPaperZDAnimInstance>(FSoftObjectPath(ANIMBP_SLIME));
		Settings.BillboardMode = ENUBillboardMode::CameraYaw;
		Settings.BillboardYawOffset = 90.0f;
		Settings.RelativeScale = FVector(0.5f);
		Settings.bCastShadow = false;
		return Settings;
	}

	FNUSpriteVisualSettings MakeFieldEnemy()
	{
		FNUSpriteVisualSettings Settings;
		Settings.AnimBlueprint = TSoftClassPtr<UPaperZDAnimInstance>(FSoftObjectPath(ANIMBP_SLIME));
		Settings.BillboardMode = ENUBillboardMode::CameraYaw;
		Settings.BillboardYawOffset = 90.0f;
		Settings.SpritePivotZ = -40.0f;
		Settings.bCastShadow = true;
		return Settings;
	}

	FNUSpriteVisualSettings MakeBattleAvatar()
	{
		// 애님 블루프린트는 전투원 종류에 따라 아바타가 런타임에 적용함
		// 아예 없을경우를 대비해서 ANIMBP_SLIME 설정, 교체되는 장면이 보이면 비우거나 투명으로 교체 필요
		FNUSpriteVisualSettings Settings;
		Settings.AnimBlueprint = TSoftClassPtr<UPaperZDAnimInstance>(FSoftObjectPath(ANIMBP_SLIME));
		Settings.BillboardMode = ENUBillboardMode::CameraYaw;
		Settings.BillboardYawOffset = 90.0f;
		Settings.bCastShadow = true;
		return Settings;
	}

	TSoftClassPtr<UPaperZDAnimInstance> GetCombatCharacterAnimBlueprint()
	{
		return TSoftClassPtr<UPaperZDAnimInstance>(FSoftObjectPath(ANIMBP_PLAYER_COMBAT));
	}

	TSoftObjectPtr<UPaperZDAnimSequence> GetCombatCharacterAttackSequence()
	{
		return TSoftObjectPtr<UPaperZDAnimSequence>(FSoftObjectPath(ANIMSEQ_PLAYER_COMBAT_ATTACK));
	}
}
