# 전투 VFX 뷰어 실행 — 에디터 없이 뷰어 맵만 게임 모드(-game)로 띄운다 (Source/ProjectM/Tools/VFXViewer)
#
#   그냥 실행           : 창이 뜨고 왼쪽 패널에서 무대 · 전투원 · 스킬을 고른다 (마지막 선택 기억)
#   스킬 바로 보기       : .\run_vfx_viewer.ps1 -Skill 3002
#   세 칸 직접 지정      : .\run_vfx_viewer.ps1 -Cast /Game/VFX/.../NS_A -Proj ... -Impact ...
#   자동 캡처(창 안 봐도 됨): .\run_vfx_viewer.ps1 -Skill 3002 -ShotAt 0.3,0.6,1.2 -Quit -Wait
#   후보 비교            : .\run_vfx_viewer.ps1 -Compare <후보.json>  (찍기는 -ShotAtHit 를 같이, 보통은 Tools\VFXViewer\vfx_compare.py 로)
#   준비 뒤 콘솔 명령     : .\run_vfx_viewer.ps1 -Cmds 'nu.VFX.Variant 0 2;nu.VFX.Choose;nu.VFX.Apply yes' -Quit -Wait  (1초 간격, 적용 · 되돌리기 시험)
#                         -> Saved\VFXViewer\<시각>\shot_*.png + report.json (경로는 -Out 으로 바꿈)
#
# 엔진 경로는 -EnginePath > 레지스트리(HKLM EpicGames\Unreal Engine\<버전>) > 런처 설치 기록 순으로 찾는다.
# -game 은 에디터 DLL(ProjectMEditor Development)을 쓰므로 C++ 를 바꿨으면 먼저 빌드해야 한다.
param(
    [int]$Skill = -1,
    [ValidateSet('', 'player', 'naia', 'enemy')][string]$Caster = '',
    [int]$Job = -1,
    [ValidateSet('', 'm', 'f')][string]$Gender = '',
    [int]$Hair = -1,
    [int]$Naia = -1,
    [int]$Enemy = -1,
    [int]$Enemies = -1,
    [int]$Target = -1,
    [string]$Stage = '',
    [string]$Cast = '',
    [string]$Proj = '',
    [string]$Impact = '',
    [string]$Color = '',
    [ValidateSet('', 'inplace', 'center', 'target')][string]$Motion = '',
    [int]$Hits = -1,
    [switch]$NoHitCue,
    [double]$Speed = -1,
    [double[]]$ShotAt = @(),
    # 여러 스킬을 한 번에: '3206:1003','3102:1002' (스킬:직업, 1003f 처럼 f · m 으로 성별) + 첫 타격 기준 시각 -ShotAtHit -0.05,0,0.1
    [string[]]$Skills = @(),
    [double[]]$ShotAtHit = @(),
    # 후보 비교 파일 (Tools\VFXViewer\README.md "후보 비교"), -ShotAtHit 를 주면 후보마다 찍음
    [string]$Compare = '',
    # 뷰어가 준비된 뒤 1초 간격으로 돌릴 콘솔 명령, ; 로 나눔 (-Quit 이면 다 돌고 종료)
    [string]$Cmds = '',
    [string]$Out = '',
    [switch]$Quit,
    [switch]$Wait,
    [switch]$HidePanel,
    [switch]$ShotUI,
    [switch]$ShotReload,
    [switch]$ShowLog,
    [int]$ResX = 1920,
    [int]$ResY = 1080,
    [string]$EnginePath = '',
    # 그 밖의 -VFX* 스위치를 그대로 넘김 (예: -Extra '-VFXHitchLog','-VFXNoPrepare')
    [string[]]$Extra = @()
)
$ErrorActionPreference = 'Stop'

$ProjectDir = Split-Path -Parent $PSScriptRoot
$UProject = Join-Path $ProjectDir 'ProjectM.uproject'

function Resolve-EngineDir {
    if ($EnginePath) { return $EnginePath }
    if ($env:UE_ENGINE_DIR) { return $env:UE_ENGINE_DIR }

    $Association = (Get-Content $UProject -Raw | ConvertFrom-Json).EngineAssociation

    $Key = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association"
    $Registered = Get-ItemProperty -Path $Key -ErrorAction SilentlyContinue
    if ($Registered -and $Registered.InstalledDirectory) { return $Registered.InstalledDirectory }

    # 소스 빌드 엔진은 GUID 로 연결된다
    $Builds = Get-ItemProperty -Path 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
    if ($Builds -and $Builds.$Association) { return $Builds.$Association }

    $Launcher = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
    if (Test-Path $Launcher) {
        $Entry = (Get-Content $Launcher -Raw | ConvertFrom-Json).InstallationList |
            Where-Object { $_.AppName -eq "UE_$Association" } | Select-Object -First 1
        if ($Entry) { return $Entry.InstallLocation }
    }

    throw "엔진 $Association 위치를 못 찾음 -EnginePath 로 지정하세요"
}

