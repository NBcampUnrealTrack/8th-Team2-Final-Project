// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/VFXViewer/NUVFXViewerTableFile.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FNUVFXViewerTableRow::operator==(const FNUVFXViewerTableRow& Other) const
{
	// 색은 CSV 에 적히는 자릿수로 비교 (읽고 쓴 값이 미세하게 달라도 같은 값)
	return Cast == Other.Cast
		&& Projectile == Other.Projectile
		&& Impact == Other.Impact
		&& bUseElementColor == Other.bUseElementColor
		&& FormatColor(FixedColor) == FormatColor(Other.FixedColor);
}

FString FNUVFXViewerTableRow::FormatColor(const FLinearColor& Color)
{
	// 기존 행과 같은 모양 (R=1,G=0.25,...), %g 라 1.0 은 1 로 씀
	return FString::Printf(TEXT("(R=%g,G=%g,B=%g,A=%g)"), Color.R, Color.G, Color.B, Color.A);
}

bool FNUVFXViewerTableFile::Load(const FString& InPath, FString& OutError)
{
	Path = InPath;
	Lines.Reset();

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("CSV 를 못 읽음: %s"), *Path);
		return false;
	}

	bHasBom = Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF;

	FString Text;
	FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());
	LineEnd = Text.Contains(TEXT("\r\n")) ? TEXT("\r\n") : TEXT("\n");

	// 끝 줄바꿈 뒤의 빈 칸은 버리고, 쓸 때 줄마다 줄 끝을 붙임
	Text.ParseIntoArray(Lines, *LineEnd, false);
	while (!Lines.IsEmpty() && Lines.Last().IsEmpty())
	{
		Lines.Pop();
	}

	if (Lines.IsEmpty())
	{
		OutError = FString::Printf(TEXT("CSV 가 비어 있음: %s"), *Path);
		return false;
	}

	const TArray<FString> Header = SplitFields(Lines[0]);
	NameColumn = 0;
	SkillColumn = Header.IndexOfByKey(TEXT("SkillId"));
	CastColumn = Header.IndexOfByKey(TEXT("CastVFX"));
	ProjectileColumn = Header.IndexOfByKey(TEXT("ProjectileVFX"));
	ImpactColumn = Header.IndexOfByKey(TEXT("ImpactVFX"));
	UseElementColumn = Header.IndexOfByKey(TEXT("bUseElementColor"));
	ColorColumn = Header.IndexOfByKey(TEXT("FixedColor"));

	if (SkillColumn == INDEX_NONE || CastColumn == INDEX_NONE || ProjectileColumn == INDEX_NONE
		|| ImpactColumn == INDEX_NONE || UseElementColumn == INDEX_NONE || ColorColumn == INDEX_NONE)
	{
		OutError = FString::Printf(TEXT("CSV 머리줄에 SkillId · CastVFX · ProjectileVFX · ImpactVFX · bUseElementColor · FixedColor 가 다 없음: %s"), *Path);
		return false;
	}

	RebuildIndex();
	return true;
}

bool FNUVFXViewerTableFile::Save(FString& OutError) const
{
	FString Text = ToString();
	const FFileHelper::EEncodingOptions Encoding = bHasBom
		? FFileHelper::EEncodingOptions::ForceUTF8
		: FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM;

	if (!FFileHelper::SaveStringToFile(Text, *Path, Encoding))
	{
		OutError = FString::Printf(TEXT("CSV 를 못 씀 (다른 프로그램이 열고 있는지 확인): %s"), *Path);
		return false;
	}

	return true;
}

FString FNUVFXViewerTableFile::ToString() const
{
	FString Text;
	for (const FString& Line : Lines)
	{
		Text += Line;
		Text += LineEnd;
	}
	return Text;
}

