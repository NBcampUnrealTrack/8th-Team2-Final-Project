# 기획 상점 시트(구글 스프레드시트 2장) -> UE DataTable 임포트용 CSV 변환
# 사용법:  .\Scripts\ConvertShopTable.ps1                 (시트에서 다운로드 후 변환)
#          .\Scripts\ConvertShopTable.ps1 -Check          (변환만 하고 Scripts\Data의 기존 CSV와 칸 단위로 대조, 파일은 안 씀)
#          .\Scripts\ConvertShopTable.ps1 -ShopCsv a.csv -ShopItemCsv b.csv   (로컬에 받아 둔 시트 CSV 변환)
# 출력:    Scripts\Data\ShopTable.csv      (Row Type = NUShopRow     -> DT_Shop)
#          Scripts\Data\ShopItemTable.csv  (Row Type = NUShopItemRow -> DT_ShopItem)
#
# 왜 시트에서 직접 굽나: 서버가 읽는 정본이 시트다. 9/6에 저장소 CSV가 시트보다 낡아(BuybackRate 0 vs 0.25) 갈린 적이 있다.
# 출력 양식은 민성 님이 9/4에 커밋한 CSV와 컬럼 이름·순서·값 표기까지 같다 --- 소비처(DT 임포트, 혹시 있을 서버 읽기)는 바뀌지 않는다.
#
# 시트 형식 전제 (9/6 기준): 맨 앞 빈 열, 묶음 제목 줄, 컬럼명 줄, 타입 줄, 그 뒤 데이터. 컬럼명 줄을 이름으로 찾으므로 열 순서가 바뀌어도 된다.
#   상점:      Shop_ID, Shop_Key, Name_KR, Area(Libregrove/Cindehearth), ShopType, NPC_ID, Location_ID, Location_KR, PriceMultiplier, BuybackRate(시트 오타 ButbackRate 허용), UnlockQuest, Description
#   상점 품목: ShopItem_ID, Shop_ID, SlotIndex, Ref_ID, RefType(Item/Equip), Name_KR, Price, Stock(-1 무한), RequiredLevel, UnlockQuest_ID, Description
param(
    [string]$ShopSheetUrl     = "https://docs.google.com/spreadsheets/d/17Au66AqatKu-tgx3_2LOykIQVqDRDcadUmydSHL_8WA/export?format=csv",
    [string]$ShopItemSheetUrl = "https://docs.google.com/spreadsheets/d/1wWNZQbc5Xxb3wv2bG7SZr9USQ76emghi2ejmZfWFA_0/export?format=csv",
    [string]$ShopCsv = "",
    [string]$ShopItemCsv = "",
    [string]$OutShopCsv = "$PSScriptRoot\Data\ShopTable.csv",
    [string]$OutShopItemCsv = "$PSScriptRoot\Data\ShopItemTable.csv",
    [string]$ItemCsv = "$PSScriptRoot\Data\ItemTable.csv",    # Ref_ID 검증용 (아이템 NumericId)
    [string]$EquipCsv = "$PSScriptRoot\Data\EquipTable.csv",  # Ref_ID 검증용 (장비 NumericId)
    [switch]$Check
)

$ErrorActionPreference = "Stop"

# 간이 CSV 파서 (따옴표 안 콤마·따옴표 이스케이프 처리) --- ConvertItemTable.ps1과 같은 방식
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

# 민성 님 CSV와 같은 표기: 콤마·따옴표·줄바꿈이 있는 값만 따옴표로 감싼다 (Export-Csv는 전부 감싸서 안 쓴다)
function Format-CsvField([string]$v) {
    if ($v -match '[,"\r\n]') { return '"' + ($v -replace '"', '""') + '"' }
    return $v
}

function Write-CsvFile([string]$path, [string[]]$header, [object[]]$rows) {
    $lines = @(($header -join ','))
    foreach ($r in $rows) { $lines += (($header | ForEach-Object { Format-CsvField ([string]$r[$_]) }) -join ',') }
    # UTF-8 BOM + CRLF --- 기존 파일과 동일
    [System.IO.File]::WriteAllText($path, (($lines -join "`r`n") + "`r`n"), (New-Object System.Text.UTF8Encoding $true))
}

# 시트 CSV를 (컬럼명 -> 값) 해시 배열로. 컬럼명 줄은 $keyColumn이 들어 있는 줄, 데이터 줄은 그 컬럼이 숫자인 줄
function Read-SheetTable([string]$path, [string]$keyColumn) {
    $lines = Get-Content $path -Encoding UTF8
    $header = $null; $rows = @()
    foreach ($line in $lines) {
        $cols = Split-CsvLine $line | ForEach-Object { $_.Trim() }
        if ($null -eq $header) {
            if ($cols -contains $keyColumn) { $header = $cols }
            continue
        }
        $keyIdx = [array]::IndexOf($header, $keyColumn)
        if ($keyIdx -lt 0 -or $cols.Count -le $keyIdx -or $cols[$keyIdx] -notmatch '^\d+$') { continue }
        $row = @{}
        for ($i = 0; $i -lt $header.Count; $i++) { if ($header[$i]) { $row[$header[$i]] = $(if ($i -lt $cols.Count) { $cols[$i] } else { "" }) } }
        $rows += ,$row
    }
    if ($null -eq $header) { throw "'$keyColumn' 컬럼명 줄을 못 찾음 --- 시트 형식이 바뀌었는지 확인: $path" }
    return ,$rows
}

