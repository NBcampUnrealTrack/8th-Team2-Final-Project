# 기획 아이템·장비 시트(구글 스프레드시트) → UE DataTable 임포트용 CSV 변환
# 사용법:  .\Scripts\ConvertItemTable.ps1                 (장비만 --- 기본)
#          .\Scripts\ConvertItemTable.ps1 -Item           (아이템만 --- 시트를 받아 ItemTable.csv를 굽는다. 장비 CSV는 안 건드린다)
#          .\Scripts\ConvertItemTable.ps1 -Item -Check    (아이템 변환 결과를 기존 ItemTable.csv와 칸 단위로 대조만, 파일은 안 씀)
#          .\Scripts\ConvertItemTable.ps1 -Item -InputCsv 받은파일.csv   (로컬에 받아 둔 시트 CSV 변환)
# 출력:    Scripts\Data\ItemTable.csv   (아이템 --- 10/6부터 이 스크립트가 시트에서 굽는다. 손으로 고치면 다음 굽기에 덮이니 시트를 고칠 것)
#          Scripts\Data\EquipTable.csv  (장비 — 클라 전용. 기획 원본 EquipTable_2.csv + 개발용 더미)
# 에디터 임포트: 둘 다 Row Type = NUItemDefRow → DT_ItemTable / DT_EquipTable
param(
    [string]$InputCsv = "",
    [string]$SheetUrl = "https://docs.google.com/spreadsheets/d/1OazrDXhcECFDASBv4JmEyeG3vbmL3Xtp0Yi8SDZK_N8/export?format=csv",
    [string]$EquipCsv = "$PSScriptRoot\Data\EquipTable_2.csv",
    [string]$OutCsv = "$PSScriptRoot\Data\ItemTable.csv",
    [string]$OutEquipCsv = "$PSScriptRoot\Data\EquipTable.csv",
    [switch]$Item,  # 아이템 변환 (기본은 장비만)
    [switch]$Check  # -Item 과 같이: 기존 CSV와 대조만 하고 쓰지 않는다
)

$ErrorActionPreference = "Stop"
if ($Check -and -not $Item) { throw "-Check 는 -Item 과 같이 쓴다 (장비 변환에는 대조 모드가 없다)" }

# 간이 CSV 파서 (따옴표 안 콤마·따옴표 이스케이프 처리 — "1,000 회복" 대응)
function Split-CsvLine([string]$line) {
    $cols = @(); $cur = ""; $inQ = $false
    for ($i = 0; $i -lt $line.Length; $i++) {
        $ch = $line[$i]
        if ($ch -eq '"') {
            if ($inQ -and $i + 1 -lt $line.Length -and $line[$i + 1] -eq '"') { $cur += '"'; $i++ }
            else { $inQ = -not $inQ }
        }
        elseif ($ch -eq ',' -and -not $inQ) { $cols += $cur; $cur = "" }
        else { $cur += $ch }
    }
    $cols += $cur
    return ,$cols
}

# 등급 (Grade => 클라 enum ENUItemGrade 이름). 표시용이라 모르는 값은 None으로 두고 경고만 한다 (9/29). 아이템·장비 공용
$gradeMap = @{ "일반" = "Common"; "고급" = "Uncommon"; "희귀" = "Rare"; "영웅" = "Heroic"; "전설" = "Legendary" }

