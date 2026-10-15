# 언리얼 클라이언트 proto 빌드 가이드

## 처음 받은 PC

ProjectM 루트에서 한 번 실행합니다. TurboLink 라이브러리 설치와 proto 코드 생성을 같이 처리합니다.

```powershell
.\Scripts\SetupTurboLink.ps1 -Platforms Win64
```

다른 PC에서도 `Source/ThirdParty`를 직접 복사하지 말고 이 명령을 사용합니다. 고정된 공식 사전 빌드 라이브러리를 내려받아 SHA-256까지 검증합니다.

Android 라이브러리도 필요하면 다음처럼 실행합니다.

```powershell
.\Scripts\SetupTurboLink.ps1 -Platforms Win64,Android
```

## proto가 변경됐을 때

proto 원본은 서버의 `RevMMOServer/Protos/game.proto`입니다. 서버 빌드 후 클라이언트에서 생성 스크립트를 실행합니다.

```powershell
# C:\WOOKPROJ\RevMMOServer
dotnet build .\RevMMOServer.sln

# C:\WOOKPROJ\RevStudio\ProjectM
.\Scripts\GenerateProto.ps1
```

## 언리얼 클라이언트 빌드

언리얼 에디터를 완전히 닫고 실행합니다.

```powershell
& "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" ProjectMEditor Win64 Development "-Project=C:\WOOKPROJ\RevStudio\ProjectM\ProjectM.uproject" -WaitMutex -NoHotReloadFromIDE
```

`Result: Succeeded`가 나오면 완료입니다.

위 `Development` 빌드는 TurboLink의 `Release` 라이브러리를 링크합니다.

> 생성 파일을 직접 수정하지 마세요. `GenerateProto.ps1`이 UE 5.8 및 proto3 `optional` 보정을 자동 적용합니다.