function Get-Col($row, [string[]]$names) {
    foreach ($n in $names) { if ($row.ContainsKey($n)) { return $row[$n] } }
    return ""
}

function Download-Sheet([string]$url, [string]$name) {
    $tmp = Join-Path $env:TEMP "nu_${name}_sheet.csv"
    Write-Host "시트 다운로드 중: $name"
    Invoke-WebRequest -Uri $url -OutFile $tmp -UseBasicParsing
    return $tmp
}

if ([string]::IsNullOrEmpty($ShopCsv))     { $ShopCsv     = Download-Sheet $ShopSheetUrl "shop" }
if ([string]::IsNullOrEmpty($ShopItemCsv)) { $ShopItemCsv = Download-Sheet $ShopItemSheetUrl "shopitem" }

$warnings = 0
function Warn([string]$msg) { Write-Warning $msg; $script:warnings++ }

# ---------------------------------------------------------------- 상점
# 지역 이름 -> AreaId (FNUShopRow.AreaId: 0 리베르그로브, 1 신데하스). 모르는 이름은 0으로 두고 경고
$areaMap = @{ "Libregrove" = 0; "Cindehearth" = 1 }
$shopHeader = @("Name","ShopId","ShopKey","DisplayName","AreaId","ShopType","NpcId","LocationId","LocationName","PriceMultiplier","BuybackRate","UnlockQuestId","Description")
$shopRows = @(); $shopIds = @{}
foreach ($r in (Read-SheetTable $ShopCsv "Shop_ID")) {
    $id = [int]$r["Shop_ID"]
    if ($shopIds.ContainsKey($id)) { Warn "상점 Shop_ID 중복: $id" } ; $shopIds[$id] = $true
    $area = Get-Col $r @("Area")
    if ($area -and -not $areaMap.ContainsKey($area)) { Warn "상점 $id 알 수 없는 Area '$area' --- AreaId 0으로 둔다 (areaMap과 클라 AreaId 규약을 같이 수정)" }
    $buyback = Get-Col $r @("BuybackRate","ButbackRate")   # 시트 헤더 오타 흡수
    if (-not $buyback) { Warn "상점 $id BuybackRate 빈칸 --- 0" ; $buyback = "0" }
    if (-not (Get-Col $r @("PriceMultiplier"))) { Warn "상점 $id PriceMultiplier 빈칸" }
    $shopRows += @{
        Name = $id; ShopId = $id
        ShopKey = $r["Shop_Key"]; DisplayName = $r["Name_KR"]
        AreaId = $(if ($areaMap.ContainsKey($area)) { $areaMap[$area] } else { 0 })
        ShopType = (Get-Col $r @("ShopType"))
        NpcId = (Get-Col $r @("NPC_ID")); LocationId = (Get-Col $r @("Location_ID")); LocationName = (Get-Col $r @("Location_KR"))
        PriceMultiplier = (Get-Col $r @("PriceMultiplier")); BuybackRate = $buyback
        UnlockQuestId = $(if (Get-Col $r @("UnlockQuest","UnlockQuest_ID")) { Get-Col $r @("UnlockQuest","UnlockQuest_ID") } else { "0" })
        Description = (Get-Col $r @("Description"))
    }
}
if ($shopRows.Count -eq 0) { throw "상점 데이터 행을 못 찾음" }

