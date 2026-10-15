@echo off
chcp 65001 > nul
setlocal
cd /d "%~dp0.."

echo ============================================================
echo  ProjectM 최초 세팅
echo ------------------------------------------------------------
echo  1) 서브모듈 받기 (TurboLink, Revvy)
echo  2) TurboLink SDK 설치 (약 6GB - 처음 한 번만, 오래 걸립니다)
echo  3) proto 코드 생성
echo  4) git 훅 설치 (pull 로 proto 가 바뀌면 생성 코드 자동 갱신)
echo ============================================================
echo.

echo [1/4] 서브모듈 받는 중...
git submodule update --init --recursive
if errorlevel 1 goto :failed
echo   완료
echo.

echo [2/4] TurboLink SDK 설치 중... (약 6GB, 네트워크 속도에 따라 오래 걸립니다)
powershell -ExecutionPolicy Bypass -File "%~dp0SetupTurboLink.ps1"
if errorlevel 1 goto :failed
echo   완료
echo.

echo [3/4] proto 코드 생성 중...
powershell -ExecutionPolicy Bypass -File "%~dp0GenerateProto.ps1"
if errorlevel 1 goto :failed
echo   완료
echo.

echo [4/4] git 훅 설치 중...
powershell -ExecutionPolicy Bypass -File "%~dp0InstallGitHooks.ps1"
if errorlevel 1 goto :failed
echo   완료
echo.

echo ============================================================
echo  세팅 완료! 이제 ProjectM.uproject 를 열면 됩니다.
echo ============================================================
pause
exit /b 0

:failed
echo.
echo ============================================================
echo  [실패] 위 오류 메시지를 확인하세요.
echo   - git 명령을 못 찾으면: Git for Windows 설치 필요
echo   - 서브모듈 접근 거부: RevStudio 저장소 권한 확인
echo ============================================================
pause
exit /b 1