bool FNUVFXViewerTableFile::FindRow(int32 SkillId, FNUVFXViewerTableRow& OutRow) const
{
	const int32* LineIndex = LineBySkill.Find(SkillId);
	if (!LineIndex)
	{
		return false;
	}

	const TArray<FString> Fields = SplitFields(Lines[*LineIndex]);
	const auto Field = [&Fields](int32 Column)
	{
		return Fields.IsValidIndex(Column) ? Fields[Column] : FString();
	};

	OutRow.Cast = Field(CastColumn);
	OutRow.Projectile = Field(ProjectileColumn);
	OutRow.Impact = Field(ImpactColumn);
	OutRow.bUseElementColor = Field(UseElementColumn).ToBool();
	OutRow.FixedColor = FLinearColor::White;
	OutRow.FixedColor.InitFromString(Field(ColorColumn));
	return true;
}

void FNUVFXViewerTableFile::SetRow(int32 SkillId, const FNUVFXViewerTableRow& Row, const FString& NewRowName)
{
	const int32* LineIndex = LineBySkill.Find(SkillId);
	TArray<FString> Fields = LineIndex ? SplitFields(Lines[*LineIndex]) : TArray<FString>();

	const int32 ColumnCount = SplitFields(Lines[0]).Num();
	Fields.SetNum(FMath::Max(Fields.Num(), ColumnCount));

	if (!LineIndex)
	{
		Fields[NameColumn] = NewRowName;
		Fields[SkillColumn] = FString::FromInt(SkillId);
	}

	Fields[CastColumn] = Row.Cast;
	Fields[ProjectileColumn] = Row.Projectile;
	Fields[ImpactColumn] = Row.Impact;
	Fields[UseElementColumn] = Row.bUseElementColor ? TEXT("TRUE") : TEXT("FALSE");
	Fields[ColorColumn] = FNUVFXViewerTableRow::FormatColor(Row.FixedColor);

	if (LineIndex)
	{
		Lines[*LineIndex] = JoinFields(Fields);
	}
	else
	{
		Lines.Add(JoinFields(Fields));
		RebuildIndex();
	}
}

bool FNUVFXViewerTableFile::RemoveRow(int32 SkillId)
{
	const int32* LineIndex = LineBySkill.Find(SkillId);
	if (!LineIndex)
	{
		return false;
	}

	Lines.RemoveAt(*LineIndex);
	RebuildIndex();
	return true;
}

TArray<FString> FNUVFXViewerTableFile::SplitFields(const FString& Line)
{
	// 따옴표 안의 쉼표는 칸을 나누지 않음 ("(R=1,G=1,B=1,A=1)"), "" 는 따옴표 하나
	TArray<FString> Fields;
	FString Field;
	bool bQuoted = false;
	for (int32 Index = 0; Index < Line.Len(); ++Index)
	{
		const TCHAR Char = Line[Index];
		if (bQuoted)
		{
			if (Char == TEXT('"') && Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
			{
				Field.AppendChar(TEXT('"'));
				++Index;
			}
			else if (Char == TEXT('"'))
			{
				bQuoted = false;
			}
			else
			{
				Field.AppendChar(Char);
			}
		}
		else if (Char == TEXT('"'))
		{
			bQuoted = true;
		}
		else if (Char == TEXT(','))
		{
			Fields.Add(Field);
			Field.Reset();
		}
		else
		{
			Field.AppendChar(Char);
		}
	}

	Fields.Add(Field);
	return Fields;
}

FString FNUVFXViewerTableFile::JoinFields(const TArray<FString>& Fields)
{
	// 쉼표 · 따옴표가 든 칸만 따옴표로 감쌈 (기존 행과 같은 모양)
	TArray<FString> Escaped;
	for (const FString& Field : Fields)
	{
		Escaped.Add(Field.Contains(TEXT(",")) || Field.Contains(TEXT("\""))
			? FString::Printf(TEXT("\"%s\""), *Field.Replace(TEXT("\""), TEXT("\"\"")))
			: Field);
	}

	return FString::Join(Escaped, TEXT(","));
}

bool FNUVFXViewerJobAttackIni::Load(const FString& InPath, FString& OutError)
{
	Path = InPath;
	Entries.Reset();

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Path))
	{
		OutError = FString::Printf(TEXT("ini 를 못 읽음: %s"), *Path);
		return false;
	}

	bHasBom = Bytes.Num() >= 3 && Bytes[0] == 0xEF && Bytes[1] == 0xBB && Bytes[2] == 0xBF;
	FFileHelper::BufferToString(Text, Bytes.GetData(), Bytes.Num());

	// 줄 첫머리의 키만 (주석 · 다른 키 안의 글자는 제외), 둘 이상이면 어느 것을 고칠지 몰라 멈춤
	const FString Key = TEXT("JobAttackVFX=");
	int32 Found = INDEX_NONE;
	int32 Count = 0;
	for (int32 At = Text.Find(Key, ESearchCase::CaseSensitive); At != INDEX_NONE;
		At = Text.Find(Key, ESearchCase::CaseSensitive, ESearchDir::FromStart, At + Key.Len()))
	{
		if (At == 0 || Text[At - 1] == TEXT('\n'))
		{
			Found = At;
			++Count;
		}
	}

	if (Count != 1)
	{
		OutError = FString::Printf(TEXT("%s 에 JobAttackVFX= 줄이 %d개 (하나여야 함)"), *FPaths::GetCleanFilename(Path), Count);
		return false;
	}

	ValueStart = Found + Key.Len();
	ValueEnd = ValueStart;
	while (ValueEnd < Text.Len() && Text[ValueEnd] != TEXT('\r') && Text[ValueEnd] != TEXT('\n'))
	{
		++ValueEnd;
	}

	// ((0, (CastVFX="...")),(1002, (...))) → 항목마다 (번호, 칸 글자)
	for (const FString& Item : SplitTopLevel(StripParens(Text.Mid(ValueStart, ValueEnd - ValueStart))))
	{
		const TArray<FString> Parts = SplitTopLevel(StripParens(Item));
		if (Parts.Num() != 2 || !Parts[0].IsNumeric())
		{
			OutError = FString::Printf(TEXT("JobAttackVFX 항목을 못 읽음: %s"), *Item);
			return false;
		}

		Entries.Emplace(FCString::Atoi(*Parts[0]), Parts[1]);
	}

	return true;
}

