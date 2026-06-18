<# 
stdatalog_to_wav_batch_tree_PINNED_v3_COMPLETE.ps1
--------------------------------------------------------------------------------
目标：始终使用“你指定的一份固定 JSON”（PIN），并行阶段不再 -cdm（避免并发写 catalog）。
本版把 **PIN 的完整路径** 提升为脚本开头的一个参数，方便你随时修改。
--------------------------------------------------------------------------------
#>

# ============== 只需改这里：PIN JSON 的完整路径 ===================
$PinnedJsonPath = 'D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk\FP_SNS_DATALOG2_Datalog2-7.json'   # ← 改成你要固定使用的 JSON 绝对路径
# ==================================================================

# ===================== 其他常用参数（按需改） =====================
# $Root      = 'E:\SPEECH_DATA\20251014-command\dat\1'    # 采集根目录（每个子目录一条采集）
# $Out       = 'E:\SPEECH_DATA\20251014-command\wav\1'    # ✅ 所有 WAV 按类目录输出到这里（不建子目录）
# $Root      = 'D:\20251014-command\dat\1'
# $Out       = 'D:\20251014-command\wav\1'
# $Root      = 'D:\20251029-command\dat\TVon'
# $Out       = 'D:\20251029-command\wav\TVon'
# $Root      = 'D:\speech_data\command-20251130\dat\20251130-lightoff'
# $Out       = 'D:\speech_data\command-20251130\wav\20251130-lightoff'
# $Root      = 'E:\SPEECH_DATA\TENG\command-20260226-ODR16000-2s\DZF-1\dat'  # 根目录：下面是 Fanoff/Fanon/... 每类一个文件夹
# $Out       = 'E:\SPEECH_DATA\TENG\command-20260226-ODR16000-2s\DZF-1\wav'  # 输出根目录：会生成 wav\Fanoff, wav\Fanon ...
# $Root      = 'E:\SPEECH_DATA\TENG\command-20260302-ODR16000-1600ms\dat'  # 根目录：下面是 Fanoff/Fanon/... 每类一个文件夹
# $Out       = 'E:\SPEECH_DATA\TENG\command-20260302-ODR16000-1600ms\wav'  # 输出根目录：会生成 wav\Fanoff, wav\Fanon ...

$Root      = 'E:\SPEECH_DATA\TENG\touming-dataset\command-20260419-ODR16000-2s-1\dat'  # 根目录：下面是 Fanoff/Fanon/... 每类一个文件夹
$Out       = 'E:\SPEECH_DATA\TENG\touming-dataset\command-20260419-ODR16000-2s-1\wav'  # 输出根目录：会生成 wav\Fanoff, wav\Fanon ...

$StageRoot = 'C:\_wav_stage'                             # Stage 缓存根（SSD 最佳）

$Py        = 'D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk\.venv_dlog\Scripts\python.exe'
$ToWav     = 'D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk\stdatalog_examples\cli_applications\stdatalog_to_wav.py'
$Sensor    = 'imp23absu_mic'                             # 或 'all'
$Workers   = 4                                           # 并行度（PS7）

# 你的“源 JSON”候选（首次运行若 PIN 不存在会从这里拷贝；含带空格容错 & 系统 Python 路径）
$JsonSources = @(
  'D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk\.venv_dlog\Lib\site-packages\stdatalog_pnpl\DTDL\dtmi\appconfig\steval_stwinbx1\FP_SNS_DATALOG2_Datalog2-7.json',
  'D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk\.venv_dlog\Lib\site-packages\stdatalog_pnpl\DTDL\dtmi\appconfig\steval_stwinbx1\ FP_SNS_DATALOG2_Datalog2-7.json',
  'C:\Users\dzf\AppData\Local\Programs\Python\Python310\Lib\site-packages\stdatalog_pnpl\DTDL\dtmi\appconfig\steval_stwinbx1\FP_SNS_DATALOG2_Datalog2-7.json'
)
$RefreshPinnedFromSource = $false   # 若你更新了“源 JSON”，设为 $true 让 PIN 副本被覆盖更新