$EngineDir = Resolve-EngineDir
$Exe = Join-Path $EngineDir 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path $Exe)) { throw "UnrealEditor.exe 없음: $Exe" }

# 뷰어 맵이 있으면 그 맵, 없으면 엔진 빈 맵 — 어느 쪽이든 GameMode 는 URL 로 지정
$ViewerMap = '/Game/Tools/VFXViewer/L_VFXViewer'
$MapFile = Join-Path $ProjectDir 'Content\Tools\VFXViewer\L_VFXViewer.umap'
$Map = if (Test-Path $MapFile) { $ViewerMap } else { '/Engine/Maps/Entry' }
$Url = "$Map" + '?game=/Script/ProjectM.NUVFXViewerGameMode'

$ArgList = @("`"$UProject`"", $Url, '-game', '-windowed', "-ResX=$ResX", "-ResY=$ResY", '-NoSound')
if ($ShowLog) { $ArgList += '-log' }
if ($Skill -ge 0) { $ArgList += "-VFXSkill=$Skill" }
if ($Caster) { $ArgList += "-VFXCaster=$Caster" }
if ($Job -ge 0) { $ArgList += "-VFXJob=$Job" }
if ($Gender) { $ArgList += "-VFXGender=$Gender" }
if ($Hair -ge 0) { $ArgList += "-VFXHair=$Hair" }
if ($Naia -ge 0) { $ArgList += "-VFXNaia=$Naia" }
if ($Enemy -ge 0) { $ArgList += "-VFXEnemy=$Enemy" }
if ($Enemies -ge 0) { $ArgList += "-VFXEnemies=$Enemies" }
if ($Target -ge 0) { $ArgList += "-VFXTarget=$Target" }
if ($Stage) { $ArgList += "-VFXStage=$Stage" }
if ($Cast) { $ArgList += "-VFXCast=$Cast" }
if ($Proj) { $ArgList += "-VFXProj=$Proj" }
if ($Impact) { $ArgList += "-VFXImpact=$Impact" }
if ($Color) { $ArgList += "`"-VFXColor=$Color`"" }
if ($Motion) { $ArgList += "-VFXMotion=$Motion" }
if ($Hits -ge 1) { $ArgList += "-VFXHits=$Hits" }
if ($NoHitCue) { $ArgList += '-VFXNoHitCue' }
if ($Speed -gt 0) { $ArgList += "-VFXSpeed=$Speed" }
if ($HidePanel) { $ArgList += '-VFXHidePanel' }
if ($Extra.Count -gt 0) { $ArgList += $Extra }
if ($Compare) { $ArgList += "`"-VFXCompare=$((Resolve-Path $Compare).Path)`"" }
if ($Cmds) {
    $ArgList += "`"-VFXCmds=$Cmds`"", '-Unattended'
    if ($Quit) { $ArgList += '-VFXQuit' }
}

if (($Skills.Count -gt 0 -or $Compare) -and $ShotAtHit.Count -gt 0) {
    $HitTimes = ($ShotAtHit | ForEach-Object { $_.ToString([Globalization.CultureInfo]::InvariantCulture) }) -join ','
    if ($Skills.Count -gt 0) { $ArgList += "-VFXSkills=$($Skills -join ',')" }
    $ArgList += "-VFXShotAtHit=$HitTimes", '-UseFixedTimeStep', '-FPS=60', '-Unattended'
    if ($Out) { $ArgList += "`"-VFXOut=$Out`"" }
    if ($Quit) { $ArgList += '-VFXQuit' }
    if ($ShotUI) { $ArgList += '-VFXShotUI' }
}
elseif ($ShotAt.Count -gt 0) {
    $Times = ($ShotAt | ForEach-Object { $_.ToString([Globalization.CultureInfo]::InvariantCulture) }) -join ','
    $ArgList += "-VFXShotAt=$Times"
    # 캡처 시각이 프레임 속도에 흔들리지 않게 고정 시간 간격
    $ArgList += '-UseFixedTimeStep', '-FPS=60', '-Unattended'
    if ($Out) { $ArgList += "`"-VFXOut=$Out`"" }
    if ($Quit) { $ArgList += '-VFXQuit' }
    if ($ShotUI) { $ArgList += '-VFXShotUI' }
    if ($ShotReload) { $ArgList += '-VFXShotReload' }
}

Write-Output "엔진: $EngineDir"
Write-Output "인자: $($ArgList -join ' ')"
$Process = Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru
Write-Output "PID: $($Process.Id)"

if ($Wait) {
    $Process.WaitForExit()
    Write-Output "종료 코드: $($Process.ExitCode)"
}
