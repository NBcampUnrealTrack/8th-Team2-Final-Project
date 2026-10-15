// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectMEditorTarget : TargetRules
{
	public ProjectMEditorTarget( TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;

		// Revvy는 파일마다 같은 이름의 익명 네임스페이스 헬퍼를 정의해서
		// 유니티 빌드로 합치면 중복 정의로 깨진다
		DisableUnityBuildForModules = new string[] { "Revvy" };

		ExtraModuleNames.AddRange(new string[] { "ProjectM", "ProjectMEditor" });
	}
}
