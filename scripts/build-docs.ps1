param([string]$DoxygenPath = "")

$ErrorActionPreference = "Stop"
$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if (-not $DoxygenPath -and $env:DOXYGEN_ROOT) {
    $DoxygenPath = Join-Path $env:DOXYGEN_ROOT "doxygen.exe"
}
if (-not $DoxygenPath) {
    $command = Get-Command doxygen -ErrorAction SilentlyContinue
    if ($command) { $DoxygenPath = $command.Source }
}
if (-not $DoxygenPath -or -not (Test-Path -LiteralPath $DoxygenPath -PathType Leaf)) {
    throw "未找到 Doxygen。请传入 -DoxygenPath 或设置 DOXYGEN_ROOT。"
}

$outputPath = Join-Path $projectRoot "out/docs/doxygen"
New-Item -ItemType Directory -Path $outputPath -Force | Out-Null
Push-Location -LiteralPath $projectRoot
try {
    & $DoxygenPath "docs/Doxyfile"
    if ($LASTEXITCODE -ne 0) { throw "Doxygen 生成失败 (exit $LASTEXITCODE)；请查看 out/docs/doxygen/warnings.log。" }
    $indexPath = Join-Path $outputPath "html/index.html"
    if (-not (Test-Path -LiteralPath $indexPath)) { throw "Doxygen 未生成 HTML 入口。" }
    Write-Host "API 文档：$indexPath"
} finally {
    Pop-Location
}