# 板卡/固件号（与采集相符）
$CDM_BOARD_ID = 14
$CDM_FW_ID    = 7

# 可选项
$UseSplitPerTags = $false
$StartTimeSec    = $null
$EndTimeSec      = $null
$ChunkSize       = $null
$CleanOut        = $true       # 开跑前是否清空 $Out 中旧 *.wav
$PurgeStageAtEnd = $true        # ✅ 结束后清空 C:\_wav_stage
$PurgeLogsAtEnd  = $false       # 是否顺带删除日志 C:\_wav_stage\_pylogs

# ================== 失败记录导出（CSV / XLSX） ==================
$WriteFailureReport = $true
$FailureReportDir   = Join-Path (Split-Path -Parent $Out) '_reports'
$FailureReportStem  = 'convert_failures'
$ExportFailureXlsx  = $true      # 若 Python 环境里有 openpyxl，则额外导出 xlsx
# ==================================================================

# ================== 后处理：生成 metadata.csv / 重排目录（usc_env31） ==================
# 这一步会调用你自己的 generate_metadata_and_restructure.py（使用 $PyPrepare 指定的 python.exe）
$DoPrepareDataset       = $true
$PrepareScript          = 'D:\workspace\project\keil\thesis_speech_recognition\generate_metadata_and_restructure.py'
$PyPrepare              = 'D:\workspace\project\keil\thesis_speech_recognition\usc_env31\Scripts\python.exe'
$PrepareMoveFiles        = $true    # 等价于命令行的 --move
$PrepareTestRatio        = 0.2
$PrepareSeed             = 611
# ==================================================================

$ErrorActionPreference = 'Stop'
function Info($m){ Write-Host $m -ForegroundColor Cyan }
function Warn($m){ Write-Host $m -ForegroundColor Yellow }
function Err ($m){ Write-Host $m -ForegroundColor Red  }

# 衍生路径与环境
$StageOut  = Join-Path $StageRoot 'out_tree'
$DoneMark  = Join-Path $StageOut '_ALL_DONE'
$LogDir    = Join-Path $StageRoot '_pylogs'
$DtdlDir   = 'D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk\.venv_dlog\Lib\site-packages\stdatalog_pnpl\DTDL'
$DeviceCatalog = (Get-ChildItem $DtdlDir -Filter '*device*catalog*.json' -Recurse -ErrorAction SilentlyContinue |
  Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
if (-not $DeviceCatalog) { $DeviceCatalog = Join-Path $DtdlDir 'usb_device_catalog.json' }

# 0) 检查路径
if ($PSVersionTable.PSVersion.Major -lt 7) { throw ("需要 PowerShell 7+ 并行。当前版本：{0}" -f $PSVersionTable.PSVersion) }
foreach($p in @($Root,$Py,$ToWav)){ if (-not (Test-Path $p)) { throw "路径不存在：$p" } }
foreach($p in @($Out,$StageRoot,$StageOut,$LogDir)){ New-Item -ItemType Directory -Path $p -Force | Out-Null }
if ($WriteFailureReport) { New-Item -ItemType Directory -Path $FailureReportDir -Force | Out-Null }

# A) 维护 PIN 副本（由 $PinnedJsonPath 决定父目录）
$PinnedStoreDir = Split-Path -Parent $PinnedJsonPath
if (-not (Test-Path $PinnedStoreDir)) { New-Item -ItemType Directory -Path $PinnedStoreDir -Force | Out-Null }

function First-Existing([string[]]$cands){
  foreach($c in $cands){ if (Test-Path $c){ return $c } }
  return $null
}
$SourceJson = First-Existing $JsonSources

if (-not (Test-Path $PinnedJsonPath)) {
  if (-not $SourceJson) { throw '找不到任何源 JSON，请检查 $JsonSources 列表。' }
  Info ("初始化 PIN：复制 {0} => {1}" -f $SourceJson, $PinnedJsonPath)
  Copy-Item -LiteralPath $SourceJson -Destination $PinnedJsonPath -Force
} elseif ($RefreshPinnedFromSource) {
  if (-not $SourceJson) { throw '启用刷新，但未找到源 JSON。' }
  Warn ("刷新 PIN：覆盖 {0} 用 {1}" -f $PinnedJsonPath, $SourceJson)
  Copy-Item -LiteralPath $SourceJson -Destination $PinnedJsonPath -Force
} else {
  Info ("已存在 PIN：{0}" -f $PinnedJsonPath)
}

# B) 串行预注册（把 PIN 写入 catalog 的 custom；并比对哈希）
$RegPy = Join-Path $StageRoot '_register_and_compare.py'
@'
import sys, json, hashlib, os
from stdatalog_core.HSD_utils.dtm import HSDatalogDTM

def filehash(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1<<20), b""):
            h.update(chunk)
    return h.hexdigest()

