// Fill out your copyright notice in the Description page of Project Settings.

#include "Tools/VFXViewer/NUVFXViewerDirector.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Battle/Avatar/NUBattleAvatar.h"
#include "Battle/Avatar/NUBattlePlayerAvatar.h"
#include "Battle/Avatar/NUBattleProjectile.h"
#include "Battle/Data/NUActiveSkillDefinition.h"
#include "Battle/Data/NUBattleDataSettings.h"
#include "Battle/Data/NUBattleDataSubsystem.h"
#include "Battle/Data/NUBattleSkillInfo.h"
#include "Battle/Data/NUJobSkillDefinition.h"
#include "Battle/Data/NUSkillVFXProfile.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/DataTable.h"
#include "Misc/MessageDialog.h"
#include "Misc/SecureHash.h"
#include "Battle/Encounter/NUBattleEncounterSubsystem.h"
#include "Battle/Presentation/NUBattleActionPresentation.h"
#include "Battle/Presentation/NUBattleVFXPreloadSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "CharacterProgression/Data/NUProgressionDataSubsystem.h"
#include "CharacterSelect/Data/NUAppearanceDataSettings.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/ViewportSplitScreen.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "GenericPlatform/GenericWindow.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Naia/Data/NUNaiaDataSubsystem.h"
#include "Naia/Data/NUNaiaDefinition.h"
#include "NiagaraComponent.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "NUGAS/Ability/NUBattleGameplayAbility.h"
#include "NUGAS/NUGameplayTags.h"
#include "PaperZDAnimationComponent.h"
#include "PaperZDAnimInstance.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "Tools/VFXViewer/NUVFXViewerTableFile.h"
#include "Tools/VFXViewer/SNUVFXViewerPanel.h"
#include "UObject/Linker.h"
#include "UObject/Package.h"
#include "UObject/ReferenceChainSearch.h"
#include "UObject/UObjectIterator.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"

#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif

// 에디터 창 찾기 (IsProjectEditorOpen)
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogNUVFXViewer, Log, All);

namespace
{
	// 마지막 선택을 남기는 곳, Saved 아래라 커밋되지 않음
	const TCHAR* SettingsSection = TEXT("NUVFXViewer");

	// 무대와 나이아가라 목록을 찾는 곳
	const TCHAR* StageSearchPath = TEXT("/Game/Maps/BattleArena");
	const TCHAR* VFXSearchPath = TEXT("/Game/VFX");

	// -game 에서 파일 핸들을 놓을 패키지, 폴리싱 중 에디터가 저장하는 것은 이펙트 · 머티리얼 · 텍스처뿐
	const TCHAR* ReleasePackagePrefix = TEXT("/Game/VFX/");

	// 후보 원본, 공용 폴더는 커밋해 팀이 같이 보고 · 개인 파일은 Saved 라 커밋되지 않음 (없으면 생길 때 읽음)
	const TCHAR* SharedCompareFolder = TEXT("Tools/VFXViewer/Candidates");
	const TCHAR* PersonalCompareFile = TEXT("VFXViewer/Candidates.json");

	// 적용 대기는 사람마다 다르므로 개인 것 (공용 후보 파일 옆에 두면 레포에 생김)
	const TCHAR* PendingChangesFile = TEXT("VFXViewer/Pending.json");

	// 적용 · 되돌리기의 CSV 백업과 이력, DT 다시 채우기 커맨드렛 로그 (모두 Saved 아래)
	const TCHAR* TableBackupFolder = TEXT("VFXViewer/Backups");
	const TCHAR* TableHistoryFolder = TEXT("VFXViewer/History");
	const TCHAR* ReimportLogFile = TEXT("VFXViewer/Reimport.log");

	// DT 다시 채우기 커맨드렛 (ProjectMEditor 의 UProjectMSkillVFXTableCommandlet)
	const TCHAR* ReimportCommandlet = TEXT("ProjectMSkillVFXTable");

	// CSV 에 적힌 에셋 경로 → 보이는 이름 (/Game/.../NS_X.NS_X → NS_X, 비면 -)
	FString ShortAssetName(const FString& Path)
	{
		if (Path.IsEmpty())
		{
			return TEXT("-");
		}

		FString Name = Path;
		int32 Dot = INDEX_NONE;
		if (Name.FindLastChar(TEXT('.'), Dot))
		{
			Name = Name.Mid(Dot + 1);
		}
		return Name;
	}

	// 적용 전 · 뒤 행의 칸별 차이 ("Impact  NS_A -> NS_B", "색  속성색 -> 고정 (...)")
	TArray<FString> DescribeRowDiff(const FNUVFXViewerTableRow& Before, const FNUVFXViewerTableRow& After)
	{
		TArray<FString> Lines;
		const auto AddSlot = [&Lines](const TCHAR* Name, const FString& From, const FString& To)
		{
			if (From != To)
			{
				Lines.Add(FString::Printf(TEXT("%s  %s -> %s"), Name, *ShortAssetName(From), *ShortAssetName(To)));
			}
		};

		AddSlot(TEXT("Cast"), Before.Cast, After.Cast);
		AddSlot(TEXT("Proj"), Before.Projectile, After.Projectile);
		AddSlot(TEXT("Impact"), Before.Impact, After.Impact);

		const auto DescribeColor = [](const FNUVFXViewerTableRow& Row)
		{
			return Row.bUseElementColor
				? FString(TEXT("속성색"))
				: FString::Printf(TEXT("고정 (%.2f, %.2f, %.2f)"), Row.FixedColor.R, Row.FixedColor.G, Row.FixedColor.B);
		};

		if (Before.bUseElementColor != After.bUseElementColor
			|| FNUVFXViewerTableRow::FormatColor(Before.FixedColor) != FNUVFXViewerTableRow::FormatColor(After.FixedColor))
		{
			Lines.Add(FString::Printf(TEXT("색  %s -> %s"), *DescribeColor(Before), *DescribeColor(After)));
		}

		return Lines;
	}

	TSharedRef<FJsonObject> TableRowToJson(const FNUVFXViewerTableRow& Row)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("cast"), Row.Cast);
		Object->SetStringField(TEXT("projectile"), Row.Projectile);
		Object->SetStringField(TEXT("impact"), Row.Impact);
		Object->SetBoolField(TEXT("useElementColor"), Row.bUseElementColor);
		Object->SetStringField(TEXT("color"), FNUVFXViewerTableRow::FormatColor(Row.FixedColor));
		return Object;
	}

	FNUVFXViewerTableRow TableRowFromJson(const TSharedPtr<FJsonObject>& Object)
	{
		FNUVFXViewerTableRow Row;
		if (Object.IsValid())
		{
			FString Color;
			Object->TryGetStringField(TEXT("cast"), Row.Cast);
			Object->TryGetStringField(TEXT("projectile"), Row.Projectile);
			Object->TryGetStringField(TEXT("impact"), Row.Impact);
			Object->TryGetBoolField(TEXT("useElementColor"), Row.bUseElementColor);
			if (Object->TryGetStringField(TEXT("color"), Color))
			{
				Row.FixedColor.InitFromString(Color);
			}
		}
		return Row;
	}

	// 후보 값을 덮은 행, 색 후보는 그 스킬만 고정색으로
	FNUVFXViewerTableRow ApplyOverrides(const FNUVFXViewerTableRow& Base, const FNUVFXViewerVariant& Overrides)
	{
		FNUVFXViewerTableRow Row = Base;
		if (Overrides.bOverrideCast)
		{
			Row.Cast = Overrides.Cast.ToString();
		}

		if (Overrides.bOverrideProjectile)
		{
			Row.Projectile = Overrides.Projectile.ToString();
		}

		if (Overrides.bOverrideImpact)
		{
			Row.Impact = Overrides.Impact.ToString();
		}

		if (Overrides.bOverrideColor)
		{
			Row.bUseElementColor = false;
			Row.FixedColor = Overrides.Color;
		}

		return Row;
	}

	FString ProjectRelativePath(const FString& FullPath)
	{
		FString Relative = FullPath;
		FPaths::MakePathRelativeTo(Relative, *(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("")));
		return Relative;
	}

	TSharedPtr<FJsonObject> LoadJsonFile(const FString& Path)
	{
		FString Json;
		TSharedPtr<FJsonObject> Root;
		if (FFileHelper::LoadFileToString(Json, *Path))
		{
			FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
		}
		return Root;
	}

	bool SaveJsonFile(const TSharedRef<FJsonObject>& Root, const FString& Path)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		return FFileHelper::SaveStringToFile(Json, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}

	EAppReturnType::Type ShowDialog(EAppMsgType::Type Type, const FString& Message)
	{
		return FMessageDialog::Open(Type, FText::FromString(Message), FText::FromString(TEXT("VFX 뷰어 - 적용")));
	}

	// 게임이 투사체 칸만 채워 임팩트를 띄우는 데 쓰는 보이지 않는 투사체 (DT_SkillVFX 직업 행)
	const TCHAR* CarrierAssetName = TEXT("NS_Job_Proj_Carrier");

	constexpr int32 MaxEnemyCount = 3;

	// 이 월드에만 있는 전투원 번호, 넷 전투와 겹쳐도 상관없음
	constexpr int32 PlayerCombatantId = 1;
	constexpr int32 AllyNaiaCombatantId = 2;
	constexpr int32 EnemyCombatantIdBase = 10;

	// 행동이 끝난 뒤 남은 이펙트를 기다리는 최대 시간, 반복 재생은 그 뒤 다시 틂
	constexpr float LoopTailMax = 3.f;

	// 피격 큐에 넘길 피해량, 큐 블루프린트는 0 초과인지만 보고 흔들림을 가름 (값 자체는 쓰지 않음)
	constexpr float PreviewHitMagnitude = 1.f;

	// 자동 캡처, 무대가 뜬 뒤 · 예열 재생 뒤 화면이 안정될 때까지 기다리는 프레임
	constexpr int32 AutomationSettleFrameCount = 60;

	// 감기, 이만큼 넘게 남으면 직전 화면을 덮고 월드를 안 그림 (그리기가 감는 시간의 대부분이라)
	// 프레임 넘기기 · 짧은 감기는 그대로 그려 덮개가 깜빡이지 않게 함
	constexpr int32 FastSeekMinSteps = 6;

	// 목표 시각에 닿았다고 볼 차이 (초)
	constexpr double SeekTolerance = 0.0005;

	// 직전 화면 캡처를 기다리는 최대 프레임, 넘으면 덮개 없이 감음
	constexpr int32 FreezeWaitMaxFrames = 5;

	// -VFXSeekShots, 감은 뒤 멈춘 화면이 가라앉기(TSR 누적)를 기다리는 프레임
	constexpr int32 SeekShotSettleFrameCount = 10;

	FString FormatColor(const FLinearColor& Color)
	{
		return FString::Printf(TEXT("(%.2f, %.2f, %.2f)"), Color.R, Color.G, Color.B);
	}

	FString MotionLabel(ENUBattleActionMotion Motion)
	{
		switch (Motion)
		{
		case ENUBattleActionMotion::InPlace:		return TEXT("제자리");
		case ENUBattleActionMotion::MoveToCenter:	return TEXT("중앙으로");
		case ENUBattleActionMotion::MoveToTarget:	return TEXT("대상에게");
		}

		return TEXT("?");
	}

	// "/Game/A/NS_X" 처럼 오브젝트 이름이 빠진 경로도 받음
	// -game 은 처음 읽을 때 나이아가라 스크립트를 컴파일함 ("Preparing Niagara Systems")
	// 끝나기 전에 틀면 그 재생이 비거나 일부 층만 보이므로 여기서 기다림
	void PrepareSystem(const TSoftObjectPtr<UNiagaraSystem>& Asset)
	{
		UNiagaraSystem* System = Asset.LoadSynchronous();
#if WITH_EDITORONLY_DATA
		if (System && !System->IsReadyToRun())
		{
			System->WaitForCompilationComplete(false, false);
		}
#endif
	}

	FSoftObjectPath ParseAssetPath(const FString& Text)
	{
		if (Text.IsEmpty())
		{
			return FSoftObjectPath();
		}

		if (Text.Contains(TEXT(".")))
		{
			return FSoftObjectPath(Text);
		}

		return FSoftObjectPath(Text + TEXT(".") + FPaths::GetBaseFilename(Text));
	}

	const FNUVFXViewerChoice* FindChoice(const TArray<FNUVFXViewerChoice>& Choices, int32 Id)
	{
		return Choices.FindByPredicate([Id](const FNUVFXViewerChoice& Choice)
		{
			return Choice.Id == Id;
		});
	}
}

ANUVFXViewerDirector::ANUVFXViewerDirector()
{
	PrimaryActorTick.bCanEverTick = true;

	// 일시정지 중에도 화면 배치 · 자동 캡처를 이어감
	PrimaryActorTick.bTickEvenWhenPaused = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

/////////////////////////////////////////////////////////////////
// 시작 · 종료
/////////////////////////////////////////////////////////////////

void ANUVFXViewerDirector::BeginPlay()
{
	Super::BeginPlay();

	BuildChoiceLists();
	BuildSkillItems();
	BuildAssetItems();
	LoadSettings();
	ParseCommandLine();

	// 저장해 둔 번호가 목록에 없으면(표가 바뀜) 첫 항목으로
	if (!StageChoices.IsValidIndex(Setup.StageIndex))
	{
		Setup.StageIndex = StageChoices.IsEmpty() ? INDEX_NONE : 0;
	}

	ValidateJobChoice();

	if (!FindChoice(NaiaChoices, Setup.AllyNaiaId) && !NaiaChoices.IsEmpty())
	{
		Setup.AllyNaiaId = NaiaChoices[0].Id;
	}

	if (!FindChoice(NaiaChoices, Setup.EnemyNaiaId) && !NaiaChoices.IsEmpty())
	{
		Setup.EnemyNaiaId = NaiaChoices[0].Id;
	}

	Setup.EnemyCount = FMath::Clamp(Setup.EnemyCount, 1, MaxEnemyCount);
	UGameplayStatics::SetGlobalTimeDilation(this, Setup.Speed);

	ScreenshotProcessedHandle = FScreenshotRequest::OnScreenshotRequestProcessed().AddUObject(
		this, &ThisClass::HandleScreenshotProcessed);
	BeginFrameHandle = FCoreDelegates::OnBeginFrame.AddUObject(this, &ThisClass::HandleBeginFrame);

	RefreshTableState();

	CreatePanel();
	bWindowFitDone = FitWindowToWorkArea();

	// 게임처럼 시작할 때 고른 스킬의 VFX 를 비동기로 부름 (끊김 측정용)
	if (bAsyncPreload)
	{
		if (UNUBattleVFXPreloadSubsystem* Preload = GetGameInstance()->GetSubsystem<UNUBattleVFXPreloadSubsystem>())
		{
			if (Setup.Mode == ENUVFXViewerMode::Skill)
			{
				Preload->PreloadSkills({ Setup.SkillId }, Setup.Caster == ENUVFXViewerCaster::Player ? Setup.JobId : INDEX_NONE);
			}
			else
			{
				Preload->PreloadSystems({ Setup.CustomCast, Setup.CustomProjectile, Setup.CustomImpact });
			}
		}
	}

	// 처음 무대가 뜨면 한 번 틀어 바로 보이게 함, 자동 캡처는 예열부터 따로 진행
	bPlayQueued = !bAutomation;
	BeginStageLoad();
}

void ANUVFXViewerDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FScreenshotRequest::OnScreenshotRequestProcessed().Remove(ScreenshotProcessedHandle);
	UGameViewportClient::OnScreenshotCaptured().Remove(FreezeCaptureHandle);

	// 빌린 고정 시간 간격 · 끈 월드 그리기를 돌려줌 (PIE 를 끝낸 뒤 에디터에 남지 않게)
	// 다음 프레임 시작을 기다릴 수 없어 바로 되돌림 (틱 밖이라 FApp 값이 유지됨)
	CancelSeek(false);
	ApplyTimeStep();
	FCoreDelegates::OnBeginFrame.Remove(BeginFrameHandle);
	StopPlayback();
	RemovePanel();

	// 다시 채우기는 끝까지 돌게 두고 핸들만 놓음 (중간에 끊으면 DT 저장이 깨질 수 있음)
	if (ReimportProcess.IsValid())
	{
		FPlatformProcess::CloseProc(ReimportProcess);
	}

	Super::EndPlay(EndPlayReason);
}

void ANUVFXViewerDirector::BuildChoiceLists()
{
	StageChoices.Reset();
	JobChoices.Reset();
	NaiaChoices.Reset();

	// 무대: BattleArena 폴더의 레벨 전부, -game 은 에셋 레지스트리가 스캔되지 않아 직접 훑음
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ StageSearchPath }, true);

	FARFilter StageFilter;
	StageFilter.PackagePaths.Add(FName(StageSearchPath));
	StageFilter.ClassPaths.Add(UWorld::StaticClass()->GetClassPathName());
	StageFilter.bRecursivePaths = true;

	TArray<FAssetData> StageAssets;
	Registry.GetAssets(StageFilter, StageAssets);
	StageAssets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.AssetName.LexicalLess(B.AssetName);
	});

	for (const FAssetData& Asset : StageAssets)
	{
		FNUVFXViewerChoice& Choice = StageChoices.AddDefaulted_GetRef();
		Choice.Id = StageChoices.Num() - 1;
		Choice.Label = Asset.AssetName.ToString();
		Choice.Path = Asset.GetSoftObjectPath();
	}

	// 직업: 몸 표에서 전투 그림이 있는 줄만, 같은 직업이 여러 줄이면 먼저 나온 줄
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUProgressionDataSubsystem* Progression =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;

	for (const FNUBodyRow& Body : GetDefault<UNUAppearanceDataSettings>()->Bodies)
	{
		if (Body.CombatBodySkin.IsNull() || FindJobChoice(Body.JobId, Body.Gender))
		{
			continue;
		}

		const FNUJobDefinitionRow* Job = IsValid(Progression) ? Progression->FindJob(Body.JobId) : nullptr;

		FNUVFXViewerChoice& Choice = JobChoices.AddDefaulted_GetRef();
		Choice.Id = Body.JobId;
		Choice.RaceId = Body.RaceId;
		Choice.Gender = Body.Gender;
		Choice.Label = Job && !Job->DisplayName.IsEmpty()
			? Job->DisplayName.ToString()
			: (Body.JobId == 0 ? FString(TEXT("기본 (직업 없음)")) : FString::Printf(TEXT("직업 %d"), Body.JobId));
	}

	// 나이아: 전투 그림(애님 BP)이 있는 것만
	const UNUNaiaDataSubsystem* NaiaData =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;

	if (IsValid(NaiaData))
	{
		for (const FNUNaiaDefinition* Definition : NaiaData->GetAllDefinitions())
		{
			if (!Definition || Definition->AnimationClass.IsNull() || Definition->DataIndex <= 0)
			{
				continue;
			}

			FNUVFXViewerChoice& Choice = NaiaChoices.AddDefaulted_GetRef();
			Choice.Id = Definition->DataIndex;
			Choice.Label = FString::Printf(TEXT("%d %s"), Definition->DataIndex, *Definition->KoreanName.ToString());
		}
	}
}

void ANUVFXViewerDirector::BuildSkillItems()
{
	SkillItems.Reset();

	// 맨 위는 기본 공격, 표에 행이 없어 캐릭터는 직업 기본 공격 연출 · 그 외는 기본 행동 어빌리티 연출
	TSharedPtr<FNUVFXViewerSkillItem> BasicAttack = MakeShared<FNUVFXViewerSkillItem>();
	BasicAttack->SkillId = 0;
	BasicAttack->Category = TEXT("기본");
	BasicAttack->Label = TEXT("기본 공격 (캐릭터는 직업별 연출)");
	BasicAttack->SearchKey = TEXT("0 기본 공격 basic");
	SkillItems.Add(BasicAttack);

	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	if (!IsValid(Data))
	{
		return;
	}

	// 직업 스킬은 어느 직업 것인지 직업 이름으로 보임, 스킬 표의 Class 가 직업 표의 JobKey 와 같은 값 (NaiaKnight · Tracker ...)
	TMap<FName, FString> JobNamesByKey;
	const UNUProgressionDataSubsystem* Progression =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUProgressionDataSubsystem>() : nullptr;
	if (IsValid(Progression))
	{
		for (const FNUJobDefinitionRow* Job : Progression->GetAllJobs())
		{
			if (Job && !Job->JobKey.IsNone())
			{
				JobNamesByKey.Add(Job->JobKey, Job->DisplayName.IsEmpty() ? Job->JobKey.ToString() : Job->DisplayName.ToString());
			}
		}
	}

	// 직업 표에 없는 Class 는 그 값 그대로, Class 가 비면 예전처럼 "직업"
	const auto GetJobCategory = [Data, &JobNamesByKey](int32 SkillId) -> FString
	{
		const FNUJobSkillDefinition* JobSkill = Data->FindJobSkillDefinition(SkillId);
		if (!JobSkill || JobSkill->Class.IsNone())
		{
			return TEXT("직업");
		}

		const FString* JobName = JobNamesByKey.Find(JobSkill->Class);
		return JobName ? *JobName : JobSkill->Class.ToString();
	};

	for (const int32 SkillId : Data->GetSkillVFXSkillIds())
	{
		TSharedPtr<FNUVFXViewerSkillItem> Item = MakeShared<FNUVFXViewerSkillItem>();
		Item->SkillId = SkillId;

		FString Name;
		bool bJobSkill = false;
		FNUBattleSkillInfo Info;
		if (Data->TryGetBattleSkillInfo(SkillId, Info))
		{
			Name = Info.DisplayName.ToString();
			bJobSkill = !Data->IsBossSkill(SkillId) && Info.Source == ENUBattleSkillSource::Job;
			Item->Category = Data->IsBossSkill(SkillId)
				? TEXT("보스")
				: (bJobSkill ? GetJobCategory(SkillId) : TEXT("나이아"));
		}
		else if (const FNUJobSkillDefinition* JobSkill = Data->FindJobSkillDefinition(SkillId))
		{
			Name = JobSkill->Name_KR.ToString();
			bJobSkill = true;
			Item->Category = GetJobCategory(SkillId);
		}
		else
		{
			Name = TEXT("(스킬 표에 없음)");
			Item->Category = TEXT("?");
		}

		Item->Label = FString::Printf(TEXT("%d  %s"), SkillId, *Name);

		// 직업 스킬은 직업 이름 · Class 값 · "직업" 어느 것으로도 찾히게 함
		Item->SearchKey = FString::Printf(TEXT("%d %s %s"), SkillId, *Name, *Item->Category);
		if (bJobSkill)
		{
			const FNUJobSkillDefinition* JobSkill = Data->FindJobSkillDefinition(SkillId);
			Item->SearchKey += FString::Printf(TEXT(" %s 직업"), JobSkill ? *JobSkill->Class.ToString() : TEXT(""));
		}

		SkillItems.Add(Item);
	}

	// 분류별 개수 (직업 이름이 Class 로 그대로 나오면 직업 표에 그 JobKey 가 없는 것)
	TMap<FString, int32> CategoryCounts;
	for (const TSharedPtr<FNUVFXViewerSkillItem>& Item : SkillItems)
	{
		++CategoryCounts.FindOrAdd(Item->Category);
	}

	TArray<FString> CountTexts;
	for (const TPair<FString, int32>& Count : CategoryCounts)
	{
		CountTexts.Add(FString::Printf(TEXT("%s %d"), *Count.Key, Count.Value));
	}
	UE_LOG(LogNUVFXViewer, Log, TEXT("스킬 목록 %d개: %s"), SkillItems.Num(), *FString::Join(CountTexts, TEXT(" · ")));
}

void ANUVFXViewerDirector::BuildAssetItems()
{
	AssetItems.Reset();

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();

	// 새로 만든 이펙트도 잡히게 매번 다시 훑음
	Registry.ScanPathsSynchronous({ VFXSearchPath }, true);

	FARFilter Filter;
	Filter.PackagePaths.Add(FName(VFXSearchPath));
	Filter.ClassPaths.Add(UNiagaraSystem::StaticClass()->GetClassPathName());
	Filter.bRecursivePaths = true;

	TArray<FAssetData> Assets;
	Registry.GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B)
	{
		return A.AssetName.LexicalLess(B.AssetName);
	});

	for (const FAssetData& Asset : Assets)
	{
		TSharedPtr<FNUVFXViewerAssetItem> Item = MakeShared<FNUVFXViewerAssetItem>();
		Item->Path = Asset.GetSoftObjectPath();
		Item->Label = Asset.AssetName.ToString();
		AssetItems.Add(Item);
	}
}

void ANUVFXViewerDirector::LoadSettings()
{
	if (!GConfig)
	{
		return;
	}

	const FString& Ini = GGameUserSettingsIni;
	FString Text;

	if (GConfig->GetString(SettingsSection, TEXT("Stage"), Text, Ini))
	{
		const FSoftObjectPath StagePath(Text);
		const int32 Found = StageChoices.IndexOfByPredicate([&StagePath](const FNUVFXViewerChoice& Choice)
		{
			return Choice.Path == StagePath;
		});
		Setup.StageIndex = Found;
	}

	int32 Value = 0;
	if (GConfig->GetInt(SettingsSection, TEXT("Caster"), Value, Ini))
	{
		Setup.Caster = static_cast<ENUVFXViewerCaster>(FMath::Clamp(Value, 0, 2));
	}

	GConfig->GetInt(SettingsSection, TEXT("JobId"), Setup.JobId, Ini);
	GConfig->GetInt(SettingsSection, TEXT("Gender"), Setup.Gender, Ini);
	GConfig->GetInt(SettingsSection, TEXT("HairId"), Setup.HairId, Ini);
	GConfig->GetInt(SettingsSection, TEXT("AllyNaiaId"), Setup.AllyNaiaId, Ini);
	GConfig->GetInt(SettingsSection, TEXT("EnemyNaiaId"), Setup.EnemyNaiaId, Ini);
	GConfig->GetInt(SettingsSection, TEXT("EnemyCount"), Setup.EnemyCount, Ini);
	GConfig->GetInt(SettingsSection, TEXT("TargetIndex"), Setup.TargetIndex, Ini);
	GConfig->GetInt(SettingsSection, TEXT("SkillId"), Setup.SkillId, Ini);
	GConfig->GetInt(SettingsSection, TEXT("CustomHitCount"), Setup.CustomHitCount, Ini);
	GConfig->GetBool(SettingsSection, TEXT("PlayHitCue"), Setup.bPlayHitCue, Ini);
	GConfig->GetBool(SettingsSection, TEXT("Loop"), Setup.bLoop, Ini);

	if (GConfig->GetInt(SettingsSection, TEXT("Mode"), Value, Ini))
	{
		Setup.Mode = Value == 1 ? ENUVFXViewerMode::Custom : ENUVFXViewerMode::Skill;
	}

	if (GConfig->GetInt(SettingsSection, TEXT("CustomMotion"), Value, Ini))
	{
		Setup.CustomMotion = static_cast<ENUBattleActionMotion>(FMath::Clamp(Value, 0, 2));
	}

	if (GConfig->GetString(SettingsSection, TEXT("CustomCast"), Text, Ini))
	{
		Setup.CustomCast = FSoftObjectPath(Text);
	}

	if (GConfig->GetString(SettingsSection, TEXT("CustomProjectile"), Text, Ini))
	{
		Setup.CustomProjectile = FSoftObjectPath(Text);
	}

	if (GConfig->GetString(SettingsSection, TEXT("CustomImpact"), Text, Ini))
	{
		Setup.CustomImpact = FSoftObjectPath(Text);
	}

	if (GConfig->GetString(SettingsSection, TEXT("CustomColor"), Text, Ini))
	{
		Setup.CustomColor.InitFromString(Text);
	}

	Setup.CustomHitCount = FMath::Max(1, Setup.CustomHitCount);
}

