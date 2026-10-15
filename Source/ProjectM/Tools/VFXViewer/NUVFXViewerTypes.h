// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Battle/NUBattleTypes.h"
#include "UObject/SoftObjectPath.h"

// 누가 행동하는지, 아군은 캐릭터와 나이아가 같이 서고 그중 하나가 시전함
enum class ENUVFXViewerCaster : uint8
{
	Player,
	AllyNaia,
	Enemy
};

// 스킬 표대로 재생할지, 세 칸을 직접 골라 재생할지
enum class ENUVFXViewerMode : uint8
{
	Skill,
	Custom
};

// 뷰어 설정 한 벌, 마지막 선택은 Saved/Config/WindowsEditor/GameUserSettings.ini [NUVFXViewer] 에 남김
struct FNUVFXViewerSetup
{
	// 무대 목록(/Game/Maps/BattleArena 의 레벨) 안의 번호
	int32 StageIndex = INDEX_NONE;

	ENUVFXViewerCaster Caster = ENUVFXViewerCaster::Player;

	// 캐릭터 직업, 0 = 노비스 (몸 줄에 전투 그림이 있는 직업만 목록에 나옴)
	int32 JobId = 0;

	// 캐릭터 성별, 0 = 남 · 1 = 여 (몸 줄의 Gender), 그 성별에 전투 그림이 있는 직업만 고를 수 있음
	int32 Gender = 0;

	// 캐릭터 머리 번호 (서버 hair_id 와 같음 = 머리 파츠 번호 + 색 번호 * 1000), 0 = 그 몸의 첫 머리
	int32 HairId = 0;

	int32 AllyNaiaId = INDEX_NONE;
	int32 EnemyNaiaId = INDEX_NONE;
	int32 EnemyCount = 1;

	// 아군이 시전하면 적 번호, 적이 시전하면 0 = 캐릭터 · 1 = 아군 나이아
	int32 TargetIndex = 0;

	ENUVFXViewerMode Mode = ENUVFXViewerMode::Skill;

	// 0 = 기본 공격 (캐릭터는 직업 기본 공격 연출, 나이아 · 적은 기본 행동 어빌리티 연출)
	int32 SkillId = 0;

	FSoftObjectPath CustomCast;
	FSoftObjectPath CustomProjectile;
	FSoftObjectPath CustomImpact;
	FLinearColor CustomColor = FLinearColor::White;
	ENUBattleActionMotion CustomMotion = ENUBattleActionMotion::MoveToCenter;
	int32 CustomHitCount = 1;

	bool bPlayHitCue = true;
	bool bLoop = false;
	float Speed = 1.f;
};

// 스킬 목록 한 줄
struct FNUVFXViewerSkillItem
{
	int32 SkillId = 0;
	FString Label;
	FString Category;

	// 검색은 번호 · 이름 · 분류를 한 번에 봄
	FString SearchKey;
};

// 나이아가라 에셋 목록 한 줄
struct FNUVFXViewerAssetItem
{
	FSoftObjectPath Path;
	FString Label;
};

// 나이아 · 직업 · 무대 선택지
struct FNUVFXViewerChoice
{
	int32 Id = 0;
	FString Label;
	FSoftObjectPath Path;

	// 직업 선택지만 씀, 그 직업 전투 그림이 있는 몸 줄의 종족 · 성별
	int32 RaceId = 0;
	int32 Gender = 0;
};

// 비교 후보 하나, 스킬 표 행 위에 적은 칸만 덮음 (안 적은 칸은 표 그대로)
// 후보 파일 형식은 Tools/VFXViewer/README.md 의 "후보 비교"
struct FNUVFXViewerVariant
{
	FString Label;
	FString Note;

	// 칸마다 덮을지와 그 값, 빈 경로로 덮으면 그 칸을 비움
	bool bOverrideCast = false;
	bool bOverrideProjectile = false;
	bool bOverrideImpact = false;
	bool bOverrideColor = false;

	FSoftObjectPath Cast;
	FSoftObjectPath Projectile;
	FSoftObjectPath Impact;
	FLinearColor Color = FLinearColor::White;