def main():
    if len(sys.argv) != 5:
        raise SystemExit("Usage: _register_and_compare.py <BOARD> <FW> <JSON> <CATALOG_PATH>")
    b = int(sys.argv[1]); f = int(sys.argv[2]); j = sys.argv[3]; catalog = sys.argv[4]
    # 1) 注册：-cdm (b,f,j)
    HSDatalogDTM.upload_custom_dtm((b, f, j))
    # 2) catalog 里定位 custom 路径并比对哈希
    with open(catalog, "r", encoding="utf-8") as fh:
        data = json.load(fh)

    target_name = os.path.basename(j).lower()
    custom_path = None

    def walk(obj):
        nonlocal custom_path
        if isinstance(obj, dict):
            for v in obj.values():
                walk(v)
        elif isinstance(obj, list):
            for it in obj:
                walk(it)
        elif isinstance(obj, str):
            s = obj.lower()
            if target_name in s and "\\dtmi\\custom\\" in s:
                custom_path = obj

    walk(data)
    def fhash(p):
        h = hashlib.sha256()
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(1<<20), b""):
                h.update(chunk)
        return h.hexdigest()

    src_hash = fhash(j)
    dst_hash = None
    if custom_path and os.path.exists(custom_path):
        dst_hash = fhash(custom_path)

    print("SRC_JSON=", j)
    print("SRC_HASH=", src_hash)
    print("CUSTOM_JSON=", custom_path or "<not found>")
    print("CUSTOM_HASH=", dst_hash or "<not found>")

    # 不一致：返回 11，交由外层决定是否继续
    if not custom_path or not dst_hash or src_hash != dst_hash:
        raise SystemExit(11)

if __name__ == "__main__":
    main()
'@ | Set-Content -Path $RegPy -Encoding UTF8

Info ("预注册 PIN：Board={0}, FW={1}" -f $CDM_BOARD_ID, $CDM_FW_ID)
$p = Start-Process -FilePath $Py -ArgumentList @($RegPy, [string]$CDM_BOARD_ID, [string]$CDM_FW_ID, $PinnedJsonPath, $DeviceCatalog) -NoNewWindow -PassThru -Wait
if ($p.ExitCode -eq 0) {
  Info "PIN 注册成功，custom 与 PIN 哈希一致。"
} elseif ($p.ExitCode -eq 11) {
  Warn "PIN 注册完成，但 custom 与 PIN 哈希不一致（可能 custom 是旧副本）。如需严格一致，可删除 venv 的 DTDL\\dtmi\\custom 后重跑。"
} else {
  Err ("PIN 注册失败（Exit={0}），请检查 JSON 路径/权限。" -f $p.ExitCode); throw "无法完成 PIN 注册。"
}

# 1) 预清理 StageOut
Info ("预清理 StageOut 残留：{0}" -f $StageOut)
Remove-Item $DoneMark -Force -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $StageOut -Recurse -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
Get-ChildItem -LiteralPath $StageOut -Recurse -Directory -ErrorAction SilentlyContinue |
  Sort-Object FullName -Descending | ForEach-Object {
    if (-not (Get-ChildItem $_.FullName -Recurse -Force | Select-Object -First 1)) {
      Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
    }
  }

# 2) 可选：清空 Out 旧 WAV
if ($CleanOut -and (Test-Path $Out)) {
  Warn ("清理 Out 目录旧 wav：{0}" -f $Out)
  Get-ChildItem -LiteralPath $Out -Filter *.wav -Recurse -File -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
}

