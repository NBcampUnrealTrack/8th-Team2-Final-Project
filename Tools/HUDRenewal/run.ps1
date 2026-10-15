# Runs the in-game HUD renewal steps in order.  .\run.ps1 -Steps kit,fx,field,battle,apply
# kit = import the art kit (ArtSource/UI/HUD2026/kit, drawn by make_hud_kit.py), fx = time-driven UI materials,
# field / battle / result = the new widgets (built from nothing every time), apply = point the game at them (backs up first),
# revert = point the game back at the previous widgets.
# Builds and play captures reuse the creation pipeline: ..\CharacterCreateGlass\run.ps1 -Steps build / shot.
param([string[]]$Steps = @('kit', 'fx', 'field', 'battle'))
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$out = Join-Path $root 'Saved\HUDRenewal'
New-Item -ItemType Directory -Force -Path $out | Out-Null

function Headless([string]$Script) {
    $ue = 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    $log = Join-Path $out ($Script + '.log')
    $list = @(('"' + (Join-Path $root 'ProjectM.uproject') + '"'), '-run=pythonscript', ('-script="' + (Join-Path $PSScriptRoot ($Script + '.py')) + '"'), '-unattended', '-nop4', '-nosplash', '-stdout', '-FullStdOutLogOutput')
    $p = Start-Process -FilePath $ue -ArgumentList $list -RedirectStandardOutput $log -PassThru -NoNewWindow
    if (-not $p.WaitForExit(840000)) { Stop-Process -Id $p.Id -Force; "$Script TIMEOUT" }
    $hits = Select-String -Path $log -Pattern '[HUD]', 'LogPython: Error', 'Traceback', 'AssertionError', 'Error:' -SimpleMatch | Where-Object { $_.Line -notmatch 'LogStreaming|LogLinker|LogTexture' } | Select-Object -First 30
    foreach ($h in $hits) { $line = $h.Line; $i = $line.IndexOf('[HUD]'); if ($i -lt 0) { $i = [Math]::Max(0, $line.IndexOf('Log')) }; $line.Substring($i, [Math]::Min(300, $line.Length - $i)) }
    $ok = [bool](Select-String -Path $log -Pattern '[HUD] DONE' -SimpleMatch -Quiet)
    "$Script " + $(if ($ok) { 'OK' } else { 'FAILED' })
}

function Fresh([string]$Folder, [string[]]$Names) {
    # Widgets are rebuilt from nothing every time: rebuilding inside an existing asset leaves stale variable ids (engine ensure).
    foreach ($name in $Names) {
        $file = Join-Path (Join-Path $root $Folder) ($name + '.uasset')
        if (Test-Path $file) { Remove-Item $file -ErrorAction Stop }       # fails while an editor has it open
    }
}

foreach ($step in $Steps) {
    switch ($step) {
        'kit' { python (Join-Path $PSScriptRoot 'make_hud_kit.py'); Headless 'apply_kit' }
        'fx' { Fresh 'Content\Art\UI\HUD2026\Fx' @('M_HUD_Shine', 'M_HUD_Pulse', 'M_HUD_Parry', 'M_HUD_KeyRing', 'MI_HUD_KeyShine', 'MI_HUD_HeadRing', 'M_HUD_WorldMarker'); Headless 'apply_fx' }
        'field' {
            Fresh 'Content\Art\UI\HUD2026\Field' @('WBP_HUD_DockButton', 'WBP_HUD_PlayerStatus', 'WBP_HUD_MapMarker', 'WBP_HUD_MapView', 'WBP_HUD_Minimap',
                'WBP_HUD_Field', 'WBP_HUD_Location', 'WBP_HUD_Notice', 'WBP_HUD_Interact', 'WBP_HUD_HeadMarker', 'WBP_HUD_Nameplate', 'WBP_HUD_QuestEntry', 'WBP_HUD_QuestObjRow',
                'WBP_HUD_QuestRewardRow', 'WBP_HUD_RewardTip', 'WBP_HUD_Quest', 'WBP_HUD_PartyRow', 'WBP_HUD_PartyList', 'WBP_HUD_PartyPrompt', 'WBP_HUD_ChatLine', 'WBP_HUD_ChatTab',
                'WBP_HUD_ChatChannel', 'WBP_HUD_Chat')
            Headless 'apply_field'
        }
        'battle' {
            Fresh 'Content\Art\UI\HUD2026\Battle' @('WBP_HUD_StatusIcon', 'WBP_HUD_JobGauge', 'WBP_HUD_JobIcon', 'WBP_HUD_AllyTag', 'WBP_HUD_FoeTag',
                'WBP_HUD_TurnChip', 'WBP_HUD_Battle', 'WBP_HUD_ActionBanner', 'WBP_HUD_ParryCue', 'WBP_HUD_CommandButton', 'WBP_HUD_BattleCommand')
            Headless 'apply_battle'
        }
        'dialogue' {
            python (Join-Path $PSScriptRoot 'make_hud_kit.py')
            Fresh 'Content\Art\UI\HUD2026\Field' @('WBP_HUD_DialogueOption', 'WBP_HUD_Dialogue')
            Headless 'apply_dialogue'
        }
        'shop' {
            python (Join-Path $PSScriptRoot 'make_hud_kit.py')
            Fresh 'Content\Art\UI\HUD2026\Field' @('WBP_HUD_Shop', 'WBP_HUD_ShopEntry', 'WBP_HUD_ShopRowButton')
            Headless 'apply_shop'
        }
        'quest' {
            # the quest widget only (tracker, offer, reward slots, reward tooltip) --- the rest of the field HUD is left alone
            Fresh 'Content\Art\UI\HUD2026\Field' @('WBP_HUD_QuestEntry', 'WBP_HUD_QuestObjRow', 'WBP_HUD_QuestRewardRow', 'WBP_HUD_RewardTip', 'WBP_HUD_Quest')
            $env:HUD_ONLY = 'build_quest_entry,build_quest_rows,build_quest'; Headless 'apply_field'; $env:HUD_ONLY = ''
        }
        'select' {
            python (Join-Path $PSScriptRoot 'make_select_art.py')
            Fresh 'Content\Art\UI\HUD2026\Select' @('WBP_HUD_JobSelect', 'WBP_HUD_JobCard', 'WBP_HUD_JobSkillRow', 'WBP_HUD_SelChip', 'WBP_HUD_NaiaSelect', 'WBP_HUD_NaiaCard')
            Headless 'apply_select'
        }
        'result' {
            # battle result window + chat speech bubbles (Docs/HUD_ResultBubble_Proposal_20261015, option A). Needs the kit (bubble tail)
            Fresh 'Content\Art\UI\HUD2026\Battle' @('WBP_HUD_ResultMember', 'WBP_HUD_ResultEntry', 'WBP_HUD_BattleResult')
            Fresh 'Content\Art\UI\HUD2026\Field' @('WBP_HUD_SpeechBubble', 'WBP_HUD_SpeechBubble_Party', 'WBP_HUD_SpeechBubble_World')
            Headless 'apply_result'
        }
        'apply' { Headless 'apply_classes' }
        'revert' { $env:HUD_REVERT = '1'; Headless 'apply_classes'; $env:HUD_REVERT = '' }
        default { if (Test-Path (Join-Path $PSScriptRoot ($step + '.py'))) { Headless $step } else { "unknown step $step" } }
    }
}