void ANUVFXViewerDirector::SaveSettings() const
{
	// 자동 캡처는 명령줄 값이라 사용자의 마지막 선택을 덮지 않음
	if (!GConfig || bAutomation)
	{
		return;
	}

	const FString& Ini = GGameUserSettingsIni;

	if (StageChoices.IsValidIndex(Setup.StageIndex))
	{
		GConfig->SetString(SettingsSection, TEXT("Stage"), *StageChoices[Setup.StageIndex].Path.ToString(), Ini);
	}

	GConfig->SetInt(SettingsSection, TEXT("Caster"), static_cast<int32>(Setup.Caster), Ini);
	GConfig->SetInt(SettingsSection, TEXT("JobId"), Setup.JobId, Ini);
	GConfig->SetInt(SettingsSection, TEXT("Gender"), Setup.Gender, Ini);
	GConfig->SetInt(SettingsSection, TEXT("HairId"), Setup.HairId, Ini);
	GConfig->SetInt(SettingsSection, TEXT("AllyNaiaId"), Setup.AllyNaiaId, Ini);
	GConfig->SetInt(SettingsSection, TEXT("EnemyNaiaId"), Setup.EnemyNaiaId, Ini);
	GConfig->SetInt(SettingsSection, TEXT("EnemyCount"), Setup.EnemyCount, Ini);
	GConfig->SetInt(SettingsSection, TEXT("TargetIndex"), Setup.TargetIndex, Ini);
	GConfig->SetInt(SettingsSection, TEXT("Mode"), Setup.Mode == ENUVFXViewerMode::Custom ? 1 : 0, Ini);
	GConfig->SetInt(SettingsSection, TEXT("SkillId"), Setup.SkillId, Ini);
	GConfig->SetString(SettingsSection, TEXT("CustomCast"), *Setup.CustomCast.ToString(), Ini);
	GConfig->SetString(SettingsSection, TEXT("CustomProjectile"), *Setup.CustomProjectile.ToString(), Ini);
	GConfig->SetString(SettingsSection, TEXT("CustomImpact"), *Setup.CustomImpact.ToString(), Ini);
	GConfig->SetString(SettingsSection, TEXT("CustomColor"), *Setup.CustomColor.ToString(), Ini);
	GConfig->SetInt(SettingsSection, TEXT("CustomMotion"), static_cast<int32>(Setup.CustomMotion), Ini);
	GConfig->SetInt(SettingsSection, TEXT("CustomHitCount"), Setup.CustomHitCount, Ini);
	GConfig->SetBool(SettingsSection, TEXT("PlayHitCue"), Setup.bPlayHitCue, Ini);
	GConfig->SetBool(SettingsSection, TEXT("Loop"), Setup.bLoop, Ini);
	GConfig->Flush(false, Ini);
}

void ANUVFXViewerDirector::ParseCommandLine()
{
	const TCHAR* Cmd = FCommandLine::Get();
	FString Text;
	int32 Value = 0;

	if (FParse::Value(Cmd, TEXT("VFXStage="), Text))
	{
		const int32 Found = StageChoices.IndexOfByPredicate([&Text](const FNUVFXViewerChoice& Choice)
		{
			return Choice.Label.Contains(Text);
		});
		if (Found != INDEX_NONE)
		{
			Setup.StageIndex = Found;
		}
	}

	if (FParse::Value(Cmd, TEXT("VFXCaster="), Text))
	{
		Setup.Caster = Text.Equals(TEXT("naia"), ESearchCase::IgnoreCase)
			? ENUVFXViewerCaster::AllyNaia
			: (Text.Equals(TEXT("enemy"), ESearchCase::IgnoreCase) ? ENUVFXViewerCaster::Enemy : ENUVFXViewerCaster::Player);
	}

	if (FParse::Value(Cmd, TEXT("VFXJob="), Value))
	{
		Setup.JobId = Value;
	}

	// 성별: m · f (또는 0 · 1)
	if (FParse::Value(Cmd, TEXT("VFXGender="), Text))
	{
		Setup.Gender = Text.Equals(TEXT("f"), ESearchCase::IgnoreCase) || Text == TEXT("1") ? 1 : 0;
	}

	if (FParse::Value(Cmd, TEXT("VFXHair="), Value))
	{
		Setup.HairId = FMath::Max(Value, 0);
	}

	if (FParse::Value(Cmd, TEXT("VFXNaia="), Value))
	{
		Setup.AllyNaiaId = Value;
	}

	if (FParse::Value(Cmd, TEXT("VFXEnemy="), Value))
	{
		Setup.EnemyNaiaId = Value;
	}

	if (FParse::Value(Cmd, TEXT("VFXEnemies="), Value))
	{
		Setup.EnemyCount = Value;
	}

	if (FParse::Value(Cmd, TEXT("VFXTarget="), Value))
	{
		Setup.TargetIndex = Value;
	}

	if (FParse::Value(Cmd, TEXT("VFXSkill="), Value))
	{
		Setup.Mode = ENUVFXViewerMode::Skill;
		Setup.SkillId = Value;
	}

	// 세 칸 중 하나라도 주면 직접 지정, 안 준 칸은 비움
	FString Cast;
	FString Projectile;
	FString Impact;
	const bool bCast = FParse::Value(Cmd, TEXT("VFXCast="), Cast);
	const bool bProjectile = FParse::Value(Cmd, TEXT("VFXProj="), Projectile);
	const bool bImpact = FParse::Value(Cmd, TEXT("VFXImpact="), Impact);
	if (bCast || bProjectile || bImpact)
	{
		Setup.Mode = ENUVFXViewerMode::Custom;
		Setup.CustomCast = ParseAssetPath(Cast);
		Setup.CustomProjectile = ParseAssetPath(Projectile);
		Setup.CustomImpact = ParseAssetPath(Impact);
	}

	if (FParse::Value(Cmd, TEXT("VFXColor="), Text, false))
	{
		Setup.CustomColor.InitFromString(Text);
	}

	if (FParse::Value(Cmd, TEXT("VFXMotion="), Text))
	{
		Setup.CustomMotion = Text.Equals(TEXT("inplace"), ESearchCase::IgnoreCase)
			? ENUBattleActionMotion::InPlace
			: (Text.Equals(TEXT("target"), ESearchCase::IgnoreCase)
				? ENUBattleActionMotion::MoveToTarget
				: ENUBattleActionMotion::MoveToCenter);
	}

	if (FParse::Value(Cmd, TEXT("VFXHits="), Value))
	{
		Setup.CustomHitCount = FMath::Max(1, Value);
	}

	if (FParse::Param(Cmd, TEXT("VFXNoHitCue")))
	{
		Setup.bPlayHitCue = false;
	}

	float Speed = 1.f;
	if (FParse::Value(Cmd, TEXT("VFXSpeed="), Speed))
	{
		Setup.Speed = FMath::Clamp(Speed, 0.05f, 2.f);
	}

	bNoPrepare = FParse::Param(Cmd, TEXT("VFXNoPrepare"));
	bAsyncPreload = FParse::Param(Cmd, TEXT("VFXAsyncPreload"));
	bNoWarmUp = FParse::Param(Cmd, TEXT("VFXNoWarmUp"));
	bHitchLog = FParse::Param(Cmd, TEXT("VFXHitchLog"));

	// -VFXSeekShots: 재생 중 그 시각을 기다리지 않고 진행바처럼 감아서 찍음 (적은 순서 그대로, 뒤로 감기 확인용)
	bSeekShots = FParse::Param(Cmd, TEXT("VFXSeekShots"));

	if (FParse::Param(Cmd, TEXT("VFXHidePanel")))
	{
		bPanelVisible = false;
	}

	// 묶음 캡처: 스킬[:시전자] 을 쉼표로, 찍는 시각은 첫 타격 기준
	// 시전자 = 직업 번호(캐릭터) · n<나이아 번호>(아군 나이아) · e<나이아 번호>(적), 비우면 지금 설정
	if (FParse::Value(Cmd, TEXT("VFXSkills="), Text, false))
	{
		TArray<FString> Entries;
		Text.ParseIntoArray(Entries, TEXT(","));
		for (const FString& Entry : Entries)
		{
			FString SkillText;
			FString CasterText;
			if (!Entry.Split(TEXT(":"), &SkillText, &CasterText))
			{
				SkillText = Entry;
			}

			FQueueEntry& Queued = SkillQueue.AddDefaulted_GetRef();
			Queued.SkillId = FCString::Atoi(*SkillText);
			ParseQueueCaster(CasterText, Queued);
		}
	}

	if (FParse::Value(Cmd, TEXT("VFXShotAtHit="), Text, false))
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			ShotOffsetsFromHit.Add(FCString::Atof(*Part));
		}
		ShotOffsetsFromHit.Sort();
	}

	// 후보 원본 = -VFXCompare 파일 + 공용 폴더 + 개인 파일
	// 찍기(-VFXShotAtHit · -VFXShotAt)는 -VFXCompare 파일의 후보만 하나씩 찍고 공용 · 개인 후보는 안 읽음
	FString ExplicitCompare;
	FString ShotAtText;
	const bool bExplicitCompare = FParse::Value(Cmd, TEXT("VFXCompare="), ExplicitCompare, false);
	bUseDefaultCompareSources = ShotOffsetsFromHit.IsEmpty() && !FParse::Value(Cmd, TEXT("VFXShotAt="), ShotAtText, false);
	if (bExplicitCompare)
	{
		CompareExplicitFiles.AddUnique(FPaths::ConvertRelativePathToFull(ExplicitCompare));
	}

	if (bExplicitCompare || bUseDefaultCompareSources)
	{
		ReloadCompareSources();
	}

	if (bExplicitCompare)
	{
		for (int32 SetIndex = 0; SetIndex < CompareSets.Num() && !ShotOffsetsFromHit.IsEmpty(); ++SetIndex)
		{
			const FNUVFXViewerCompareSet& Set = CompareSets[SetIndex];
			for (int32 VariantIndex = 0; VariantIndex < Set.Variants.Num(); ++VariantIndex)
			{
				FQueueEntry& Queued = SkillQueue.AddDefaulted_GetRef();
				Queued.SkillId = Set.SkillId;
				ParseQueueCaster(Set.CasterText, Queued);
				Queued.CompareSet = SetIndex;
				Queued.CompareVariant = VariantIndex;
			}
		}

		// -VFXCompare 로 띄우면 그 파일의 첫 스킬을 표 그대로부터 (공용 · 개인 후보만이면 지금 고른 스킬 그대로)
		// 전투원은 무대가 뜬 뒤 FinishStageLoad 가 이 설정으로 세움
		if (ShotOffsetsFromHit.IsEmpty() && HasCandidates())
		{
			Setup.Mode = ENUVFXViewerMode::Skill;
			Setup.SkillId = CandidateSkillOrder[0];
			CandidateIndex = 0;
			ApplyCandidateCaster(Setup.SkillId);
		}
	}

	// 준비된 뒤 차례로 돌릴 콘솔 명령 (-VFXCmds="nu.VFX.Variant 0 2;nu.VFX.Choose;nu.VFX.Apply yes")
	if (FParse::Value(Cmd, TEXT("VFXCmds="), Text, false))
	{
		Text.ParseIntoArray(ReadyCommands, TEXT(";"));
		bQuitAfterReadyCommands = FParse::Param(Cmd, TEXT("VFXQuit"));
		UE_LOG(LogNUVFXViewer, Log, TEXT("준비된 뒤 명령 %d개: %s"), ReadyCommands.Num(), *Text);
	}

	// 자동 캡처: 재생 시작 기준 초, 쉼표로 여러 장
	if (FParse::Value(Cmd, TEXT("VFXShotAt="), Text, false))
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","));
		for (const FString& Part : Parts)
		{
			ShotTimes.Add(FCString::Atof(*Part));
		}
		if (!bSeekShots)
		{
			ShotTimes.Sort();
		}
	}

	if (!ShotTimes.IsEmpty() || (!SkillQueue.IsEmpty() && !ShotOffsetsFromHit.IsEmpty()))
	{
		// 패널까지 찍으려면 -VFXShotUI (패널 확인용), 보통은 3D 화면만
		bShotWithUI = FParse::Param(Cmd, TEXT("VFXShotUI"));

		// 패널 아래쪽 카드(적용 대기 등)를 찍을 때, 패널을 끝까지 내려 둠
		bShotPanelEnd = bShotWithUI && FParse::Param(Cmd, TEXT("VFXShotPanelEnd"));
		bShotReloadPass = FParse::Param(Cmd, TEXT("VFXShotReload"));
		bAutomation = true;
		bPanelVisible = bShotWithUI && bPanelVisible;
		Setup.bLoop = false;
		bAutomationQuit = FParse::Param(Cmd, TEXT("VFXQuit"));

		if (!FParse::Value(Cmd, TEXT("VFXOut="), ShotDirectory, false))
		{
			ShotDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VFXViewer"),
				FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
		}

		ShotDirectory = FPaths::ConvertRelativePathToFull(ShotDirectory);
		UE_LOG(LogNUVFXViewer, Log, TEXT("자동 캡처 %d장 -> %s"), ShotTimes.Num(), *ShotDirectory);
	}
}

/////////////////////////////////////////////////////////////////
// 무대 · 화면
/////////////////////////////////////////////////////////////////

void ANUVFXViewerDirector::BeginStageLoad()
{
	CancelSeek();
	State = EState::LoadingStage;
	StopPlayback();
	DestroyAvatars();

	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* Encounter = IsValid(World) ? World->GetSubsystem<UNUBattleEncounterSubsystem>() : nullptr;
	if (!IsValid(Encounter) || !StageChoices.IsValidIndex(Setup.StageIndex))
	{
		UE_LOG(LogNUVFXViewer, Error, TEXT("무대를 올릴 수 없음 (%s 아래에 레벨이 없거나 인카운터 서브시스템 없음)"),
			StageSearchPath);
		FinishStageLoad();
		return;
	}

	// 세트 조명은 이미 적용 중이면 다시 안 켜지므로, 무대를 갈 때는 먼저 되돌림
	Encounter->RestoreFieldLighting();
	Encounter->EnsureStageLoaded(TSoftObjectPtr<UWorld>(StageChoices[Setup.StageIndex].Path));
}

void ANUVFXViewerDirector::FinishStageLoad()
{
	UWorld* World = GetWorld();
	UNUBattleEncounterSubsystem* Encounter = IsValid(World) ? World->GetSubsystem<UNUBattleEncounterSubsystem>() : nullptr;
	APlayerController* PC = IsValid(World) ? World->GetFirstPlayerController() : nullptr;

	if (IsValid(Encounter))
	{
		Encounter->ApplyStageLighting();

		// 게임과 같은 세트 카메라 (ANUPlayerController::AdoptStageCamera 와 같은 처리)
		AActor* StageCamera = Encounter->GetStageViewTarget();
		if (IsValid(StageCamera) && IsValid(PC))
		{
			if (UCameraComponent* CameraComponent = StageCamera->FindComponentByClass<UCameraComponent>())
			{
				CameraComponent->SetConstraintAspectRatio(false);
			}

			PC->SetViewTarget(StageCamera);
		}
		else
		{
			UE_LOG(LogNUVFXViewer, Warning, TEXT("무대에 카메라가 없어 기본 시점으로 봄"));
		}
	}

	SpawnAvatars();
	State = EState::Ready;

	ReleaseFileHandles();

	if (bPlayQueued)
	{
		Play();
	}
}

void ANUVFXViewerDirector::CreatePanel()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (!Viewport)
	{
		return;
	}

	// 패널 · 진행바는 IsPanelVisible 을 보고 스스로 숨음, 감기 덮개는 패널을 숨겨도 떠야 해서 위젯 자체는 늘 둠
	Panel = SNew(SNUVFXViewerPanel).Director(this);
	Viewport->AddViewportWidgetContent(Panel.ToSharedRef(), 50);
}

void ANUVFXViewerDirector::RemovePanel()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (Viewport && Panel.IsValid())
	{
		Viewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
	}

	Panel.Reset();
}

void ANUVFXViewerDirector::TogglePanel()
{
	bPanelVisible = !bPanelVisible;
}

void ANUVFXViewerDirector::UpdateViewLayout()
{
	UWorld* World = GetWorld();
	UGameViewportClient* Viewport = IsValid(World) ? World->GetGameViewport() : nullptr;
	if (!Viewport || Viewport->SplitscreenInfo.Num() <= ESplitScreenType::None
		|| Viewport->SplitscreenInfo[ESplitScreenType::None].PlayerData.IsEmpty())
	{
		return;
	}

	FVector2D ViewportSize = FVector2D::ZeroVector;
	Viewport->GetViewportSize(ViewportSize);
	if (ViewportSize.X <= 1.0 || ViewportSize.Y <= 1.0)
	{
		return;
	}

	// 패널은 왼쪽, 3D 화면은 오른쪽에 게임과 같은 16:9 로 (가로 화각 45 가 그대로라 구도가 게임과 같음)
	// 그 아래에 진행바 줄을 남김, 창이 납작하면 3D 화면이 16:9 보다 넓어짐 (가로 화각은 그대로)
	const float ViewportWidth = static_cast<float>(ViewportSize.X);
	const float ViewportHeight = static_cast<float>(ViewportSize.Y);
	const float Scale = FMath::Max(GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(
		FIntPoint(FMath::RoundToInt(ViewportWidth), FMath::RoundToInt(ViewportHeight))), KINDA_SMALL_NUMBER);

	float AreaLeft = 0.f;
	float AreaWidth = ViewportWidth;
	float AreaHeight = ViewportHeight;
	if (bPanelVisible)
	{
		AreaLeft = FMath::Min(PanelWidth * Scale, ViewportWidth * 0.5f);
		AreaWidth = ViewportWidth - AreaLeft;
		AreaHeight = FMath::Max(FMath::Min(ViewportHeight - TransportHeight * Scale, AreaWidth * 9.f / 16.f), 1.f);
	}

	ViewAreaPixels = FIntRect(
		FMath::RoundToInt(AreaLeft),
		0,
		FMath::RoundToInt(AreaLeft + AreaWidth),
		FMath::RoundToInt(AreaHeight));
	ViewAreaSlateSize = FVector2D(AreaWidth / Scale, AreaHeight / Scale);

	const FPerPlayerSplitscreenData Layout(
		AreaWidth / ViewportWidth,
		AreaHeight / ViewportHeight,
		AreaLeft / ViewportWidth,
		0.f);

	FPerPlayerSplitscreenData& Current = Viewport->SplitscreenInfo[ESplitScreenType::None].PlayerData[0];
	if (FMath::IsNearlyEqual(Current.SizeX, Layout.SizeX) && FMath::IsNearlyEqual(Current.SizeY, Layout.SizeY)
		&& FMath::IsNearlyEqual(Current.OriginX, Layout.OriginX) && FMath::IsNearlyEqual(Current.OriginY, Layout.OriginY))
	{
		return;
	}

	Current = Layout;
	Viewport->LayoutPlayers();
}

bool ANUVFXViewerDirector::FitWindowToWorkArea()
{
	// PIE 창은 에디터가 다룸
	if (GIsEditor || !FSlateApplication::IsInitialized())
	{
		return true;
	}

	UWorld* World = GetWorld();
	UGameViewportClient* ViewportClient = IsValid(World) ? World->GetGameViewport() : nullptr;
	const TSharedPtr<SWindow> Window = ViewportClient ? ViewportClient->GetWindow() : nullptr;
	const TSharedPtr<FGenericWindow> NativeWindow = Window.IsValid() ? Window->GetNativeWindow() : nullptr;
	if (!NativeWindow.IsValid())
	{
		// 아직 창이 없음, 다음 틱에 다시
		return false;
	}

	if (Window->GetWindowMode() != EWindowMode::Windowed)
	{
		return true;
	}

	// 위치 · 크기는 창 안쪽 기준, 바깥 틀은 위로 제목 표시줄 + 테두리 · 옆과 아래로 테두리만큼 더 나옴
	const FVector2D ClientPosition = Window->GetPositionInScreen();
	const FVector2D ClientSize = Window->GetSizeInScreen();
	if (ClientSize.X <= 1.0 || ClientSize.Y <= 1.0)
	{
		return false;
	}

	const double Border = NativeWindow->GetWindowBorderSize();
	const double Top = NativeWindow->GetWindowTitleBarSize() + Border;

	FDisplayMetrics DisplayMetrics;
	FSlateApplication::Get().GetCachedDisplayMetrics(DisplayMetrics);
	const FPlatformRect WorkArea = DisplayMetrics.GetMonitorWorkAreaFromPoint(ClientPosition + ClientSize * 0.5);
	const double WorkWidth = WorkArea.Right - WorkArea.Left;
	const double WorkHeight = WorkArea.Bottom - WorkArea.Top;
	if (WorkWidth <= 1.0 || WorkHeight <= 1.0)
	{
		return true;
	}

	const bool bFits = ClientPosition.X - Border >= WorkArea.Left
		&& ClientPosition.Y - Top >= WorkArea.Top
		&& ClientPosition.X + ClientSize.X + Border <= WorkArea.Right
		&& ClientPosition.Y + ClientSize.Y + Border <= WorkArea.Bottom;
	if (bFits)
	{
		return true;
	}

	// 비율은 지키고 작업 영역에 들어가는 만큼만 줄여 가운데에 둠
	const double MaxWidth = WorkWidth - Border * 2.0;
	const double MaxHeight = WorkHeight - Top - Border;
	const double Fit = FMath::Min3(1.0, MaxWidth / ClientSize.X, MaxHeight / ClientSize.Y);
	const FVector2D NewSize(FMath::FloorToDouble(ClientSize.X * Fit), FMath::FloorToDouble(ClientSize.Y * Fit));
	const FVector2D NewPosition(
		WorkArea.Left + Border + FMath::FloorToDouble((MaxWidth - NewSize.X) * 0.5),
		WorkArea.Top + Top + FMath::FloorToDouble((MaxHeight - NewSize.Y) * 0.5));

	Window->ReshapeWindow(NewPosition, NewSize);

	UE_LOG(LogNUVFXViewer, Log, TEXT("창을 작업 영역에 맞춤: 안쪽 %.0fx%.0f @(%.0f,%.0f) -> %.0fx%.0f @(%.0f,%.0f), 작업 영역 %.0fx%.0f @(%d,%d), 제목 표시줄 %.0f"),
		ClientSize.X, ClientSize.Y, ClientPosition.X, ClientPosition.Y,
		NewSize.X, NewSize.Y, NewPosition.X, NewPosition.Y,
		WorkWidth, WorkHeight, WorkArea.Left, WorkArea.Top, Top);
	return true;
}

/////////////////////////////////////////////////////////////////
// 전투원
/////////////////////////////////////////////////////////////////

void ANUVFXViewerDirector::SpawnAvatars()
{
	DestroyAvatars();

	PlayerAvatar = SpawnAvatar(ENUVFXViewerCaster::Player, 0, 1, PlayerCombatantId);
	AllyNaiaAvatar = SpawnAvatar(ENUVFXViewerCaster::AllyNaia, 0, 1, AllyNaiaCombatantId);

	for (int32 Index = 0; Index < Setup.EnemyCount; ++Index)
	{
		if (ANUBattleAvatar* Enemy = SpawnAvatar(ENUVFXViewerCaster::Enemy, Index, Setup.EnemyCount,
			EnemyCombatantIdBase + Index))
		{
			EnemyAvatars.Add(Enemy);
		}
	}

	ClampTargetIndex();
}

void ANUVFXViewerDirector::DestroyAvatars()
{
	for (ANUBattleAvatar* Avatar : { PlayerAvatar.Get(), AllyNaiaAvatar.Get() })
	{
		if (IsValid(Avatar))
		{
			Avatar->Destroy();
		}
	}

	for (ANUBattleAvatar* Enemy : EnemyAvatars)
	{
		if (IsValid(Enemy))
		{
			Enemy->Destroy();
		}
	}

	PlayerAvatar = nullptr;
	AllyNaiaAvatar = nullptr;
	EnemyAvatars.Reset();
}