# 3) 搬运线程（只搬根部 *.wav，保持按类目录输出）
Info ("启动搬运线程：{0}  =>  {1}（递归搬运 *.wav，保留子目录结构）" -f $StageOut,$Out)
$mover = Start-Job -Name mover -ScriptBlock {
  param($StageOut,$Out,$DoneMark)
  try {
    while (-not (Test-Path $DoneMark)) {
      & robocopy $StageOut $Out *.wav /MOV /S /NP /NJH /NJS /NFL /NDL /R:1 /W:1 | Out-Null
      Start-Sleep -Milliseconds 400
    }
    & robocopy $StageOut $Out *.wav /MOV /S /NP /NJH /NJS /NFL /NDL /R:1 /W:1 | Out-Null
  } catch {
    $_ | Out-String | Write-Error
  }
} -ArgumentList $StageOut,$Out,$DoneMark

# 4) 收集采集目录（两级：Class -> Acquisitions）
$classDirs = Get-ChildItem -LiteralPath $Root -Directory -ErrorAction SilentlyContinue
$workItems = @()

foreach ($c in $classDirs) {
  $acqs = Get-ChildItem -LiteralPath $c.FullName -Directory -ErrorAction SilentlyContinue
  if ($acqs.Count -eq 0) {
    # 兼容：若该目录本身就是一次采集（里面直接有 .dat/.json），也处理
    $workItems += [PSCustomObject]@{ Class = $c.Name; AcqName = $c.Name; AcqPath = $c.FullName }
  } else {
    foreach ($a in $acqs) {
      $workItems += [PSCustomObject]@{ Class = $c.Name; AcqName = $a.Name; AcqPath = $a.FullName }
    }
  }
}

