# 环境配置说明：命令词数据预处理

本目录用于将上位机保存的 STM32/ST-HSD 风格命令词采集数据批量转换为 WAV，并检查 WAV 文件和元数据完整性。

主要程序：

```text
convert_stdatalog_to_wav.ps1
audit_wav_and_metadata.py
```

## 1. 推荐环境

- 操作系统：Windows 10/11
- PowerShell：7
- Python：实验命令使用 Python 3.10；ST 官方 STDATALOG-PYSDK 也兼容 Python 3.10、3.11、3.12 和 3.13
- Git：用于递归克隆 STDATALOG-PYSDK 及其子模块
- ST DATALOG / STDATALOG Python SDK：用于 `.dat` 到 `.wav` 转换
- `openpyxl`：用于导出 XLSX 格式失败报告
- 后处理脚本运行环境：用于执行 `generate_metadata_and_restructure.py` 并生成 `metadata.csv`

本目录中的 `convert_stdatalog_to_wav.ps1` 不是完全独立脚本。为了运行本文处理流程，建议将该脚本复制到 ST 官方 `STDATALOG-PYSDK\stdatalog-pysdk` 根目录下运行。

推荐放置位置：

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\convert_stdatalog_to_wav.ps1
```

其中 `<STDATALOG-PYSDK_ROOT>` 是下载或克隆 ST 官方 STDATALOG-PYSDK 的目录。

ST 官方 STDATALOG-PYSDK 仓库：

```text
https://github.com/STMicroelectronics/stdatalog-pysdk
```

ST 官方 README 说明该仓库需要使用 `--recursive` 克隆子模块，并建议在干净的 Python 虚拟环境中运行安装脚本。

官方克隆命令：

```powershell
git clone --recursive https://github.com/STMicroelectronics/stdatalog-pysdk.git
```

## 2. PowerShell 配置

`convert_stdatalog_to_wav.ps1` 使用 PowerShell 7 的并行能力。

检查 PowerShell 版本：

```powershell
$PSVersionTable.PSVersion
```

如果主版本号小于 7，需要安装 PowerShell 7。

运行脚本前，可能需要临时允许当前终端执行脚本：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

## 3. ST DATALOG / STDATALOG 环境安装

`convert_stdatalog_to_wav.ps1` 会调用 ST 的 `stdatalog_to_wav.py`。

以实验使用的 Windows 路径为例，STDATALOG-PYSDK 放在：

```powershell
D:
cd D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk
```

通用写法为：

```powershell
cd <STDATALOG-PYSDK_ROOT>\stdatalog-pysdk
```

创建并激活虚拟环境。实验中使用 Python 3.10，因此命令为：

```powershell
py -3.10 -m venv .venv_dlog
.\.venv_dlog\Scripts\Activate.ps1
python -m pip install --upgrade pip setuptools wheel
```

然后在同一目录下运行 ST 官方安装脚本：

```powershell
.\STDATALOG-PYSDK_install.bat
```

如果只需要命令行转换能力，也可使用 ST 官方提供的 no-GUI 安装脚本：

```powershell
.\STDATALOG-PYSDK_install_noGUI.bat
```

安装完成后，需要能找到 ST 官方转换脚本：

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\stdatalog_examples\cli_applications\stdatalog_to_wav.py
```

并建议运行以下检查命令：

```powershell
python -c "import stdatalog_core, stdatalog_pnpl; print('STDATALOG-PYSDK environment OK')"
python .\stdatalog_examples\cli_applications\stdatalog_to_wav.py --help
```

如果需要导出 XLSX 失败报告，还需要在 `.venv_dlog` 中安装：

```powershell
python -m pip install openpyxl
```

## 4. 批量转换脚本放置位置