# ── 아이템 테이블 (-Item) ──
# 왜 시트에서 직접 굽나: 서버가 읽는 정본이 시트다. 손으로 옮긴 CSV는 시트에만 있는 칸(Grade)이 빠지고 값이 갈린다 (10/6).
# 시트 형식 전제 (10/6 기준): 맨 앞 빈 열, 제목 줄, 컬럼명 줄, 타입 줄, 그 뒤 데이터. 컬럼명 줄을 이름으로 찾으므로 열 순서가 바뀌어도 된다.
# 출력 양식은 민성 님이 9/20에 커밋한 CSV(컬럼 이름·순서·전부 따옴표·BOM·CRLF)에 Grade 한 칸을 끝에 붙인 것
if ($Item) {
    if ([string]::IsNullOrEmpty($InputCsv)) {
        $InputCsv = Join-Path $env:TEMP "nu_item_sheet.csv"
        Write-Host "시트 다운로드 중: $SheetUrl"
        Invoke-WebRequest -Uri $SheetUrl -OutFile $InputCsv -UseBasicParsing
    }

    $warnings = 0
    function Warn([string]$msg) { Write-Warning $msg; $script:warnings++ }

    # CSV 컬럼 = 시트 컬럼. CSV 컬럼 이름은 FNUItemDefRow 필드 이름이다. Grade는 enum 이름으로 바꿔 끝에 붙인다
    $columnMap = [ordered]@{
        Name = "Item_Key"; NumericId = "Item_ID"; DisplayName = "Item_KR"; Description = "Description"
        ItemType = "ItemType"; ItemTag = "ItemTag"; MaxStack = "MaxStack"; UseEffect = "UseEffect"
        Effect1_Type = "Effect1_Type"; Effect1_Value = "Effect1_Value"; Effect1_IsPercent = "Effect1_IsPercent"
        Effect2_Type = "Effect2_Type"; Effect2_Value = "Effect2_Value"; Effect2_IsPercent = "Effect2_IsPercent"
        bUsableInField = "bUsableInField"; bUsableInBattle = "bUsableInBattle"; BattleTargetPolicy = "BattleTargetPolicy"
    }
    $header = @($columnMap.Keys) + "Grade"
    $itemTypes = @("Consumable", "Equipment", "Quest", "Commune", "Material", "Blueprint", "KeyItem", "Keepsake")   # 클라 enum ENUItemType

    # 시트 수정 대기 --- 시트 값(Sheet)이 비었거나 틀려 클라 값(Value)을 대신 쓰는 칸. 시트가 Value와 같아지면 안내가 뜨니 그때 줄을 지운다.
    # 시트가 Sheet도 Value도 아닌 값으로 바뀌면 경고한다 --- 그때는 맞는 값을 확인하고 줄을 고치거나 지운다
    $pendingFixes = @(
        @{ Row = "C_VIT_05"; Col = "UseEffect";   Sheet = ""; Value = "/Script/ProjectM.NUGE_ItemHeal" },
        @{ Row = "C_SP_05";  Col = "UseEffect";   Sheet = ""; Value = "/Script/ProjectM.NUGE_ItemManaRestore" },
        @{ Row = "C_MIX_03"; Col = "Description"; Sheet = "단일 대상 HP 900 · SP 1400 회복"; Value = "단일 대상 HP 900 · SP 140 회복" }   # Effect2_Value는 140
    )

    $sheetHeader = $null; $idIdx = -1; $rows = @(); $seenIds = @{}; $seenKeys = @{}
    foreach ($line in (Get-Content $InputCsv -Encoding UTF8)) {
        $cols = @((Split-CsvLine $line) | ForEach-Object { $_.Trim() })
        if ($null -eq $sheetHeader) {
            if ($cols -contains "Item_ID") {
                $sheetHeader = $cols; $idIdx = [array]::IndexOf($cols, "Item_ID")
                $missing = @(@($columnMap.Values) + "Grade" | Where-Object { $sheetHeader -notcontains $_ })
                if ($missing.Count -gt 0) { throw "시트에 없는 컬럼: $($missing -join ', ') --- 시트 형식이 바뀌었는지 확인" }
            }
            continue
        }
        if ($cols.Count -le $idIdx -or $cols[$idIdx] -notmatch '^\d+$') { continue }   # 타입 줄·빈 줄
        if ($cols.Count -lt $sheetHeader.Count) { throw "Item_ID $($cols[$idIdx]) 줄의 칸 수가 모자람 --- 셀 안에 줄바꿈이 있는지 확인" }

        $sheet = @{}
        for ($i = 0; $i -lt $sheetHeader.Count; $i++) { if ($sheetHeader[$i]) { $sheet[$sheetHeader[$i]] = $cols[$i] } }
        $row = @{}
        foreach ($col in $columnMap.Keys) { $row[$col] = $sheet[$columnMap[$col]] }
        $key = $row["Name"]

        if (-not $key) { Warn "Item_ID $($row['NumericId']) 의 Item_Key 빈칸 --- 건너뜀"; continue }
        if ($seenIds.ContainsKey($row["NumericId"])) { Warn "Item_ID 중복: $($row['NumericId']) ($key)" } ; $seenIds[$row["NumericId"]] = $true
        if ($seenKeys.ContainsKey($key)) { Warn "Item_Key 중복: $key" } ; $seenKeys[$key] = $true

        # 클라 전용 값 --- 교감석(Effect1_Type 7 = Capture)은 ItemType Commune + 교감석 태그. 전투 코드가 ItemType == Commune 으로 교감석을 가린다.
        # 시트는 Consumable 그대로 둔다: 서버가 시트의 ItemType으로 가방 구간(소모품 칸)을 정한다
        if ($row["Effect1_Type"] -eq "7") { $row["ItemType"] = "Commune"; $row["ItemTag"] = "NU.Item.CommuneStone" }

        foreach ($fix in ($pendingFixes | Where-Object { $_.Row -eq $key })) {
            if ($row[$fix.Col] -ceq $fix.Value) { Write-Host "시트 수정 대기 $key $($fix.Col): 시트가 같은 값이 됨 --- 스크립트의 이 줄은 이제 지워도 된다" -ForegroundColor Cyan }
            elseif ($row[$fix.Col] -cne $fix.Sheet) { Warn "시트 수정 대기 $key $($fix.Col): 시트 값이 '$($row[$fix.Col])' 로 바뀜 --- 클라 값 '$($fix.Value)' 를 그대로 쓴다. 맞는 값을 확인하고 스크립트의 이 줄을 고치거나 지울 것" }
            $row[$fix.Col] = $fix.Value
        }

        if ($itemTypes -notcontains $row["ItemType"]) { Warn "$key 알 수 없는 ItemType '$($row['ItemType'])' (종류가 늘었으면 클라 enum ENUItemType을 같이 수정)" }
        $grade = $sheet["Grade"]
        if (-not $gradeMap.ContainsKey($grade)) { Warn "$key 알 수 없는 등급 '$grade' --- 등급 없음으로 둠 (등급이 늘었으면 gradeMap과 클라 enum을 같이 수정)" }
        $row["Grade"] = $(if ($gradeMap.ContainsKey($grade)) { $gradeMap[$grade] } else { "None" })
        $rows += ,$row
    }
    if ($null -eq $sheetHeader) { throw "'Item_ID' 컬럼명 줄을 못 찾음 --- 시트 형식이 바뀌었는지 확인: $InputCsv" }
    if ($rows.Count -eq 0) { throw "데이터 행을 못 찾음 --- 시트 형식이 바뀌었는지 확인: $InputCsv" }

    # 기존 CSV와 칸 단위 대조 --- 시트는 계속 바뀌므로 무엇이 달라지는지 쓰기 전에도 항상 보여 준다
    $lines = @(if (Test-Path $OutCsv) { Get-Content $OutCsv -Encoding UTF8 })
    if ($lines.Count -gt 0) {
        $oldHeader = @((Split-CsvLine $lines[0].TrimStart([char]0xFEFF)) | ForEach-Object { $_.Trim() })
        $existing = @{}
        foreach ($line in ($lines | Select-Object -Skip 1)) {
            $c = Split-CsvLine $line
            if ($c.Count -lt 2) { continue }
            $old = @{}; for ($i = 0; $i -lt $oldHeader.Count -and $i -lt $c.Count; $i++) { $old[$oldHeader[$i]] = $c[$i] }
            $existing[$c[0]] = $old
        }
        $diff = 0
        foreach ($col in ($header | Where-Object { $oldHeader -notcontains $_ })) { Write-Host "ItemTable : 새 컬럼 $col"; $diff++ }
        foreach ($col in ($oldHeader | Where-Object { $header -notcontains $_ })) { Write-Host "ItemTable : 없어지는 컬럼 $col"; $diff++ }
        foreach ($row in $rows) {
            $key = $row["Name"]
            if (-not $existing.ContainsKey($key)) { Write-Host "ItemTable : 새 행 $key ($($row['DisplayName']))"; $diff++; continue }
            foreach ($col in ($header | Where-Object { $oldHeader -contains $_ })) {
                if ($row[$col] -cne $existing[$key][$col]) { Write-Host ("ItemTable : {0} {1}: 시트={2} / 기존={3}" -f $key, $col, $row[$col], $existing[$key][$col]); $diff++ }
            }
        }
        foreach ($key in ($existing.Keys | Where-Object { -not $seenKeys.ContainsKey($_) })) { Write-Host "ItemTable : 시트에 없는 기존 행 $key"; $diff++ }
        Write-Host "ItemTable : 차이 $diff 건 (행 시트 $($rows.Count) / 기존 $($existing.Count))"
    }
    else { Write-Host "ItemTable : 기존 파일이 없거나 비어 있음 ($OutCsv)" }

    if (-not $Check) {
        # 전부 따옴표 + UTF-8 BOM + CRLF --- 기존 파일과 동일 (Export-Csv는 PowerShell 버전에 따라 BOM이 달라져 안 쓴다)
        $out = @(($header | ForEach-Object { '"' + $_ + '"' }) -join ',')
        foreach ($row in $rows) { $out += (($header | ForEach-Object { '"' + (([string]$row[$_]) -replace '"', '""') + '"' }) -join ',') }
        New-Item -ItemType Directory -Force (Split-Path $OutCsv) | Out-Null
        [System.IO.File]::WriteAllText($OutCsv, (($out -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding $true))
        Write-Host "변환 완료: 아이템 $($rows.Count)행 → $OutCsv"
    }
    if ($warnings -gt 0) { Write-Host "경고 $warnings 건 --- 위 내용을 민성 님과 확인" -ForegroundColor Yellow }
    return
}

# ── 장비 테이블 (클라 전용 EquipTable.csv — 서버 미소비) ──
# 정식 장비: 기획 원본(EquipTable_2.csv, 8/18 김민성)을 파싱한다.
#   컬럼 매핑: Equip_Key=>RowName / Equip_ID=>NumericId / Equip_KR=>DisplayName / Slot=>EquipType(부위 9종)
#   8/26 규격: 맨 앞 빈 열 + MATK_Bonus(ATK 다음) 추가, 5행이 타입 행
#   스탯은 원본 값 그대로 옮긴다 — ATK·HP는 비율(0.112=+11.2%), DEF는 절대값(혼합).
#   적용 방식(곱/합)이 기획에서 확정되면 GE(UNUGE_EquipmentStats) 쪽에서 해석한다 — 컨버터는 무변형.
#   보류 컬럼(클라 미사용): Craft*/Material/Price (RequiredLevel·JobRestrict·DamageReduce·HealBonus는 8/24 툴팁용, Grade는 9/29 등급 표시용으로 채택)
#   — DamageReduce·HealBonus는 스탯 어트리뷰트 신설(진영 님 GAS 협의) 후 매핑 추가
# 부위별 겹침 스킨 (개발용 Testa 세트). 1등급 정식 장비와 더미에 같은 파츠를 붙여
# 장착하면 캐릭터에 그려지게 한다. 등급별 파츠가 나오면 등급까지 보고 고르도록 확장한다
$partsFolder = "/Game/Developers/dbals/Equip/Parts2"
$skinBySlot = @{
    "Chest"  = "$partsFolder/SK_Testa_Armor.SK_Testa_Armor"
    "Legs"   = "$partsFolder/SK_Testa_Legs.SK_Testa_Legs"
    "Gloves" = "$partsFolder/SK_Testa_Gloves.SK_Testa_Gloves"
    "Boots"  = "$partsFolder/SK_Testa_Boots.SK_Testa_Boots"
}
# 팔보다 앞에 그릴 보조 스킨 (지금은 갑옷 어깨뿐)
$overlayBySlot = @{
    "Chest" = "$partsFolder/SK_Testa_Shoulder.SK_Testa_Shoulder"
}
# 겹침 스킨은 1등급(낡은~)에만 붙인다 --- 파츠 그림이 아직 한 세트뿐이다
function Get-EquipSkin([string]$Type, [string]$RowName, [bool]$IsOverlay) {
    if ($RowName -notmatch '_01$' -and $RowName -notmatch '^E_DEV_') { return "" }
    $table = if ($IsOverlay) { $overlayBySlot } else { $skinBySlot }
    if ($table.ContainsKey($Type)) { return $table[$Type] }
    return ""
}

# 직업 전용 표기 (JobRestrict => 한국어). All은 빈 값 = 툴팁에 줄이 안 생긴다
$jobMap = @{
    "All" = ""; "NaiaKnight" = "나이아 나이트"; "Tracker" = "트래커"
    "Elementalist" = "엘리멘탈리스트"; "Gambler" = "갬블러"; "Druid" = "드루이드"
}
# 직업 번호 (Scripts/Data/CharacterProgression/JobDefinition.csv의 JobId). 클라가 이 번호로 착용을 막고 툴팁을 붉게 한다 (9/14). All = 0
$jobIdMap = @{ "All" = 0; "NaiaKnight" = 1001; "Tracker" = 1002; "Elementalist" = 1003; "Gambler" = 1004; "Druid" = 1005 }

$slotMap = @{
    "Weapon" = "Weapon"; "Armor" = "Chest"; "Helmet" = "Head"; "Gloves" = "Gloves"
    "Boots" = "Boots"; "Earring" = "Earring"; "Ring1" = "Ring1"; "Ring2" = "Ring2"
    "Pants" = "Legs"   # 8/21 기획 추가 --- 클라 enum 이름은 Legs
}
$equipRows = @()
if (Test-Path $EquipCsv) {
    # 8/26 시트 규격 변경: 맨 앞에 빈 열이 붙고, ATK_Bonus 다음에 MATK_Bonus가 삽입됐다.
    # 데이터 행은 6행부터 (1 빈 / 2 제목 / 3 그룹 / 4 컬럼명 / 5 타입) --- Equip_ID가 숫자인 행만 골라 읽는다
    foreach ($line in (Get-Content $EquipCsv -Encoding UTF8)) {
        $cols = Split-CsvLine $line
        if ($cols.Count -lt 21 -or $cols[1] -notmatch '^\d+$') { continue }
        $slot = $cols[5].Trim()
        if (-not $slotMap.ContainsKey($slot)) {
            Write-Warning "알 수 없는 부위 '$slot' — $($cols[2]) 행 건너뜀 (부위가 바뀌었으면 slotMap과 클라 enum을 같이 수정)"
            continue
        }
        $stats = "(AttackPower={0},MagicAttackPower={1},Defense={2},Speed=0,CriticalRate=0,CriticalDamage=0,MaxHealth={3},MaxMana=0,DamageReduce={4},HealBonus={5})" -f $cols[9].Trim(), $cols[10].Trim(), $cols[12].Trim(), $cols[11].Trim(), $cols[13].Trim(), $cols[14].Trim()
        $job = $cols[7].Trim()
        if (-not $jobMap.ContainsKey($job)) { Write-Warning "알 수 없는 직업 '$job' --- $($cols[2]) 행은 원문 그대로 표기" }
        $grade = $cols[6].Trim()
        if (-not $gradeMap.ContainsKey($grade)) { Write-Warning "알 수 없는 등급 '$grade' --- $($cols[2]) 행은 등급 없음으로 둠 (등급이 늘었으면 gradeMap과 클라 enum을 같이 수정)" }
        $equipRows += [pscustomobject]@{
            Name = $cols[2].Trim(); NumericId = [int]$cols[1]; DisplayName = $cols[3].Trim(); Description = $cols[4].Trim()
            ItemType = "Equipment"; ItemTag = ""; MaxStack = 1
            UseEffect = ""; Effect1_Type = 0; Effect1_Value = 0; Effect1_IsPercent = "False"; Effect2_Type = 0; Effect2_Value = 0; Effect2_IsPercent = "False"   # 장비는 사용 효과 없음 --- 구조체 칸을 비우면 임포트 경고창이 뜬다
            bUsableInField = "False"; bUsableInBattle = "False"
            BattleTargetPolicy = "SingleAlly"; EquipType = $slotMap[$slot]; EquipStats = $stats
            EquipSkin = (Get-EquipSkin $slotMap[$slot] $cols[2].Trim() $false)
            EquipSkinOverlay = (Get-EquipSkin $slotMap[$slot] $cols[2].Trim() $true)
            RequiredLevel = [int]$cols[8]   # 서버가 이 값으로 장착을 거절한다(1308) --- 툴팁 표시용
            RequiredJobText = $(if ($jobMap.ContainsKey($job)) { $jobMap[$job] } else { $job })
            RequiredJobId = $(if ($jobIdMap.ContainsKey($job)) { $jobIdMap[$job] } else { 0 })
            Grade = $(if ($gradeMap.ContainsKey($grade)) { $gradeMap[$grade] } else { "None" })
        }
    }
    Write-Host "정식 장비 $($equipRows.Count)행 파싱: $EquipCsv"
}
else {
    Write-Warning "기획 장비 원본 없음($EquipCsv) — 개발용 더미만 출력"
}

# 개발용 더미 — 서버가 아직 9000번대를 지급하므로 정식(7000번대)과 병행 유지. 서버가 정식 번호로 갈아타면 제거.
# NumericId 9000번대 = 기획 Index와 충돌 방지. EquipStats는 8필드 전부 명시 (SetByCaller 8키 세팅 규칙과 일치)
function New-DevEquip([int]$Id, [string]$RowName, [string]$Name, [string]$Type, [string]$Stats) {
    [pscustomobject]@{
        Name = $RowName; NumericId = $Id; DisplayName = $Name; Description = "개발용 더미 장비"
        ItemType = "Equipment"; ItemTag = ""; MaxStack = 1
        UseEffect = ""; Effect1_Type = 0; Effect1_Value = 0; Effect1_IsPercent = "False"; Effect2_Type = 0; Effect2_Value = 0; Effect2_IsPercent = "False"
        bUsableInField = "False"; bUsableInBattle = "False"
        BattleTargetPolicy = "SingleAlly"; EquipType = $Type; EquipStats = $Stats
        EquipSkin = (Get-EquipSkin $Type $RowName $false)
        EquipSkinOverlay = (Get-EquipSkin $Type $RowName $true)
        RequiredLevel = 0   # 개발용 더미는 제한 없음
        RequiredJobText = ""
        RequiredJobId = 0
        Grade = "None"   # 개발용 더미는 등급 없음
    }
}
# 부위 9종 (8/21 하의 추가): Head/Chest/Legs/Gloves/Boots/Weapon/Ring1/Ring2/Earring — 방패는 무기 통합.
# 서버가 지급 중인 NumericId(9001·9003~9010)는 유지 — 이름/부위 표시는 클라 테이블 소유라 바꿔도 된다
$equipRows += @(
    (New-DevEquip 9001 "E_DEV_01" "낡은 검"      "Weapon"  "(AttackPower=10,MagicAttackPower=0,Defense=0,Speed=0,CriticalRate=0,CriticalDamage=0,MaxHealth=0,MaxMana=0)"),
    (New-DevEquip 9010 "E_DEV_02" "가죽 장갑"    "Gloves"  "(AttackPower=0,MagicAttackPower=0,Defense=2,Speed=1,CriticalRate=0,CriticalDamage=0,MaxHealth=20,MaxMana=0)"),
    (New-DevEquip 9003 "E_DEV_03" "녹슨 투구"    "Head"    "(AttackPower=0,MagicAttackPower=0,Defense=1,Speed=0,CriticalRate=0,CriticalDamage=0,MaxHealth=50,MaxMana=0)"),
    (New-DevEquip 9004 "E_DEV_04" "가죽 갑옷"    "Chest"   "(AttackPower=0,MagicAttackPower=0,Defense=3,Speed=0,CriticalRate=0,CriticalDamage=0,MaxHealth=30,MaxMana=0)"),
    (New-DevEquip 9005 "E_DEV_05" "구리 반지"    "Ring1"   "(AttackPower=0,MagicAttackPower=0,Defense=0,Speed=0,CriticalRate=2,CriticalDamage=0,MaxHealth=0,MaxMana=0)"),
    (New-DevEquip 9006 "E_DEV_06" "은 반지"      "Ring2"   "(AttackPower=0,MagicAttackPower=0,Defense=0,Speed=0,CriticalRate=0,CriticalDamage=10,MaxHealth=0,MaxMana=0)"),
    (New-DevEquip 9007 "E_DEV_07" "금 반지"      "Ring1"   "(AttackPower=0,MagicAttackPower=0,Defense=0,Speed=3,CriticalRate=0,CriticalDamage=0,MaxHealth=0,MaxMana=0)"),
    (New-DevEquip 9008 "E_DEV_08" "수정 귀걸이"  "Earring" "(AttackPower=0,MagicAttackPower=5,Defense=0,Speed=0,CriticalRate=0,CriticalDamage=0,MaxHealth=0,MaxMana=0)"),
    (New-DevEquip 9009 "E_DEV_09" "진주 귀걸이"  "Earring" "(AttackPower=0,MagicAttackPower=0,Defense=0,Speed=0,CriticalRate=0,CriticalDamage=0,MaxHealth=0,MaxMana=20)"),
    (New-DevEquip 9011 "E_DEV_10" "가죽 신발"    "Boots"   "(AttackPower=0,MagicAttackPower=0,Defense=2,Speed=2,CriticalRate=0,CriticalDamage=0,MaxHealth=0,MaxMana=0)"),
    (New-DevEquip 9012 "E_DEV_11" "가죽 하의"    "Legs"    "(AttackPower=0,MagicAttackPower=0,Defense=3,Speed=0,CriticalRate=0,CriticalDamage=0,MaxHealth=25,MaxMana=0)")
)

New-Item -ItemType Directory -Force (Split-Path $OutEquipCsv) | Out-Null
$equipRows | Export-Csv -Path $OutEquipCsv -NoTypeInformation -Encoding UTF8
Write-Host "변환 완료: 장비 $($equipRows.Count)행 (정식+더미) → $OutEquipCsv"