	bool HasOverride() const
	{
		return bOverrideCast || bOverrideProjectile || bOverrideImpact || bOverrideColor;
	}
};

// 비교 묶음 하나, 한 스킬의 후보들 (후보 파일의 sets 한 칸)
// 화면에는 묶음이 안 보임, 스킬을 고르면 그 스킬의 후보가 모든 묶음에서 모여 나옴 (FNUVFXViewerCandidate)
struct FNUVFXViewerCompareSet
{
	FString Title;
	int32 SkillId = 0;

	// -VFXSkills 와 같은 표기, 숫자 = 직업 캐릭터(뒤에 f · m 으로 성별) · n<번호> = 아군 나이아 · e<번호> = 적 나이아, 비우면 지금 설정
	FString CasterText;

	TArray<FNUVFXViewerVariant> Variants;

	// 읽어 온 후보 파일(전체 경로)과 그 파일 안의 묶음 번호, 고른 기록이 이것으로 후보를 다시 찾음
	FString SourceFile;
	int32 SourceSetIndex = INDEX_NONE;
};

// 한 스킬의 후보 한 칸, 첫 칸은 늘 표 그대로
// 묶음 번호가 없으면 후보 파일에 표 그대로인 후보("지금")가 없어 뷰어가 붙인 칸
struct FNUVFXViewerCandidate
{
	int32 SetIndex = INDEX_NONE;
	int32 VariantIndex = INDEX_NONE;
};

// 적용 대기 한 건, 스킬마다 하나 (Saved/VFXViewer/Pending.json)
// 고를 때의 후보 값을 같이 남겨, 후보 파일이 빠지거나 바뀌어도 고른 그대로 적용할 수 있음
struct FNUVFXViewerPendingChange
{
	int32 SkillId = 0;
	FString Label;
	FString Title;
	FString Time;

	// 고른 후보가 온 파일(프로젝트 기준 경로)과 그 파일 안의 묶음 · 후보 번호
	FString SourceFile;
	int32 SourceSetIndex = INDEX_NONE;
	int32 VariantIndex = INDEX_NONE;

	// 표 행 위에 덮을 값, 후보를 지금 읽었으면 그 후보 값으로 새로 고침
	FNUVFXViewerVariant Overrides;

	// 지금 읽은 후보 중 이 후보의 칸, 못 찾으면 INDEX_NONE (남겨 둔 값으로 적용)
	int32 CandidateIndex = INDEX_NONE;

	// 기본 공격(스킬 0)만: 담을 때 보던 캐릭터의 직업, 적용은 DefaultGame.ini JobAttackVFX 의 이 직업 항목 (INDEX_NONE = 모름)
	int32 JobId = INDEX_NONE;
};

// 마지막 재생의 결과, 패널 정보 칸과 자동 캡처 보고서가 읽음
struct FNUVFXViewerPlayReport
{
	bool bValid = false;

	// 아바타가 직업 기본 공격으로 바꾼 뒤의 최종 스펙
	FNUBattleActionMotionSpec Spec;

	bool bCasterIsAlly = true;
	bool bHasSkillProfile = false;
	bool bDealsDamage = false;
	int32 HitCount = 1;

	float FirstHitDelay = 0.f;
	float TotalDuration = 0.f;

	// 행동 시작(Acting) 기준 시전 · 타격 시각, 접근 시간을 더하면 재생 시작 기준
	float CastTime = 0.f;
	float HitTime = 0.f;

	TArray<FString> Warnings;
};

// 진행바에 그리는 시간표, 전부 재생 시작 기준 초
// 아바타가 실제로 도는 순서대로 셈 (제자리 행동은 접근을 건너뜀 - ANUBattleAvatar::BeginActionMotion)
struct FNUVFXViewerTimeline
{
	float TelegraphEnd = 0.f;
	float ApproachEnd = 0.f;
	float ActingEnd = 0.f;
	float ReturnEnd = 0.f;

	// 반복 타격이면 모션마다 하나씩
	TArray<float> CastTimes;
	TArray<float> HitTimes;
};
