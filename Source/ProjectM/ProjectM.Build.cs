// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProjectM : ModuleRules
{
	public ProjectM(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Module-root-relative includes (e.g. "Core/NUTypes.h") used across subfolders
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Paper2D", "PaperZD",

			// GAS
			"GameplayAbilities", "GameplayTags", "GameplayTasks",

			// 인벤토리 FastArray 복제 (FFastArraySerializer)
			"NetCore",

			// 프로젝트 세팅
            "DeveloperSettings",

			// VFX
			"Niagara"
        });

		// Json/JsonUtilities: 서버 response_data(JSON) 파싱 — 캐릭터 목록 등
		// ImageCore: 스프라이트 소스 픽셀에서 프레임별 바운스를 재는 데 사용 (NUSpriteFrameOffset)
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore", "ApplicationCore", "LevelSequence", "MovieScene", "Json", "JsonUtilities", "ImageCore" });

		// AssetRegistry: 전투 VFX 뷰어가 -game 에서 이펙트 · 무대 목록을 직접 훑음 (Tools/VFXViewer)
		PrivateDependencyModuleNames.Add("AssetRegistry");

		// MovieSceneTracks: 프롤로그 큐가 시퀀스의 속성 트랙으로 자기 바인딩을 찾음 (NUStorySequenceCue)
		PrivateDependencyModuleNames.Add("MovieSceneTracks");

		// 서버 연결: TurboLink(gRPC) + OpenSSL(세션 토큰 AES 파생)
		PrivateDependencyModuleNames.Add("TurboLinkGrpc");

		// 구글 로그인 (Auth/NUGoogleAuthSubsystem): 토큰 교환 HTTPS + 브라우저가 돌아올 127.0.0.1 콜백 수신
		PrivateDependencyModuleNames.AddRange(new string[] { "HTTP", "HTTPServer" });
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
