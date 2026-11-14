# Run inside the SkellCore checkout from a Windows developer terminal.
# Python, Git, CMake, Ninja and MSVC must be on PATH.
param(
    [string]$Plugin = "skell-preset-vst",
    [string]$Ref = "",
    [string]$Workspace = "$HOME\Skelly\Quick4",
    [switch]$Update,
    [switch]$Offline,
    [switch]$Full,
    [int]$Jobs = 1
)
$ErrorActionPreference = "Stop"
$runner = Join-Path $PSScriptRoot "tools\quick4_workspace.py"
$argsList = @($runner, "--plugin", $Plugin, "--workspace", $Workspace, "--jobs", "$Jobs")
if ($Ref) { $argsList += @("--ref", $Ref) }
if ($Update) { $argsList += "--update" }
if ($Offline) { $argsList += "--offline" }
if ($Full) { $argsList += "--full" }
python @argsList
exit $LASTEXITCODE