ANUBattleAvatar* ANUVFXViewerDirector::SpawnAvatar(ENUVFXViewerCaster Kind, int32 SlotIndex, int32 SlotCount,
	int32 CombatantId)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	// 넷 전투(UNUBattleNetStageSubsystem)와 같은 아바타 클래스 · 같은 초기화 순서
	const UNUBattleDataSettings* Settings = GetDefault<UNUBattleDataSettings>();
	const bool bIsPlayerVisual = Kind == ENUVFXViewerCaster::Player;
	const bool bIsAlly = Kind != ENUVFXViewerCaster::Enemy;

	TSoftClassPtr<ANUBattleAvatar> SoftClass = Settings->NetEnemyAvatarClass;
	ENUBattleFormationRow Row = ENUBattleFormationRow::Enemy;
	int32 NaiaId = Setup.EnemyNaiaId;

	if (Kind == ENUVFXViewerCaster::Player)
	{
		SoftClass = Settings->NetPlayerAvatarClass;
		Row = ENUBattleFormationRow::Player;
		NaiaId = INDEX_NONE;
	}
	else if (Kind == ENUVFXViewerCaster::AllyNaia)
	{
		SoftClass = Settings->NetSpiritAvatarClass;
		Row = ENUBattleFormationRow::Naia;
		NaiaId = Setup.AllyNaiaId;
	}

	UClass* AvatarClass = SoftClass.LoadSynchronous();
	if (!AvatarClass)
	{
		UE_LOG(LogNUVFXViewer, Error, TEXT("아바타 클래스가 설정되지 않음 (NUBattleDataSettings Net*AvatarClass)"));
		return nullptr;
	}

	const UNUBattleEncounterSubsystem* Encounter = World->GetSubsystem<UNUBattleEncounterSubsystem>();
	FTransform SpawnTransform = FTransform::Identity;
	if (!IsValid(Encounter) || !Encounter->ResolveFormationTransform(Row, SlotIndex, SlotCount, SpawnTransform))
	{
		const FVector Arena = IsValid(Encounter) ? Encounter->GetArenaLocation() : FVector::ZeroVector;
		SpawnTransform = FTransform(FRotator::ZeroRotator,
			Arena + NUBattlePresentation::GetFallbackFormationOffset(Row, SlotIndex));
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ANUBattleAvatar* Avatar = World->SpawnActor<ANUBattleAvatar>(AvatarClass, SpawnTransform, SpawnParams);
	if (!IsValid(Avatar))
	{
		return nullptr;
	}

	// 외형은 InitializeNetAvatar 가 입히므로 그 전에 넣어야 함, 머리 · 얼굴 0 은 그 몸의 첫 파츠
	// 머리는 고른 번호(-VFXHair · nu.VFX.Hair), 그 머리의 전투 그림이 없으면 몸 줄의 기본 전투 머리로 섬
	if (bIsPlayerVisual)
	{
		if (ANUBattlePlayerAvatar* PlayerVisual = Cast<ANUBattlePlayerAvatar>(Avatar))
		{
			const FNUVFXViewerChoice* Job = FindJobChoice(Setup.JobId, Setup.Gender);
			PlayerVisual->SetServerAppearance(Job ? Job->RaceId : 0, Job ? Job->Gender : Setup.Gender, Setup.HairId, 0, Setup.JobId);
		}
	}

	Avatar->InitializeNetAvatar(CombatantId, NaiaId, bIsPlayerVisual,
		ANUBattleAvatar::ResolveBattleFacing(bIsAlly, bIsPlayerVisual));
	Avatar->SetIsAlly(bIsAlly);

	// 이름 · 체력바가 이펙트를 가리지 않게 숨김
	Avatar->SetVitalBarHidden(true);

	// 기본 행동 연출(직업 기본 공격 포함)을 미리 불러 첫 재생의 끊김을 없앰
	if (!bNoPrepare)
	{
		const UClass* DefaultAbilityClass = Settings->NetDefaultActionAbility.LoadSynchronous();
		const UNUBattleGameplayAbility* DefaultAbility =
			DefaultAbilityClass ? DefaultAbilityClass->GetDefaultObject<UNUBattleGameplayAbility>() : nullptr;
		Avatar->PreloadActionVFX({}, { DefaultAbility });
	}

	return Avatar;
}

ANUBattleAvatar* ANUVFXViewerDirector::GetCaster() const
{
	switch (Setup.Caster)
	{
	case ENUVFXViewerCaster::Player:	return PlayerAvatar;
	case ENUVFXViewerCaster::AllyNaia:	return AllyNaiaAvatar;
	case ENUVFXViewerCaster::Enemy:		return EnemyAvatars.IsEmpty() ? nullptr : EnemyAvatars[0].Get();
	}

	return nullptr;
}

ANUBattleAvatar* ANUVFXViewerDirector::GetTarget() const
{
	// 자신 · 아군에게 쓰는 스킬(버프 · 회복)은 게임처럼 그쪽으로 감, 고른 대상 칸은 적에게 쓰는 스킬에만 씀
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	FNUBattleSkillInfo SkillInfo;
	if (Setup.Mode == ENUVFXViewerMode::Skill && Setup.SkillId > 0 && IsValid(Data)
		&& Data->TryGetBattleSkillInfo(Setup.SkillId, SkillInfo) && SkillInfo.TargetPolicy.IsSet())
	{
		const ENUTargetPolicy Policy = SkillInfo.TargetPolicy.GetValue();
		if (Policy == ENUTargetPolicy::Self)
		{
			return GetCaster();
		}

		// 아군 대상은 시전자 옆 전투원, 없으면 시전자 자신
		if (Policy == ENUTargetPolicy::SingleAlly || Policy == ENUTargetPolicy::AllAllies)
		{
			ANUBattleAvatar* Ally = nullptr;
			switch (Setup.Caster)
			{
			case ENUVFXViewerCaster::Player:	Ally = AllyNaiaAvatar.Get(); break;
			case ENUVFXViewerCaster::AllyNaia:	Ally = PlayerAvatar.Get(); break;
			case ENUVFXViewerCaster::Enemy:		Ally = EnemyAvatars.IsValidIndex(1) ? EnemyAvatars[1].Get() : nullptr; break;
			}

			return IsValid(Ally) ? Ally : GetCaster();
		}
	}

	if (Setup.Caster == ENUVFXViewerCaster::Enemy)
	{
		return Setup.TargetIndex == 1 ? AllyNaiaAvatar.Get() : PlayerAvatar.Get();
	}

	return EnemyAvatars.IsValidIndex(Setup.TargetIndex) ? EnemyAvatars[Setup.TargetIndex].Get() : nullptr;
}

void ANUVFXViewerDirector::ClampTargetIndex()
{
	const int32 MaxIndex = Setup.Caster == ENUVFXViewerCaster::Enemy ? 1 : FMath::Max(0, Setup.EnemyCount - 1);
	Setup.TargetIndex = FMath::Clamp(Setup.TargetIndex, 0, MaxIndex);
}

/////////////////////////////////////////////////////////////////
// 재생
/////////////////////////////////////////////////////////////////

FNUBattleActionMotionSpec ANUVFXViewerDirector::BuildSpec(int32& OutHitCount, bool& bOutDealsDamage,
	bool& bOutHasProfile) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	ANUBattleAvatar* Caster = GetCaster();
	ANUBattleAvatar* Target = GetTarget();
	const bool bCasterIsAlly = Setup.Caster != ENUVFXViewerCaster::Enemy;

	// 타격 수는 표의 값, 넷 전투는 서버 타격 칸 수라 다를 수 있음
	OutHitCount = 1;
	bOutDealsDamage = true;
	if (Setup.Mode == ENUVFXViewerMode::Skill && Setup.SkillId > 0 && IsValid(Data))
	{
		FNUBattleSkillInfo Info;
		if (Data->TryGetBattleSkillInfo(Setup.SkillId, Info))
		{
			OutHitCount = FMath::Max(1, Info.HitCount);
			bOutDealsDamage = Info.bDealsDamage;
		}
	}
	else if (Setup.Mode == ENUVFXViewerMode::Custom)
	{
		OutHitCount = FMath::Max(1, Setup.CustomHitCount);
	}

	FNUBattleActionMotionSpec Spec;
	NUBattlePresentation::ApplyNetPreset(Spec, bCasterIsAlly, OutHitCount);

	if (Setup.Mode == ENUVFXViewerMode::Skill)
	{
		bOutHasProfile = NUBattlePresentation::ApplySkillPresentation(Spec, Data, Setup.SkillId);

		// 비교 후보는 표 행 위에 적은 칸만 덮음, 표 행처럼 취급해 직업 기본 공격 연출로 바뀌지 않게 함
		// 캐릭터 기본 공격은 그 직업의 기본 공격 연출(ini) 위에 덮음 - 적용도 그 항목에 씀
		const FNUVFXViewerVariant* Variant = GetActiveVariant();
		if (Variant && Variant->HasOverride())
		{
			FNUVFXViewerTableRow JobRow;
			if (Setup.SkillId == 0 && Setup.Caster == ENUVFXViewerCaster::Player && FindJobAttackRow(Setup.JobId, JobRow))
			{
				Spec.CastVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(JobRow.Cast));
				Spec.ProjectileVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(JobRow.Projectile));
				Spec.ImpactVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(JobRow.Impact));
				Spec.EffectColor = JobRow.FixedColor;
			}

			if (Variant->bOverrideCast)
			{
				Spec.CastVFX = TSoftObjectPtr<UNiagaraSystem>(Variant->Cast);
			}

			if (Variant->bOverrideProjectile)
			{
				Spec.ProjectileVFX = TSoftObjectPtr<UNiagaraSystem>(Variant->Projectile);
			}

			if (Variant->bOverrideImpact)
			{
				Spec.ImpactVFX = TSoftObjectPtr<UNiagaraSystem>(Variant->Impact);
			}

			if (Variant->bOverrideColor)
			{
				Spec.EffectColor = Variant->Color;
			}

			Spec.bHasSkillVFXProfile = true;
			bOutHasProfile = true;
		}
	}
	else
	{
		// 기본 행동의 공격 모션은 그대로 쓰고 세 칸 · 색 · 이동만 바꿈
		// 표 행처럼 취급해 직업 기본 공격 연출로 바뀌지 않게 함
		NUBattlePresentation::ApplySkillPresentation(Spec, Data, 0);
		Spec.CastVFX = TSoftObjectPtr<UNiagaraSystem>(Setup.CustomCast);
		Spec.ProjectileVFX = TSoftObjectPtr<UNiagaraSystem>(Setup.CustomProjectile);
		Spec.ImpactVFX = TSoftObjectPtr<UNiagaraSystem>(Setup.CustomImpact);
		Spec.EffectColor = Setup.CustomColor;
		Spec.Motion = Setup.CustomMotion;
		Spec.bHasSkillVFXProfile = true;
		bOutHasProfile = true;
	}

	// 넷 전투처럼 진형 중심으로 행동 위치를 잡음, 전투원은 늘 제자리(Home)에서 시작함
	FVector AllySum = FVector::ZeroVector;
	int32 AllyCount = 0;
	for (const ANUBattleAvatar* Ally : { PlayerAvatar.Get(), AllyNaiaAvatar.Get() })
	{
		if (IsValid(Ally))
		{
			AllySum += Ally->GetHomeTransform().GetLocation();
			++AllyCount;
		}
	}

	FVector EnemySum = FVector::ZeroVector;
	for (const ANUBattleAvatar* Enemy : EnemyAvatars)
	{
		if (IsValid(Enemy))
		{
			EnemySum += Enemy->GetHomeTransform().GetLocation();
		}
	}

	const UWorld* World = GetWorld();
	const UNUBattleEncounterSubsystem* Encounter = IsValid(World) ? World->GetSubsystem<UNUBattleEncounterSubsystem>() : nullptr;
	const FVector CasterHome = IsValid(Caster) ? Caster->GetHomeTransform().GetLocation() : FVector::ZeroVector;
	const FVector TargetHome = IsValid(Target) ? Target->GetHomeTransform().GetLocation() : FVector::ZeroVector;

	Spec.ActionLocation = CasterHome;
	if (AllyCount > 0 && !EnemyAvatars.IsEmpty() && IsValid(Encounter))
	{
		Spec.ActionLocation = NUBattlePresentation::ResolveNetActionLocation(
			Spec.Motion,
			CasterHome,
			bCasterIsAlly,
			AllySum / AllyCount,
			EnemySum / EnemyAvatars.Num(),
			TargetHome,
			IsValid(Target),
			Encounter->GetArenaLocation()
			);
	}

	Spec.ProjectileTargetLocation = TargetHome;
	return Spec;
}

void ANUVFXViewerDirector::Play()
{
	if (State != EState::Ready)
	{
		bPlayQueued = true;
		return;
	}

	bPlayQueued = false;
	CancelSeek();
	StartPlayback();
	SaveSettings();
}

void ANUVFXViewerDirector::Replay()
{
	Play();
	SetWorldPaused(false);
}

void ANUVFXViewerDirector::StartPlayback()
{
	StopPlayback();

	ANUBattleAvatar* Caster = GetCaster();
	ANUBattleAvatar* Target = GetTarget();

	Report = FNUVFXViewerPlayReport();
	Timeline = FNUVFXViewerTimeline();
	LastElapsed = 0.f;
	if (!IsValid(Caster) || !IsValid(Target))
	{
		Report.Warnings.Add(TEXT("시전자나 대상이 없음 (아바타 스폰 실패, 로그 확인)"));
		return;
	}

	// 직전 재생이 끝나기 전에 다시 틀어도 늘 깨끗한 무대 · 제자리에서 시작
	ResetPlaybackScene();

	int32 HitCount = 1;
	bool bDealsDamage = true;
	bool bHasProfile = false;
	FNUBattleActionMotionSpec Spec = BuildSpec(HitCount, bDealsDamage, bHasProfile);

	// 아바타가 시전 시각에 동기 로드하는 것을 미리 불러 컴파일까지 끝냄
	// 직업 기본 공격 연출은 아바타를 세울 때(PreloadActionVFX) 이미 불러 둠
	if (!bNoPrepare)
	{
		PrepareSystem(Spec.CastVFX);
		PrepareSystem(Spec.ProjectileVFX);
		PrepareSystem(Spec.ImpactVFX);
	}

	// 넷 전투(UNUBattleNetActionQueueSubsystem::PlayAction)와 같은 순서
	Caster->ResolveMotionTiming(Spec);
	Caster->MulticastPlayAction(Spec);

	// 아바타가 직업 기본 공격 연출로 바꾼 뒤의 최종 값
	Report.bValid = true;
	Report.Spec = Caster->GetActiveSpec();
	Report.bCasterIsAlly = Setup.Caster != ENUVFXViewerCaster::Enemy;
	Report.bHasSkillProfile = bHasProfile;
	Report.bDealsDamage = bDealsDamage;
	Report.HitCount = HitCount;
	Report.CastTime = Spec.ActionTime * FMath::Clamp(Spec.CastTimeNormalized, 0.f, 1.f);
	Report.HitTime = Spec.ActionTime * FMath::Clamp(Spec.HitTimeNormalized, 0.f, 1.f);
	NUBattlePresentation::ComputeTimeline(Spec, Report.FirstHitDelay, Report.TotalDuration);
	BuildTimeline();

	// 같은 연출을 다시 틀면(R · 반복 · 되감기) 잰 진행바 끝을 지키고, 연출이 바뀌면 다시 잼
	const FString Signature = FString::Printf(TEXT("%s|%s|%s|%.3f"), *Report.Spec.CastVFX.ToString(),
		*Report.Spec.ProjectileVFX.ToString(), *Report.Spec.ImpactVFX.ToString(), Report.TotalDuration);
	if (Signature != PlaybackSignature)
	{
		PlaybackSignature = Signature;
		MeasuredEndTime = 0.f;
	}

	// 직업 기본 공격으로 바뀐 칸도 준비한 뒤 경고를 모으고 파일 핸들을 놓음
	if (!bNoPrepare)
	{
		PrepareSystem(Report.Spec.CastVFX);
		PrepareSystem(Report.Spec.ProjectileVFX);
		PrepareSystem(Report.Spec.ImpactVFX);
		CollectWarnings(Report.Spec);
	}
	ReleaseFileHandles();

	UWorld* World = GetWorld();
	bPlaying = true;
	PlayStartTime = IsValid(World) ? World->GetTimeSeconds() : 0.0;

	// 피격 큐는 넷 전투처럼 첫 타격 시각부터 타격 간격마다
	RemainingHitCues = Setup.bPlayHitCue && bDealsDamage ? HitCount : 0;
	if (RemainingHitCues > 0 && IsValid(World))
	{
		if (Report.FirstHitDelay <= KINDA_SMALL_NUMBER)
		{
			PlayHitCue();
		}
		else
		{
			World->GetTimerManager().SetTimer(HitCueTimer, this, &ThisClass::PlayHitCue, Report.FirstHitDelay, false);
		}
	}

	// 자동 캡처 점검 기록은 재생마다 새로 (시전 이펙트는 위치를 시전자 +60 에 띄움 - ANUBattleAvatar::PlaySkillVFX)
	EffectSamples.Reset();
	LingeringEffects.Reset();
	PlaybackEndTime = -1.f;
	bEndedAtTailCap = false;
	CasterScreen = ProjectToView(Caster->GetActorLocation() + FVector(0.f, 0.f, 60.f));
	// 임팩트는 투사체 도착점(대상 위치 +50, ANUBattleProjectile::Launch)에 뜨므로 같은 자리와 견줌
	TargetScreen = ProjectToView(Report.Spec.ProjectileTargetLocation + FVector(0.f, 0.f, 50.f));
}

void ANUVFXViewerDirector::ResetPlaybackScene()
{
	DestroyLiveEffects();

	// 피격 큐가 건 화면 흔들림 · 암전(NUBattleHitDim 의 카메라 페이드)
	UWorld* World = GetWorld();
	APlayerController* Controller = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* Camera = IsValid(Controller) ? Controller->PlayerCameraManager.Get() : nullptr;
	if (IsValid(Camera))
	{
		Camera->StopAllCameraShakes(true);
		Camera->StopCameraFade();
		Camera->FadeAmount = 0.f;
	}

	// 밀려남 · 점등 · 공격 · 피격 모션
	TArray<ANUBattleAvatar*, TInlineAllocator<8>> Avatars = { PlayerAvatar.Get(), AllyNaiaAvatar.Get() };
	for (ANUBattleAvatar* Enemy : EnemyAvatars)
	{
		Avatars.Add(Enemy);
	}

	for (ANUBattleAvatar* Avatar : Avatars)
	{
		if (!IsValid(Avatar))
		{
			continue;
		}

		// 전투 도트 배율(UNUFieldDotZoom)이 깊이마다 넣은 액터 크기는 지킴
		// 제자리(HomeTransform)는 BeginPlay 때 크기 1 로 저장돼 있어 그대로 되돌리면 크기가 1 이 됨, 도트 배율은 월드가 흐를 때만 다시 넣어서
		// 멈춘 프레임에서 다시 틀면 그동안 캐릭터는 커 보이고 적은 작아 보임 (되감기 0 ~ 6 프레임에서 보이던 크기 튐)
		const FVector ActorScale = Avatar->GetActorScale3D();
		Avatar->ResetPresentationState();
		Avatar->SetActorScale3D(ActorScale);

		const UPaperZDAnimationComponent* Animation = Avatar->FindComponentByClass<UPaperZDAnimationComponent>();
		if (UPaperZDAnimInstance* AnimInstance = Animation ? Animation->GetAnimInstance() : nullptr)
		{
			AnimInstance->StopAllAnimationOverrides();
		}
	}
}

void ANUVFXViewerDirector::BuildTimeline()
{
	// 아바타가 도는 순서 그대로 (ANUBattleAvatar::BeginActionMotion · TickAnimation · TickCastVFX)
	const FNUBattleActionMotionSpec& Spec = Report.Spec;
	const int32 HitCount = FMath::Max(1, Spec.HitCount);
	const float Interval = FMath::Max(Spec.HitInterval, 0.f);
	const float CastOffset = Spec.ActionTime * FMath::Clamp(Spec.CastTimeNormalized, 0.f, 1.f);
	const float HitOffset = Spec.ActionTime * FMath::Clamp(Spec.HitTimeNormalized, 0.f, 1.f);

	// 반복 모션은 첫 모션의 Hit 보다 한 간격 앞에서 틀므로, 그보다 앞에 있는 시전 지점은 모션 시작으로 당겨짐
	const float RepeatStart = Interval > KINDA_SMALL_NUMBER ? FMath::Max(HitOffset - Interval, 0.f) : 0.f;

	Timeline.TelegraphEnd = FMath::Max(Spec.TelegraphTime, 0.f);
	Timeline.ApproachEnd = Timeline.TelegraphEnd
		+ (Spec.Motion == ENUBattleActionMotion::InPlace ? 0.f : FMath::Max(Spec.ApproachTime, 0.f));
	Timeline.ActingEnd = Timeline.ApproachEnd + FMath::Max(Spec.ActionTime, 0.f) + (HitCount - 1) * Interval;
	Timeline.ReturnEnd = Timeline.ActingEnd + FMath::Max(Spec.ReturnTime, 0.f);

	for (int32 Index = 0; Index < HitCount; ++Index)
	{
		const float CastPosition = Index == 0 ? CastOffset : FMath::Max(CastOffset, RepeatStart);
		Timeline.CastTimes.Add(Timeline.ApproachEnd + Index * Interval + CastPosition);
		Timeline.HitTimes.Add(Timeline.ApproachEnd + Index * Interval + HitOffset);
	}
}

double ANUVFXViewerDirector::GetElapsed() const
{
	const UWorld* World = GetWorld();
	return IsValid(World) ? World->GetTimeSeconds() - PlayStartTime : 0.0;
}

float ANUVFXViewerDirector::GetShotAnchorTime() const
{
	return Timeline.HitTimes.IsEmpty() ? Report.FirstHitDelay : Timeline.HitTimes[0];
}

void ANUVFXViewerDirector::PlayHitCue()
{
	if (RemainingHitCues <= 0)
	{
		return;
	}

	--RemainingHitCues;
	NUBattlePresentation::ExecuteBattleCue(GetTarget(), GetCaster(), GameplayCue::Battle::Hit, PreviewHitMagnitude);

	UWorld* World = GetWorld();
	if (RemainingHitCues > 0 && IsValid(World))
	{
		const float Interval = FMath::Max(Report.Spec.HitInterval, KINDA_SMALL_NUMBER);
		World->GetTimerManager().SetTimer(HitCueTimer, this, &ThisClass::PlayHitCue, Interval, false);
	}
}

void ANUVFXViewerDirector::StopPlayback()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitCueTimer);
	}

	RemainingHitCues = 0;
	bPlaying = false;
}

void ANUVFXViewerDirector::CollectWarnings(const FNUBattleActionMotionSpec& Resolved)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	if (Resolved.ProjectileVFX.IsNull() && !Resolved.ImpactVFX.IsNull())
	{
		Report.Warnings.Add(TEXT("임팩트는 투사체가 도착할 때만 뜸 - 게임에서도 안 보임 (직접 지정의 [운반체 투사체])"));
	}

	if (Setup.Mode == ENUVFXViewerMode::Skill && Setup.SkillId > 0 && IsValid(Data))
	{
		const FNUSkillVFXProfile* Profile = Data->FindSkillVFXProfile(Setup.SkillId);
		if (!Profile)
		{
			Report.Warnings.Add(TEXT("DT_SkillVFX 행 없음 - 기본 연출로 나감 (캐릭터는 직업 기본 공격)"));
		}
		else if (Profile->bUseElementColor && !Data->FindActiveSkillDefinition(Setup.SkillId))
		{
			Report.Warnings.Add(TEXT("속성색이 켜져 있지만 직업 스킬이라 행의 고정색을 씀"));
		}

		if (Report.Spec.Motion == ENUBattleActionMotion::InPlace)
		{
			Report.Warnings.Add(TEXT("피해 줄이 없는 스킬이라 제자리에서 씀"));
		}
	}

	if (Setup.Caster != ENUVFXViewerCaster::Player)
	{
		const UNUNaiaDataSubsystem* NaiaData =
			IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUNaiaDataSubsystem>() : nullptr;
		const int32 CasterNaiaId = Setup.Caster == ENUVFXViewerCaster::AllyNaia ? Setup.AllyNaiaId : Setup.EnemyNaiaId;
		const FNUNaiaDefinition* Definition = IsValid(NaiaData) ? NaiaData->FindDefinitionById(CasterNaiaId) : nullptr;
		if (Definition && Definition->bAttackInPlace && Report.Spec.Motion != ENUBattleActionMotion::InPlace)
		{
			Report.Warnings.Add(TEXT("이 나이아는 제자리 공격(bAttackInPlace) - 앞으로 나가지 않음"));
		}
	}

	// 타격 처리 시각(ComputeTimeline)과 아바타가 실제로 도는 단계(BuildTimeline)가 어긋나면 피격 큐가 모션과 따로 놂
	// 10/14 전에는 제자리 행동이 접근을 건너뛰는데 처리 시각엔 접근 시간이 더해져 0.45초 늦었음
	if (!Timeline.HitTimes.IsEmpty() && FMath::Abs(Report.FirstHitDelay - Timeline.HitTimes[0]) > 1.f / 60.f)
	{
		Report.Warnings.Add(FString::Printf(
			TEXT("타격 처리(피격 큐 · 회복 숫자)가 타격 모션과 %+.2f초 어긋남 - ComputeTimeline 과 아바타 단계가 다름"),
			Report.FirstHitDelay - Timeline.HitTimes[0]));
	}

	for (const TSoftObjectPtr<UNiagaraSystem>* Slot : { &Resolved.CastVFX, &Resolved.ProjectileVFX, &Resolved.ImpactVFX })
	{
		if (Slot->IsNull())
		{
			continue;
		}

		const UNiagaraSystem* System = Slot->LoadSynchronous();
		if (!System)
		{
			Report.Warnings.Add(FString::Printf(TEXT("에셋을 못 읽음: %s"), *Slot->ToString()));
		}
		else if (!System->IsReadyToRun())
		{
			Report.Warnings.Add(FString::Printf(TEXT("%s 를 실행 준비하지 못함 - 에디터에서 컴파일 오류 · 저장 상태 확인"),
				*System->GetName()));
		}
	}
}

bool ANUVFXViewerDirector::HasLiveEffects() const
{
	UWorld* World = GetWorld();
	if (TActorIterator<ANUBattleProjectile>(World))
	{
		return true;
	}

	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && It->IsActive())
		{
			return true;
		}
	}

	return false;
}

void ANUVFXViewerDirector::DestroyLiveEffects()
{
	UWorld* World = GetWorld();
	for (TActorIterator<ANUBattleProjectile> It(World); It; ++It)
	{
		It->Destroy();
	}

	TArray<UNiagaraComponent*> Components;
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		if (It->GetWorld() == World && !It->IsTemplate())
		{
			Components.Add(*It);
		}
	}

	for (UNiagaraComponent* Component : Components)
	{
		Component->DestroyComponent();
	}
}

void ANUVFXViewerDirector::ReleaseFileHandles()
{
	// PIE 는 에디터가 에셋을 들고 있어 해당 없음
	if (GIsEditor || FParse::Param(FCommandLine::Get(), TEXT("VFXKeepLoaders")))
	{
		return;
	}

	FlushAsyncLoading();

	TArray<UObject*> Packages;
	for (TObjectIterator<UPackage> It; It; ++It)
	{
		if (It->GetName().StartsWith(ReleasePackagePrefix))
		{
			Packages.Add(*It);
		}
	}

	if (!Packages.IsEmpty())
	{
		ResetLoaders(Packages);
	}
}

/////////////////////////////////////////////////////////////////
// 조작
/////////////////////////////////////////////////////////////////

void ANUVFXViewerDirector::SetSpeed(float InSpeed)
{
	Setup.Speed = FMath::Clamp(InSpeed, 0.05f, 2.f);
	UGameplayStatics::SetGlobalTimeDilation(this, Setup.Speed);
}

void ANUVFXViewerDirector::TogglePause()
{
	if (State != EState::Ready)
	{
		return;
	}

	if (IsSeeking())
	{
		bResumeAfterSeek = !bResumeAfterSeek;
		return;
	}

	// 끝난 뒤면 동영상처럼 처음부터
	if (!bPlaying)
	{
		Replay();
		return;
	}

	SetWorldPaused(!UGameplayStatics::IsGamePaused(this));
}

bool ANUVFXViewerDirector::IsPaused() const
{
	if (IsSeeking())
	{
		return !bResumeAfterSeek;
	}

	return UGameplayStatics::IsGamePaused(this);
}