bool FNUVFXViewerJobAttackIni::Save(FString& OutError) const
{
	TArray<FString> Items;
	for (const TPair<int32, FString>& Entry : Entries)
	{
		Items.Add(FString::Printf(TEXT("(%d, %s)"), Entry.Key, *Entry.Value));
	}

	const FString NewText = Text.Left(ValueStart) + TEXT("(") + FString::Join(Items, TEXT(",")) + TEXT(")") + Text.Mid(ValueEnd);
	const FFileHelper::EEncodingOptions Encoding = bHasBom
		? FFileHelper::EEncodingOptions::ForceUTF8
		: FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM;

	if (!FFileHelper::SaveStringToFile(NewText, *Path, Encoding))
	{
		OutError = FString::Printf(TEXT("ini 를 못 씀 (다른 프로그램이 열고 있는지 확인): %s"), *Path);
		return false;
	}

	return true;
}

bool FNUVFXViewerJobAttackIni::FindEntry(int32 JobId, FString& OutEntry) const
{
	const TPair<int32, FString>* Entry = Entries.FindByPredicate([JobId](const TPair<int32, FString>& Candidate)
	{
		return Candidate.Key == JobId;
	});

	if (!Entry)
	{
		return false;
	}

	OutEntry = Entry->Value;
	return true;
}

void FNUVFXViewerJobAttackIni::SetEntry(int32 JobId, const FString& Entry)
{
	if (TPair<int32, FString>* Existing = Entries.FindByPredicate([JobId](const TPair<int32, FString>& Candidate)
		{
			return Candidate.Key == JobId;
		}))
	{
		Existing->Value = Entry;
		return;
	}

	const int32 Before = Entries.IndexOfByPredicate([JobId](const TPair<int32, FString>& Candidate)
	{
		return Candidate.Key > JobId;
	});
	Entries.Insert(TPair<int32, FString>(JobId, Entry), Before == INDEX_NONE ? Entries.Num() : Before);
}

bool FNUVFXViewerJobAttackIni::RemoveEntry(int32 JobId)
{
	return Entries.RemoveAll([JobId](const TPair<int32, FString>& Candidate)
	{
		return Candidate.Key == JobId;
	}) > 0;
}

