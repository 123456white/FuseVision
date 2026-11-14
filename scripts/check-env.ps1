<#
FuseVision 开发环境自检脚本
在 git clone 后运行一次，确认所有必需工具已就绪。
#>
param(
    [ValidateSet("Ninja", "VisualStudio")]
    [string]$Generator = "Ninja"
)
$ErrorActionPreference = "Continue"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "===== FuseVision 开发环境检查 =====" -ForegroundColor Cyan
Write-Host ""

$allOk = $true
$minimumCMakeVersion = [version]"3.21.0"
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if ($Generator -eq "VisualStudio" -and (Test-Path -LiteralPath $vsWherePath)) {
    $vsVersion = & $vsWherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
    if ($vsVersion -and ([version]$vsVersion).Major -ge 18) { $minimumCMakeVersion = [version]"4.2.0" }
}

function Check-Tool {
    param(
        [string]$Name,
        [string]$Command,
        [string]$MinVersion = "",
        [string]$VersionPattern = "(\d+\.\d+\.\d+)"
    )
    $output = & cmd /c "$Command 2>&1" 2>$null | Out-String
    if ($LASTEXITCODE -ne 0 -and -not $output) {
        Write-Host "[FAIL] $Name 未找到或无法执行" -ForegroundColor Red
        $script:allOk = $false
        return
    }
    if ($MinVersion -ne "" -and $output -match $VersionPattern) {
        if ([version]$matches[1] -lt [version]$MinVersion) {
            Write-Host "[FAIL] $Name 版本过低 ($($matches[1]))，需要 >= $MinVersion" -ForegroundColor Red
            $script:allOk = $false
            return
        }
    }
    $verInfo = if ($output -match $VersionPattern) { " (v$($matches[1]))" } else { "" }
    Write-Host "[OK]   $Name$verInfo" -ForegroundColor Green
}

# CMake
$cmakeOutput = cmake --version 2>$null | Select-Object -First 1
if ($cmakeOutput -match "(\d+\.\d+\.\d+)") {
    if ([version]$matches[1] -ge $minimumCMakeVersion) {
        Write-Host "[OK]   CMake v$($matches[1])" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] CMake 版本过低 (v$($matches[1]))，$Generator 入口需要 >= $minimumCMakeVersion" -ForegroundColor Red
        $allOk = $false
    }
} else {
    Write-Host "[FAIL] CMake 未找到" -ForegroundColor Red
    $allOk = $false
}

# Git
$gitOutput = git --version 2>$null
if ($gitOutput -match "(\d+\.\d+\.\d+)") {
    Write-Host "[OK]   Git v$($matches[1])" -ForegroundColor Green
} else {
    Write-Host "[FAIL] Git 未找到" -ForegroundColor Red
    $allOk = $false
}

# Ninja is needed on PATH only for the Ninja presets.
if ($Generator -eq "Ninja") {
    $ninjaOutput = ninja --version 2>$null
    if ($ninjaOutput -match "(\d+\.\d+\.\d+)") {
        Write-Host "[OK]   Ninja v$($matches[1])" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] Ninja 未找到 — Ninja 预设要求此工具加入 PATH" -ForegroundColor Red
        $allOk = $false
    }
}

# Visual Studio / MSVC
if (Test-Path $vsWherePath) {
    $vsInfo = & $vsWherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
    if ($vsInfo) {
        Write-Host "[OK]   Visual Studio C++ 工具已安装: $vsInfo" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] 未找到 Visual Studio x64 C++ 工具" -ForegroundColor Red
        $allOk = $false
    }
} else {
    Write-Host "[WARN] 未检测到 Visual Studio（如仅使用 VS Code + Ninja + MSVC BuildTools 可忽略）" -ForegroundColor Yellow
}

# C++ 编译器
$clTest = where.exe cl.exe 2>$null
if ($clTest) {
    Write-Host "[OK]   MSVC 编译器 (cl.exe) 在 PATH 中" -ForegroundColor Green
} else {
    Write-Host "[WARN] cl.exe 不在 PATH — 运行 CmakeBuild.ps1 将自动配置，或手动运行 vcvars64.bat" -ForegroundColor Yellow
}

# vcpkg
if (Test-Path "$PSScriptRoot\..\vcpkg\vcpkg.exe") {
    Write-Host "[OK]   vcpkg 已引导" -ForegroundColor Green
} elseif (Test-Path "$PSScriptRoot\..\vcpkg\bootstrap-vcpkg.bat") {
    Write-Host "[INFO] vcpkg 需要引导 — 运行 .\vcpkg\bootstrap-vcpkg.bat" -ForegroundColor Yellow
} else {
    Write-Host "[FAIL] vcpkg 子模块未初始化 — 运行 git submodule update --init" -ForegroundColor Red
    $allOk = $false
}

# Halcon（可选 — 自动检测系统安装版或 sdk/halcon 内置版）
if ($env:HALCONROOT) {
    Write-Host "[OK]   Halcon（系统安装）: $env:HALCONROOT (架构: $env:HALCONARCH)" -ForegroundColor Green
} else {
    $sdkHalconDir = Join-Path $ProjectRoot "sdk/halcon"
    $halconHeader = Join-Path $sdkHalconDir "include/HalconCpp.h"
    $halconCppHeader = Join-Path $sdkHalconDir "include/halconcpp/HalconCpp.h"
    $halconLib = Join-Path $sdkHalconDir "lib/halcon.lib"
    $halconcppLib = Join-Path $sdkHalconDir "lib/halconcpp.lib"
    
    if (((Test-Path $halconHeader) -or (Test-Path $halconCppHeader)) -and (Test-Path $halconLib) -and (Test-Path $halconcppLib)) {
        Write-Host "[OK]   Halcon（sdk/halcon 内置）: $sdkHalconDir" -ForegroundColor Green
    } else {
        Write-Host "[INFO] Halcon 未配置 — 传统视觉模块的 Halcon 功能将不可用" -ForegroundColor Gray
        if (Test-Path $sdkHalconDir) {
            Write-Host "       sdk/halcon 目录存在但结构不完整，请检查: include/HalconCpp.h 或 include/halconcpp/HalconCpp.h，以及 lib/halcon*.lib" -ForegroundColor Yellow
        } else {
            Write-Host "       可通过以下方式启用 Halcon 功能：" -ForegroundColor Gray
            Write-Host "         1. 安装 Halcon SDK 并设置 HALCONROOT / HALCONARCH 环境变量" -ForegroundColor Gray
            Write-Host "         2. 将 Halcon SDK 文件放入 sdk/halcon/ 目录" -ForegroundColor Gray
        }
    }
}

# OpenCV（通过 vcpkg 管理，这里仅做提示）
Write-Host "[INFO] OpenCV 通过 vcpkg manifest 管理，将在 cmake configure 时自动安装" -ForegroundColor Gray

Write-Host ""
if ($allOk) {
    Write-Host "===== 环境检查通过！可以开始构建 =====" -ForegroundColor Green
    Write-Host "  运行:  ./CmakeBuild.ps1 -Generator $Generator -Configuration Debug -NoRun -Test"
    # Optional native probes (for example where.exe cl.exe) may leave exit code 1.
    # GitHub Actions propagates LASTEXITCODE, so return the aggregate result explicitly.
    exit 0
} else {
    Write-Host "===== 存在 [FAIL] 项，请先修复后再构建 =====" -ForegroundColor Red
    exit 1
}
