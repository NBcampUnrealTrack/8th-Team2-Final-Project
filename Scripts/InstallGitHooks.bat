@echo off
chcp 65001 > nul
setlocal
cd /d "%~dp0.."

echo ============================================================
echo  git 훅 설치
echo ------------------------------------------------------------
echo  pull 이나 브랜치 전환으로 Protos\*.proto 가 바뀌면
echo  GenerateProto 를 자동으로 실행해 생성 코드를 맞춥니다.
echo  클론마다 한 번만 실행하면 됩니다. 다시 실행해도 안전합니다.
echo ============================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0InstallGitHooks.ps1"
if errorlevel 1 goto :failed

echo.
echo  완료.
pause
exit /b 0

:failed
echo.
echo ============================================================
echo  [실패] 위 오류 메시지를 확인하세요.
echo ============================================================
pause
exit /b 1
