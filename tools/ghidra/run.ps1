# Runs a script of this folder on the Ghidra project of alone4.exe, without
# opening Ghidra. The project is not changed unless -Write is given (to save
# what the script does, e.g. RenameFunctions.java).
# Examples:
#   tools\ghidra\run.ps1 DecompileByString.java "Loading VIEW_VRAM" "LMVR" out.c
#   tools\ghidra\run.ps1 -Write RenameFunctions.java tools\ghidra\functionNames.txt
param(
    [switch]$Write,
    [Parameter(Mandatory = $true)][string]$Script,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$ScriptArgs
)
# Ghidra: GHIDRA_INSTALL_DIR, else the newest ghidra_* folder beside the project's
$install = $env:GHIDRA_INSTALL_DIR
if (-not $install) {
    $install = Get-ChildItem (Join-Path $PSScriptRoot "..\..\..") -Directory -Filter "ghidra_*" |
        Sort-Object Name | Select-Object -Last 1 -ExpandProperty FullName
}
if (-not $install) { throw "Ghidra not found: set GHIDRA_INSTALL_DIR" }
$ghidra = Join-Path $install "support\analyzeHeadless.bat"
$project = Join-Path $PSScriptRoot "..\..\ghidra"
[string[]]$mode = if ($Write) { @() } else { @("-readOnly") }
& $ghidra (Resolve-Path $project) Alone4 -process alone4.exe -noanalysis @mode `
    -scriptPath $PSScriptRoot -postScript $Script @ScriptArgs 2>&1 |
    Select-String -Pattern "> |ERROR|Exception" | ForEach-Object { $_.Line }
