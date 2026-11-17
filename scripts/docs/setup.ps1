param([string]$PythonPath = "python")
$ErrorActionPreference = "Stop"
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "../.."))
$pythonOutput = Join-Path $root "out/docs-tools/python"
$nodeOutput = Join-Path $root "out/docs-tools/node"
New-Item -ItemType Directory -Path $pythonOutput, $nodeOutput -Force | Out-Null
& $PythonPath -m pip install --disable-pip-version-check --target $pythonOutput -r (Join-Path $PSScriptRoot "requirements.txt")
if ($LASTEXITCODE -ne 0) { throw "文档 Python 依赖安装失败。" }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "package.json") -Destination $nodeOutput
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "package-lock.json") -Destination $nodeOutput
Push-Location -LiteralPath $nodeOutput
try {
    npm.cmd ci --ignore-scripts --no-audit --no-fund
    if ($LASTEXITCODE -ne 0) { throw "文档 Node 依赖安装失败。" }
} finally { Pop-Location }
Write-Host "文档依赖安装到 out/docs-tools；未修改全局环境。"