void ANUVFXViewerDirector::SetWorldPaused(bool bPaused)
{
	if (UGameplayStatics::IsGamePaused(this) != bPaused)
	{
		UGameplayStatics::SetGamePaused(this, bPaused);
	}
}

/////////////////////////////////////////////////////////////////
// 재생 위치 · 감기
/////////////////////////////////////////////////////////////////

bool ANUVFXViewerDirector::IsSeeking() const
{
	return SeekPhase != ESeekPhase::None;
}

float ANUVFXViewerDirector::GetPlaybackTime() const
{
	if (IsSeeking())
	{
		return SeekTarget;
	}

	return bPlaying ? static_cast<float>(GetElapsed()) : LastElapsed;
}

float ANUVFXViewerDirector::GetTimelineLength() const
{
	// 처음 재생은 끝을 아직 몰라 이펙트가 남아 있는 동안 늘어남
	float Length = FMath::Max3(Report.TotalDuration, Timeline.ReturnEnd, MeasuredEndTime);
	if (bPlaying && !IsSeeking())
	{
		Length = FMath::Max(Length, static_cast<float>(GetElapsed()));
	}

	return FMath::Max(Length, FrameStep);
}

void ANUVFXViewerDirector::SeekTo(float Time)
{
	if (State != EState::Ready || !Report.bValid)
	{
		return;
	}

	// 처음 재생이 끝을 재기 전이면 지금까지 흐른 데까지는 진행바 끝으로 남김 (감는 동안 막대가 줄지 않게)
	MeasuredEndTime = FMath::Max(MeasuredEndTime, GetTimelineLength());
	const float Target = FMath::Clamp(Time, 0.f, MeasuredEndTime);

	// 이미 그 시각이면 다시 흘리지 않음 (끝에서 → 를 누를 때 등)
	if (!IsSeeking() && FMath::Abs(Target - GetPlaybackTime()) < SeekTolerance)
	{
		return;
	}

	if (!IsSeeking())
	{
		// 재생 중이었으면 감은 뒤 이어서 재생, 진행바를 끄는 중이면 놓을 때까지 멈춤
		bResumeAfterSeek = !bScrubbing && !UGameplayStatics::IsGamePaused(this);
		SeekPhase = ESeekPhase::Run;
		SeekStartRealTime = FPlatformTime::Seconds();
		SeekStepCount = 0;
	}

	SeekTarget = Target;
}

void ANUVFXViewerDirector::StepFrames(int32 FrameCount)
{
	if (State != EState::Ready || !Report.bValid)
	{
		return;
	}

	// 한 프레임씩 볼 때는 멈춘 채로
	SeekTo(GetPlaybackTime() + FrameCount * FrameStep);
	bResumeAfterSeek = false;
}

void ANUVFXViewerDirector::StepSeconds(float Seconds)
{
	SeekTo(GetPlaybackTime() + Seconds);
}

void ANUVFXViewerDirector::BeginScrub()
{
	if (State != EState::Ready || !Report.bValid)
	{
		return;
	}

	bScrubbing = true;
	bResumeAfterScrub = !IsPaused();

	if (IsSeeking())
	{
		bResumeAfterSeek = false;
	}
	else
	{
		SetWorldPaused(true);
	}
}

void ANUVFXViewerDirector::EndScrub()
{
	if (!bScrubbing)
	{
		return;
	}

	bScrubbing = false;
	if (!bResumeAfterScrub)
	{
		return;
	}

	if (IsSeeking())
	{
		bResumeAfterSeek = true;
	}
	else
	{
		SetWorldPaused(false);
	}
}

void ANUVFXViewerDirector::TickSeek()
{
	if (!IsSeeking())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !Report.bValid)
	{
		CancelSeek();
		return;
	}

	// 이번 프레임에 월드가 흘렀는지 (프레임 시작 때 정해짐)
	const bool bFramePaused = World->IsPaused();

	// 직전 화면이 넘어오면 3D 칸을 덮고 월드 그리기를 끔, 끝내 안 오면 덮개 없이 감음
	if (SeekPhase == ESeekPhase::Capture)
	{
		if (!bFreezeCaptured && ++FreezeWaitFrames < FreezeWaitMaxFrames)
		{
			return;
		}

		ShowFreezeFrame();
		SeekPhase = ESeekPhase::Run;
	}

	// 뒤로 가거나 이미 끝난 재생이면 처음부터 다시 흘림
	double Elapsed = GetElapsed();
	const bool bRestart = !bPlaying || SeekTarget < Elapsed - SeekTolerance;
	const double From = bRestart ? 0.0 : Elapsed;
	const int32 StepsLeft = FMath::CeilToInt32((SeekTarget - From - SeekTolerance) / FrameStep);

	// 멀리 가면 이 프레임을 멈춘 채 그려 찍어 두고, 다음 틱부터 안 그리고 흘림
	// 처음부터 다시 틀기 전에 찍어야 덮개가 지금 보던 화면이 됨
	// PIE 는 에디터 뷰포트도 화면 캡처 요청을 받을 수 있어 덮개 없이 그리면서 감음 (느리지만 깜빡이지 않음)
	if (!bFreezeVisible && StepsLeft > FastSeekMinSteps && !bShotPending && !GIsEditor && !ViewAreaPixels.IsEmpty())
	{
		SetWorldPaused(true);
		RequestFreezeCapture();
		FreezeWaitFrames = 0;
		SeekPhase = ESeekPhase::Capture;
		return;
	}

	if (bRestart)
	{
		// 월드가 흐른 프레임에 다시 틀면 감독보다 뒤에 틱하는 전투원 · 이펙트만 이 프레임만큼 앞서 나감
		// 멈춘 프레임에서 틀어야 재생 시작과 연출 시계가 맞음
		if (!bFramePaused)
		{
			SetWorldPaused(true);
			return;
		}

		// 처음 재생이 끝을 재기 전에 되감으면 지금까지 본 데까지는 진행바에 남김
		if (bPlaying)
		{
			MeasuredEndTime = FMath::Max(MeasuredEndTime, static_cast<float>(Elapsed));
		}

		StartPlayback();
		Elapsed = 0.0;
	}

	const double Remaining = SeekTarget - Elapsed;
	if (Remaining <= SeekTolerance || !bPlaying)
	{
		FinishSeek();
		return;
	}

	StepWorldNextFrame(static_cast<float>(FMath::Min(Remaining, static_cast<double>(FrameStep))));
}

void ANUVFXViewerDirector::FinishSeek()
{
	// 이 프레임이 목표 시각, 이번에 그려서 보여 줌
	const bool bUsedFreeze = bFreezeVisible;
	RestoreTimeStep();
	SetWorldRenderingEnabled(true);
	HideFreezeFrame();

	SeekPhase = ESeekPhase::None;
	LastElapsed = bPlaying ? static_cast<float>(GetElapsed()) : LastElapsed;
	SetWorldPaused(!bResumeAfterSeek);

	// 끌기마다 찍히면 많으니 자동 캡처에서만 보통 로그
	const double RealMs = (FPlatformTime::Seconds() - SeekStartRealTime) * 1000.0;
	if (bAutomation)
	{
		UE_LOG(LogNUVFXViewer, Log, TEXT("감기 끝: %.3f초 (목표 %.3f, %d칸, 실제 %.0fms, 덮개 %s)"),
			LastElapsed, SeekTarget, SeekStepCount, RealMs, bUsedFreeze ? TEXT("있음") : TEXT("없음"));
	}
	else
	{
		UE_LOG(LogNUVFXViewer, Verbose, TEXT("감기 끝: %.3f초 (%d칸, 실제 %.0fms)"), LastElapsed, SeekStepCount, RealMs);
	}
}

void ANUVFXViewerDirector::CancelSeek(bool bApplyPauseIntent)
{
	RestoreTimeStep();
	SetWorldRenderingEnabled(true);
	HideFreezeFrame();

	if (!IsSeeking())
	{
		return;
	}

	SeekPhase = ESeekPhase::None;
	if (bApplyPauseIntent)
	{
		SetWorldPaused(!bResumeAfterSeek);
	}
}

void ANUVFXViewerDirector::StepWorldNextFrame(float Step)
{
	// 월드 시간 = 엔진 시간 x 속도라서 속도로 나눠 넘김, 실제 적용은 다음 프레임 시작(HandleBeginFrame)
	const AWorldSettings* WorldSettings = GetWorldSettings();
	const float Dilation = IsValid(WorldSettings) ? WorldSettings->GetEffectiveTimeDilation() : 1.f;

	bWantFixedTimeStep = true;
	WantedFixedDeltaTime = static_cast<double>(Step) / FMath::Max(Dilation, KINDA_SMALL_NUMBER);
	SetWorldPaused(false);
	++SeekStepCount;
}

void ANUVFXViewerDirector::RestoreTimeStep()
{
	bWantFixedTimeStep = false;
}

void ANUVFXViewerDirector::HandleBeginFrame()
{
	ApplyTimeStep();
}

void ANUVFXViewerDirector::ApplyTimeStep()
{
	// FApp 시간 값은 스레드별 시간 문맥에 있어 액터 틱(태스크) 안에서 바꾸면 태스크가 끝날 때 되돌아감 (FAppTime)
	// 그래서 프레임 시작(엔진이 이번 프레임 시간을 정하기 전, 태스크 밖)에서 바꿈
	if (bWantFixedTimeStep)
	{
		if (!bTimeStepOverridden)
		{
			bSavedUseFixedTimeStep = FApp::UseFixedTimeStep();
			SavedFixedDeltaTime = FApp::GetFixedDeltaTime();
			bTimeStepOverridden = true;
		}

		FApp::SetUseFixedTimeStep(true);
		FApp::SetFixedDeltaTime(WantedFixedDeltaTime);
		return;
	}

	if (bTimeStepOverridden)
	{
		FApp::SetUseFixedTimeStep(bSavedUseFixedTimeStep);
		FApp::SetFixedDeltaTime(SavedFixedDeltaTime);
		bTimeStepOverridden = false;
	}
}

void ANUVFXViewerDirector::SetWorldRenderingEnabled(bool bEnabled)
{
	if (bWorldRenderingDisabled == !bEnabled)
	{
		return;
	}

	bWorldRenderingDisabled = !bEnabled;

	UWorld* World = GetWorld();
	if (UGameViewportClient* Viewport = IsValid(World) ? World->GetGameViewport() : nullptr)
	{
		Viewport->bDisableWorldRendering = !bEnabled;
	}
}

void ANUVFXViewerDirector::RequestFreezeCapture()
{
	bFreezeCaptured = false;
	FreezePixels.Reset();
	FreezeSize = FIntPoint::ZeroValue;

	// 신호를 묶어 두면 파일로 남기지 않고 픽셀을 넘겨 줌 (UGameViewportClient::ProcessScreenShots)
	if (!FreezeCaptureHandle.IsValid())
	{
		FreezeCaptureHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &ThisClass::HandleFreezeCaptured);
	}

	// 이 프레임을 그린 직후 3D 칸만 읽어 옴
	FScreenshotRequest::RequestScreenshot(TEXT("VFXViewerFreeze"), false, false, false, ViewAreaPixels, true);
}

void ANUVFXViewerDirector::HandleFreezeCaptured(int32 Width, int32 Height, const TArray<FColor>& Colors)
{
	UGameViewportClient::OnScreenshotCaptured().Remove(FreezeCaptureHandle);
	FreezeCaptureHandle.Reset();

	// 못 읽었으면 덮개 없이 감음 (그동안 3D 칸이 검게 보임)
	bFreezeCaptured = true;
	if (Width > 0 && Height > 0 && Colors.Num() == Width * Height)
	{
		FreezePixels = Colors;
		FreezeSize = FIntPoint(Width, Height);
	}
}

void ANUVFXViewerDirector::ShowFreezeFrame()
{
	if (FreezeSize.X > 0 && FreezeSize.Y > 0)
	{
		// 같은 크기면 텍스처를 다시 쓰고 내용만 올림 (끌기마다 새로 만들면 수거 전까지 쌓임)
		if (!FreezeTexture || FreezeTexture->GetSizeX() != FreezeSize.X || FreezeTexture->GetSizeY() != FreezeSize.Y)
		{
			FreezeTexture = UTexture2D::CreateTransient(FreezeSize.X, FreezeSize.Y, PF_B8G8R8A8, TEXT("VFXViewerFreeze"));
			FreezeTexture->SRGB = true;
			FreezeTexture->Filter = TF_Bilinear;
			FreezeTexture->UpdateResource();
		}

		const int32 ByteCount = FreezePixels.Num() * sizeof(FColor);
		uint8* Pixels = static_cast<uint8*>(FMemory::Malloc(ByteCount));
		FMemory::Memcpy(Pixels, FreezePixels.GetData(), ByteCount);

		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, FreezeSize.X, FreezeSize.Y);
		FreezeTexture->UpdateTextureRegions(0, 1, Region, FreezeSize.X * sizeof(FColor), sizeof(FColor), Pixels,
			[](uint8* SourceData, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(SourceData);
				delete Regions;
			});

		FreezeBrush.SetResourceObject(FreezeTexture);
		FreezeBrush.ImageSize = FVector2D(FreezeSize.X, FreezeSize.Y);
		FreezeBrush.DrawAs = ESlateBrushDrawType::Image;
		bFreezeVisible = true;
	}

	SetWorldRenderingEnabled(false);
}

void ANUVFXViewerDirector::HideFreezeFrame()
{
	bFreezeVisible = false;
}

void ANUVFXViewerDirector::SetStage(int32 InStageIndex)
{
	if (!StageChoices.IsValidIndex(InStageIndex) || InStageIndex == Setup.StageIndex)
	{
		return;
	}

	Setup.StageIndex = InStageIndex;
	bPlayQueued = true;
	BeginStageLoad();
}

void ANUVFXViewerDirector::SetCaster(ENUVFXViewerCaster InCaster)
{
	Setup.Caster = InCaster;
	ClampTargetIndex();
	Play();
}

void ANUVFXViewerDirector::SetGender(int32 InGender)
{
	const int32 NewGender = InGender == 1 ? 1 : 0;

	// 누른 성별을 지킴, 그 성별에 지금 직업 그림이 없으면 그 성별의 첫 직업으로 (예: 남캐 나이아 나이트 → 여캐 첫 직업)
	// ValidateJobChoice 는 직업을 지키려고 성별을 되돌려서 여기서 쓰면 전환이 안 됨
	if (!FindJobChoice(Setup.JobId, NewGender))
	{
		const FNUVFXViewerChoice* FirstJob = JobChoices.FindByPredicate([NewGender](const FNUVFXViewerChoice& Choice)
		{
			return Choice.Gender == NewGender;
		});

		if (!FirstJob)
		{
			UE_LOG(LogNUVFXViewer, Warning, TEXT("%s 전투 그림이 있는 직업이 없어 성별을 바꾸지 않음"),
				NewGender == 1 ? TEXT("여캐") : TEXT("남캐"));
			return;
		}

		Setup.JobId = FirstJob->Id;
	}

	Setup.Gender = NewGender;
	if (State == EState::Ready)
	{
		SpawnAvatars();
	}
	Play();
}

const FNUVFXViewerChoice* ANUVFXViewerDirector::FindJobChoice(int32 InJobId, int32 InGender) const
{
	return JobChoices.FindByPredicate([InJobId, InGender](const FNUVFXViewerChoice& Choice)
	{
		return Choice.Id == InJobId && Choice.Gender == InGender;
	});
}

void ANUVFXViewerDirector::ValidateJobChoice()
{
	if (JobChoices.IsEmpty() || FindJobChoice(Setup.JobId, Setup.Gender))
	{
		return;
	}

	// 같은 직업이 다른 성별에만 있으면 직업을 지키고 성별을 바꿈 (예: 나이아 나이트는 남캐만)
	const bool bJobOnlyInOtherGender = FindJobChoice(Setup.JobId, 1 - Setup.Gender) != nullptr;
	const FNUVFXViewerChoice* SameGender = JobChoices.FindByPredicate([this](const FNUVFXViewerChoice& Choice)
	{
		return Choice.Gender == Setup.Gender;
	});

	if (SameGender && !bJobOnlyInOtherGender)
	{
		Setup.JobId = SameGender->Id;
		return;
	}

	if (bJobOnlyInOtherGender)
	{
		Setup.Gender = 1 - Setup.Gender;
		return;
	}

	Setup.JobId = JobChoices[0].Id;
	Setup.Gender = JobChoices[0].Gender;
}

void ANUVFXViewerDirector::SetHair(int32 InHairId)
{
	Setup.HairId = FMath::Max(InHairId, 0);
	if (State == EState::Ready)
	{
		SpawnAvatars();
	}
	Play();
}

void ANUVFXViewerDirector::SetJob(int32 InJobId)
{
	Setup.JobId = InJobId;
	if (State == EState::Ready)
	{
		SpawnAvatars();
	}
	Play();
}

void ANUVFXViewerDirector::SetAllyNaia(int32 InNaiaId)
{
	Setup.AllyNaiaId = InNaiaId;
	if (State == EState::Ready)
	{
		SpawnAvatars();
	}
	Play();
}

void ANUVFXViewerDirector::SetEnemyNaia(int32 InNaiaId)
{
	Setup.EnemyNaiaId = InNaiaId;
	if (State == EState::Ready)
	{
		SpawnAvatars();
	}
	Play();
}

void ANUVFXViewerDirector::SetEnemyCount(int32 InCount)
{
	Setup.EnemyCount = FMath::Clamp(InCount, 1, MaxEnemyCount);
	if (State == EState::Ready)
	{
		SpawnAvatars();
	}
	Play();
}

void ANUVFXViewerDirector::SetTargetIndex(int32 InIndex)
{
	Setup.TargetIndex = InIndex;
	ClampTargetIndex();
	Play();
}

void ANUVFXViewerDirector::SetMode(ENUVFXViewerMode InMode)
{
	Setup.Mode = InMode;
	Play();
}

void ANUVFXViewerDirector::SelectSkill(int32 InSkillId)
{
	// 다른 스킬로 가면 표 그대로(첫 후보)부터, 같은 줄을 다시 누르면 보던 후보를 다시 틂
	const bool bSkillChanged = Setup.Mode != ENUVFXViewerMode::Skill || Setup.SkillId != InSkillId;
	Setup.Mode = ENUVFXViewerMode::Skill;
	Setup.SkillId = InSkillId;

	if (bSkillChanged)
	{
		CandidateIndex = 0;

		// 후보가 있는 스킬은 후보를 적은 묶음의 시전자로 (후보끼리 같은 조건)
		if (GetCandidateCount(InSkillId) > 0)
		{
			ApplyCandidateCaster(InSkillId);
		}
	}

	Play();
}

void ANUVFXViewerDirector::SetCustomSlot(int32 SlotIndex, const FSoftObjectPath& Path)
{
	switch (SlotIndex)
	{
	case 0: Setup.CustomCast = Path; break;
	case 1: Setup.CustomProjectile = Path; break;
	case 2: Setup.CustomImpact = Path; break;
	default: return;
	}

	Setup.Mode = ENUVFXViewerMode::Custom;
	Play();
}

void ANUVFXViewerDirector::SetCustomColor(const FLinearColor& InColor)
{
	Setup.CustomColor = InColor;
	if (Setup.Mode == ENUVFXViewerMode::Custom)
	{
		Play();
	}
}

void ANUVFXViewerDirector::SetCustomMotion(ENUBattleActionMotion InMotion)
{
	Setup.CustomMotion = InMotion;
	if (Setup.Mode == ENUVFXViewerMode::Custom)
	{
		Play();
	}
}

void ANUVFXViewerDirector::SetCustomHitCount(int32 InCount)
{
	Setup.CustomHitCount = FMath::Clamp(InCount, 1, 10);
	if (Setup.Mode == ENUVFXViewerMode::Custom)
	{
		Play();
	}
}

void ANUVFXViewerDirector::SetPlayHitCue(bool bInPlayHitCue)
{
	Setup.bPlayHitCue = bInPlayHitCue;
	SaveSettings();
}

void ANUVFXViewerDirector::SetLoop(bool bInLoop)
{
	Setup.bLoop = bInLoop;
	SaveSettings();
}

void ANUVFXViewerDirector::CopySkillToCustom()
{
	// 마지막 재생의 최종 값(직업 기본 공격으로 바뀐 것 포함)을 그대로 옮김
	if (Report.bValid)
	{
		Setup.CustomCast = Report.Spec.CastVFX.ToSoftObjectPath();
		Setup.CustomProjectile = Report.Spec.ProjectileVFX.ToSoftObjectPath();
		Setup.CustomImpact = Report.Spec.ImpactVFX.ToSoftObjectPath();
		Setup.CustomColor = Report.Spec.EffectColor;
		Setup.CustomMotion = Report.Spec.Motion;
		Setup.CustomHitCount = Report.HitCount;
	}

	Setup.Mode = ENUVFXViewerMode::Custom;
	Play();
}

void ANUVFXViewerDirector::FillCarrierProjectile()
{
	const TSharedPtr<FNUVFXViewerAssetItem>* Carrier = AssetItems.FindByPredicate(
		[](const TSharedPtr<FNUVFXViewerAssetItem>& Item)
		{
			return Item.IsValid() && Item->Label == CarrierAssetName;
		});

	if (!Carrier)
	{
		UE_LOG(LogNUVFXViewer, Warning, TEXT("%s 를 %s 아래에서 못 찾음"), CarrierAssetName, VFXSearchPath);
		return;
	}

	SetCustomSlot(1, (*Carrier)->Path);
}

/////////////////////////////////////////////////////////////////
// 후보 비교
/////////////////////////////////////////////////////////////////

bool ANUVFXViewerDirector::LoadCompareFile(const FString& Path)
{
	const FString FullPath = FPaths::ConvertRelativePathToFull(Path);
	CompareExplicitFiles.AddUnique(FullPath);
	ReloadCompareSources();

	return CompareSets.ContainsByPredicate([&FullPath](const FNUVFXViewerCompareSet& Set)
	{
		return Set.SourceFile == FullPath;
	});
}

TArray<FString> ANUVFXViewerDirector::GatherCompareFiles() const
{
	TArray<FString> Files;
	for (const FString& File : CompareExplicitFiles)
	{
		if (IFileManager::Get().FileExists(*File))
		{
			Files.AddUnique(File);
		}
	}

	if (!bUseDefaultCompareSources)
	{
		return Files;
	}

	// 공용 폴더는 이름순 (작업마다 파일 하나)
	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), SharedCompareFolder));
	TArray<FString> Names;
	IFileManager::Get().FindFiles(Names, *FPaths::Combine(Folder, TEXT("*.json")), true, false);
	Names.Sort();
	for (const FString& Name : Names)
	{
		Files.AddUnique(FPaths::Combine(Folder, Name));
	}

	const FString Personal = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), PersonalCompareFile));
	if (IFileManager::Get().FileExists(*Personal))
	{
		Files.AddUnique(Personal);
	}

	return Files;
}

void ANUVFXViewerDirector::ReadVariantFields(const TSharedPtr<FJsonObject>& Object, FNUVFXViewerVariant& OutVariant,
	TArray<FString>& OutMissingNames) const
{
	Object->TryGetStringField(TEXT("label"), OutVariant.Label);
	Object->TryGetStringField(TEXT("note"), OutVariant.Note);

	// 칸 값: 키가 없으면 표 그대로, 빈 문자열이면 그 칸을 비움, 그 밖에는 경로나 이름
	auto ReadSlot = [this, &Object, &OutMissingNames](const TCHAR* Key, bool& bOutOverride, FSoftObjectPath& OutPath)
	{
		FString Text;
		if (!Object->TryGetStringField(Key, Text))
		{
			return;
		}

		Text.TrimStartAndEndInline();
		bOutOverride = true;
		OutPath = ResolveVariantAsset(Text);
		if (!Text.IsEmpty() && OutPath.IsNull())
		{
			OutMissingNames.AddUnique(Text);
		}
	};

	ReadSlot(TEXT("cast"), OutVariant.bOverrideCast, OutVariant.Cast);
	ReadSlot(TEXT("projectile"), OutVariant.bOverrideProjectile, OutVariant.Projectile);
	ReadSlot(TEXT("impact"), OutVariant.bOverrideImpact, OutVariant.Impact);

	// 색은 [R, G, B] 또는 "(R=1,G=0.5,B=0.2,A=1)"
	const TArray<TSharedPtr<FJsonValue>>* ColorValues = nullptr;
	FString ColorText;
	if (Object->TryGetArrayField(TEXT("color"), ColorValues) && ColorValues->Num() >= 3)
	{
		OutVariant.bOverrideColor = true;
		OutVariant.Color = FLinearColor(
			static_cast<float>((*ColorValues)[0]->AsNumber()),
			static_cast<float>((*ColorValues)[1]->AsNumber()),
			static_cast<float>((*ColorValues)[2]->AsNumber()),
			1.f);
	}
	else if (Object->TryGetStringField(TEXT("color"), ColorText) && !ColorText.IsEmpty())
	{
		OutVariant.bOverrideColor = OutVariant.Color.InitFromString(ColorText);
	}
}

TSharedRef<FJsonObject> ANUVFXViewerDirector::WriteVariantOverrides(const FNUVFXViewerVariant& Variant)
{
	// 덮는 칸만 적음, 경로는 전체 경로 (이름만 적으면 그 뒤에 만든 에셋을 못 찾을 수 있음)
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	if (Variant.bOverrideCast)
	{
		Object->SetStringField(TEXT("cast"), Variant.Cast.ToString());
	}

	if (Variant.bOverrideProjectile)
	{
		Object->SetStringField(TEXT("projectile"), Variant.Projectile.ToString());
	}

	if (Variant.bOverrideImpact)
	{
		Object->SetStringField(TEXT("impact"), Variant.Impact.ToString());
	}

	// 색은 소수 넷째 자리까지 (float 를 그대로 쓰면 0.30000001 처럼 꼬리가 붙음)
	if (Variant.bOverrideColor)
	{
		const auto Round = [](float Value)
		{
			return MakeShared<FJsonValueNumber>(FMath::RoundToDouble(static_cast<double>(Value) * 10000.0) / 10000.0);
		};
		Object->SetArrayField(TEXT("color"), { Round(Variant.Color.R), Round(Variant.Color.G), Round(Variant.Color.B) });
	}

	return Object;
}