$Total = $workItems.Count
Info ("发现类别目录：{0}；待处理采集数：{1}；并行度：{2}" -f $classDirs.Count, $Total, $Workers)
if ($Total -eq 0) {
  Warn "没有采集目录可处理，退出。"
  New-Item -ItemType File -Path $DoneMark -Force | Out-Null
  Wait-Job $mover | Out-Null; Receive-Job $mover | Out-Null; Remove-Job $mover -Force | Out-Null
  exit 0
}
# 5) 并行转换（不传 -cdm；使用 PIN 已注册模型）
$sw = [Diagnostics.Stopwatch]::StartNew()
$results = $workItems | ForEach-Object -Parallel {
  $StageOut       = $using:StageOut
  $LogDir         = $using:LogDir
  $Py             = $using:Py
  $ToWav          = $using:ToWav
  $Sensor         = $using:Sensor
  $UseSplitPerTags= $using:UseSplitPerTags
  $StartTimeSec   = $using:StartTimeSec
  $EndTimeSec     = $using:EndTimeSec
  $ChunkSize      = $using:ChunkSize

  $class = $_.Class
  $acq  = $_.AcqPath
  $leaf = $_.AcqName


  $tmpBase = Join-Path $using:StageRoot 'tmp'
  New-Item -ItemType Directory -Path $tmpBase -Force | Out-Null
  $tmpOut  = Join-Path $tmpBase ("_tmp_{0}__{1}" -f $class, $leaf)
  
  
  New-Item -ItemType Directory -Path $tmpOut -Force | Out-Null

  $stdout  = Join-Path $LogDir ("{0}__{1}.stdout.txt" -f $class, $leaf)
  $stderr  = Join-Path $LogDir ("{0}__{1}.stderr.txt" -f $class, $leaf)

  $env:PYTHONIOENCODING = 'utf-8'
  $env:PYTHONUNBUFFERED = '1'

  $args = @($ToWav, $acq, '-o', $tmpOut, '-s', $Sensor)
  if ($UseSplitPerTags) { $args += '-spt' }
  if ($StartTimeSec -ne $null) { $args += @('-st', [string]$StartTimeSec) }
  if ($EndTimeSec   -ne $null) { $args += @('-et', [string]$EndTimeSec) }
  if ($ChunkSize    -ne $null) { $args += @('-cs', [string]$ChunkSize) }

  Write-Host ("[PY] {0}" -f ($args -join ' ')) -ForegroundColor DarkGray

  $p = Start-Process -FilePath $Py -ArgumentList $args -NoNewWindow -PassThru -Wait `
        -RedirectStandardOutput $stdout -RedirectStandardError $stderr

  $count = 0
  $exit = $p.ExitCode
  if ($exit -eq 0) {
    $wavFiles = Get-ChildItem -LiteralPath $tmpOut -Recurse -Filter *.wav -File -ErrorAction SilentlyContinue
    $count = $wavFiles.Count
    if ($count -eq 0) {
      Write-Host ("NO WAV GENERATED: {0}" -f $leaf) -ForegroundColor Red
      $exit = 22
    }
  }

  if ($exit -eq 0) {

    $stageClassDir = Join-Path $StageOut $class
    New-Item -ItemType Directory -Path $stageClassDir -Force | Out-Null
    function CopyToStageAtomic {
      param([string]$src,[string]$dst,[int]$maxRetry = 120)
      $tmp = ("{0}.__tmp" -f $dst)   # 注意：mover 只搬 *.wav，临时文件不会被搬走
      for ($i=1; $i -le $maxRetry; $i++) {
        try {
          Copy-Item -LiteralPath $src -Destination $tmp -Force -ErrorAction Stop
          Move-Item -LiteralPath $tmp -Destination $dst -Force -ErrorAction Stop  # 原子“落盘”
          return $true
        } catch {
          Start-Sleep -Milliseconds ([Math]::Min(2000, 50*$i))
        } finally {
          if (Test-Path $tmp) { Remove-Item -LiteralPath $tmp -Force -ErrorAction SilentlyContinue }
        }
      }
      return $false
    }

    function MoveOrCopyToStage {
      param([string]$src,[string]$dst)
      # 先尝试 Move（同盘重命名最快），失败则回退到 Copy+原子落盘
      for ($i=1; $i -le 20; $i++) {
        try {
          Move-Item -LiteralPath $src -Destination $dst -Force -ErrorAction Stop
          return $true
        } catch {
          Start-Sleep -Milliseconds ([Math]::Min(1200, 60*$i))
        }
      }
      return (CopyToStageAtomic -src $src -dst $dst -maxRetry 160)
    }

    $transferOk = $true

    # 目标结构：Out\<Class>\<AcqName>\<sensor>.wav
    # 例如：wav\Fanoff\20260226_17_04_12\imp23absu_mic.wav
    $stageAcqDir = Join-Path $stageClassDir $leaf
    New-Item -ItemType Directory -Path $stageAcqDir -Force | Out-Null

    foreach($w in $wavFiles){
      # 保留 stdatalog_to_wav.py 在 tmpOut 里生成的相对路径（通常就是 imp23absu_mic.wav）
      $rel = $w.FullName.Substring($tmpOut.Length + 1)

      $dest = Join-Path $stageAcqDir $rel
      $destDir = Split-Path -Parent $dest
      New-Item -ItemType Directory -Path $destDir -Force | Out-Null

      # 若重名（例如重复运行未清理），在文件名后追加 -1/-2...
      $target = $dest
      $k = 1
      while (Test-Path $target) {
        $dir = Split-Path -Parent $dest
        $nameNoExt = [System.IO.Path]::GetFileNameWithoutExtension($dest)
        $ext = [System.IO.Path]::GetExtension($dest)
        $target = Join-Path $dir ("{0}-{1}{2}" -f $nameNoExt, $k, $ext)
        $k++
      }

      if (-not (MoveOrCopyToStage -src $w.FullName -dst $target)) {
        Write-Host ("MOVE/COPY FAILED: {0} -> {1}" -f $w.FullName, $target) -ForegroundColor Red
        $transferOk = $false
      }
    }
    if (-not $transferOk) { $exit = 21 }
    Remove-Item $tmpOut -Recurse -Force -ErrorAction SilentlyContinue
  } else {
    Write-Host ("FAILED: {0} (exit {1})" -f $leaf, $exit) -ForegroundColor Red
    if (Test-Path $stderr) { Get-Content $stderr | Select-Object -Last 60 }
  }

  function Get-LogSummary {
    param([string]$stdoutPath,[string]$stderrPath)
    foreach($log in @($stderrPath,$stdoutPath)) {
      if (Test-Path $log) {
        $lines = Get-Content -LiteralPath $log -ErrorAction SilentlyContinue | Where-Object { $_ -and $_.Trim() -ne '' }
        if ($lines -and $lines.Count -gt 0) {
          $tail = $lines | Select-Object -Last 15
          $msg = ($tail -join ' || ')
          $msg = ($msg -replace '\s+', ' ').Trim()
          if ($msg.Length -gt 1500) { $msg = $msg.Substring(0,1500) + ' ...' }
          if ($msg) { return $msg }
        }
      }
    }
    return ''
  }

  [PSCustomObject]@{
    Name         = ("{0}\{1}" -f $class, $leaf)
    Class        = $class
    AcqName      = $leaf
    AcqPath      = $acq
    ExitCode     = $exit
    WavCount     = $count
    StdoutLog    = $stdout
    StderrLog    = $stderr
    ErrorSummary = $(if ($exit -ne 0) { Get-LogSummary -stdoutPath $stdout -stderrPath $stderr } else { '' })
  }
} -ThrottleLimit $Workers

$sw.Stop()

$ok  = ($results | Where-Object { $_.ExitCode -eq 0 }).Count
$err = $results.Count - $ok

# 6) 停止搬运、收尾
New-Item -ItemType File -Path $DoneMark -Force | Out-Null
Wait-Job $mover | Out-Null
Receive-Job $mover | Out-Null
Remove-Job $mover -Force | Out-Null

Info ("转换完成：OK={0}，ERR={1}；耗时：{2:n1}s；Out：{3}" -f $ok,$err,$sw.Elapsed.TotalSeconds,$Out)

# 6.25) 导出失败明细（CSV / XLSX）
if ($WriteFailureReport) {
  $failed = @($results | Where-Object { $_.ExitCode -ne 0 })

  if ($failed.Count -gt 0) {
    $stamp   = Get-Date -Format 'yyyyMMdd_HHmmss'
    $csvPath = Join-Path $FailureReportDir ("{0}_{1}.csv" -f $FailureReportStem, $stamp)
    $xlsxPath= Join-Path $FailureReportDir ("{0}_{1}.xlsx" -f $FailureReportStem, $stamp)

    $failed |
      Select-Object `
        @{Name='Status';Expression={'FAILED'}},
        Class, AcqName, AcqPath, ExitCode, WavCount, StdoutLog, StderrLog, ErrorSummary |
      Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

    Info ("失败明细 CSV：{0}" -f $csvPath)

    if ($ExportFailureXlsx) {
      $pyForXlsx = $null
      foreach($cand in @($PyPrepare, $Py)) {
        if ($cand -and (Test-Path $cand)) { $pyForXlsx = $cand; break }
      }

      if ($pyForXlsx) {
        $CsvToXlsxPy = Join-Path $StageRoot '_csv_to_xlsx_failures.py'
@'
import sys, csv
try:
    from openpyxl import Workbook
    from openpyxl.styles import Font, Alignment, PatternFill
except Exception as e:
    print(f"IMPORT_OPENPYXL_FAILED: {e}")
    raise SystemExit(3)

csv_path, xlsx_path = sys.argv[1], sys.argv[2]
wb = Workbook()
ws = wb.active
ws.title = "failures"

with open(csv_path, "r", encoding="utf-8-sig", newline="") as f:
    for row in csv.reader(f):
        ws.append(row)

for cell in ws[1]:
    cell.font = Font(bold=True)
    cell.alignment = Alignment(vertical="top", wrap_text=True)
    cell.fill = PatternFill(fill_type="solid", fgColor="D9EAF7")

for row in ws.iter_rows(min_row=2):
    for cell in row:
        cell.alignment = Alignment(vertical="top", wrap_text=True)

widths = {
    "A": 12,  # Status
    "B": 16,  # Class
    "C": 24,  # AcqName
    "D": 70,  # AcqPath
    "E": 10,  # ExitCode
    "F": 10,  # WavCount
    "G": 55,  # StdoutLog
    "H": 55,  # StderrLog
    "I": 120, # ErrorSummary
}
for col, width in widths.items():
    ws.column_dimensions[col].width = width

ws.freeze_panes = "A2"
wb.save(xlsx_path)
print(xlsx_path)
'@ | Set-Content -Path $CsvToXlsxPy -Encoding UTF8

        $xp = Start-Process -FilePath $pyForXlsx -ArgumentList @($CsvToXlsxPy, $csvPath, $xlsxPath) -NoNewWindow -PassThru -Wait
        if ($xp.ExitCode -eq 0 -and (Test-Path $xlsxPath)) {
          Info ("失败明细 XLSX：{0}" -f $xlsxPath)
        } else {
          Warn ("XLSX 导出失败（可能缺少 openpyxl），已保留 CSV：{0}" -f $csvPath)
        }
      } else {
        Warn ("找不到可用的 Python 环境来导出 XLSX，已保留 CSV：{0}" -f $csvPath)
      }
    }
  } else {
    Info "本次转换无失败项，因此未生成失败明细表。"
  }
}


