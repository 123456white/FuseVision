<#
FuseVision 开发环境自检脚本
在 git clone 后运行一次，确认所有必需工具已就绪。
#>
$ErrorActionPreference = "Continue"

Write-Host "===== FuseVision 开发环境检查 =====" -ForegroundColor Cyan
Write-Host ""

$allOk = $true

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
    if ([version]$matches[1] -ge [version]"3.21.0") {
        Write-Host "[OK]   CMake v$($matches[1])" -ForegroundColor Green
    } else {
        Write-Host "[FAIL] CMake 版本过低 (v$($matches[1]))，需要 >= 3.21" -ForegroundColor Red
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

# Ninja
$ninjaOutput = ninja --version 2>$null
if ($ninjaOutput -match "(\d+\.\d+\.\d+)") {
    Write-Host "[OK]   Ninja v$($matches[1])" -ForegroundColor Green
} else {
    Write-Host "[WARN] Ninja 未找到 — 构建可能失败，请确保 PATH 中包含 Ninja 或使用 MSVC 生成器" -ForegroundColor Yellow
}

# Visual Studio / MSVC
$vsWherePath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWherePath) {
    $vsInfo = & $vsWherePath -latest -property installationPath 2>$null
    Write-Host "[OK]   Visual Studio 已安装: $vsInfo" -ForegroundColor Green
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
    $halconLib = Join-Path $sdkHalconDir "lib/x64-win64/halcon.lib"
    $halconcppLib = Join-Path $sdkHalconDir "lib/x64-win64/halconcpp.lib"
    
    if ((Test-Path $halconHeader) -and (Test-Path $halconLib) -and (Test-Path $halconcppLib)) {
        Write-Host "[OK]   Halcon（sdk/halcon 内置）: $sdkHalconDir" -ForegroundColor Green
    } else {
        Write-Host "[INFO] Halcon 未配置 — 传统视觉模块的 Halcon 功能将不可用" -ForegroundColor Gray
        if (Test-Path $sdkHalconDir) {
            Write-Host "       sdk/halcon 目录存在但结构不完整，请检查: include/HalconCpp.h, lib/x64-win64/halcon*.lib" -ForegroundColor Yellow
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
    Write-Host "  运行:  cmake --preset windows-x64-debug"
    Write-Host "  然后:  cmake --build --preset windows-debug"
} else {
    Write-Host "===== 存在 [FAIL] 项，请先修复后再构建 =====" -ForegroundColor Red
}