bool ANUVFXViewerDirector::ParseCompareFile(const FString& FullPath, TArray<FNUVFXViewerCompareSet>& OutSets,
	TArray<FString>& OutMissingNames, FString& OutError) const
{
	FString Json;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Json, *FullPath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
	{
		OutError = FString::Printf(TEXT("%s 를 못 읽음 (JSON 확인)"), *FPaths::GetCleanFilename(FullPath));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* SetValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("sets"), SetValues))
	{
		OutError = FString::Printf(TEXT("%s 에 sets 가 없음"), *FPaths::GetCleanFilename(FullPath));
		return false;
	}

	for (int32 SetIndex = 0; SetIndex < SetValues->Num(); ++SetIndex)
	{
		const TSharedPtr<FJsonValue>& SetValue = (*SetValues)[SetIndex];
		const TSharedPtr<FJsonObject> SetObject = SetValue.IsValid() ? SetValue->AsObject() : nullptr;
		const TArray<TSharedPtr<FJsonValue>>* VariantValues = nullptr;
		if (!SetObject.IsValid() || !SetObject->TryGetArrayField(TEXT("variants"), VariantValues) || VariantValues->IsEmpty())
		{
			continue;
		}

		FNUVFXViewerCompareSet& Set = OutSets.AddDefaulted_GetRef();
		Set.SourceFile = FullPath;
		Set.SourceSetIndex = SetIndex;
		SetObject->TryGetStringField(TEXT("title"), Set.Title);
		SetObject->TryGetNumberField(TEXT("skill"), Set.SkillId);
		SetObject->TryGetStringField(TEXT("caster"), Set.CasterText);

		for (const TSharedPtr<FJsonValue>& VariantValue : *VariantValues)
		{
			const TSharedPtr<FJsonObject> VariantObject = VariantValue.IsValid() ? VariantValue->AsObject() : nullptr;
			if (!VariantObject.IsValid())
			{
				continue;
			}

			FNUVFXViewerVariant& Variant = Set.Variants.AddDefaulted_GetRef();
			ReadVariantFields(VariantObject, Variant, OutMissingNames);

			if (Variant.Label.IsEmpty())
			{
				Variant.Label = FString::Printf(TEXT("후보 %d"), Set.Variants.Num());
			}
		}

		if (Set.Variants.IsEmpty())
		{
			OutSets.Pop();
		}
	}

	return true;
}

bool ANUVFXViewerDirector::ReloadCompareSources()
{
	const TArray<FString> Files = GatherCompareFiles();

	TArray<FNUVFXViewerCompareSet> Sets;
	TArray<FString> MissingNames;
	TArray<FString> Errors;
	CompareFileStamps.Reset();
	for (const FString& File : Files)
	{
		// 못 읽어도 시각은 남겨 같은 파일을 매초 다시 읽지 않게 함 (쓰는 도중일 수 있어 다음 저장을 기다림)
		CompareFileStamps.Add(File, IFileManager::Get().GetTimeStamp(*File));

		FString Error;
		if (!ParseCompareFile(File, Sets, MissingNames, Error))
		{
			Errors.Add(Error);
		}
	}

	CompareSets = MoveTemp(Sets);

	// 스킬별 후보, 칸을 덮는 후보는 읽은 순서대로 · 표 그대로인 후보("지금")는 하나만 첫 칸에 (없으면 뷰어가 붙임)
	CandidatesBySkill.Reset();
	CandidateSkillOrder.Reset();
	TMap<int32, FNUVFXViewerCandidate> TableBySkill;
	for (int32 SetIndex = 0; SetIndex < CompareSets.Num(); ++SetIndex)
	{
		const FNUVFXViewerCompareSet& Set = CompareSets[SetIndex];
		CandidateSkillOrder.AddUnique(Set.SkillId);
		TArray<FNUVFXViewerCandidate>& Candidates = CandidatesBySkill.FindOrAdd(Set.SkillId);
		for (int32 VariantIndex = 0; VariantIndex < Set.Variants.Num(); ++VariantIndex)
		{
			if (Set.Variants[VariantIndex].HasOverride())
			{
				Candidates.Add({ SetIndex, VariantIndex });
			}
			else if (!TableBySkill.Contains(Set.SkillId))
			{
				TableBySkill.Add(Set.SkillId, { SetIndex, VariantIndex });
			}
		}
	}

	for (TPair<int32, TArray<FNUVFXViewerCandidate>>& Pair : CandidatesBySkill)
	{
		Pair.Value.Insert(TableBySkill.FindRef(Pair.Key), 0);
	}

	LoadPendingChanges();

	// 보던 후보가 줄었으면 남은 끝 후보
	CandidateIndex = FMath::Clamp(CandidateIndex, 0, FMath::Max(GetCandidateCount(Setup.SkillId) - 1, 0));

	int32 VariantCount = 0;
	for (const TPair<int32, TArray<FNUVFXViewerCandidate>>& Pair : CandidatesBySkill)
	{
		VariantCount += Pair.Value.Num() - 1;
	}

	CompareMessage = FString::Printf(TEXT("후보 파일 %d개 · 스킬 %d개 · 후보 %d개 (표 그대로 빼고)"),
		Files.Num(), CandidateSkillOrder.Num(), VariantCount);
	if (!Errors.IsEmpty())
	{
		CompareMessage += TEXT(" · ") + FString::Join(Errors, TEXT(", "));
	}

	if (!MissingNames.IsEmpty())
	{
		CompareMessage += FString::Printf(TEXT(" · /Game/VFX 에서 못 찾은 이름: %s"), *FString::Join(MissingNames, TEXT(", ")));
	}

	UE_LOG(LogNUVFXViewer, Log, TEXT("%s"), *CompareMessage);
	for (const FString& File : Files)
	{
		UE_LOG(LogNUVFXViewer, Log, TEXT("  후보 파일: %s"), *File);
	}

	// 스킬 목록의 후보 수 표시 · 거르기를 다시 그림
	if (Panel.IsValid())
	{
		Panel->RefreshLists();
	}

	return !CompareSets.IsEmpty();
}

FSoftObjectPath ANUVFXViewerDirector::ResolveVariantAsset(const FString& Text) const
{
	if (Text.IsEmpty() || Text.StartsWith(TEXT("/")))
	{
		return ParseAssetPath(Text);
	}

	// 이름만 주면 시작할 때 훑은 /Game/VFX 목록에서 찾음, 그 뒤에 만든 에셋은 전체 경로로 적을 것
	const TSharedPtr<FNUVFXViewerAssetItem>* Found = AssetItems.FindByPredicate(
		[&Text](const TSharedPtr<FNUVFXViewerAssetItem>& Item)
		{
			return Item.IsValid() && Item->Label == Text;
		});

	return Found ? (*Found)->Path : FSoftObjectPath();
}

int32 ANUVFXViewerDirector::GetCandidateCount(int32 SkillId) const
{
	const TArray<FNUVFXViewerCandidate>* Candidates = CandidatesBySkill.Find(SkillId);
	return Candidates ? Candidates->Num() : 0;
}

const FNUVFXViewerCandidate* ANUVFXViewerDirector::FindCandidate(int32 SkillId, int32 Index) const
{
	const TArray<FNUVFXViewerCandidate>* Candidates = CandidatesBySkill.Find(SkillId);
	return Candidates && Candidates->IsValidIndex(Index) ? &(*Candidates)[Index] : nullptr;
}

FString ANUVFXViewerDirector::GetCandidateLabel(int32 SkillId, int32 Index) const
{
	const FNUVFXViewerCandidate* Candidate = FindCandidate(SkillId, Index);
	if (!Candidate)
	{
		return FString();
	}

	return CompareSets.IsValidIndex(Candidate->SetIndex)
		? CompareSets[Candidate->SetIndex].Variants[Candidate->VariantIndex].Label
		: FString(TEXT("지금"));
}

FString ANUVFXViewerDirector::GetCandidateNote(int32 SkillId, int32 Index) const
{
	const FNUVFXViewerCandidate* Candidate = FindCandidate(SkillId, Index);
	if (!Candidate)
	{
		return FString();
	}

	// 어느 후보 파일에서 왔는지 같이 보여 줌 (공용 폴더 · 개인 파일)
	if (!CompareSets.IsValidIndex(Candidate->SetIndex))
	{
		return TEXT("스킬 표 그대로");
	}

	const FNUVFXViewerCompareSet& Set = CompareSets[Candidate->SetIndex];
	const FString& Note = Set.Variants[Candidate->VariantIndex].Note;
	const FString Source = FPaths::GetCleanFilename(Set.SourceFile);
	return Note.IsEmpty() ? FString::Printf(TEXT("(%s)"), *Source) : FString::Printf(TEXT("%s  (%s)"), *Note, *Source);
}

int32 ANUVFXViewerDirector::GetChosenCandidate(int32 SkillId) const
{
	const FNUVFXViewerPendingChange* Change = PendingChanges.FindByPredicate([SkillId](const FNUVFXViewerPendingChange& Pending)
	{
		return Pending.SkillId == SkillId;
	});
	return Change ? Change->CandidateIndex : INDEX_NONE;
}

int32 ANUVFXViewerDirector::FindCandidateIndex(int32 SkillId, int32 SetIndex, int32 VariantIndex) const
{
	const TArray<FNUVFXViewerCandidate>* Candidates = CandidatesBySkill.Find(SkillId);
	if (!Candidates)
	{
		return INDEX_NONE;
	}

	const int32 Found = Candidates->IndexOfByPredicate([SetIndex, VariantIndex](const FNUVFXViewerCandidate& Candidate)
	{
		return Candidate.SetIndex == SetIndex && Candidate.VariantIndex == VariantIndex;
	});
	if (Found != INDEX_NONE)
	{
		return Found;
	}

	// 칸을 안 덮는 후보는 모두 표 그대로라 첫 칸
	const bool bTableVariant = !CompareSets.IsValidIndex(SetIndex)
		|| !CompareSets[SetIndex].Variants.IsValidIndex(VariantIndex)
		|| !CompareSets[SetIndex].Variants[VariantIndex].HasOverride();
	return bTableVariant ? 0 : INDEX_NONE;
}

const FNUVFXViewerVariant* ANUVFXViewerDirector::GetActiveVariant() const
{
	if (Setup.Mode != ENUVFXViewerMode::Skill)
	{
		return nullptr;
	}

	const FNUVFXViewerCandidate* Candidate = FindCandidate(Setup.SkillId, CandidateIndex);
	return Candidate && CompareSets.IsValidIndex(Candidate->SetIndex)
		? &CompareSets[Candidate->SetIndex].Variants[Candidate->VariantIndex]
		: nullptr;
}

void ANUVFXViewerDirector::ApplyCandidateCaster(int32 SkillId)
{
	const FNUVFXViewerCompareSet* Set = CompareSets.FindByPredicate([SkillId](const FNUVFXViewerCompareSet& Candidate)
	{
		return Candidate.SkillId == SkillId && !Candidate.CasterText.IsEmpty();
	});

	if (Set)
	{
		FQueueEntry Entry;
		ParseQueueCaster(Set->CasterText, Entry);
		ApplyQueueEntry(Entry);
	}
}

void ANUVFXViewerDirector::SelectCandidate(int32 Index)
{
	if (Setup.Mode != ENUVFXViewerMode::Skill || !FindCandidate(Setup.SkillId, Index))
	{
		return;
	}

	CandidateIndex = Index;
	Play();
}

void ANUVFXViewerDirector::StepCandidate(int32 Delta)
{
	const int32 Count = Setup.Mode == ENUVFXViewerMode::Skill ? GetCandidateCount(Setup.SkillId) : 0;
	if (Count > 0)
	{
		SelectCandidate(((CandidateIndex + Delta) % Count + Count) % Count);
	}
}

void ANUVFXViewerDirector::StepCandidateSkill(int32 Delta)
{
	if (CandidateSkillOrder.IsEmpty())
	{
		return;
	}

	// 지금 스킬이 후보 목록에 없으면 앞으로 가든 뒤로 가든 첫 스킬부터
	const int32 Count = CandidateSkillOrder.Num();
	const int32 Current = Setup.Mode == ENUVFXViewerMode::Skill ? CandidateSkillOrder.IndexOfByKey(Setup.SkillId) : INDEX_NONE;
	const int32 Next = Current == INDEX_NONE ? 0 : ((Current + Delta) % Count + Count) % Count;
	const int32 SkillId = CandidateSkillOrder[Next];
	const bool bSkillChanged = Setup.Mode != ENUVFXViewerMode::Skill || Setup.SkillId != SkillId;

	Setup.Mode = ENUVFXViewerMode::Skill;
	Setup.SkillId = SkillId;

	// 이 스킬에서 이미 고른 후보가 있으면 그것부터
	CandidateIndex = FMath::Max(GetChosenCandidate(SkillId), 0);
	if (bSkillChanged)
	{
		ApplyCandidateCaster(SkillId);
	}

	Play();
}

void ANUVFXViewerDirector::ChooseCandidate()
{
	if (Setup.Mode != ENUVFXViewerMode::Skill || !FindCandidate(Setup.SkillId, CandidateIndex))
	{
		CompareMessage = TEXT("고를 후보가 없음 - 후보가 있는 스킬을 틀고 있을 때만 고를 수 있음");
		return;
	}

	const int32 SkillId = Setup.SkillId;
	const int32 Existing = PendingChanges.IndexOfByPredicate([SkillId](const FNUVFXViewerPendingChange& Pending)
	{
		return Pending.SkillId == SkillId;
	});

	// 기본 공격 연출은 직업마다 따로라 캐릭터로 볼 때만 담음 (적용은 그 직업의 ini 항목)
	if (SkillId == 0 && CandidateIndex != 0 && Setup.Caster != ENUVFXViewerCaster::Player)
	{
		CompareMessage = TEXT("기본 공격 후보는 캐릭터로 틀 때만 담을 수 있음 - 직업 기본 공격 연출은 직업마다 따로");
		return;
	}

	// "지금"(표 그대로)을 고르면 바꿀 것이 없으므로 대기에서 뺌
	if (CandidateIndex == 0)
	{
		if (Existing != INDEX_NONE)
		{
			PendingChanges.RemoveAt(Existing);
		}

		CompareMessage = FString::Printf(TEXT("%s: 지금 그대로 - 적용 대기에서 뺌"), *GetSkillLabel(SkillId));
	}
	else
	{
		const FNUVFXViewerCandidate* Candidate = FindCandidate(SkillId, CandidateIndex);
		const FNUVFXViewerCompareSet& Set = CompareSets[Candidate->SetIndex];

		FNUVFXViewerPendingChange Change;
		Change.SkillId = SkillId;
		Change.Label = GetCandidateLabel(SkillId, CandidateIndex);
		Change.Title = Set.Title;
		Change.Time = FDateTime::UtcNow().ToIso8601();
		Change.SourceFile = Set.SourceFile;
		FPaths::MakePathRelativeTo(Change.SourceFile, *(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()) / TEXT("")));
		Change.SourceSetIndex = Set.SourceSetIndex;
		Change.VariantIndex = Candidate->VariantIndex;
		Change.Overrides = Set.Variants[Candidate->VariantIndex];
		Change.CandidateIndex = CandidateIndex;
		Change.JobId = SkillId == 0 ? Setup.JobId : INDEX_NONE;

		if (Existing != INDEX_NONE)
		{
			PendingChanges[Existing] = Change;
		}
		else
		{
			PendingChanges.Add(Change);
		}

		CompareMessage = FString::Printf(TEXT("적용 대기에 담음: %s -> %s"), *GetSkillLabel(SkillId), *Change.Label);
	}

	++PendingVersion;
	SavePendingChanges();
	UE_LOG(LogNUVFXViewer, Log, TEXT("%s (%s)"), *CompareMessage, *GetPendingPath());

	// 무엇이 바뀌는지 로그에도 남김 (AI 가 로그로 확인)
	if (const FNUVFXViewerPendingChange* Change = PendingChanges.FindByPredicate([SkillId](const FNUVFXViewerPendingChange& Pending)
		{
			return Pending.SkillId == SkillId;
		}))
	{
		for (const FString& Line : DescribePendingChange(*Change))
		{
			UE_LOG(LogNUVFXViewer, Log, TEXT("  %s"), *Line);
		}
	}
}

FString ANUVFXViewerDirector::GetSkillLabel(int32 SkillId) const
{
	const TSharedPtr<FNUVFXViewerSkillItem>* Item = SkillItems.FindByPredicate(
		[SkillId](const TSharedPtr<FNUVFXViewerSkillItem>& Candidate)
		{
			return Candidate.IsValid() && Candidate->SkillId == SkillId;
		});

	return Item ? (*Item)->Label : FString::Printf(TEXT("스킬 %d"), SkillId);
}

void ANUVFXViewerDirector::ViewPendingChange(int32 SkillId)
{
	const bool bSkillChanged = Setup.Mode != ENUVFXViewerMode::Skill || Setup.SkillId != SkillId;
	Setup.Mode = ENUVFXViewerMode::Skill;
	Setup.SkillId = SkillId;

	// 후보 파일에서 못 찾은 건이면 표 그대로를 틂 (남겨 둔 값은 적용할 때만 씀)
	CandidateIndex = FMath::Max(GetChosenCandidate(SkillId), 0);
	if (bSkillChanged && GetCandidateCount(SkillId) > 0)
	{
		ApplyCandidateCaster(SkillId);
	}

	Play();
}

void ANUVFXViewerDirector::RemovePendingChange(int32 SkillId)
{
	const int32 Removed = PendingChanges.RemoveAll([SkillId](const FNUVFXViewerPendingChange& Pending)
	{
		return Pending.SkillId == SkillId;
	});

	if (Removed > 0)
	{
		CompareMessage = FString::Printf(TEXT("%s: 적용 대기에서 뺌"), *GetSkillLabel(SkillId));
		++PendingVersion;
		SavePendingChanges();
	}
}

void ANUVFXViewerDirector::ClearPendingChanges()
{
	if (!PendingChanges.IsEmpty())
	{
		CompareMessage = FString::Printf(TEXT("적용 대기 %d건을 모두 비움"), PendingChanges.Num());
		PendingChanges.Reset();
		++PendingVersion;
		SavePendingChanges();
	}
}

void ANUVFXViewerDirector::BuildPendingRows(const FNUVFXViewerPendingChange& Change, FNUSkillVFXProfile& OutBefore,
	FNUSkillVFXProfile& OutAfter, bool& bOutHasRow) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	const FNUSkillVFXProfile* Current = IsValid(Data) ? Data->FindSkillVFXProfile(Change.SkillId) : nullptr;
	bOutHasRow = Current != nullptr;
	OutBefore = Current ? *Current : FNUSkillVFXProfile();
	OutBefore.SkillId = Change.SkillId;
	OutAfter = OutBefore;

	const FNUVFXViewerVariant& Overrides = Change.Overrides;
	if (Overrides.bOverrideCast)
	{
		OutAfter.CastVFX = TSoftObjectPtr<UNiagaraSystem>(Overrides.Cast);
	}

	if (Overrides.bOverrideProjectile)
	{
		OutAfter.ProjectileVFX = TSoftObjectPtr<UNiagaraSystem>(Overrides.Projectile);
	}

	if (Overrides.bOverrideImpact)
	{
		OutAfter.ImpactVFX = TSoftObjectPtr<UNiagaraSystem>(Overrides.Impact);
	}

	// 색 후보는 그 스킬만 고정색으로 (속성색 행이면 속성색을 끔, 같은 속성의 다른 스킬은 그대로)
	if (Overrides.bOverrideColor)
	{
		OutAfter.bUseElementColor = false;
		OutAfter.FixedColor = Overrides.Color;
	}
}

TArray<FString> ANUVFXViewerDirector::DescribePendingChange(const FNUVFXViewerPendingChange& Change) const
{
	// 기본 공격은 DT 가 아니라 ini 의 직업 항목
	if (Change.SkillId <= 0)
	{
		return DescribeJobAttackChange(Change);
	}

	FNUSkillVFXProfile Before;
	FNUSkillVFXProfile After;
	bool bHasRow = false;
	BuildPendingRows(Change, Before, After, bHasRow);

	TArray<FString> Lines;
	if (!bHasRow)
	{
		Lines.Add(TEXT("! 표에 행이 없음 - 적용하면 새 행"));
	}

	if (Change.CandidateIndex == INDEX_NONE)
	{
		Lines.Add(TEXT("! 후보 파일에서 못 찾음 - 담을 때 남긴 값으로 적용"));
	}

	const auto AddSlot = [&Lines](const TCHAR* Name, const TSoftObjectPtr<UNiagaraSystem>& From, const TSoftObjectPtr<UNiagaraSystem>& To)
	{
		if (From.ToSoftObjectPath() != To.ToSoftObjectPath())
		{
			Lines.Add(FString::Printf(TEXT("%s  %s -> %s"), Name, *DescribeAsset(From), *DescribeAsset(To)));
		}
	};

	AddSlot(TEXT("Cast"), Before.CastVFX, After.CastVFX);
	AddSlot(TEXT("Proj"), Before.ProjectileVFX, After.ProjectileVFX);
	AddSlot(TEXT("Impact"), Before.ImpactVFX, After.ImpactVFX);

	const auto DescribeColor = [](const FNUSkillVFXProfile& Row)
	{
		return Row.bUseElementColor ? FString(TEXT("속성색")) : FString::Printf(TEXT("고정 %s"), *FormatColor(Row.FixedColor));
	};

	if (Before.bUseElementColor != After.bUseElementColor || !Before.FixedColor.Equals(After.FixedColor, 0.001f))
	{
		Lines.Add(FString::Printf(TEXT("색  %s -> %s"), *DescribeColor(Before), *DescribeColor(After)));
	}

	if (Lines.IsEmpty())
	{
		Lines.Add(TEXT("바뀌는 칸 없음 (표와 같음)"));
	}

	return Lines;
}

TArray<FString> ANUVFXViewerDirector::DescribeJobAttackChange(const FNUVFXViewerPendingChange& Change) const
{
	TArray<FString> Lines;
	if (Change.JobId == INDEX_NONE)
	{
		Lines.Add(TEXT("! 직업을 모름 - 캐릭터로 틀고 다시 담기 (적용 안 함)"));
		return Lines;
	}

	FNUVFXViewerTableRow Before;
	const bool bHasRow = FindJobAttackRow(Change.JobId, Before);
	Lines.Add(FString::Printf(TEXT("DefaultGame.ini JobAttackVFX 직업 %d%s"), Change.JobId,
		bHasRow ? TEXT("") : TEXT("  (새 항목 - 지금은 어빌리티 기본 연출)")));

	if (Change.CandidateIndex == INDEX_NONE)
	{
		Lines.Add(TEXT("! 후보 파일에서 못 찾음 - 담을 때 남긴 값으로 적용"));
	}

	const TArray<FString> Diff = DescribeRowDiff(Before, ApplyOverrides(Before, Change.Overrides));
	if (Diff.IsEmpty())
	{
		Lines.Add(TEXT("바뀌는 칸 없음 (ini 와 같음)"));
	}
	else
	{
		Lines.Append(Diff);
	}

	return Lines;
}

FString ANUVFXViewerDirector::GetJobAttackIniPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::SourceConfigDir(), TEXT("DefaultGame.ini")));
}

bool ANUVFXViewerDirector::FindJobAttackRow(int32 JobId, FNUVFXViewerTableRow& OutRow) const
{
	const FNUJobAttackVFX* Row = GetDefault<UNUBattleDataSettings>()->JobAttackVFX.Find(JobId);
	if (!Row)
	{
		return false;
	}

	// 직업 연출은 속성색이 없어 색은 늘 고정색
	OutRow = FNUVFXViewerTableRow();
	OutRow.Cast = Row->CastVFX.ToSoftObjectPath().ToString();
	OutRow.Projectile = Row->ProjectileVFX.ToSoftObjectPath().ToString();
	OutRow.Impact = Row->ImpactVFX.ToSoftObjectPath().ToString();
	OutRow.FixedColor = Row->EffectColor;
	return true;
}

void ANUVFXViewerDirector::SetJobAttackRow(int32 JobId, const FNUVFXViewerTableRow* Row)
{
	UNUBattleDataSettings* Settings = GetMutableDefault<UNUBattleDataSettings>();
	if (!Row)
	{
		Settings->JobAttackVFX.Remove(JobId);
		return;
	}

	FNUJobAttackVFX& Entry = Settings->JobAttackVFX.FindOrAdd(JobId);
	Entry.CastVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Row->Cast));
	Entry.ProjectileVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Row->Projectile));
	Entry.ImpactVFX = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(Row->Impact));
	Entry.EffectColor = Row->FixedColor;
}

void ANUVFXViewerDirector::SetCompareCycle(bool bInCycle)
{
	bCompareCycle = bInCycle;

	// 끝난 화면에서 켜면 바로 다음 후보를 틂
	if (bCompareCycle && !bPlaying)
	{
		StepCandidate(1);
	}
}

FString ANUVFXViewerDirector::GetPendingPath() const
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), PendingChangesFile));
}

int32 ANUVFXViewerDirector::FindPendingCandidate(const FNUVFXViewerPendingChange& Change) const
{
	// 같은 파일 · 같은 이름을 먼저, 없으면 같은 이름 (번호는 후보 파일을 고치면 바뀜)
	const FString FullFile = Change.SourceFile.IsEmpty()
		? FString()
		: FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), Change.SourceFile);

	int32 SameLabel = INDEX_NONE;
	for (int32 Index = 1; Index < GetCandidateCount(Change.SkillId); ++Index)
	{
		if (GetCandidateLabel(Change.SkillId, Index) != Change.Label)
		{
			continue;
		}

		const FNUVFXViewerCandidate* Candidate = FindCandidate(Change.SkillId, Index);
		if (CompareSets[Candidate->SetIndex].SourceFile == FullFile)
		{
			return Index;
		}

		if (SameLabel == INDEX_NONE)
		{
			SameLabel = Index;
		}
	}

	return SameLabel;
}

void ANUVFXViewerDirector::LoadPendingChanges()
{
	PendingChanges.Reset();
	++PendingVersion;

	FString Json;
	TSharedPtr<FJsonObject> Root;
	const TArray<TSharedPtr<FJsonValue>>* ChangeValues = nullptr;
	if (!FFileHelper::LoadFileToString(Json, *GetPendingPath())
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid()
		|| !Root->TryGetArrayField(TEXT("changes"), ChangeValues))
	{
		return;
	}

	for (const TSharedPtr<FJsonValue>& ChangeValue : *ChangeValues)
	{
		const TSharedPtr<FJsonObject> Object = ChangeValue.IsValid() ? ChangeValue->AsObject() : nullptr;
		FNUVFXViewerPendingChange Change;
		if (!Object.IsValid() || !Object->TryGetNumberField(TEXT("skill"), Change.SkillId))
		{
			continue;
		}

		Object->TryGetStringField(TEXT("label"), Change.Label);
		Object->TryGetStringField(TEXT("title"), Change.Title);
		Object->TryGetStringField(TEXT("time"), Change.Time);
		Object->TryGetStringField(TEXT("file"), Change.SourceFile);
		Object->TryGetNumberField(TEXT("set"), Change.SourceSetIndex);
		Object->TryGetNumberField(TEXT("variant"), Change.VariantIndex);
		Object->TryGetNumberField(TEXT("job"), Change.JobId);

		const TSharedPtr<FJsonObject>* Apply = nullptr;
		TArray<FString> MissingNames;
		if (Object->TryGetObjectField(TEXT("apply"), Apply))
		{
			ReadVariantFields(*Apply, Change.Overrides, MissingNames);
		}

		// 후보를 지금 읽었으면 그 값으로 새로 고침 (후보 파일을 고쳤을 수 있음)
		Change.CandidateIndex = FindPendingCandidate(Change);
		if (const FNUVFXViewerCandidate* Candidate = FindCandidate(Change.SkillId, Change.CandidateIndex))
		{
			Change.Overrides = CompareSets[Candidate->SetIndex].Variants[Candidate->VariantIndex];
		}

		PendingChanges.Add(Change);
	}
}

