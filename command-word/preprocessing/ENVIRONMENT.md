# Environment Setup: Command-Word Data Preprocessing

This directory is used to batch-convert STM32/ST-HSD-style command-word acquisition data saved by the host program into WAV files, and to check the integrity of the WAV files and metadata.

Main programs:

```text
convert_stdatalog_to_wav.ps1
audit_wav_and_metadata.py
```

## 1. Recommended Environment

- Operating system: Windows 10/11
- PowerShell: version 7
- Python: the experimental commands used Python 3.10; the official ST STDATALOG-PYSDK is also compatible with Python 3.10, 3.11, 3.12, and 3.13
- Git: used to recursively clone STDATALOG-PYSDK and its submodules
- ST DATALOG / STDATALOG Python SDK: used for `.dat` to `.wav` conversion
- `openpyxl`: used to export XLSX-format failure reports
- Post-processing script environment: used to run `generate_metadata_and_restructure.py` and generate `metadata.csv`

The `convert_stdatalog_to_wav.ps1` script in this directory is not a fully standalone script. To run the preprocessing workflow in this study, copy this script into the root directory of the official ST `STDATALOG-PYSDK\stdatalog-pysdk` package and run it there.

Recommended location:

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\convert_stdatalog_to_wav.ps1
```

Here, `<STDATALOG-PYSDK_ROOT>` is the directory where the official ST STDATALOG-PYSDK repository is downloaded or cloned.

Official ST STDATALOG-PYSDK repository:

```text
https://github.com/STMicroelectronics/stdatalog-pysdk
```

The official README states that this repository should be cloned with `--recursive` to obtain submodules, and that the installation script should be run in a clean Python virtual environment.

Official clone command:

```powershell
git clone --recursive https://github.com/STMicroelectronics/stdatalog-pysdk.git
```

## 2. PowerShell Configuration

`convert_stdatalog_to_wav.ps1` uses the parallel execution capability of PowerShell 7.

Check the PowerShell version:

```powershell
$PSVersionTable.PSVersion
```

If the major version is lower than 7, install PowerShell 7.

Before running the script, it may be necessary to temporarily allow script execution in the current terminal:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

## 3. Install the ST DATALOG / STDATALOG Environment

`convert_stdatalog_to_wav.ps1` calls ST's `stdatalog_to_wav.py`.

The Windows path used in this study was:

```powershell
D:
cd D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk
```

Generic form:

```powershell
cd <STDATALOG-PYSDK_ROOT>\stdatalog-pysdk
```

Create and activate the virtual environment. Python 3.10 was used in the experiment:

```powershell
py -3.10 -m venv .\.venv_dlog
.\.venv_dlog\Scripts\Activate.ps1
python -m pip install --upgrade pip setuptools wheel
```

Then run the official ST installation script in the same directory:

```powershell
.\STDATALOG-PYSDK_install.bat
```

If only command-line conversion is required, the official no-GUI installation script can also be used:

```powershell
.\STDATALOG-PYSDK_install_noGUI.bat
```

After installation, the official ST conversion script should be available at:

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\stdatalog_examples\cli_applications\stdatalog_to_wav.py
```

Run the following checks:

```powershell
python -c "import stdatalog_core, stdatalog_pnpl; print('STDATALOG-PYSDK environment OK')"
python .\stdatalog_examples\cli_applications\stdatalog_to_wav.py --help
```

If XLSX failure reports are required, install `openpyxl` in `.venv_dlog`:

```powershell
python -m pip install openpyxl
```

## 4. Location of the Batch Conversion Script

Copy the script in this repository into the official STDATALOG-PYSDK directory:

```text
<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\convert_stdatalog_to_wav.ps1
```

The recommended working directory is the STDATALOG-PYSDK root:

```powershell
cd <STDATALOG-PYSDK_ROOT>\stdatalog-pysdk
```

In this layout, `$ToWav`, `.venv_dlog`, the DTDL device JSON, and the official ST example scripts are all under the same official SDK environment, making the path relationships clear.

## 5. Paths to Configure in the Batch Conversion Script

Before running the script, check the path variables at the beginning of the script. The following template can be used:

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

Meaning of the variables:

- `$Py`: Python interpreter in the STDATALOG-PYSDK virtual environment.
- `$ToWav`: official ST `stdatalog_to_wav.py` conversion script.
- `$PinnedJsonPath`: fixed device JSON copy. On the first run, it can be copied from one of the source JSON files in `$JsonSources`.
- `$DtdlDir` and `$JsonSources`: DTDL device JSON paths in the STDATALOG-PYSDK installation environment.

The data input and output paths must also be set according to the local environment. The recommended data directory structure is:

```text
<COMMAND_DATASET_ROOT>/
|-- dat/
|   |-- Fanoff/
|   |   |-- acquisition_001/
|   |   `-- acquisition_002/
|   |-- Fanon/
|   `-- background/
`-- wav/
```

Each class directory under `dat/` contains several acquisition folders saved by the host program. Each acquisition folder should contain `.dat` files and the corresponding JSON configuration files.

The script paths can be set as:

```powershell
$Root = "<COMMAND_DATASET_ROOT>\dat"
$Out = "<COMMAND_DATASET_ROOT>\wav"
$StageRoot = "C:\_wav_stage"
```

The paths at the beginning of the script should match the current local environment. Configure them according to this section and Section 6.

## 6. Parameters to Set for the Local Environment

At least the following variables should be set:

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

Parameter descriptions:

- `$PinnedJsonPath`: path of the fixed device JSON copy. It is recommended to place it in the `stdatalog-pysdk` root directory, for example `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\FP_SNS_DATALOG2_Datalog2-7.json`.
- `$Root`: root directory of the raw command-word `.dat` acquisition data, namely the STM32/ST-HSD-style acquisition data directory saved by the host program.
- `$Out`: WAV output directory.
- `$StageRoot`: temporary staging directory. An empty directory on a local SSD is recommended.
- `$Py`: Python interpreter in the STDATALOG-PYSDK virtual environment, for example `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\.venv_dlog\Scripts\python.exe`.
- `$ToWav`: path of the official `stdatalog_to_wav.py`, for example `<STDATALOG-PYSDK_ROOT>\stdatalog-pysdk\stdatalog_examples\cli_applications\stdatalog_to_wav.py`.
- `$Sensor`: microphone sensor name. The current setting is `imp23absu_mic`.
- `$Workers`: number of parallel conversion workers.
- `$JsonSources`: candidate paths for the ST DTDL device JSON.
- `$DtdlDir`: DTDL root directory in the STDATALOG-PYSDK installation environment.
- `$CDM_BOARD_ID` and `$CDM_FW_ID`: used to register the fixed device JSON into the STDATALOG catalog. The current script uses `14` and `7`, corresponding to the Datalog2 device JSON used in the experiment. These values should remain consistent when the acquisition firmware or device JSON is changed.
- `$CleanOut`: when set to `$true`, existing WAV files in `$Out` are cleaned before the script runs.
- `$WriteFailureReport`: when set to `$true`, conversion failure details are exported.
- `$ExportFailureXlsx`: when set to `$true`, an XLSX file is exported in addition to the CSV report. This feature requires `openpyxl`.

If post-processing is enabled, also set:

```powershell
$DoPrepareDataset
$PrepareScript
$PyPrepare
$PrepareMoveFiles
$PrepareTestRatio
$PrepareSeed
```

`$PrepareScript` and `$PyPrepare` must point to the post-processing script and Python interpreter in the current local environment. If only `.dat` to `.wav` conversion is required, set `$DoPrepareDataset` to `$false` first, and then manually organize `metadata.csv`.

Recommended settings:

```powershell
$DoPrepareDataset = $true
$PrepareScript = "<POSTPROCESSING_SCRIPT_DIR>\generate_metadata_and_restructure.py"
$PyPrepare = "<POSTPROCESSING_VENV>\Scripts\python.exe"
$PrepareMoveFiles = $true
$PrepareTestRatio = 0.2
$PrepareSeed = 611
```

When `$DoPrepareDataset = $true`, the script calls the post-processing script after WAV conversion. The input and output relationship of the post-processing script is:

```text
project_root = Split-Path -Parent $Out
wav_dir      = Split-Path -Leaf $Out
metadata.csv = <project_root>\metadata.csv
```

For example:

```text
$Out = E:\SPEECH_DATA\TENG\command-20260419-ODR16000-2s-1\wav
```

then `metadata.csv` is generated at:

```text
E:\SPEECH_DATA\TENG\command-20260419-ODR16000-2s-1\metadata.csv
```

## 7. Input and Output Directory Configuration

Common variables:

```powershell
$Root
$Out
$StageRoot
```

Meaning:

- `$Root`: root directory of the raw command-word `.dat` acquisition data.
- `$Out`: output directory of converted WAV files.
- `$StageRoot`: temporary staging directory used during conversion.

The data directory should be organized by class, for example:

```text
dat/
|-- microwaveoff/
|-- microwaveon/
|-- fanoff/
|-- fanon/
|-- humidifieroff/
|-- humidifieron/
|-- lightoff/
|-- lighton/
|-- Televisionoff/
|-- Televisionon/
`-- background/
```

## 8. Running Procedure

The following steps correspond to the procedure used in the experiment for running `stdatalog_to_wav_batch_tree_PINNED_v3_COMPLETE_ACQFOLDER_WITH_PREP_FAILREPORT.ps1`.

1. Open PowerShell 7.

2. Enter the official STDATALOG-PYSDK directory:

```powershell
D:
cd D:\workspace\project\Python\STDATALOG-PYSDK\stdatalog-pysdk
```

Generic form:

```powershell
cd <STDATALOG-PYSDK_ROOT>\stdatalog-pysdk
```

3. Create and activate `.venv_dlog`:

```powershell
py -3.10 -m venv .\.venv_dlog
.\.venv_dlog\Scripts\Activate.ps1
```

4. Install STDATALOG-PYSDK dependencies and the failure-report dependency:

