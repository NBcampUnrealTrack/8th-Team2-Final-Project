@echo off
chcp 65001 > nul
setlocal
cd /d "%~dp0.."

echo ============================================================
echo  proto 코드 생성
echo ------------------------------------------------------------
echo  서버에서 Protos\game.proto / Protos\chat.proto 가 갱신된 뒤 실행하세요.
echo  (둘 다 한 번에 생성됩니다. 하나만 따로 돌리면 다른 쪽 산출물이 지워집니다)
echo  (이걸 빠뜨리면 proto 와 생성 코드가 어긋난 채 빌드됩니다)
echo ============================================================
echo.

powershell -ExecutionPolicy Bypass -File "%~dp0GenerateProto.ps1"
if errorlevel 1 goto :failed

echo.
echo  완료. 언리얼 에디터를 닫은 상태에서 다시 빌드하세요.
pause
exit /b 0

:failed
echo.
echo ============================================================
echo  [실패] SDK 가 없다는 오류면 Scripts\Setup.bat 을 먼저 실행하세요.
echo ============================================================
pause
exit /b 1