void ANUVFXViewerDirector::SavePendingChanges() const
{
	TArray<TSharedPtr<FJsonValue>> Changes;
	for (const FNUVFXViewerPendingChange& Change : PendingChanges)
	{
		// apply = 표 행 위에 덮을 값, 후보 파일이 없어져도 이것으로 적용함
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("skill"), Change.SkillId);
		Object->SetStringField(TEXT("label"), Change.Label);
		Object->SetStringField(TEXT("title"), Change.Title);
		Object->SetStringField(TEXT("file"), Change.SourceFile);
		Object->SetNumberField(TEXT("set"), Change.SourceSetIndex);
		Object->SetNumberField(TEXT("variant"), Change.VariantIndex);
		Object->SetStringField(TEXT("time"), Change.Time);
		if (Change.JobId != INDEX_NONE)
		{
			Object->SetNumberField(TEXT("job"), Change.JobId);
		}
		Object->SetObjectField(TEXT("apply"), WriteVariantOverrides(Change.Overrides));
		Changes.Add(MakeShared<FJsonValueObject>(Object));
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("changes"), Changes);

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	FFileHelper::SaveStringToFile(Json, *GetPendingPath(), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void ANUVFXViewerDirector::TickCompareFileWatch()
{
	if (bAutomation || (CompareExplicitFiles.IsEmpty() && !bUseDefaultCompareSources))
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now < NextCompareWatchTime)
	{
		return;
	}

	// 공용 폴더에 파일이 생기거나(pull · AI) 빠지거나, 읽은 파일이 고쳐지면 다시 읽음
	NextCompareWatchTime = Now + 1.0;
	const TArray<FString> Files = GatherCompareFiles();
	bool bChanged = Files.Num() != CompareFileStamps.Num();
	for (int32 Index = 0; Index < Files.Num() && !bChanged; ++Index)
	{
		const FDateTime* Stamp = CompareFileStamps.Find(Files[Index]);
		bChanged = !Stamp || *Stamp != IFileManager::Get().GetTimeStamp(*Files[Index]);
	}

	if (!bChanged)
	{
		return;
	}

	// 고친 후보를 바로 보여 줌, 시전자 표기가 바뀌었을 수 있어 다시 맞춤
	ReloadCompareSources();
	if (Setup.Mode == ENUVFXViewerMode::Skill && GetCandidateCount(Setup.SkillId) > 0)
	{
		ApplyCandidateCaster(Setup.SkillId);
		Play();
	}
}

/////////////////////////////////////////////////////////////////
// 적용 (원본 CSV → DT_SkillVFX)
/////////////////////////////////////////////////////////////////

UDataTable* ANUVFXViewerDirector::GetSkillVFXTable() const
{
	return GetDefault<UNUBattleDataSettings>()->SkillVFXTable.LoadSynchronous();
}

FString ANUVFXViewerDirector::GetSkillVFXCsvPath() const
{
	// DT 가 기억하는 원본 파일 (DT 폴더 기준 상대 경로라 팀원 PC 에서도 그 사람의 레포 경로로 풀림)
#if WITH_EDITORONLY_DATA
	const UDataTable* Table = GetSkillVFXTable();
	if (IsValid(Table) && IsValid(Table->AssetImportData))
	{
		return FPaths::ConvertRelativePathToFull(Table->AssetImportData->GetFirstFilename());
	}
#endif
	return FString();
}

void ANUVFXViewerDirector::RefreshTableState()
{
	// 되돌릴 수 있는 마지막 적용 = 이력 중 가장 늦고 아직 안 되돌린 적용
	LastApplyHistory.Reset();
	LastApplySummary.Reset();

	const FString HistoryFolder = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TableHistoryFolder));
	TArray<FString> Names;
	IFileManager::Get().FindFiles(Names, *FPaths::Combine(HistoryFolder, TEXT("Apply_*.json")), true, false);
	Names.Sort();
	for (int32 Index = Names.Num() - 1; Index >= 0; --Index)
	{
		const FString HistoryPath = FPaths::Combine(HistoryFolder, Names[Index]);
		const TSharedPtr<FJsonObject> Root = LoadJsonFile(HistoryPath);
		bool bReverted = false;
		if (!Root.IsValid() || (Root->TryGetBoolField(TEXT("reverted"), bReverted) && bReverted))
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* JobChanges = nullptr;
		Root->TryGetArrayField(TEXT("changes"), Changes);
		Root->TryGetArrayField(TEXT("jobAttack"), JobChanges);
		FString Time;
		Root->TryGetStringField(TEXT("localTime"), Time);
		LastApplyHistory = HistoryPath;
		LastApplySummary = FString::Printf(TEXT("%s · %d건"), *Time, (Changes ? Changes->Num() : 0) + (JobChanges ? JobChanges->Num() : 0));
		break;
	}

	// DT 가 기억하는 원본 MD5 와 지금 CSV 를 견줌
	bTableNeedsReimport = false;
#if WITH_EDITORONLY_DATA
	const UDataTable* Table = GetSkillVFXTable();
	const FString CsvPath = GetSkillVFXCsvPath();
	if (IsValid(Table) && IsValid(Table->AssetImportData) && !Table->AssetImportData->GetSourceData().SourceFiles.IsEmpty()
		&& FPaths::FileExists(CsvPath))
	{
		const FMD5Hash Recorded = Table->AssetImportData->GetSourceData().SourceFiles[0].FileHash;
		const FMD5Hash Current = FMD5Hash::HashFile(*CsvPath);
		bTableNeedsReimport = Recorded.IsValid() && Current.IsValid() && Recorded != Current;
	}
#endif
}

bool ANUVFXViewerDirector::UpdateViewerTable(const FNUVFXViewerTableFile& File, FString& OutError)
{
	UDataTable* Table = GetSkillVFXTable();
	if (!IsValid(Table))
	{
		OutError = TEXT("스킬 연출 표를 못 찾음 (NUBattleDataSettings SkillVFXTable)");
		return false;
	}

	// 표가 바뀌면 전투 데이터의 스킬 연출 색인이 다시 만들어짐 (OnDataTableChanged)
	const TArray<FString> Problems = Table->CreateTableFromCSVString(File.ToString());
	if (!Problems.IsEmpty())
	{
		OutError = FString::Join(Problems, TEXT(" / "));
		return false;
	}

	// 새 행이 생겼을 수 있어 스킬 목록 · 후보 칸 · 적용 대기의 지금 값을 다시 그림
	BuildSkillItems();
	++PendingVersion;
	if (Panel.IsValid())
	{
		Panel->RefreshLists();
	}

	return true;
}

FString ANUVFXViewerDirector::MakeNewRowName(int32 SkillId) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UNUBattleDataSubsystem* Data =
		IsValid(GameInstance) ? GameInstance->GetSubsystem<UNUBattleDataSubsystem>() : nullptr;

	if (IsValid(Data) && Data->IsBossSkill(SkillId))
	{
		return FString::Printf(TEXT("Boss_%d"), SkillId);
	}

	if (IsValid(Data) && Data->FindJobSkillDefinition(SkillId))
	{
		return FString::Printf(TEXT("Job_%d"), SkillId);
	}

	return FString::FromInt(SkillId);
}

FString ANUVFXViewerDirector::BackupFile(const FString& SourcePath, const FString& Stamp) const
{
	const FString Backup = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TableBackupFolder,
		FString::Printf(TEXT("%s_%s.%s"), *FPaths::GetBaseFilename(SourcePath), *Stamp, *FPaths::GetExtension(SourcePath))));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Backup), true);
	return IFileManager::Get().Copy(*Backup, *SourcePath) == COPY_OK ? Backup : FString();
}

bool ANUVFXViewerDirector::IsProjectEditorOpen() const
{
	// PIE 면 지금 이 에디터가 DT 를 들고 있음
	if (GIsEditor)
	{
		return true;
	}

#if PLATFORM_WINDOWS
	struct FSearch
	{
		FString Project;
		bool bFound = false;
	};

	FSearch Search;
	Search.Project = FApp::GetProjectName();
	::EnumWindows([](HWND Window, LPARAM Param) -> BOOL
	{
		FSearch& State = *reinterpret_cast<FSearch*>(Param);
		TCHAR Title[512];
		if (::IsWindowVisible(Window) && ::GetWindowTextW(Window, Title, UE_ARRAY_COUNT(Title)) > 0)
		{
			const FString Text(Title);
			if (Text.Contains(TEXT("Unreal Editor")) && Text.Contains(State.Project))
			{
				State.bFound = true;
				return 0;
			}
		}
		return 1;
	}, reinterpret_cast<LPARAM>(&Search));

	return Search.bFound;
#else
	return false;
#endif
}

bool ANUVFXViewerDirector::AskOrLog(bool bAskUser, EAppMsgType::Type Type, const FString& Message) const
{
	if (!bAskUser)
	{
		UE_LOG(LogNUVFXViewer, Log, TEXT("%s"), *Message);
		return true;
	}

	return ShowDialog(Type, Message) == EAppReturnType::Ok;
}

void ANUVFXViewerDirector::ApplyPendingChanges(bool bAskUser)
{
	if (IsReimportingTable())
	{
		TableStatus = TEXT("DT 다시 채우는 중 - 끝난 뒤 적용");
		return;
	}

	// 스킬은 원본 CSV(→ DT_SkillVFX), 기본 공격(0)은 DefaultGame.ini 의 직업 항목(JobAttackVFX)
	const bool bHasTableChange = PendingChanges.ContainsByPredicate([](const FNUVFXViewerPendingChange& Change)
	{
		return Change.SkillId > 0;
	});
	const bool bHasIniChange = PendingChanges.ContainsByPredicate([](const FNUVFXViewerPendingChange& Change)
	{
		return Change.SkillId <= 0 && Change.JobId != INDEX_NONE;
	});

	const FString CsvPath = GetSkillVFXCsvPath();
	FNUVFXViewerTableFile File;
	FString Error;
	if (bHasTableChange && (CsvPath.IsEmpty() || !File.Load(CsvPath, Error)))
	{
		TableStatus = CsvPath.IsEmpty() ? TEXT("DT_SkillVFX 에 원본 CSV 기록이 없어 적용할 수 없음") : Error;
		AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
		return;
	}

	const FString IniPath = GetJobAttackIniPath();
	FNUVFXViewerJobAttackIni Ini;
	if (bHasIniChange && !Ini.Load(IniPath, Error))
	{
		TableStatus = Error;
		AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
		return;
	}

	// 실제로 쓰는 값은 지금 CSV 행 · ini 항목 위에 덮은 것 (뷰어가 아는 값과 다를 수 있음)
	struct FApplied
	{
		int32 SkillId = 0;
		int32 JobId = INDEX_NONE;
		FString Label;
		bool bNewRow = false;
		FNUVFXViewerTableRow Before;
		FNUVFXViewerTableRow After;

		// 기본 공격만: ini 항목 글자 그대로 (되돌리기가 그대로 되돌려 씀)
		FString BeforeEntry;
		FString AfterEntry;
	};

	TArray<FApplied> Applied;
	TArray<FApplied> AppliedIni;
	TArray<FString> Skipped;
	TArray<FString> Lines;
	for (const FNUVFXViewerPendingChange& Change : PendingChanges)
	{
		if (Change.SkillId <= 0)
		{
			if (Change.JobId == INDEX_NONE)
			{
				Skipped.Add(FString::Printf(TEXT("%s (직업을 모름 - 캐릭터로 틀고 다시 담기)"), *GetSkillLabel(Change.SkillId)));
				continue;
			}

			FApplied Item;
			Item.SkillId = Change.SkillId;
			Item.JobId = Change.JobId;
			Item.Label = Change.Label;
			Item.bNewRow = !Ini.FindEntry(Change.JobId, Item.BeforeEntry);
			Item.Before = Item.bNewRow ? FNUVFXViewerTableRow() : FNUVFXViewerJobAttackIni::ParseEntry(Item.BeforeEntry);
			Item.After = ApplyOverrides(Item.Before, Change.Overrides);
			Item.AfterEntry = FNUVFXViewerJobAttackIni::FormatEntry(Item.After);
			if (!Item.bNewRow && Item.After == Item.Before)
			{
				Skipped.Add(FString::Printf(TEXT("기본 공격 · 직업 %d (ini 가 이미 같음)"), Item.JobId));
				continue;
			}

			Lines.Add(FString::Printf(TEXT("기본 공격 · 직업 %d -> %s%s"), Item.JobId, *Item.Label, Item.bNewRow ? TEXT("  (새 항목)") : TEXT("")));
			for (const FString& Line : DescribeRowDiff(Item.Before, Item.After))
			{
				Lines.Add(TEXT("    ") + Line);
			}
			AppliedIni.Add(Item);
			continue;
		}

		FApplied Item;
		Item.SkillId = Change.SkillId;
		Item.Label = Change.Label;
		Item.bNewRow = !File.FindRow(Change.SkillId, Item.Before);
		Item.After = ApplyOverrides(Item.Before, Change.Overrides);
		if (!Item.bNewRow && Item.After == Item.Before)
		{
			Skipped.Add(FString::Printf(TEXT("%s (CSV 가 이미 같음)"), *GetSkillLabel(Change.SkillId)));
			continue;
		}

		Lines.Add(FString::Printf(TEXT("%s -> %s%s"), *GetSkillLabel(Item.SkillId), *Item.Label, Item.bNewRow ? TEXT("  (새 행)") : TEXT("")));
		for (const FString& Line : DescribeRowDiff(Item.Before, Item.After))
		{
			Lines.Add(TEXT("    ") + Line);
		}
		Applied.Add(Item);
	}

	const int32 AppliedCount = Applied.Num() + AppliedIni.Num();
	if (AppliedCount == 0)
	{
		TableStatus = Skipped.IsEmpty() ? TEXT("적용할 것이 없음") : TEXT("적용할 것이 없음 - ") + FString::Join(Skipped, TEXT(", "));
		return;
	}

	TArray<FString> Targets;
	if (!Applied.IsEmpty())
	{
		Targets.Add(FPaths::GetCleanFilename(CsvPath));
	}
	if (!AppliedIni.IsEmpty())
	{
		Targets.Add(TEXT("DefaultGame.ini (JobAttackVFX)"));
	}

	const bool bEditorOpen = IsProjectEditorOpen();
	FString Message = FString::Printf(TEXT("다음 %d건을 %s 에 씁니다.\n\n%s"), AppliedCount, *FString::Join(Targets, TEXT(" · ")),
		*FString::Join(Lines, TEXT("\n")));
	if (!Skipped.IsEmpty())
	{
		Message += TEXT("\n\n건너뜀: ") + FString::Join(Skipped, TEXT(", "));
	}

	if (!Applied.IsEmpty())
	{
		if (bTableNeedsReimport)
		{
			Message += TEXT("\n\n주의: CSV 가 DT 를 마지막으로 채운 뒤 바뀌어 있습니다 (받기만 하고 안 채웠거나 손으로 고침). 그 변경도 같이 DT 에 들어갑니다.");
		}

		Message += bEditorOpen
			? TEXT("\n\n에디터가 켜져 있어 CSV 만 고칩니다. DT 는 에디터에서 다시 가져오거나, 에디터를 끄고 [DT 다시 채우기]를 누르세요.")
			: TEXT("\n\nCSV 를 고친 뒤 화면 없는 에디터로 DT_SkillVFX 를 다시 채웁니다 (1~2분).");
	}

	if (!AppliedIni.IsEmpty())
	{
		Message += TEXT("\n\n기본 공격은 DefaultGame.ini 의 그 직업 항목만 고칩니다. 게임 · 에디터는 켤 때 ini 를 읽으므로 켜져 있으면 다시 켜야 보입니다 (뷰어는 바로 바뀜).");
	}

	Message += TEXT("\n\n적용 전 파일은 백업하고, [마지막 적용 되돌리기]로 되돌릴 수 있습니다.");

	if (!AskOrLog(bAskUser, EAppMsgType::OkCancel, Message))
	{
		TableStatus = TEXT("적용 취소");
		return;
	}

	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString Backup = Applied.IsEmpty() ? FString() : BackupFile(CsvPath, Stamp);
	const FString IniBackup = AppliedIni.IsEmpty() ? FString() : BackupFile(IniPath, Stamp);
	if ((!Applied.IsEmpty() && Backup.IsEmpty()) || (!AppliedIni.IsEmpty() && IniBackup.IsEmpty()))
	{
		TableStatus = TEXT("백업을 못 만들어 적용하지 않음");
		AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
		return;
	}

	if (!Applied.IsEmpty())
	{
		for (const FApplied& Item : Applied)
		{
			File.SetRow(Item.SkillId, Item.After, MakeNewRowName(Item.SkillId));
		}

		if (!File.Save(Error))
		{
			TableStatus = Error;
			AskOrLog(bAskUser, EAppMsgType::Ok, Error);
			return;
		}
	}

	if (!AppliedIni.IsEmpty())
	{
		for (const FApplied& Item : AppliedIni)
		{
			Ini.SetEntry(Item.JobId, Item.AfterEntry);
		}

		if (!Ini.Save(Error))
		{
			TableStatus = Applied.IsEmpty() ? Error : Error + TEXT(" (CSV 는 씀)");
			AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
			return;
		}

		// 뷰어 안의 설정도 바로 바꿔 "지금" 이 새 값이 됨
		for (const FApplied& Item : AppliedIni)
		{
			SetJobAttackRow(Item.JobId, &Item.After);
		}
	}

	// 이력: 행 · 항목마다 적용 전 · 뒤 값 (되돌리기가 씀)
	TArray<TSharedPtr<FJsonValue>> Changes;
	for (const FApplied& Item : Applied)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("skill"), Item.SkillId);
		Object->SetStringField(TEXT("label"), Item.Label);
		Object->SetBoolField(TEXT("newRow"), Item.bNewRow);
		Object->SetObjectField(TEXT("before"), TableRowToJson(Item.Before));
		Object->SetObjectField(TEXT("after"), TableRowToJson(Item.After));
		Changes.Add(MakeShared<FJsonValueObject>(Object));
	}

	TArray<TSharedPtr<FJsonValue>> JobChanges;
	for (const FApplied& Item : AppliedIni)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("job"), Item.JobId);
		Object->SetStringField(TEXT("label"), Item.Label);
		Object->SetBoolField(TEXT("newRow"), Item.bNewRow);
		Object->SetStringField(TEXT("before"), Item.BeforeEntry);
		Object->SetStringField(TEXT("after"), Item.AfterEntry);
		JobChanges.Add(MakeShared<FJsonValueObject>(Object));
	}

	const TSharedRef<FJsonObject> History = MakeShared<FJsonObject>();
	History->SetStringField(TEXT("type"), TEXT("apply"));
	History->SetStringField(TEXT("time"), FDateTime::UtcNow().ToIso8601());
	History->SetStringField(TEXT("localTime"), FDateTime::Now().ToString(TEXT("%m/%d %H:%M")));
	History->SetStringField(TEXT("user"), FPlatformProcess::UserName());
	if (!Applied.IsEmpty())
	{
		History->SetStringField(TEXT("csv"), ProjectRelativePath(CsvPath));
		History->SetStringField(TEXT("backup"), ProjectRelativePath(Backup));
	}
	if (!AppliedIni.IsEmpty())
	{
		History->SetStringField(TEXT("ini"), ProjectRelativePath(IniPath));
		History->SetStringField(TEXT("iniBackup"), ProjectRelativePath(IniBackup));
	}
	History->SetArrayField(TEXT("changes"), Changes);
	History->SetArrayField(TEXT("jobAttack"), JobChanges);
	History->SetBoolField(TEXT("reverted"), false);
	History->SetStringField(TEXT("reimport"), Applied.IsEmpty() ? TEXT("none") : TEXT("pending"));

	const FString HistoryPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TableHistoryFolder,
		FString::Printf(TEXT("Apply_%s.json"), *Stamp)));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(HistoryPath), true);
	SaveJsonFile(History, HistoryPath);

	// 적용한 스킬은 대기에서 뺌
	PendingChanges.RemoveAll([&Applied, &AppliedIni](const FNUVFXViewerPendingChange& Change)
	{
		const auto Same = [&Change](const FApplied& Item)
		{
			return Item.SkillId == Change.SkillId;
		};
		return Applied.ContainsByPredicate(Same) || AppliedIni.ContainsByPredicate(Same);
	});
	++PendingVersion;
	SavePendingChanges();

	UE_LOG(LogNUVFXViewer, Log, TEXT("적용 %d건 -> %s (백업 %s %s)"), AppliedCount, *FString::Join(Targets, TEXT(" · ")), *Backup, *IniBackup);
	for (const FString& Line : Lines)
	{
		UE_LOG(LogNUVFXViewer, Log, TEXT("  %s"), *Line);
	}

	const FString Summary = FString::Printf(TEXT("적용 %d건"), AppliedCount);
	if (!Applied.IsEmpty())
	{
		if (!UpdateViewerTable(File, Error))
		{
			UE_LOG(LogNUVFXViewer, Warning, TEXT("뷰어의 표를 못 바꿈 (CSV 는 씀): %s"), *Error);
		}

		FinishTableWrite(HistoryPath, Summary, bAskUser);
	}
	else
	{
		// ini 만 고침, DT 는 그대로라 다시 채우지 않음
		RefreshTableState();
		++PendingVersion;
		TableStatus = FString::Printf(TEXT("%s - DefaultGame.ini 기본 공격 항목 고침 (커밋할 때 같이)"), *Summary);
	}

	if (Setup.Mode == ENUVFXViewerMode::Skill)
	{
		Play();
	}
}