```powershell
python -m pip install --upgrade pip setuptools wheel
.\STDATALOG-PYSDK_install.bat
python -m pip install openpyxl
```

5. Place the batch conversion script in the current directory, and set the variables at the beginning of the script according to Sections 5 and 6.

6. Run batch conversion, failure-report export, and metadata generation:

```powershell
pwsh -ExecutionPolicy Bypass -File .\convert_stdatalog_to_wav.ps1
```

If using the original experimental script name, run:

```powershell
pwsh -ExecutionPolicy Bypass -File .\stdatalog_to_wav_batch_tree_PINNED_v3_COMPLETE_ACQFOLDER_WITH_PREP_FAILREPORT.ps1
```

The script counts conversion failures internally and exports failure reports when `$WriteFailureReport = $true`. A separate script is not required to count failed files.

7. Check the outputs:

```text
<COMMAND_DATASET_ROOT>\wav\                 # converted WAV files
<COMMAND_DATASET_ROOT>\metadata.csv          # metadata for training
<COMMAND_DATASET_ROOT>\_reports\             # conversion failure reports
C:\_wav_stage\_pylogs\                       # logs from stdatalog_to_wav.py calls
```

Failure report filenames are similar to:

```text
convert_failures_YYYYMMDD_HHMMSS.csv
convert_failures_YYYYMMDD_HHMMSS.xlsx
```

If `XLSX export failed (possibly missing openpyxl)` appears, the CSV failure report is still preserved.

## 9. Python Audit Script Dependencies

`audit_wav_and_metadata.py` depends on:

- `soundfile`

Installation command:

```powershell
python -m pip install soundfile
```

Check command:

```powershell
python -c "import soundfile; print('wav audit environment OK')"
```

## 10. Post-Conversion Audit

After conversion and `metadata.csv` generation, use the audit script in this repository to check whether WAV files and metadata are consistent.

Run the command from this repository or from the directory containing `audit_wav_and_metadata.py`:

```text
<repo>\command-word\preprocessing\
```

```powershell
python audit_wav_and_metadata.py <COMMAND_DATASET_ROOT> <frame_len> [offset]
```

Example:

```powershell
cd D:\workspace\project\keil\thesis_speech_recognition
python .\audit_wav_and_metadata.py "E:\SPEECH_DATA\TENG\touming-dataset\command-20260315-merge" 30721 0
```

Here, `30721` is the required number of WAV samples for this check, and the final `0` is the read offset.

Audit output:

```text
audit_report.csv
```

This report is used to locate samples that are recorded in `metadata.csv` but missing from the WAV directory, WAV files that cannot be read, or samples whose length is insufficient.

## 11. Configuration Checklist

1. Confirm that `stdatalog-pysdk` is from the official ST repository.
2. Confirm that `git clone --recursive` was used to obtain submodules.
3. Confirm that PowerShell is version 7.
4. Confirm that `.venv_dlog` can import `stdatalog_core` and `stdatalog_pnpl`.
5. Confirm that `stdatalog_to_wav.py` exists.
6. Confirm that `$Py` points to the STDATALOG-PYSDK virtual environment.
7. Confirm that `$ToWav` points to the official `stdatalog_to_wav.py`.
8. Confirm that `$PinnedJsonPath`, `$JsonSources`, and `$DtdlDir` point to the device JSON matching the acquisition firmware.
9. Confirm that `$Root` points to the raw command-word `.dat` data.
10. Confirm that `$Out` is empty or allowed to be cleaned.
11. Confirm that `$Sensor` is consistent with the sensor name in the acquisition JSON. The current value is `imp23absu_mic`.
12. If `$DoPrepareDataset = $true`, confirm that `$PrepareScript` and `$PyPrepare` are available.
13. Confirm that output WAV files are generated by class directory.
14. Confirm that `metadata.csv` is generated in the project root corresponding to `Split-Path -Parent $Out`.
15. Confirm that failure reports exist under `_reports`; if failures are present, use `AcqPath`, `StdoutLog`, `StderrLog`, and `ErrorSummary` in the CSV/XLSX report to locate the cause.
16. Confirm whether `audit_report.csv` contains missing or short samples.

## 12. Troubleshooting

- If a path does not exist, first check `$Py`, `$ToWav`, and `$PinnedJsonPath`.
- If many conversions fail, check whether JSON configuration files are missing from the acquisition folders.
- If DTDL or device catalog errors are reported, check `$DtdlDir`, `$JsonSources`, `$CDM_BOARD_ID`, and `$CDM_FW_ID`.
- If the XLSX failure report cannot be generated, install `openpyxl` in the environment corresponding to `$PyPrepare` or `$Py`.
- If `metadata.csv` is not generated, check whether `$DoPrepareDataset` is `$true`, and whether `$PrepareScript` and `$PyPrepare` exist.
- If WAV files are too short, check whether the acquisition duration, sampling rate, and `frame_len` in the training script are consistent.
- If training labels are mismatched later, check whether class names in `metadata.csv` and directory names are consistent.
