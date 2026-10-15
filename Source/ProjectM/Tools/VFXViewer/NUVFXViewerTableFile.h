// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

// 스킬 연출 표(DT_SkillVFX)의 원본 CSV 한 행에서 VFX 칸만 (VFX 뷰어 [적용] · [되돌리기] 가 씀)
// 에셋 경로는 CSV 에 적힌 문자열 그대로 (/Game/.../NS_X.NS_X, 비면 없음)
struct FNUVFXViewerTableRow
{
	FString Cast;
	FString Projectile;
	FString Impact;
	bool bUseElementColor = false;
	FLinearColor FixedColor = FLinearColor::White;

	bool operator==(const FNUVFXViewerTableRow& Other) const;
	bool operator!=(const FNUVFXViewerTableRow& Other) const { return !(*this == Other); }

	// CSV 칸 형식 "(R=1,G=0.25,B=0.3,A=1)"
	static FString FormatColor(const FLinearColor& Color);
};

// 원본 CSV 를 줄 그대로 들고, 바꾼 행만 다시 씀 (다른 행 · 머리줄 · BOM · 줄 끝은 그대로)
// 형식: Name,SkillId,CastVFX,ProjectileVFX,ImpactVFX,bUseElementColor,FixedColor (열 순서는 머리줄에서 찾음)
class FNUVFXViewerTableFile
{
public:
	bool Load(const FString& InPath, FString& OutError);
	bool Save(FString& OutError) const;

	// BOM 없는 전체 내용, UDataTable::CreateTableFromCSVString 에 그대로 넣음
	FString ToString() const;

	const FString& GetPath() const { return Path; }
	bool HasRow(int32 SkillId) const { return LineBySkill.Contains(SkillId); }
	bool FindRow(int32 SkillId, FNUVFXViewerTableRow& OutRow) const;

	// 있으면 그 행의 VFX 칸만 바꾸고, 없으면 끝에 새 행 (행 이름은 NewRowName)
	void SetRow(int32 SkillId, const FNUVFXViewerTableRow& Row, const FString& NewRowName);
	bool RemoveRow(int32 SkillId);

private:
	static TArray<FString> SplitFields(const FString& Line);
	static FString JoinFields(const TArray<FString>& Fields);
	void RebuildIndex();

	FString Path;
	bool bHasBom = false;
	FString LineEnd = TEXT("\r\n");
	TArray<FString> Lines;

	int32 NameColumn = INDEX_NONE;
	int32 SkillColumn = INDEX_NONE;
	int32 CastColumn = INDEX_NONE;
	int32 ProjectileColumn = INDEX_NONE;
	int32 ImpactColumn = INDEX_NONE;
	int32 UseElementColumn = INDEX_NONE;
	int32 ColorColumn = INDEX_NONE;

	// 스킬 → 줄 번호 (같은 스킬이 여러 줄이면 첫 줄, 게임의 표 색인과 같음)
	TMap<int32, int32> LineBySkill;
};

// 직업 기본 공격 연출(UNUBattleDataSettings::JobAttackVFX)이 적힌 ini 의 한 줄 "JobAttackVFX=((직업, (칸=값,...)),...)"
// 기본 공격(스킬 0)은 DT_SkillVFX 가 아니라 여기라서 VFX 뷰어 [적용] 이 이 줄 안의 바꾼 직업 항목만 다시 씀
// (다른 줄 · 다른 직업 항목 · 줄 끝 · 인코딩은 그대로), 항목은 기존 줄처럼 빈 칸을 안 적음: (1004, (ProjectileVFX="...",ImpactVFX="..."))
class FNUVFXViewerJobAttackIni
{
public:
	bool Load(const FString& InPath, FString& OutError);
	bool Save(FString& OutError) const;

	const FString& GetPath() const { return Path; }

	// 그 직업 항목의 칸 글자 그대로 "(CastVFX=\"...\")", 없으면 false
	bool FindEntry(int32 JobId, FString& OutEntry) const;

	// 있으면 그 항목만 바꾸고, 없으면 직업 번호 순서 자리에 넣음
	void SetEntry(int32 JobId, const FString& Entry);
	bool RemoveEntry(int32 JobId);

	// 칸 → 항목 글자 (빈 칸 · 흰색은 안 적음, 직업 연출은 속성색이 없어 FixedColor 가 EffectColor) / 그 반대
	static FString FormatEntry(const FNUVFXViewerTableRow& Row);
	static FNUVFXViewerTableRow ParseEntry(const FString& Entry);

private:
	// 괄호 · 따옴표 밖의 쉼표로 나눔 (빈 조각은 버림)
	static TArray<FString> SplitTopLevel(const FString& Text);
	static FString StripParens(const FString& Text);

	FString Path;
	bool bHasBom = false;

	// 파일 전체와 그 안의 "JobAttackVFX=" 뒤 ~ 줄 끝 자리, 쓸 때 이 구간만 갈아 끼움
	FString Text;
	int32 ValueStart = INDEX_NONE;
	int32 ValueEnd = INDEX_NONE;

	// 직업 번호와 칸 글자, ini 에 적힌 순서
	TArray<TPair<int32, FString>> Entries;
};