void ANUVFXViewerDirector::RevertLastApply(bool bAskUser)
{
	if (IsReimportingTable())
	{
		TableStatus = TEXT("DT 다시 채우는 중 - 끝난 뒤 되돌리기");
		return;
	}

	const TSharedPtr<FJsonObject> Root = LastApplyHistory.IsEmpty() ? nullptr : LoadJsonFile(LastApplyHistory);
	const TArray<TSharedPtr<FJsonValue>>* Changes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* JobChanges = nullptr;
	if (Root.IsValid())
	{
		Root->TryGetArrayField(TEXT("changes"), Changes);
		Root->TryGetArrayField(TEXT("jobAttack"), JobChanges);
	}

	const bool bHasTable = Changes && !Changes->IsEmpty();
	const bool bHasIni = JobChanges && !JobChanges->IsEmpty();
	if (!bHasTable && !bHasIni)
	{
		TableStatus = TEXT("되돌릴 적용이 없음");
		return;
	}

	const FString CsvPath = GetSkillVFXCsvPath();
	FNUVFXViewerTableFile File;
	FString Error;
	if (bHasTable && (CsvPath.IsEmpty() || !File.Load(CsvPath, Error)))
	{
		TableStatus = CsvPath.IsEmpty() ? TEXT("DT_SkillVFX 에 원본 CSV 기록이 없음") : Error;
		AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
		return;
	}

	const FString IniPath = GetJobAttackIniPath();
	FNUVFXViewerJobAttackIni Ini;
	if (bHasIni && !Ini.Load(IniPath, Error))
	{
		TableStatus = Error;
		AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
		return;
	}

	// 적용 뒤에 다시 바뀐 행 · 항목은 되돌리면 그 변경도 사라지므로 따로 알림
	TArray<FString> Lines;
	TArray<FString> Changed;
	TArray<TSharedPtr<FJsonValue>> RevertChanges;
	for (const TSharedPtr<FJsonValue>& Value : bHasTable ? *Changes : TArray<TSharedPtr<FJsonValue>>())
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		int32 SkillId = 0;
		if (!Object.IsValid() || !Object->TryGetNumberField(TEXT("skill"), SkillId))
		{
			continue;
		}

		bool bNewRow = false;
		Object->TryGetBoolField(TEXT("newRow"), bNewRow);
		const TSharedPtr<FJsonObject>* BeforeObject = nullptr;
		const TSharedPtr<FJsonObject>* AfterObject = nullptr;
		Object->TryGetObjectField(TEXT("before"), BeforeObject);
		Object->TryGetObjectField(TEXT("after"), AfterObject);
		const FNUVFXViewerTableRow Before = TableRowFromJson(BeforeObject ? *BeforeObject : nullptr);
		const FNUVFXViewerTableRow After = TableRowFromJson(AfterObject ? *AfterObject : nullptr);

		FNUVFXViewerTableRow Current;
		const bool bHasRow = File.FindRow(SkillId, Current);
		if (!bHasRow || Current != After)
		{
			Changed.Add(GetSkillLabel(SkillId));
		}

		Lines.Add(FString::Printf(TEXT("%s%s"), *GetSkillLabel(SkillId), bNewRow ? TEXT("  (적용 때 만든 행 - 지움)") : TEXT("")));
		if (!bNewRow)
		{
			for (const FString& Line : DescribeRowDiff(bHasRow ? Current : FNUVFXViewerTableRow(), Before))
			{
				Lines.Add(TEXT("    ") + Line);
			}
		}

		const TSharedRef<FJsonObject> Revert = MakeShared<FJsonObject>();
		Revert->SetNumberField(TEXT("skill"), SkillId);
		Revert->SetBoolField(TEXT("removeRow"), bNewRow);
		Revert->SetObjectField(TEXT("before"), TableRowToJson(Current));
		Revert->SetObjectField(TEXT("after"), TableRowToJson(Before));
		RevertChanges.Add(MakeShared<FJsonValueObject>(Revert));
	}

	// 기본 공격: ini 항목을 적용 전 글자 그대로 (적용 때 만든 항목은 지움)
	TArray<TSharedPtr<FJsonValue>> RevertJobChanges;
	for (const TSharedPtr<FJsonValue>& Value : bHasIni ? *JobChanges : TArray<TSharedPtr<FJsonValue>>())
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		int32 JobId = INDEX_NONE;
		if (!Object.IsValid() || !Object->TryGetNumberField(TEXT("job"), JobId))
		{
			continue;
		}

		bool bNewRow = false;
		FString BeforeEntry;
		FString AfterEntry;
		Object->TryGetBoolField(TEXT("newRow"), bNewRow);
		Object->TryGetStringField(TEXT("before"), BeforeEntry);
		Object->TryGetStringField(TEXT("after"), AfterEntry);

		FString CurrentEntry;
		const bool bHasEntry = Ini.FindEntry(JobId, CurrentEntry);
		const FNUVFXViewerTableRow Current = bHasEntry ? FNUVFXViewerJobAttackIni::ParseEntry(CurrentEntry) : FNUVFXViewerTableRow();
		if (!bHasEntry || Current != FNUVFXViewerJobAttackIni::ParseEntry(AfterEntry))
		{
			Changed.Add(FString::Printf(TEXT("기본 공격 · 직업 %d"), JobId));
		}

		Lines.Add(FString::Printf(TEXT("기본 공격 · 직업 %d%s"), JobId, bNewRow ? TEXT("  (적용 때 만든 항목 - 지움)") : TEXT("")));
		if (!bNewRow)
		{
			for (const FString& Line : DescribeRowDiff(Current, FNUVFXViewerJobAttackIni::ParseEntry(BeforeEntry)))
			{
				Lines.Add(TEXT("    ") + Line);
			}
		}

		const TSharedRef<FJsonObject> Revert = MakeShared<FJsonObject>();
		Revert->SetNumberField(TEXT("job"), JobId);
		Revert->SetBoolField(TEXT("removeRow"), bNewRow);
		Revert->SetStringField(TEXT("before"), bHasEntry ? CurrentEntry : FString());
		Revert->SetStringField(TEXT("after"), BeforeEntry);
		RevertJobChanges.Add(MakeShared<FJsonValueObject>(Revert));
	}

	FString Message = FString::Printf(TEXT("마지막 적용(%s)을 그 전 값으로 되돌립니다.\n\n%s"), *LastApplySummary, *FString::Join(Lines, TEXT("\n")));
	if (!Changed.IsEmpty())
	{
		Message += TEXT("\n\n주의: 다음은 적용 뒤에 다시 바뀌었습니다. 되돌리면 그 변경도 사라집니다: ") + FString::Join(Changed, TEXT(", "));
	}

	Message += TEXT("\n\n되돌리기 전 파일도 백업합니다.");
	if (!AskOrLog(bAskUser, EAppMsgType::OkCancel, Message))
	{
		TableStatus = TEXT("되돌리기 취소");
		return;
	}

	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"));
	const FString Backup = bHasTable ? BackupFile(CsvPath, Stamp) : FString();
	const FString IniBackup = bHasIni ? BackupFile(IniPath, Stamp) : FString();
	if ((bHasTable && Backup.IsEmpty()) || (bHasIni && IniBackup.IsEmpty()))
	{
		TableStatus = TEXT("백업을 못 만들어 되돌리지 않음");
		AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
		return;
	}

	if (bHasTable)
	{
		for (const TSharedPtr<FJsonValue>& Value : RevertChanges)
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			const int32 SkillId = static_cast<int32>(Object->GetNumberField(TEXT("skill")));
			if (Object->GetBoolField(TEXT("removeRow")))
			{
				File.RemoveRow(SkillId);
			}
			else
			{
				File.SetRow(SkillId, TableRowFromJson(Object->GetObjectField(TEXT("after"))), MakeNewRowName(SkillId));
			}
		}

		if (!File.Save(Error))
		{
			TableStatus = Error;
			AskOrLog(bAskUser, EAppMsgType::Ok, Error);
			return;
		}
	}

	if (bHasIni)
	{
		for (const TSharedPtr<FJsonValue>& Value : RevertJobChanges)
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			const int32 JobId = static_cast<int32>(Object->GetNumberField(TEXT("job")));
			if (Object->GetBoolField(TEXT("removeRow")))
			{
				Ini.RemoveEntry(JobId);
			}
			else
			{
				Ini.SetEntry(JobId, Object->GetStringField(TEXT("after")));
			}
		}

		if (!Ini.Save(Error))
		{
			TableStatus = bHasTable ? Error + TEXT(" (CSV 는 되돌림)") : Error;
			AskOrLog(bAskUser, EAppMsgType::Ok, TableStatus);
			return;
		}

		for (const TSharedPtr<FJsonValue>& Value : RevertJobChanges)
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			const int32 JobId = static_cast<int32>(Object->GetNumberField(TEXT("job")));
			const FNUVFXViewerTableRow Row = FNUVFXViewerJobAttackIni::ParseEntry(Object->GetStringField(TEXT("after")));
			SetJobAttackRow(JobId, Object->GetBoolField(TEXT("removeRow")) ? nullptr : &Row);
		}
	}

	// 원래 적용 이력은 되돌림으로 표시하고, 되돌리기도 이력으로 남김
	SetHistoryField(LastApplyHistory, TEXT("reverted"), TEXT("true"));

	const TSharedRef<FJsonObject> History = MakeShared<FJsonObject>();
	History->SetStringField(TEXT("type"), TEXT("revert"));
	History->SetStringField(TEXT("of"), ProjectRelativePath(LastApplyHistory));
	History->SetStringField(TEXT("time"), FDateTime::UtcNow().ToIso8601());
	History->SetStringField(TEXT("localTime"), FDateTime::Now().ToString(TEXT("%m/%d %H:%M")));
	History->SetStringField(TEXT("user"), FPlatformProcess::UserName());
	if (bHasTable)
	{
		History->SetStringField(TEXT("csv"), ProjectRelativePath(CsvPath));
		History->SetStringField(TEXT("backup"), ProjectRelativePath(Backup));
	}
	if (bHasIni)
	{
		History->SetStringField(TEXT("ini"), ProjectRelativePath(IniPath));
		History->SetStringField(TEXT("iniBackup"), ProjectRelativePath(IniBackup));
	}
	History->SetArrayField(TEXT("changes"), RevertChanges);
	History->SetArrayField(TEXT("jobAttack"), RevertJobChanges);
	History->SetStringField(TEXT("reimport"), bHasTable ? TEXT("pending") : TEXT("none"));

	const FString HistoryPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TableHistoryFolder,
		FString::Printf(TEXT("Revert_%s.json"), *Stamp)));
	SaveJsonFile(History, HistoryPath);

	const int32 RevertCount = RevertChanges.Num() + RevertJobChanges.Num();
	UE_LOG(LogNUVFXViewer, Log, TEXT("되돌림 %d건 (백업 %s %s)"), RevertCount, *Backup, *IniBackup);
	for (const FString& Line : Lines)
	{
		UE_LOG(LogNUVFXViewer, Log, TEXT("  %s"), *Line);
	}

	const FString Summary = FString::Printf(TEXT("되돌림 %d건"), RevertCount);
	if (bHasTable)
	{
		if (!UpdateViewerTable(File, Error))
		{
			UE_LOG(LogNUVFXViewer, Warning, TEXT("뷰어의 표를 못 바꿈 (CSV 는 씀): %s"), *Error);
		}

		FinishTableWrite(HistoryPath, Summary, bAskUser);
	}
	else
	{
		RefreshTableState();
		++PendingVersion;
		TableStatus = FString::Printf(TEXT("%s - DefaultGame.ini 기본 공격 항목 되돌림"), *Summary);
	}

	if (Setup.Mode == ENUVFXViewerMode::Skill)
	{
		Play();
	}
}

void ANUVFXViewerDirector::FinishTableWrite(const FString& HistoryPath, const FString& Summary, bool bAskUser)
{
	RefreshTableState();

	// CSV 를 썼으니 디스크의 DT 는 다시 채울 때까지 CSV 와 다름
	bTableNeedsReimport = true;

	if (IsProjectEditorOpen())
	{
		SetHistoryField(HistoryPath, TEXT("reimport"), TEXT("editor-open"));
		TableStatus = FString::Printf(TEXT("%s - CSV 만 고침 (에디터가 켜져 있음), DT 는 에디터에서 다시 가져오기"), *Summary);
		AskOrLog(bAskUser, EAppMsgType::Ok, FString::Printf(
			TEXT("%s: %s 를 고쳤습니다.\n\n에디터가 켜져 있어 DT_SkillVFX 는 다시 채우지 않았습니다 (에디터가 같은 DT 를 들고 있어 뒤에서 저장하면 서로 덮어씀).\n\n- 에디터에서 DT_SkillVFX 를 열어 [다시 가져오기]를 누르거나\n- 에디터를 끄고 뷰어의 [DT 다시 채우기]를 누르세요.\n\n뷰어에는 이미 바뀐 값으로 보입니다."),
			*Summary, *FPaths::GetCleanFilename(GetSkillVFXCsvPath())));
		return;
	}

	ReimportHistory = HistoryPath;
	ReimportSkillVFXTable();
	if (IsReimportingTable())
	{
		TableStatus = FString::Printf(TEXT("%s - DT 다시 채우는 중 (화면 없는 에디터, 1~2분)"), *Summary);
	}
}

void ANUVFXViewerDirector::ReimportSkillVFXTable()
{
	if (IsReimportingTable())
	{
		return;
	}

	if (IsProjectEditorOpen())
	{
		TableStatus = TEXT("에디터가 켜져 있어 DT 를 다시 채우지 않음 - 에디터에서 다시 가져오기");
		ShowDialog(EAppMsgType::Ok, TEXT("에디터가 켜져 있어 DT_SkillVFX 를 다시 채우지 않았습니다.\n\n에디터에서 DT_SkillVFX 를 열어 [다시 가져오기]를 누르거나, 에디터를 끄고 다시 누르세요."));
		return;
	}

	UDataTable* Table = GetSkillVFXTable();
	const FString CsvPath = GetSkillVFXCsvPath();
	const FString EditorCmd = FPaths::Combine(FPaths::GetPath(FPlatformProcess::ExecutablePath()), TEXT("UnrealEditor-Cmd.exe"));
	if (!IsValid(Table) || CsvPath.IsEmpty() || !FPaths::FileExists(EditorCmd))
	{
		TableStatus = FString::Printf(TEXT("DT 다시 채우기를 시작 못 함 (표 · CSV 기록 · %s 확인)"), *EditorCmd);
		return;
	}

	// 뷰어가 DT 파일을 열고 있으면 커맨드렛이 저장하지 못함 (-game 은 읽은 패키지 파일을 잠금)
	FlushAsyncLoading();
	ResetLoaders(Table->GetOutermost());

	const FString Project = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
	const FString ReimportLog = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), ReimportLogFile));
	const FString Params = FString::Printf(TEXT("\"%s\" -run=%s -Csv=\"%s\" -unattended -nosplash -nopause -abslog=\"%s\""),
		*Project, ReimportCommandlet, *CsvPath, *ReimportLog);

	ReimportProcess = FPlatformProcess::CreateProc(*EditorCmd, *Params, false, true, true, nullptr, 0, nullptr, nullptr);
	ReimportStartTime = FPlatformTime::Seconds();
	if (!ReimportProcess.IsValid())
	{
		TableStatus = FString::Printf(TEXT("DT 다시 채우기를 시작 못 함: %s"), *EditorCmd);
		return;
	}

	TableStatus = TEXT("DT 다시 채우는 중 (화면 없는 에디터, 1~2분)");
	UE_LOG(LogNUVFXViewer, Log, TEXT("DT 다시 채우기 시작: %s %s"), *EditorCmd, *Params);
}

void ANUVFXViewerDirector::TickTableReimport()
{
	if (!ReimportProcess.IsValid())
	{
		return;
	}

	const double Elapsed = FPlatformTime::Seconds() - ReimportStartTime;
	if (FPlatformProcess::IsProcRunning(ReimportProcess))
	{
		TableStatus = FString::Printf(TEXT("DT 다시 채우는 중 (화면 없는 에디터) %.0f초"), Elapsed);
		return;
	}

	int32 ReturnCode = -1;
	FPlatformProcess::GetProcReturnCode(ReimportProcess, &ReturnCode);
	FPlatformProcess::CloseProc(ReimportProcess);
	ReimportProcess = FProcHandle();

	if (ReturnCode == 0)
	{
		// 뷰어가 들고 있는 DT 의 원본 기록도 맞춰 둠 (다음 비교에서 다시 "다름" 으로 나오지 않게)
#if WITH_EDITORONLY_DATA
		UDataTable* Table = GetSkillVFXTable();
		if (IsValid(Table) && IsValid(Table->AssetImportData))
		{
			Table->AssetImportData->Update(GetSkillVFXCsvPath());
		}
#endif
		bTableNeedsReimport = false;
		TableStatus = FString::Printf(TEXT("DT 다시 채움 (%.0f초) - %s 와 DT_SkillVFX.uasset 을 같이 커밋"),
			Elapsed, *FPaths::GetCleanFilename(GetSkillVFXCsvPath()));
	}
	else
	{
		TableStatus = FString::Printf(TEXT("DT 다시 채우기 실패 (끝 코드 %d) - Saved/%s 확인"), ReturnCode, ReimportLogFile);
		ShowDialog(EAppMsgType::Ok, TableStatus + TEXT("\n\nCSV 는 고쳐져 있습니다. 에디터에서 DT_SkillVFX 를 다시 가져와도 됩니다."));
	}

	UE_LOG(LogNUVFXViewer, Log, TEXT("%s"), *TableStatus);
	if (!ReimportHistory.IsEmpty())
	{
		SetHistoryField(ReimportHistory, TEXT("reimport"), ReturnCode == 0 ? TEXT("ok") : FString::Printf(TEXT("failed:%d"), ReturnCode));
		ReimportHistory.Reset();
	}
}

void ANUVFXViewerDirector::SetHistoryField(const FString& HistoryPath, const FString& Field, const FString& Value) const
{
	const TSharedPtr<FJsonObject> Root = LoadJsonFile(HistoryPath);
	if (!Root.IsValid())
	{
		return;
	}

	// "true" · "false" 는 참거짓으로
	if (Value == TEXT("true") || Value == TEXT("false"))
	{
		Root->SetBoolField(Field, Value == TEXT("true"));
	}
	else
	{
		Root->SetStringField(Field, Value);
	}

	SaveJsonFile(Root.ToSharedRef(), HistoryPath);
}

void ANUVFXViewerDirector::RequestReload()
{
	if (State != EState::Ready)
	{
		return;
	}

	// 다시 읽었는지 확인할 에셋, 지금 재생 중인 것과 직접 지정 칸
	ReloadWatchList.Reset();
	for (const FSoftObjectPath& Path : { Report.Spec.CastVFX.ToSoftObjectPath(), Report.Spec.ProjectileVFX.ToSoftObjectPath(),
		Report.Spec.ImpactVFX.ToSoftObjectPath(), Setup.CustomCast, Setup.CustomProjectile, Setup.CustomImpact })
	{
		if (Path.IsValid())
		{
			ReloadWatchList.AddUnique(Path);
		}
	}

	CancelSeek();
	StopPlayback();
	DestroyLiveEffects();
	DestroyAvatars();

	// PIE 는 에디터에서 고친 것이 이미 메모리에 있음, 다시 세우기만 함
	if (GIsEditor)
	{
		ReloadMessage = TEXT("PIE - 에디터의 현재 에셋으로 다시 세움");
		BuildAssetItems();
		if (Panel.IsValid())
		{
			Panel->RefreshLists();
		}

		SpawnAvatars();
		Play();
		return;
	}

	// 쓰레기 수거가 끝날 때까지 몇 프레임 기다린 뒤 남은 것이 있는지 봄 (TickReload)
	State = EState::Reloading;
	ReloadFramesLeft = 3;
	ReloadMessage = TEXT("새로고침 중...");
	if (GEngine)
	{
		GEngine->ForceGarbageCollection(true);
	}
}

void ANUVFXViewerDirector::TickReload()
{
	if (--ReloadFramesLeft > 0)
	{
		return;
	}

	// 아직 메모리에 있으면 다음 로드가 디스크를 안 읽음, 누가 잡고 있는지 로그로 남김
	int32 Leftover = 0;
	for (const FSoftObjectPath& Path : ReloadWatchList)
	{
		if (!FindPackage(nullptr, *Path.GetLongPackageName()))
		{
			continue;
		}

		++Leftover;
		UE_LOG(LogNUVFXViewer, Warning, TEXT("새로고침 후에도 메모리에 남음: %s"), *Path.ToString());

		if (UObject* Object = Path.ResolveObject())
		{
			FReferenceChainSearch Search(Object, EReferenceChainSearchMode::Shortest | EReferenceChainSearchMode::PrintResults);
		}
	}

	ReloadMessage = Leftover == 0
		? FString::Printf(TEXT("새로고침 완료 - 이펙트 %d개를 디스크에서 다시 읽음"), ReloadWatchList.Num())
		: FString::Printf(TEXT("%d개가 메모리에 남아 반영 안 됨 (로그) - 뷰어를 다시 켜 주세요"), Leftover);

	BuildAssetItems();
	if (Panel.IsValid())
	{
		Panel->RefreshLists();
	}

	State = EState::Ready;
	SpawnAvatars();
	Play();
}

/////////////////////////////////////////////////////////////////
// 틱 · 자동 캡처
/////////////////////////////////////////////////////////////////

void ANUVFXViewerDirector::TickReadyCommands()
{
	if (State != EState::Ready || (ReadyCommands.IsEmpty() && !bQuitAfterReadyCommands))
	{
		return;
	}

	// 준비된 뒤 1초 (첫 재생 · 후보 읽기가 자리 잡게), 명령 사이도 1초, DT 다시 채우는 중이면 기다림
	const double Now = FPlatformTime::Seconds();
	if (NextReadyCommandTime <= 0.0)
	{
		NextReadyCommandTime = Now + 1.0;
		return;
	}

	if (Now < NextReadyCommandTime || IsReimportingTable())
	{
		return;
	}

	if (ReadyCommands.IsEmpty())
	{
		bQuitAfterReadyCommands = false;
		if (!bAutomation)
		{
			UE_LOG(LogNUVFXViewer, Log, TEXT("준비된 뒤 명령을 다 돌림 - 종료"));
			UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
		}
		return;
	}

	const FString Command = ReadyCommands[0].TrimStartAndEnd();
	ReadyCommands.RemoveAt(0);
	NextReadyCommandTime = Now + 1.0;
	UE_LOG(LogNUVFXViewer, Log, TEXT("명령: %s"), *Command);
	GEngine->Exec(GetWorld(), *Command);
}

void ANUVFXViewerDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bWindowFitDone)
	{
		bWindowFitDone = FitWindowToWorkArea();
	}

	UpdateViewLayout();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	if (bHitchLog)
	{
		const double Now = FPlatformTime::Seconds();
		const double FrameMs = (Now - LastTickRealTime) * 1000.0;
		if (LastTickRealTime > 0.0 && FrameMs > 40.0)
		{
			UE_LOG(LogNUVFXViewer, Warning, TEXT("끊김 %.0fms (재생 %s %.2f초)"), FrameMs,
				bPlaying ? TEXT("중") : TEXT("아님"), bPlaying ? World->GetTimeSeconds() - PlayStartTime : 0.0);
		}
		LastTickRealTime = Now;
	}

	switch (State)
	{
	case EState::LoadingStage:
		{
			const UNUBattleEncounterSubsystem* Encounter = World->GetSubsystem<UNUBattleEncounterSubsystem>();
			if (!IsValid(Encounter) || Encounter->IsStageReady())
			{
				FinishStageLoad();
			}
		}
		break;

	case EState::Reloading:
		TickReload();
		break;

	case EState::Ready:
		TickSeek();

		// 감는 중에는 목표 시각까지 끝으로 치지 않음
		if (bPlaying && !IsSeeking())
		{
			const double Elapsed = GetElapsed();
			LastElapsed = static_cast<float>(Elapsed);

			if (bAutomation && !World->IsPaused())
			{
				SampleEffects();
			}

			if (!World->IsPaused() && Elapsed >= Report.TotalDuration
				&& (!HasLiveEffects() || Elapsed >= Report.TotalDuration + LoopTailMax))
			{
				// 이펙트가 남은 채 끝났으면 대기 상한에 걸린 것 — 끝나지 않는 이미터가 있음
				bEndedAtTailCap = HasLiveEffects();
				PlaybackEndTime = LastElapsed;
				if (bEndedAtTailCap)
				{
					LingeringEffects = DescribeLingeringEffects();
					UE_LOG(LogNUVFXViewer, Warning, TEXT("잔상 대기 상한(%.1f초)까지 안 끝남: %s"), LoopTailMax,
						*FString::Join(LingeringEffects, TEXT(" / ")));
				}

				bPlaying = false;
				MeasuredEndTime = FMath::Max(MeasuredEndTime, LastElapsed);

				// 번갈아 보기는 다음 후보로, 반복은 같은 설정이라 잰 끝을 지킴
				if (bCompareCycle && !bAutomation && Setup.Mode == ENUVFXViewerMode::Skill && GetCandidateCount(Setup.SkillId) > 1)
				{
					StepCandidate(1);
				}
				else if (Setup.bLoop)
				{
					StartPlayback();
				}
			}
		}

		TickCompareFileWatch();
		break;
	}

	TickTableReimport();
	TickAutomation();
	TickReadyCommands();
}

void ANUVFXViewerDirector::TickAutomation()
{
	if (bShotPanelEnd && Panel.IsValid())
	{
		Panel->ScrollPanelToEnd();
	}

	if (!bAutomation || State != EState::Ready || bShotPending)
	{
		return;
	}

	// 무대가 뜬 직후는 셰이더 · 텍스처가 준비 중이라 몇 프레임 기다림
	if (AutomationSettleFrames < AutomationSettleFrameCount)
	{
		++AutomationSettleFrames;
		return;
	}

	// 비동기 미리 불러오기를 재는 중이면 다 불러올 때까지 기다림
	if (bAsyncPreload)
	{
		const UNUBattleVFXPreloadSubsystem* Preload = GetGameInstance()->GetSubsystem<UNUBattleVFXPreloadSubsystem>();
		if (Preload && (Preload->IsLoading() || Preload->IsWarmingUp()))
		{
			return;
		}
	}

	if (!SkillQueue.IsEmpty())
	{
		TickSkillQueue();
		return;
	}

	// 예열 없이 첫 재생을 바로 잼
	if (bNoWarmUp && !bWarmUpDone)
	{
		bWarmUpDone = true;
		UE_LOG(LogNUVFXViewer, Log, TEXT("첫 재생 시작 (예열 없음)"));
		Play();
		return;
	}

	// 예열: 한 번 끝까지 틀어 이펙트 머티리얼 셰이더 · 나이아가라 컴파일을 끝냄 (첫 장면이 비지 않게)
	if (!bWarmUpDone)
	{
		if (!bPlaying && !bWarmUpStarted)
		{
			bWarmUpStarted = true;
			Play();

#if WITH_EDITORONLY_DATA
			for (const TSoftObjectPtr<UNiagaraSystem>* Slot : { &Report.Spec.CastVFX, &Report.Spec.ProjectileVFX, &Report.Spec.ImpactVFX })
			{
				if (UNiagaraSystem* System = Slot->Get())
				{
					System->WaitForCompilationComplete(true, false);
				}
			}
#endif
			return;
		}

		if (bPlaying)
		{
			return;
		}

#if WITH_EDITOR
		if (GShaderCompilingManager)
		{
			GShaderCompilingManager->FinishAllCompilation();
		}
#endif

		bWarmUpDone = true;
		DestroyLiveEffects();
		Play();
		return;
	}

	if (NextShotIndex < ShotTimes.Num() && bSeekShots)
	{
		// 감기로 찍기: 그 시각으로 감아 멈춘 뒤 화면(TSR)이 가라앉으면 찍음
		if (!bSeekShotStarted)
		{
			SeekTo(ShotTimes[NextShotIndex]);
			bResumeAfterSeek = false;
			bSeekShotStarted = true;
			SeekShotSettleFrames = 0;
			return;
		}

		if (IsSeeking() || ++SeekShotSettleFrames < SeekShotSettleFrameCount)
		{
			return;
		}

		bSeekShotStarted = false;
		RequestShot(NextShotIndex);
		return;
	}

	if (NextShotIndex < ShotTimes.Num())
	{
		const double Elapsed = GetWorld()->GetTimeSeconds() - PlayStartTime;
		if (Elapsed >= ShotTimes[NextShotIndex])
		{
			RequestShot(NextShotIndex);
		}
		return;
	}

	// 새로고침 확인: 다시 읽은 뒤 같은 시각을 한 번 더 찍음, 파일 이름 앞에 reload_
	if (bShotReloadPass && !bReloadPassDone)
	{
		bReloadPassDone = true;
		NextShotIndex = 0;
		RequestReload();
		return;
	}

	WriteAutomationReport();
	bAutomation = false;

	if (bAutomationQuit)
	{
		UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	}
}

void ANUVFXViewerDirector::RequestShot(int32 ShotIndex)
{
	// 찍는 프레임에 멈춰 두면 정확히 그 시각의 모습이 남음, 처리되면 다시 풂
	UGameplayStatics::SetGamePaused(this, true);

	const double Elapsed = GetWorld()->GetTimeSeconds() - PlayStartTime;
	const FString VariantTag = QueueVariantIndex != INDEX_NONE ? FString::Printf(TEXT("_v%d"), QueueVariantIndex) : FString();
	const FString FileName = FPaths::Combine(ShotDirectory, QueueSkillId != INDEX_NONE
		? FString::Printf(TEXT("q%02d_s%d%s_%02d_hit%+.2f.png"), SkillQueueIndex, QueueSkillId, *VariantTag, ShotIndex,
			Elapsed - GetShotAnchorTime())
		: FString::Printf(TEXT("%s%sshot_%02d_%.2fs.png"), bReloadPassDone ? TEXT("reload_") : TEXT(""),
			bSeekShots ? TEXT("seek_") : TEXT(""), ShotIndex, Elapsed));

	bShotPending = true;
	WrittenShots.Add(FileName);
	FScreenshotRequest::RequestScreenshot(FileName, bShotWithUI, false, false, FIntRect(), true);
}

FString ANUVFXViewerDirector::TakeShotNow()
{
	const FString Directory = ShotDirectory.IsEmpty()
		? FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("VFXViewer"), TEXT("Manual")))
		: ShotDirectory;
	const FString FileName = FPaths::Combine(Directory,
		FString::Printf(TEXT("manual_%s.png"), *FDateTime::Now().ToString(TEXT("%H%M%S_%s"))));

	FScreenshotRequest::RequestScreenshot(FileName, bPanelVisible, false, false, FIntRect(), true);
	return FileName;
}

void ANUVFXViewerDirector::HandleScreenshotProcessed()
{
	if (!bShotPending)
	{
		return;
	}

	bShotPending = false;
	++NextShotIndex;
	UGameplayStatics::SetGamePaused(this, false);
}