将本仓库中的脚本复制到 STDATALOG-PYSDK 官方目录：

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\convert_stdatalog_to_wav.ps1
```

推荐运行目录就是 STDATALOG-PYSDK 根目录：

```powershell
cd <STDATALOG-PYSDK_ROOT>\stdatalog-pysdk
```

这样 `$ToWav`、`.venv_dlog`、DTDL 设备 JSON 和 ST 官方示例脚本都在同一个官方 SDK 环境下，路径关系最清楚。

## 5. 批量转换脚本需要设置的路径

使用前需要确认脚本开头的路径。读者可按下面的模板配置：

```powershell
$SdkRoot = "<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk"
$Py = "$SdkRoot\.venv_dlog\Scripts\python.exe"
$ToWav = "$SdkRoot\stdatalog_examples\cli_applications\stdatalog_to_wav.py"
$PinnedJsonPath = "$SdkRoot\FP_SNS_DATALOG2_Datalog2-7.json"
$DtdlDir = "$SdkRoot\.venv_dlog\Lib\site-packages\stdatalog_pnpl\DTDL"
$JsonSources = @(
  "$DtdlDir\dtmi\appconfig\steval_stwinbx1\FP_SNS_DATALOG2_Datalog2-7.json"
)
```

配置含义：

- `$Py`：STDATALOG-PYSDK 虚拟环境中的 Python 解释器。
- `$ToWav`：ST 官方 `stdatalog_to_wav.py` 转换脚本。
- `$PinnedJsonPath`：固定使用的设备 JSON 副本。首次运行时可由 `$JsonSources` 中的源 JSON 复制得到。
- `$DtdlDir` 和 `$JsonSources`：STDATALOG-PYSDK 安装环境中的 DTDL 设备 JSON 路径。

同时需要根据本机环境设置数据输入输出路径。推荐数据目录结构如下：

```text
<COMMAND_DATASET_ROOT>/
├── dat/
│   ├── Fanoff/
│   │   ├── acquisition_001/
│   │   └── acquisition_002/
│   ├── Fanon/
│   └── background/
└── wav/
```

其中 `dat/` 下每个类别目录中包含若干个上位机保存的采集文件夹，每个采集文件夹中应包含 `.dat` 文件和对应 JSON 配置文件。

脚本中的路径可设置为：

```powershell
$Root = "<COMMAND_DATASET_ROOT>\dat"
$Out = "<COMMAND_DATASET_ROOT>\wav"
$StageRoot = "C:\_wav_stage"
```

注意：脚本开头的路径应以当前本机环境为准，请按本节和第 6 节设置。

## 6. 需要根据本机环境设置的参数

读者至少需要设置以下变量：

```powershell
$PinnedJsonPath
$Root
$Out
$StageRoot
$Py
$ToWav
$Sensor
$Workers
$JsonSources
$DtdlDir
```

参数含义：

- `$PinnedJsonPath`：固定使用的设备 JSON 副本路径，建议放在 `stdatalog-pysdk` 根目录，例如 `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\FP_SNS_DATALOG2_Datalog2-7.json`。
- `$Root`：命令词 `.dat` 原始采集数据根目录，即上位机保存的 STM32/ST-HSD 风格采集数据目录。
- `$Out`：WAV 输出目录。
- `$StageRoot`：临时缓存目录，建议使用本机 SSD 上的空目录。
- `$Py`：STDATALOG-PYSDK 虚拟环境中的 Python，例如 `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\.venv_dlog\Scripts\python.exe`。
- `$ToWav`：官方 `stdatalog_to_wav.py` 的路径，例如 `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\stdatalog_examples\cli_applications\stdatalog_to_wav.py`。
- `$Sensor`：麦克风传感器名称，当前为 `imp23absu_mic`。
- `$Workers`：并行转换数量。
- `$JsonSources`：ST DTDL 设备 JSON 的候选路径。
- `$DtdlDir`：STDATALOG-PYSDK 安装环境中的 DTDL 根目录。
- `$CDM_BOARD_ID` 和 `$CDM_FW_ID`：用于把固定设备 JSON 注册到 STDATALOG catalog 中。当前脚本使用 `14` 和 `7`，对应实验使用的 Datalog2 设备 JSON；更换采集固件或设备 JSON 时需要保持一致。
- `$CleanOut`：为 `$true` 时，脚本运行前会清理 `$Out` 中已有 WAV 文件。
- `$WriteFailureReport`：为 `$true` 时，脚本会导出转换失败明细。
- `$ExportFailureXlsx`：为 `$true` 时，脚本会在 CSV 之外额外导出 XLSX；该功能需要 `openpyxl`。

如果启用后处理，还需要设置：

```powershell
$DoPrepareDataset
$PrepareScript
$PyPrepare
$PrepareMoveFiles
$PrepareTestRatio
$PrepareSeed
```

注意：`$PrepareScript` 和 `$PyPrepare` 需要指向当前本机环境中的后处理脚本和 Python 解释器。若只需要执行 `.dat` 到 `.wav` 转换，可先将 `$DoPrepareDataset` 设置为 `$false`，再手动整理 `metadata.csv`。

推荐设置方式：

```powershell
$DoPrepareDataset = $true
$PrepareScript = "<后处理脚本目录>\generate_metadata_and_restructure.py"
$PyPrepare = "<后处理虚拟环境>\Scripts\python.exe"
$PrepareMoveFiles = $true
$PrepareTestRatio = 0.2
$PrepareSeed = 611
```

当 `$DoPrepareDataset = $true` 时，脚本会在 WAV 转换完成后调用后处理脚本。后处理脚本的输入输出关系为：

```text
project_root = Split-Path -Parent $Out
wav_dir      = Split-Path -Leaf $Out
metadata.csv = <project_root>\metadata.csv
```

例如：

```text
$Out = E:\SPEECH_DATA\TENG\command-20260419-ODR16000-2s-1\wav
```

则 `metadata.csv` 会生成在：

```text
E:\SPEECH_DATA\TENG\command-20260419-ODR16000-2s-1\metadata.csv
```

## 7. 输入输出目录配置

常用变量：

```powershell
$Root
$Out
$StageRoot
```

含义：

- `$Root`：命令词原始 `.dat` 采集数据根目录。
- `$Out`：转换后的 WAV 输出目录。
- `$StageRoot`：转换过程中的临时缓存目录。

建议数据目录按类别组织，例如：

```text
dat/
├── microwaveoff/
├── microwaveon/
├── fanoff/
├── fanon/
├── humidifieroff/
├── humidifieron/
├── lightoff/
├── lighton/
├── Televisionoff/
├── Televisionon/
└── background/
```

## 8. 实际运行步骤

以下步骤对应实验中执行 `stdatalog_to_wav_batch_tree_PINNED_v3_COMPLETE_ACQFOLDER_WITH_PREP_FAILREPORT.ps1` 的流程。

1. 打开 PowerShell 7。
2. 进入 STDATALOG-PYSDK 官方目录：

```powershell
D:
cd D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk
```

通用写法：

```powershell
cd <STDATALOG-PYSDK_ROOT>\stdatalog-pysdk
```

3. 创建并激活 `.venv_dlog`：

```powershell
py -3.10 -m venv .venv_dlog
.\.venv_dlog\Scripts\Activate.ps1
```

4. 安装 STDATALOG-PYSDK 依赖和失败报告依赖：

```powershell
python -m pip install --upgrade pip setuptools wheel
.\STDATALOG-PYSDK_install.bat
python -m pip install openpyxl
```

5. 将批量转换脚本放到当前目录，并按第 5、6 节设置脚本开头的变量。

6. 运行批量转换、失败报告导出和 metadata 生成：

```powershell
pwsh -ExecutionPolicy Bypass -File .\convert_stdatalog_to_wav.ps1
```

如果使用实验中的原始脚本名，则运行：

```powershell
pwsh -ExecutionPolicy Bypass -File .\stdatalog_to_wav_batch_tree_PINNED_v3_COMPLETE_ACQFOLDER_WITH_PREP_FAILREPORT.ps1
```

说明：该脚本内部已经统计转换失败项，并在 `$WriteFailureReport = $true` 时导出失败报告；不需要再单独运行一个统计失败文件的脚本。

7. 检查输出结果：

```text
<COMMAND_DATASET_ROOT>\wav\                 # 转换后的 WAV 文件
<COMMAND_DATASET_ROOT>\metadata.csv          # 训练用元数据
<COMMAND_DATASET_ROOT>\_reports\             # 转换失败报告
C:\_wav_stage\_pylogs\                       # stdatalog_to_wav.py 调用日志
```

失败报告文件名形如：

```text
convert_failures_YYYYMMDD_HHMMSS.csv
convert_failures_YYYYMMDD_HHMMSS.xlsx
```

如果 `XLSX 导出失败（可能缺少 openpyxl）`，CSV 失败报告仍会保留。

## 9. Python 审核脚本依赖

`audit_wav_and_metadata.py` 依赖：

- `soundfile`

安装命令：

```powershell
python -m pip install soundfile
```

检查命令：

```powershell
python -c "import soundfile; print('wav audit environment OK')"
```

## 10. 转换后审核

转换完成并生成 `metadata.csv` 后，可使用本仓库的审核脚本检查 WAV 文件和元数据是否对应。

在本仓库目录或包含 `audit_wav_and_metadata.py` 的目录运行：

```text
<repo>\command-word\preprocessing\
```

```powershell
python audit_wav_and_metadata.py <COMMAND_DATASET_ROOT> <frame_len> [offset]
```

示例：

```powershell
cd D:\workspace\project\keil\thesis_speech_recognition
python .\audit_wav_and_metadata.py "E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge" 30721 0
```

其中，`30721` 为本次检查要求的 WAV 采样点数，最后的 `0` 为读取偏移量。

审核输出：

```text
audit_report.csv
```

用于定位 `metadata.csv` 中记录存在但 WAV 缺失、WAV 无法读取或长度不足的样本。

## 11. 配置检查清单

1. `stdatalog-pysdk` 是否来自 ST 官方仓库。
2. 是否使用 `git clone --recursive` 获取子模块。
3. PowerShell 是否为 版本7。
4. `.venv_dlog` 是否能导入 `stdatalog_core` 和 `stdatalog_pnpl`。
5. `stdatalog_to_wav.py` 是否存在。
6. `$Py` 是否指向 STDATALOG-PYSDK 的虚拟环境。
7. `$ToWav` 是否指向官方 `stdatalog_to_wav.py`。
8. `$PinnedJsonPath`、`$JsonSources` 和 `$DtdlDir` 是否指向与采集固件匹配的设备 JSON。
9. `$Root` 是否指向命令词原始 `.dat` 数据。
10. `$Out` 是否为空目录或允许被清理。
11. `$Sensor` 是否与采集 JSON 中传感器名一致，当前为 `imp23absu_mic`。
12. `$DoPrepareDataset = $true` 时，`$PrepareScript` 和 `$PyPrepare` 是否可用。
13. 输出 WAV 是否按类别目录生成。
14. `metadata.csv` 是否生成在 `Split-Path -Parent $Out` 对应的项目根目录。
15. `_reports` 中是否存在失败报告；如有失败项，根据 CSV/XLSX 中的 `AcqPath`、`StdoutLog`、`StderrLog` 和 `ErrorSummary` 定位原因。
16. `audit_report.csv` 中是否存在缺失或长度不足样本。

## 12. 常见问题

- 如果提示路径不存在，优先检查 `$Py`、`$ToWav`、`$PinnedJsonPath`。
- 如果转换失败较多，检查采集目录中是否缺少 JSON 配置文件。
- 如果提示找不到 DTDL 或 device catalog，检查 `$DtdlDir`、`$JsonSources`、`$CDM_BOARD_ID` 和 `$CDM_FW_ID`。
- 如果 XLSX 失败报告无法生成，在 `$PyPrepare` 或 `$Py` 对应环境中安装 `openpyxl`。
- 如果 `metadata.csv` 未生成，检查 `$DoPrepareDataset` 是否为 `$true`，以及 `$PrepareScript` 和 `$PyPrepare` 是否存在。
- 如果 WAV 长度不足，检查采集时长、采样率和训练脚本中的 `frame_len` 是否一致。
- 如果后续训练标签错乱，检查 `metadata.csv` 中的类别名和目录名是否一致。