# ---------------------------------------------------------------- 상점 품목
# Ref_ID 검증용: 아이템·장비 CSV의 NumericId (없으면 검증만 건너뛴다)
$knownIds = @{}
foreach ($f in @($ItemCsv, $EquipCsv)) {
    if (Test-Path $f) {
        foreach ($line in (Get-Content $f -Encoding UTF8 | Select-Object -Skip 1)) {
            $c = Split-CsvLine $line
            if ($c.Count -gt 1 -and $c[1].Trim() -match '^\d+$') { $knownIds[[int]$c[1].Trim()] = $c[0].Trim() }
        }
    } else { Warn "Ref_ID 검증용 파일 없음: $f" }
}
$itemHeader = @("Name","ShopItemId","ShopId","SlotIndex","RefType","RefId","DisplayName","Price","Stock","RequiredLevel","UnlockQuestId","Description")
$itemRows = @(); $itemIds = @{}
foreach ($r in (Read-SheetTable $ShopItemCsv "ShopItem_ID")) {
    $id = [int]$r["ShopItem_ID"]; $shopId = [int]$r["Shop_ID"]; $refId = [int]$r["Ref_ID"]
    if ($itemIds.ContainsKey($id)) { Warn "품목 ShopItem_ID 중복: $id" } ; $itemIds[$id] = $true
    if (-not $shopIds.ContainsKey($shopId)) { Warn "품목 $id 의 Shop_ID $shopId 가 상점 표에 없음" }
    if ($knownIds.Count -gt 0 -and -not $knownIds.ContainsKey($refId)) { Warn "품목 $id ($($r['Name_KR'])) Ref_ID $refId 가 아이템·장비 CSV에 없음" }
    $refType = Get-Col $r @("RefType")
    if ($refType -notin @("Item","Equip")) { Warn "품목 $id RefType '$refType' (Item/Equip 아님)" }
    $price = Get-Col $r @("Price"); $stock = Get-Col $r @("Stock")
    if (-not $price -or [int]$price -lt 0) { Warn "품목 $id Price 이상: '$price'" }
    if (-not $stock -or [int]$stock -lt -1) { Warn "품목 $id Stock 이상: '$stock' (-1 무한 / N)" }
    $itemRows += @{
        Name = $id; ShopItemId = $id; ShopId = $shopId
        SlotIndex = (Get-Col $r @("SlotIndex")); RefType = $refType; RefId = $refId
        DisplayName = $r["Name_KR"]; Price = $price; Stock = $stock
        RequiredLevel = $(if (Get-Col $r @("RequiredLevel")) { Get-Col $r @("RequiredLevel") } else { "1" })
        UnlockQuestId = $(if (Get-Col $r @("UnlockQuest_ID","UnlockQuest")) { Get-Col $r @("UnlockQuest_ID","UnlockQuest") } else { "0" })
        Description = (Get-Col $r @("Description"))
    }
}
if ($itemRows.Count -eq 0) { throw "품목 데이터 행을 못 찾음" }

# ---------------------------------------------------------------- 대조 (-Check) 또는 쓰기
function Compare-WithExisting([string]$path, [string[]]$header, [object[]]$rows, [string]$label) {
    if (-not (Test-Path $path)) { Write-Host "$label : 기존 파일 없음 ($path)"; return }
    $existing = @{}
    $lines = Get-Content $path -Encoding UTF8
    $exHeader = Split-CsvLine ($lines[0] -replace '^﻿', '')
    foreach ($line in ($lines | Select-Object -Skip 1)) {
        $c = Split-CsvLine $line; if ($c.Count -lt 2) { continue }
        $h = @{}; for ($i = 0; $i -lt $exHeader.Count -and $i -lt $c.Count; $i++) { $h[$exHeader[$i].Trim()] = $c[$i] }
        $existing[$c[0].Trim()] = $h
    }
    $diff = 0
    if (($exHeader | ForEach-Object { $_.Trim() }) -join ',' -ne ($header -join ',')) { Write-Host "$label : 컬럼이 다름!  기존=$($exHeader -join ',')"; $diff++ }
    foreach ($r in $rows) {
        $key = [string]$r["Name"]
        if (-not $existing.ContainsKey($key)) { Write-Host "$label : 기존에 없는 행 $key"; $diff++; continue }
        foreach ($col in $header) {
            $new = [string]$r[$col]; $old = [string]$existing[$key][$col]
            if ($new -ne $old) { Write-Host ("{0} : {1} {2}: 시트={3} / 기존={4}" -f $label, $key, $col, $new, $old); $diff++ }
        }
    }
    foreach ($key in $existing.Keys) { if (-not ($rows | Where-Object { [string]$_["Name"] -eq $key })) { Write-Host "$label : 시트에 없는 기존 행 $key"; $diff++ } }
    Write-Host "$label : 차이 $diff 칸 (행 시트 $($rows.Count) / 기존 $($existing.Count))"
}

if ($Check) {
    Compare-WithExisting $OutShopCsv $shopHeader $shopRows "ShopTable"
    Compare-WithExisting $OutShopItemCsv $itemHeader $itemRows "ShopItemTable"
}
else {
    New-Item -ItemType Directory -Force (Split-Path $OutShopCsv) | Out-Null
    Write-CsvFile $OutShopCsv $shopHeader $shopRows
    Write-CsvFile $OutShopItemCsv $itemHeader $itemRows
    Write-Host "변환 완료: 상점 $($shopRows.Count)행 -> $OutShopCsv"
    Write-Host "변환 완료: 품목 $($itemRows.Count)행 -> $OutShopItemCsv"
}
if ($warnings -gt 0) { Write-Host "경고 $warnings 건 --- 위 내용을 민성 님과 확인" -ForegroundColor Yellow }