void ANUVFXViewerDirector::ParseQueueCaster(const FString& CasterText, FQueueEntry& OutEntry)
{
	const FString Text = CasterText.TrimStartAndEnd();
	if (Text.IsEmpty())
	{
		return;
	}

	OutEntry.bCasterGiven = true;
	const TCHAR Kind = FChar::ToLower(Text[0]);
	if (Kind == TEXT('n') || Kind == TEXT('e'))
	{
		OutEntry.Caster = Kind == TEXT('n') ? ENUVFXViewerCaster::AllyNaia : ENUVFXViewerCaster::Enemy;
		const FString NaiaText = Text.Mid(1);
		OutEntry.NaiaId = NaiaText.IsEmpty() ? INDEX_NONE : FCString::Atoi(*NaiaText);
	}
	else
	{
		OutEntry.Caster = ENUVFXViewerCaster::Player;
		OutEntry.JobId = FCString::Atoi(*Text);

		// 1003f · 1003m 처럼 끝 글자로 성별을 고름, 같은 직업 그림이 남 · 여 둘 다 있을 때 (엘리멘탈리스트 등)
		const TCHAR GenderMark = FChar::ToLower(Text[Text.Len() - 1]);
		OutEntry.Gender = GenderMark == TEXT('f') ? 1 : (GenderMark == TEXT('m') ? 0 : INDEX_NONE);
	}
}

void ANUVFXViewerDirector::ApplyQueueEntry(const FQueueEntry& Entry)
{
	bool bRespawn = false;
	if (Entry.bCasterGiven)
	{
		Setup.Caster = Entry.Caster;
	}

	// 직업은 남캐부터(f 를 붙이면 여캐부터), 그 성별에 그 직업 그림이 없으면 다른 성별로 (ValidateJobChoice)
	if (Entry.Caster == ENUVFXViewerCaster::Player && Entry.JobId != INDEX_NONE)
	{
		const int32 OldJob = Setup.JobId;
		const int32 OldGender = Setup.Gender;
		Setup.JobId = Entry.JobId;
		Setup.Gender = Entry.Gender != INDEX_NONE ? Entry.Gender : 0;
		ValidateJobChoice();
		bRespawn = Setup.JobId != OldJob || Setup.Gender != OldGender;
	}

	// 나이아는 전투 그림이 있어야 세울 수 있음, 없으면 지금 나이아로 하고 보고서에 남김 (allyNaiaId · enemyNaiaId)
	if (Entry.Caster != ENUVFXViewerCaster::Player && Entry.NaiaId != INDEX_NONE)
	{
		int32& NaiaSlot = Entry.Caster == ENUVFXViewerCaster::AllyNaia ? Setup.AllyNaiaId : Setup.EnemyNaiaId;
		if (!FindChoice(NaiaChoices, Entry.NaiaId))
		{
			UE_LOG(LogNUVFXViewer, Warning, TEXT("묶음 %d: 나이아 %d 는 전투 그림이 없어 지금 나이아(%d)로 틂"),
				Entry.SkillId, Entry.NaiaId, NaiaSlot);
		}
		else if (NaiaSlot != Entry.NaiaId)
		{
			NaiaSlot = Entry.NaiaId;
			bRespawn = true;
		}
	}

	ClampTargetIndex();

	// 무대가 아직 안 섰으면 FinishStageLoad 가 이 설정으로 세움
	if (bRespawn && State == EState::Ready)
	{
		SpawnAvatars();
	}
}

void ANUVFXViewerDirector::TickSkillQueue()
{
	switch (SkillQueuePhase)
	{
	case 0:	// 다음 스킬을 세우고 예열 재생
		{
			if (!SkillQueue.IsValidIndex(SkillQueueIndex))
			{
				WriteAutomationReport();
				bAutomation = false;
				if (bAutomationQuit)
				{
					UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
				}
				return;
			}

			const FQueueEntry& Entry = SkillQueue[SkillQueueIndex];
			ApplyQueueEntry(Entry);

			// 비교 후보 항목은 그 후보를 표 행 위에 덮어 틂, 그 밖의 항목은 표 그대로
			const int32 Candidate = Entry.CompareSet != INDEX_NONE
				? FindCandidateIndex(Entry.SkillId, Entry.CompareSet, Entry.CompareVariant)
				: 0;
			CandidateIndex = FMath::Max(Candidate, 0);

			Setup.Mode = ENUVFXViewerMode::Skill;
			Setup.SkillId = Entry.SkillId;
			QueueSkillId = INDEX_NONE;
			QueueCompareSet = INDEX_NONE;
			QueueVariantIndex = INDEX_NONE;
			Play();
			SkillQueuePhase = 1;
		}
		break;

	case 1:	// 예열이 끝나면 본 재생, 찍을 시각을 첫 타격 기준으로 정함
		{
			if (bPlaying)
			{
				return;
			}

#if WITH_EDITOR
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->FinishAllCompilation();
			}
#endif

			DestroyLiveEffects();
			Play();

			ShotTimes.Reset();
			for (const float Offset : ShotOffsetsFromHit)
			{
				ShotTimes.Add(FMath::Max(0.f, GetShotAnchorTime() + Offset));
			}
			NextShotIndex = 0;
			QueueSkillId = SkillQueue[SkillQueueIndex].SkillId;
			QueueCompareSet = SkillQueue[SkillQueueIndex].CompareSet;
			QueueVariantIndex = SkillQueue[SkillQueueIndex].CompareVariant;
			SkillQueuePhase = 2;
		}
		break;

	case 2:	// 찍기
		if (NextShotIndex < ShotTimes.Num())
		{
			if (GetWorld()->GetTimeSeconds() - PlayStartTime >= ShotTimes[NextShotIndex])
			{
				RequestShot(NextShotIndex);
			}
			return;
		}
		SkillQueuePhase = 3;
		break;

	default:	// 끝날 때까지 기다렸다가 다음 스킬
		if (bPlaying)
		{
			return;
		}

		QueueReports.Add(MakeShared<FJsonValueObject>(BuildReportJson()));
		WrittenShots.Reset();
		QueueSkillId = INDEX_NONE;
		QueueCompareSet = INDEX_NONE;
		QueueVariantIndex = INDEX_NONE;
		++SkillQueueIndex;
		SkillQueuePhase = 0;
		break;
	}
}

void ANUVFXViewerDirector::WriteAutomationReport() const
{
	TSharedRef<FJsonObject> Root = QueueReports.IsEmpty() ? BuildReportJson() : MakeShared<FJsonObject>();
	if (!QueueReports.IsEmpty())
	{
		Root->SetArrayField(TEXT("entries"), QueueReports);
	}

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);

	const FString ReportPath = FPaths::Combine(ShotDirectory, TEXT("report.json"));
	FFileHelper::SaveStringToFile(Json, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	UE_LOG(LogNUVFXViewer, Log, TEXT("자동 캡처 보고서: %s"), *ReportPath);
}

TSharedRef<FJsonObject> ANUVFXViewerDirector::BuildReportJson() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

	const FNUBattleActionMotionSpec& Spec = Report.Spec;
	Root->SetStringField(TEXT("stage"), StageChoices.IsValidIndex(Setup.StageIndex) ? StageChoices[Setup.StageIndex].Label : TEXT(""));
	Root->SetStringField(TEXT("mode"), Setup.Mode == ENUVFXViewerMode::Skill ? TEXT("skill") : TEXT("custom"));
	Root->SetNumberField(TEXT("skillId"), Setup.SkillId);

	// 비교 후보 묶음 캡처면 후보 파일의 묶음 · 후보 번호와 이름 (vfx_compare.py 가 이 번호로 묶음)
	if (CompareSets.IsValidIndex(QueueCompareSet) && CompareSets[QueueCompareSet].Variants.IsValidIndex(QueueVariantIndex))
	{
		Root->SetNumberField(TEXT("compareSet"), QueueCompareSet);
		Root->SetStringField(TEXT("compareTitle"), CompareSets[QueueCompareSet].Title);
		Root->SetNumberField(TEXT("variant"), QueueVariantIndex);
		Root->SetStringField(TEXT("variantLabel"), CompareSets[QueueCompareSet].Variants[QueueVariantIndex].Label);
	}

	Root->SetStringField(TEXT("caster"), Setup.Caster == ENUVFXViewerCaster::Player
		? TEXT("player") : (Setup.Caster == ENUVFXViewerCaster::AllyNaia ? TEXT("naia") : TEXT("enemy")));
	Root->SetNumberField(TEXT("jobId"), Setup.JobId);
	Root->SetNumberField(TEXT("gender"), Setup.Gender);
	Root->SetNumberField(TEXT("hairId"), Setup.HairId);
	Root->SetNumberField(TEXT("allyNaiaId"), Setup.AllyNaiaId);
	Root->SetNumberField(TEXT("enemyNaiaId"), Setup.EnemyNaiaId);
	Root->SetStringField(TEXT("cast"), Spec.CastVFX.ToString());
	Root->SetStringField(TEXT("projectile"), Spec.ProjectileVFX.ToString());
	Root->SetStringField(TEXT("impact"), Spec.ImpactVFX.ToString());
	Root->SetStringField(TEXT("color"), Spec.EffectColor.ToString());
	Root->SetStringField(TEXT("motion"), MotionLabel(Spec.Motion));
	Root->SetBoolField(TEXT("hasSkillProfile"), Report.bHasSkillProfile);
	Root->SetNumberField(TEXT("telegraph"), Spec.TelegraphTime);
	Root->SetNumberField(TEXT("approach"), Spec.ApproachTime);
	Root->SetNumberField(TEXT("action"), Spec.ActionTime);
	Root->SetNumberField(TEXT("castAt"), Report.CastTime);
	Root->SetNumberField(TEXT("hitAt"), Report.HitTime);
	Root->SetNumberField(TEXT("return"), Spec.ReturnTime);
	Root->SetNumberField(TEXT("hitCount"), Report.HitCount);
	Root->SetNumberField(TEXT("hitInterval"), Spec.HitInterval);
	Root->SetNumberField(TEXT("firstHit"), Report.FirstHitDelay);
	Root->SetNumberField(TEXT("total"), Report.TotalDuration);

	TArray<TSharedPtr<FJsonValue>> Warnings;
	for (const FString& Warning : Report.Warnings)
	{
		Warnings.Add(MakeShared<FJsonValueString>(Warning));
	}
	Root->SetArrayField(TEXT("warnings"), Warnings);

	TArray<TSharedPtr<FJsonValue>> Shots;
	for (const FString& Shot : WrittenShots)
	{
		Shots.Add(MakeShared<FJsonValueString>(Shot));
	}
	Root->SetArrayField(TEXT("shots"), Shots);
	Root->SetStringField(TEXT("reload"), ReloadMessage);

	// 점검 기록 — 아바타가 도는 순서로 센 시간표, 뜬 이펙트, 끝
	TSharedRef<FJsonObject> TimelineJson = MakeShared<FJsonObject>();
	TimelineJson->SetNumberField(TEXT("telegraphEnd"), Timeline.TelegraphEnd);
	TimelineJson->SetNumberField(TEXT("approachEnd"), Timeline.ApproachEnd);
	TimelineJson->SetNumberField(TEXT("actingEnd"), Timeline.ActingEnd);
	TimelineJson->SetNumberField(TEXT("returnEnd"), Timeline.ReturnEnd);
	TArray<TSharedPtr<FJsonValue>> CastTimes;
	for (const float Time : Timeline.CastTimes)
	{
		CastTimes.Add(MakeShared<FJsonValueNumber>(Time));
	}
	TArray<TSharedPtr<FJsonValue>> HitTimes;
	for (const float Time : Timeline.HitTimes)
	{
		HitTimes.Add(MakeShared<FJsonValueNumber>(Time));
	}
	TimelineJson->SetArrayField(TEXT("castTimes"), CastTimes);
	TimelineJson->SetArrayField(TEXT("hitTimes"), HitTimes);
	Root->SetObjectField(TEXT("timeline"), TimelineJson);

	const auto ScreenJson = [](const FVector2D& Screen)
	{
		TArray<TSharedPtr<FJsonValue>> Pair;
		Pair.Add(MakeShared<FJsonValueNumber>(Screen.X));
		Pair.Add(MakeShared<FJsonValueNumber>(Screen.Y));
		return Pair;
	};
	Root->SetArrayField(TEXT("casterScreen"), ScreenJson(CasterScreen));
	Root->SetArrayField(TEXT("targetScreen"), ScreenJson(TargetScreen));

	TArray<TSharedPtr<FJsonValue>> Effects;
	for (const TPair<FString, FEffectSample>& Sample : EffectSamples)
	{
		const FString& Path = Sample.Key;
		TSharedRef<FJsonObject> EffectJson = MakeShared<FJsonObject>();
		EffectJson->SetStringField(TEXT("asset"), Path);
		EffectJson->SetStringField(TEXT("slot"),
			Path == Spec.CastVFX.ToSoftObjectPath().ToString() ? TEXT("cast")
			: (Path == Spec.ProjectileVFX.ToSoftObjectPath().ToString() ? TEXT("projectile")
			: (Path == Spec.ImpactVFX.ToSoftObjectPath().ToString() ? TEXT("impact") : TEXT("other"))));
		EffectJson->SetNumberField(TEXT("firstSeen"), Sample.Value.FirstSeen);
		EffectJson->SetNumberField(TEXT("lastSeen"), Sample.Value.LastSeen);
		EffectJson->SetNumberField(TEXT("maxParticles"), Sample.Value.MaxParticles);
		EffectJson->SetArrayField(TEXT("firstScreen"), ScreenJson(Sample.Value.FirstScreen));
		EffectJson->SetArrayField(TEXT("casterScreenAtFirst"), ScreenJson(Sample.Value.CasterScreenAtFirst));
		Effects.Add(MakeShared<FJsonValueObject>(EffectJson));
	}
	Root->SetArrayField(TEXT("effects"), Effects);

	Root->SetNumberField(TEXT("end"), PlaybackEndTime);
	Root->SetBoolField(TEXT("endedAtTailCap"), bEndedAtTailCap);
	TArray<TSharedPtr<FJsonValue>> Lingering;
	for (const FString& Line : LingeringEffects)
	{
		Lingering.Add(MakeShared<FJsonValueString>(Line));
	}
	Root->SetArrayField(TEXT("lingering"), Lingering);

	return Root;
}

void ANUVFXViewerDirector::SampleEffects()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const float Elapsed = static_cast<float>(GetElapsed());
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Component = *It;
		if (Component->GetWorld() != World || !Component->IsActive() || !Component->GetAsset())
		{
			continue;
		}

		// 켜져 있어도 입자가 없으면 안 보이는 것이라 치지 않음
		int32 Particles = 0;
		FNiagaraSystemInstanceControllerPtr Controller = Component->GetSystemInstanceController();
		if (FNiagaraSystemInstance* Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr)
		{
			for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
			{
				Particles += Emitter->GetNumParticles();
			}
		}

		if (Particles <= 0)
		{
			continue;
		}

		FEffectSample& Sample = EffectSamples.FindOrAdd(Component->GetAsset()->GetPathName());
		if (Sample.FirstSeen < 0.f)
		{
			Sample.FirstSeen = Elapsed;
			Sample.FirstScreen = ProjectToView(Component->GetComponentLocation());
			if (const ANUBattleAvatar* Caster = GetCaster())
			{
				Sample.CasterScreenAtFirst = ProjectToView(Caster->GetActorLocation() + FVector(0.f, 0.f, 60.f));
			}
		}
		Sample.LastSeen = Elapsed;
		Sample.MaxParticles = FMath::Max(Sample.MaxParticles, Particles);
	}
}

FVector2D ANUVFXViewerDirector::ProjectToView(const FVector& WorldLocation) const
{
	UWorld* World = GetWorld();
	APlayerController* Controller = IsValid(World) ? World->GetFirstPlayerController() : nullptr;
	FVector2D Screen;
	if (!IsValid(Controller) || ViewAreaPixels.IsEmpty()
		|| !UGameplayStatics::ProjectWorldToScreen(Controller, WorldLocation, Screen, true))
	{
		return FVector2D(-1.0, -1.0);
	}

	return FVector2D(Screen.X / ViewAreaPixels.Width(), Screen.Y / ViewAreaPixels.Height());
}

TArray<FString> ANUVFXViewerDirector::DescribeLingeringEffects() const
{
	TArray<FString> Lines;
	UWorld* World = GetWorld();
	for (TObjectIterator<UNiagaraComponent> It; It; ++It)
	{
		UNiagaraComponent* Component = *It;
		if (Component->GetWorld() != World || !Component->IsActive())
		{
			continue;
		}

		FNiagaraSystemInstanceControllerPtr Controller = Component->GetSystemInstanceController();
		FNiagaraSystemInstance* Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
		TArray<FString> Emitters;
		if (Instance)
		{
			for (const FNiagaraEmitterInstanceRef& Emitter : Instance->GetEmitters())
			{
				if (Emitter->IsComplete())
				{
					continue;
				}

				// 남은 입자의 수명 (끝나지 않는 이미터는 대개 이 값이 비정상으로 김)
				const int32 Particles = Emitter->GetNumParticles();
				const float Lifetime = Particles > 0
					? FNiagaraDataSetAccessor<float>::CreateReader(Emitter->GetParticleData(), FName(TEXT("Lifetime"))).GetSafe(0, -1.f)
					: -1.f;
				Emitters.Add(FString::Printf(TEXT("%s(입자 %d, 수명 %.1f)"),
					*Emitter->GetEmitterHandle().GetName().ToString(), Particles, Lifetime));
			}
		}

		Lines.Add(FString::Printf(TEXT("%s: %s"), *GetNameSafe(Component->GetAsset()),
			Emitters.IsEmpty() ? TEXT("-") : *FString::Join(Emitters, TEXT(", "))));
	}

	// 투사체가 남아 있으면 그것도 (임팩트를 아직 안 냄)
	for (TActorIterator<ANUBattleProjectile> It(World); It; ++It)
	{
		Lines.Add(FString::Printf(TEXT("투사체 %s"), *It->GetName()));
	}

	return Lines;
}

/////////////////////////////////////////////////////////////////
// 표시
/////////////////////////////////////////////////////////////////

FString ANUVFXViewerDirector::GetChoiceLabel(const TArray<FNUVFXViewerChoice>& Choices, int32 Id) const
{
	const FNUVFXViewerChoice* Choice = FindChoice(Choices, Id);
	return Choice ? Choice->Label : FString::Printf(TEXT("%d (없음)"), Id);
}

FString ANUVFXViewerDirector::DescribeAsset(const TSoftObjectPtr<UNiagaraSystem>& Asset)
{
	return Asset.IsNull() ? FString(TEXT("-")) : Asset.GetAssetName();
}

FText ANUVFXViewerDirector::GetStatusText() const
{
	FString Status;

	switch (State)
	{
	case EState::LoadingStage:
		Status = TEXT("무대 불러오는 중...");
		break;

	case EState::Reloading:
		Status = TEXT("새로고침 중...");
		break;

	case EState::Ready:
		if (IsSeeking())
		{
			Status = FString::Printf(TEXT("%.2f초로 감는 중"), SeekTarget);
		}
		else if (bPlaying)
		{
			Status = IsPaused() ? TEXT("일시정지") : TEXT("재생 중");
		}
		else
		{
			Status = TEXT("끝 (Space · R 처음부터)");
		}
		break;
	}

	Status += FString::Printf(TEXT("  x%.2f"), Setup.Speed);

	if (!ReloadMessage.IsEmpty())
	{
		Status += TEXT("\n") + ReloadMessage;
	}

	return FText::FromString(Status);
}

FText ANUVFXViewerDirector::GetReportText() const
{
	if (!Report.bValid)
	{
		return FText::FromString(Report.Warnings.IsEmpty() ? FString(TEXT("아직 재생 안 함")) : FString::Join(Report.Warnings, TEXT("\n")));
	}

	const FNUBattleActionMotionSpec& Spec = Report.Spec;
	TArray<FString> Lines;
	const int32 CandidateCount = Setup.Mode == ENUVFXViewerMode::Skill ? GetCandidateCount(Setup.SkillId) : 0;
	if (CandidateCount > 0)
	{
		Lines.Add(FString::Printf(TEXT("후보 %d/%d %s"), CandidateIndex + 1, CandidateCount,
			*GetCandidateLabel(Setup.SkillId, CandidateIndex)));
	}

	Lines.Add(FString::Printf(TEXT("Cast    %s"), *DescribeAsset(Spec.CastVFX)));
	Lines.Add(FString::Printf(TEXT("Proj    %s"), *DescribeAsset(Spec.ProjectileVFX)));
	Lines.Add(FString::Printf(TEXT("Impact  %s"), *DescribeAsset(Spec.ImpactVFX)));
	Lines.Add(FString::Printf(TEXT("색 %s · 반전 %s · %s"),
		*FormatColor(Spec.EffectColor),
		Report.bCasterIsAlly ? TEXT("-1 (아군)") : TEXT("+1 (적)"),
		*MotionLabel(Spec.Motion)));
	Lines.Add(FString::Printf(TEXT("예고 %.2f · 접근 %.2f · 행동 %.2f · 복귀 %.2f"),
		Spec.TelegraphTime, Spec.ApproachTime, Spec.ActionTime, Spec.ReturnTime));
	Lines.Add(FString::Printf(TEXT("행동 안 시전 @%.2f · 타격 @%.2f · 타격 %d회 간격 %.2f"),
		Report.CastTime, Report.HitTime, Report.HitCount, Spec.HitInterval));
	Lines.Add(FString::Printf(TEXT("첫 타격 %.2f초 · 전체 %.2f초"), Report.FirstHitDelay, Report.TotalDuration));

	for (const FString& Warning : Report.Warnings)
	{
		Lines.Add(TEXT("! ") + Warning);
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

/////////////////////////////////////////////////////////////////
// 콘솔 명령 (PIE · -ExecCmds · 콘솔 창)
/////////////////////////////////////////////////////////////////

namespace
{
	ANUVFXViewerDirector* FindViewerDirector(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}

		TActorIterator<ANUVFXViewerDirector> It(World);
		if (!It)
		{
			UE_LOG(LogNUVFXViewer, Warning, TEXT("VFX 뷰어가 아님 (뷰어 맵 · NUVFXViewerGameMode 에서만 동작)"));
			return nullptr;
		}

		return *It;
	}

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerPlayCommand(
		TEXT("nu.VFX.Play"),
		TEXT("VFX 뷰어: 재생. 인자로 SkillId 를 주면 그 스킬 (0 = 기본 공격)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				if (Args.IsEmpty())
				{
					Viewer->Play();
				}
				else
				{
					Viewer->SelectSkill(FCString::Atoi(*Args[0]));
				}
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerSpeedCommand(
		TEXT("nu.VFX.Speed"),
		TEXT("VFX 뷰어: 재생 속도 (0.05 ~ 2)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->SetSpeed(Args.IsEmpty() ? 1.f : FCString::Atof(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerReloadCommand(
		TEXT("nu.VFX.Reload"),
		TEXT("VFX 뷰어: 바뀐 에셋 다시 읽기 (F5 와 같음)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->RequestReload();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerSeekCommand(
		TEXT("nu.VFX.Seek"),
		TEXT("VFX 뷰어: 재생 시작 기준 그 초로 감음 (진행바와 같음)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->SeekTo(Args.IsEmpty() ? 0.f : FCString::Atof(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerStepCommand(
		TEXT("nu.VFX.Step"),
		TEXT("VFX 뷰어: 그 수만큼 프레임을 넘김 (음수면 뒤로, 기본 1)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->StepFrames(Args.IsEmpty() ? 1 : FCString::Atoi(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerGenderCommand(
		TEXT("nu.VFX.Gender"),
		TEXT("VFX 뷰어: 캐릭터 성별 m · f (패널의 성별 버튼과 같음)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				const bool bFemale = !Args.IsEmpty() && (Args[0].Equals(TEXT("f"), ESearchCase::IgnoreCase) || Args[0] == TEXT("1"));
				Viewer->SetGender(bFemale ? 1 : 0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerHairCommand(
		TEXT("nu.VFX.Hair"),
		TEXT("VFX 뷰어: 캐릭터 머리 번호 (서버 hair_id 와 같음, 0 = 그 몸의 첫 머리)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->SetHair(Args.IsEmpty() ? 0 : FCString::Atoi(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerCompareCommand(
		TEXT("nu.VFX.Compare"),
		TEXT("VFX 뷰어: 후보 파일을 읽고 후보가 있는 첫 스킬을 틂 (-VFXCompare 와 같음)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ANUVFXViewerDirector* Viewer = FindViewerDirector(World);
			if (Viewer && !Args.IsEmpty() && Viewer->LoadCompareFile(FString::Join(Args, TEXT(" "))))
			{
				Viewer->StepCandidateSkill(0);
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerVariantCommand(
		TEXT("nu.VFX.Variant"),
		TEXT("VFX 뷰어: 지금 스킬의 그 번호 후보를 틂 (1 = 표 그대로), 스킬도 주려면 '스킬번호 후보'"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ANUVFXViewerDirector* Viewer = FindViewerDirector(World);
			if (!Viewer || Args.IsEmpty())
			{
				return;
			}

			if (Args.Num() >= 2)
			{
				Viewer->SelectSkill(FCString::Atoi(*Args[0]));
			}

			Viewer->SelectCandidate(FCString::Atoi(*Args.Last()) - 1);
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerChooseCommand(
		TEXT("nu.VFX.Choose"),
		TEXT("VFX 뷰어: 지금 틀고 있는 후보를 적용 대기에 담음 (Saved/VFXViewer/Pending.json), 지금(표 그대로)이면 대기에서 뺌"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->ChooseCandidate();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerApplyCommand(
		TEXT("nu.VFX.Apply"),
		TEXT("VFX 뷰어: 적용 대기를 원본 CSV 에 쓰고 DT 를 다시 채움 ([적용] 과 같음), 'yes' 를 주면 확인 창 없이 (내용은 로그)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->ApplyPendingChanges(!(Args.Num() > 0 && Args[0].Equals(TEXT("yes"), ESearchCase::IgnoreCase)));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerRevertCommand(
		TEXT("nu.VFX.Revert"),
		TEXT("VFX 뷰어: 마지막 적용을 되돌림 ([마지막 적용 되돌리기] 와 같음), 'yes' 를 주면 확인 창 없이"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->RevertLastApply(!(Args.Num() > 0 && Args[0].Equals(TEXT("yes"), ESearchCase::IgnoreCase)));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerReimportCommand(
		TEXT("nu.VFX.Reimport"),
		TEXT("VFX 뷰어: 원본 CSV 로 DT_SkillVFX 를 다시 채움 ([DT 다시 채우기] 와 같음)"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				Viewer->ReimportSkillVFXTable();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs GVFXViewerShotCommand(
		TEXT("nu.VFX.Shot"),
		TEXT("VFX 뷰어: 지금 화면을 Saved/VFXViewer/Manual 에 찍음"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (ANUVFXViewerDirector* Viewer = FindViewerDirector(World))
			{
				UE_LOG(LogNUVFXViewer, Log, TEXT("스크린샷: %s"), *Viewer->TakeShotNow());
			}
		}));
}
