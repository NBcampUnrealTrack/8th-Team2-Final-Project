#include "ProjectMSkillVFXTableCommandlet.h"

#include "Battle/Data/NUBattleDataSettings.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogProjectMSkillVFXTable, Log, All);

UProjectMSkillVFXTableCommandlet::UProjectMSkillVFXTableCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UProjectMSkillVFXTableCommandlet::Main(const FString& Params)
{
	UDataTable* Table = GetDefault<UNUBattleDataSettings>()->SkillVFXTable.LoadSynchronous();
	if (!IsValid(Table))
	{
		UE_LOG(LogProjectMSkillVFXTable, Error, TEXT("스킬 연출 표를 못 찾음 (NUBattleDataSettings SkillVFXTable)"));
		return 1;
	}

	FString CsvPath;
	if (!FParse::Value(*Params, TEXT("Csv="), CsvPath) && IsValid(Table->AssetImportData))
	{
		CsvPath = Table->AssetImportData->GetFirstFilename();
	}

	FString Csv;
	if (CsvPath.IsEmpty() || !FFileHelper::LoadFileToString(Csv, *CsvPath))
	{
		UE_LOG(LogProjectMSkillVFXTable, Error, TEXT("CSV 를 못 읽음: %s"), *CsvPath);
		return 2;
	}

	// 에디터의 "다시 가져오기"처럼 표를 통째로 다시 만들고, 원본 기록(경로 · MD5)도 새로 남김
	const TArray<FString> Problems = Table->CreateTableFromCSVString(Csv);
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogProjectMSkillVFXTable, Error, TEXT("CSV 문제: %s"), *Problem);
	}

	if (!Problems.IsEmpty())
	{
		return 3;
	}

	if (IsValid(Table->AssetImportData))
	{
		Table->AssetImportData->Update(FPaths::ConvertRelativePathToFull(CsvPath));
	}

	UPackage* Package = Table->GetOutermost();
	Package->MarkPackageDirty();

	const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	if (!UPackage::SavePackage(Package, Table, *Filename, SaveArgs))
	{
		UE_LOG(LogProjectMSkillVFXTable, Error, TEXT("저장 실패 (파일이 잠겼는지 확인): %s"), *Filename);
		return 4;
	}

	UE_LOG(LogProjectMSkillVFXTable, Display, TEXT("다시 채움: %s 행 %d개 <- %s"),
		*Table->GetPathName(), Table->GetRowMap().Num(), *CsvPath);
	return 0;
}