FString FNUVFXViewerJobAttackIni::FormatEntry(const FNUVFXViewerTableRow& Row)
{
	TArray<FString> Fields;
	const auto AddPath = [&Fields](const TCHAR* Name, const FString& Value)
	{
		if (!Value.IsEmpty())
		{
			Fields.Add(FString::Printf(TEXT("%s=\"%s\""), Name, *Value));
		}
	};

	AddPath(TEXT("CastVFX"), Row.Cast);
	AddPath(TEXT("ProjectileVFX"), Row.Projectile);
	AddPath(TEXT("ImpactVFX"), Row.Impact);

	if (FNUVFXViewerTableRow::FormatColor(Row.FixedColor) != FNUVFXViewerTableRow::FormatColor(FLinearColor::White))
	{
		Fields.Add(TEXT("EffectColor=") + FNUVFXViewerTableRow::FormatColor(Row.FixedColor));
	}

	return TEXT("(") + FString::Join(Fields, TEXT(",")) + TEXT(")");
}

FNUVFXViewerTableRow FNUVFXViewerJobAttackIni::ParseEntry(const FString& Entry)
{
	FNUVFXViewerTableRow Row;
	for (const FString& Field : SplitTopLevel(StripParens(Entry)))
	{
		FString Name;
		FString Value;
		if (!Field.Split(TEXT("="), &Name, &Value))
		{
			continue;
		}

		Name.TrimStartAndEndInline();
		Value.TrimStartAndEndInline();
		if (Value.Len() >= 2 && Value.StartsWith(TEXT("\"")) && Value.EndsWith(TEXT("\"")))
		{
			Value = Value.Mid(1, Value.Len() - 2);
		}

		if (Value == TEXT("None"))
		{
			Value.Reset();
		}

		if (Name == TEXT("CastVFX"))
		{
			Row.Cast = Value;
		}
		else if (Name == TEXT("ProjectileVFX"))
		{
			Row.Projectile = Value;
		}
		else if (Name == TEXT("ImpactVFX"))
		{
			Row.Impact = Value;
		}
		else if (Name == TEXT("EffectColor"))
		{
			Row.FixedColor.InitFromString(Value);
		}
	}

	return Row;
}

TArray<FString> FNUVFXViewerJobAttackIni::SplitTopLevel(const FString& InText)
{
	TArray<FString> Parts;
	FString Part;
	int32 Depth = 0;
	bool bQuoted = false;
	for (const TCHAR Char : InText)
	{
		if (Char == TEXT('"'))
		{
			bQuoted = !bQuoted;
		}
		else if (!bQuoted && Char == TEXT('('))
		{
			++Depth;
		}
		else if (!bQuoted && Char == TEXT(')'))
		{
			--Depth;
		}
		else if (!bQuoted && Depth == 0 && Char == TEXT(','))
		{
			Part.TrimStartAndEndInline();
			if (!Part.IsEmpty())
			{
				Parts.Add(Part);
			}
			Part.Reset();
			continue;
		}

		Part.AppendChar(Char);
	}

	Part.TrimStartAndEndInline();
	if (!Part.IsEmpty())
	{
		Parts.Add(Part);
	}

	return Parts;
}

FString FNUVFXViewerJobAttackIni::StripParens(const FString& InText)
{
	const FString Trimmed = InText.TrimStartAndEnd();
	return Trimmed.Len() >= 2 && Trimmed.StartsWith(TEXT("(")) && Trimmed.EndsWith(TEXT(")"))
		? Trimmed.Mid(1, Trimmed.Len() - 2)
		: Trimmed;
}

void FNUVFXViewerTableFile::RebuildIndex()
{
	LineBySkill.Reset();
	for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
	{
		const TArray<FString> Fields = SplitFields(Lines[LineIndex]);
		if (Fields.IsValidIndex(SkillColumn) && !Fields[SkillColumn].IsEmpty())
		{
			const int32 SkillId = FCString::Atoi(*Fields[SkillColumn]);
			if (!LineBySkill.Contains(SkillId))
			{
				LineBySkill.Add(SkillId, LineIndex);
			}
		}
	}
}