# 6.5) 可选：调用 generate_metadata_and_restructure.py（usc_env31）生成 metadata.csv 并整理目录
if ($DoPrepareDataset) {
  if (-not (Test-Path $PrepareScript)) { throw ("找不到 Prepare 脚本：{0}" -f $PrepareScript) }
  if (-not (Test-Path $PyPrepare))     { throw ("找不到 usc_env31 的 python.exe：{0}" -f $PyPrepare) }

  $ProjectRoot = Split-Path -Parent $Out           # 例如 ...\DZF
  $WavDirName  = Split-Path -Leaf   $Out           # 例如 wav

  $prepStdout = Join-Path $StageRoot '_prepare.stdout.txt'
  $prepStderr = Join-Path $StageRoot '_prepare.stderr.txt'
  Remove-Item $prepStdout,$prepStderr -Force -ErrorAction SilentlyContinue

  $prepArgs = @(
    $PrepareScript,
    '--project_root', $ProjectRoot,
    '--wav_dir', $WavDirName,
    '--test_ratio', [string]$PrepareTestRatio,
    '--seed', [string]$PrepareSeed
  )
  if ($PrepareMoveFiles) { $prepArgs += '--move' }

  Info ("运行整理脚本（usc_env31）：{0} {1}" -f $PyPrepare, ($prepArgs -join ' '))
  $pp = Start-Process -FilePath $PyPrepare -ArgumentList $prepArgs -WorkingDirectory (Split-Path -Parent $PrepareScript) -NoNewWindow -PassThru -Wait `
        -RedirectStandardOutput $prepStdout -RedirectStandardError $prepStderr

  if ($pp.ExitCode -ne 0) {
    Err ("整理脚本失败 Exit={0}。请查看：{1} / {2}" -f $pp.ExitCode, $prepStdout, $prepStderr)
    throw ("整理脚本失败 Exit={0}" -f $pp.ExitCode)
  }
  Info ("整理完成：metadata.csv 已生成于 {0}" -f (Join-Path $ProjectRoot 'metadata.csv'))
}


# 7) ✅ 收尾清理：按需清空 C:\_wav_stage 的临时缓冲
if ($PurgeStageAtEnd) {
  Info ("清空 C 盘临时缓冲：{0}" -f $StageRoot)
  Remove-Item $DoneMark -Force -ErrorAction SilentlyContinue
  Get-ChildItem -LiteralPath $StageOut -Recurse -File -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
  Get-ChildItem -LiteralPath $StageOut -Recurse -Directory -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | ForEach-Object {
      if (-not (Get-ChildItem $_.FullName -Recurse -Force | Select-Object -First 1)) {
        Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
      }
    }
  if ($PurgeLogsAtEnd -and (Test-Path $LogDir)) {
    Warn ("按配置清理日志目录：{0}" -f $LogDir)
    Remove-Item $LogDir -Recurse -Force -ErrorAction SilentlyContinue
  }
}

Info ("全部完成。输出目录：{0}" -f $Out)
# 结束
