<#
FuseVision 构建脚本（Windows + Ninja）
自动检测 VS 环境，无需手动打开开发者命令行
#>
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration,
    [switch]$NoRun,
    [switch]$Test,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
chcp 65001 > $null
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ProjectRoot = $PSScriptRoot
$DebugBuildDir  = Join-Path $ProjectRoot "out/build/windows-x64-debug"
$ReleaseBuildDir = Join-Path $ProjectRoot "out/build/windows-x64-release"
$ExeName = "FuseVision.exe"

function Initialize-VSEnv {
    param([string]$BuildDir)
    $candidates = @()
    # Prefer the cached toolchain to avoid changing compilers on an incremental build.
    $cachePath = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath) {
        $compilerLine = Get-Content -LiteralPath $cachePath | Where-Object { $_ -match '^CMAKE_CXX_COMPILER:FILEPATH=' } | Select-Object -First 1
        if ($compilerLine -match '^CMAKE_CXX_COMPILER:FILEPATH=(.+)[/\\]VC[/\\]Tools[/\\]MSVC[/\\]') {
            $candidates += Join-Path $matches[1] "VC/Auxiliary/Build/vcvars64.bat"
        }
    }
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vsWhere) {
        $installation = & $vsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($installation) { $candidates += Join-Path $installation "VC/Auxiliary/Build/vcvars64.bat" }
    }
    $VcVars64 = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $VcVars64) {
        throw "未找到 VS x64 C++ 编译工具，请安装 Visual Studio / Build Tools 的 C++ 工作负载。"
    }
    Write-Host "初始化 VS x64 编译环境..." -ForegroundColor Cyan
    
    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        cmd /c "`"$VcVars64`" > nul 2>&1 && set > `"$tempFile`""
        if ($LASTEXITCODE -ne 0) { throw "VS 环境初始化失败 (exit $LASTEXITCODE)" }
        Get-Content $tempFile | ForEach-Object {
            if ($_ -match '^([^=]+)=(.*)') {
                $name = $matches[1]
                $value = $matches[2]
                [System.Environment]::SetEnvironmentVariable($name, $value, "Process")
                Set-Item -Path "env:$name" -Value $value -Force
            }
        }
    } finally {
        Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
    }
    
    $clPath = ($env:PATH -split ';' | Where-Object { $_ -match 'VC\\Tools\\MSVC.*\\bin\\Hostx64\\x64' } | Select-Object -First 1)
    if ($clPath) {
        $env:CMAKE_C_COMPILER = Join-Path $clPath "cl.exe"
        $env:CMAKE_CXX_COMPILER = Join-Path $clPath "cl.exe"
        [System.Environment]::SetEnvironmentVariable("CMAKE_C_COMPILER", $env:CMAKE_C_COMPILER, "Process")
        [System.Environment]::SetEnvironmentVariable("CMAKE_CXX_COMPILER", $env:CMAKE_CXX_COMPILER, "Process")
        Write-Host "VS 环境已配置 (编译器: $env:CMAKE_C_COMPILER)" -ForegroundColor Green
    } else {
        throw "未找到 x64 编译器路径"
    }
}

function Invoke-Backup {
    $srcPath = Join-Path $ProjectRoot "src"
    $backupRoot = Join-Path $ProjectRoot "backup"
    if (-not (Test-Path $backupRoot)) { New-Item -ItemType Directory -Path $backupRoot | Out-Null }
    
    $subDirs = Get-ChildItem -Path $srcPath -Directory | Select-Object -ExpandProperty Name
    Write-Host "`n===== 选择备份目录 =====" -ForegroundColor Cyan
    Write-Host "0. 备份整个 src"
    for ($i = 0; $i -lt $subDirs.Count; $i++) {
        Write-Host "$($i + 1). $($subDirs[$i])"
    }
    
    do {
        $choice = Read-Host "请选择（多选用逗号分隔）[0-$($subDirs.Count)]"
        $choices = $choice -split ',' | ForEach-Object { $_.Trim() }
        $valid = $true
        foreach ($c in $choices) {
            if (-not ($c -match '^\d+$' -and [int]$c -ge 0 -and [int]$c -le $subDirs.Count)) {
                $valid = $false; break
            }
        }
        if ($valid -and $choices.Count -gt 0) { break }
        Write-Host "无效选择" -ForegroundColor Red
    } while ($true)
    
    $suffix = Read-Host "自定义后缀（可选）"
    $backupName = "$(Get-Date -Format 'yyyyMMdd')$(if ($suffix) { "_$suffix" })"
    $backupDir = Join-Path $backupRoot $backupName
    
    if ($choices -contains "0") {
        Write-Host "备份: $srcPath -> $backupDir" -ForegroundColor Cyan
        if (Test-Path $backupDir) { Remove-Item -Path $backupDir -Recurse -Force }
        New-Item -ItemType Directory -Path $backupDir | Out-Null
        Get-ChildItem -Path $srcPath | Copy-Item -Destination $backupDir -Recurse -Force
    } else {
        Write-Host "备份到: $backupDir" -ForegroundColor Cyan
        if (Test-Path $backupDir) { Remove-Item -Path $backupDir -Recurse -Force }
        New-Item -ItemType Directory -Path $backupDir | Out-Null
        foreach ($c in $choices) {
            $source = Join-Path $srcPath $subDirs[[int]$c - 1]
            Copy-Item -Path $source -Destination $backupDir -Recurse -Force
        }
    }
    Write-Host "备份完成" -ForegroundColor Green
}

function Invoke-Build {
    param([string]$BuildType, [string]$BuildDir, [bool]$Clean = $false)
    
    Initialize-VSEnv -BuildDir $BuildDir
    
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "清理: $BuildDir" -ForegroundColor Yellow
        $resolvedBuildDir = [System.IO.Path]::GetFullPath($BuildDir)
        if ($resolvedBuildDir -notin @([System.IO.Path]::GetFullPath($DebugBuildDir), [System.IO.Path]::GetFullPath($ReleaseBuildDir))) {
            throw "拒绝清理非项目构建目录: $resolvedBuildDir"
        }
        Remove-Item -LiteralPath $resolvedBuildDir -Recurse -Force
        $rootCache = Join-Path $ProjectRoot "CMakeCache.txt"
        if (Test-Path $rootCache) { Remove-Item $rootCache -Force }
    }
    
    $configPreset = "windows-x64-$($BuildType.ToLower())"
    $buildPreset = "windows-$($BuildType.ToLower())"
    
    Push-Location -LiteralPath $ProjectRoot
    try {
        Write-Host "配置: $configPreset" -ForegroundColor Cyan
        cmake --preset $configPreset
        if ($LASTEXITCODE -ne 0) { throw "配置失败 (exit $LASTEXITCODE)" }

        Write-Host "构建: $buildPreset" -ForegroundColor Cyan
        cmake --build --preset $buildPreset
        if ($LASTEXITCODE -ne 0) { throw "构建失败 (exit $LASTEXITCODE)" }

        if ($Test) {
            $reportDir = Join-Path $BuildDir "Testing"
            New-Item -ItemType Directory -Path $reportDir -Force | Out-Null
            ctest --test-dir $BuildDir --build-config $BuildType --output-on-failure --no-tests=error --output-junit (Join-Path $reportDir "junit.xml")
            if ($LASTEXITCODE -ne 0) { throw "测试失败 (exit $LASTEXITCODE)" }
        }
    } finally {
        Pop-Location
    }
    
    $exePath = Join-Path $BuildDir "bin\$BuildType\$ExeName"
    if (-not (Test-Path $exePath)) {
        throw "未找到可执行文件: $exePath"
    }
    if ($NoRun) { return }
    
    Write-Host "运行: $BuildType" -ForegroundColor Cyan
    & $exePath
    if ($LASTEXITCODE -ne 0) { throw "应用运行失败 (exit $LASTEXITCODE)" }
}

if ($Configuration) {
    $buildDir = if ($Configuration -eq "Debug") { $DebugBuildDir } else { $ReleaseBuildDir }
    Invoke-Build -BuildType $Configuration -BuildDir $buildDir -Clean $Clean.IsPresent
    exit 0
}
if ($NoRun -or $Test -or $Clean) { throw "非交互选项需要同时指定 -Configuration Debug 或 Release。" }

do {
    Write-Host "`n===== FuseVision 构建菜单 =====" -ForegroundColor Cyan
    Write-Host "1. Debug 构建并运行"
    Write-Host "2. Release 构建并运行"
    Write-Host "3. 清理 + Debug 重建"
    Write-Host "4. 清理 + Release 重建"
    Write-Host "5. 备份 src 目录"
    Write-Host "6. 退出"
    $choice = Read-Host "请选择 [1-6]"

    switch ($choice) {
        "1" { Invoke-Build -BuildType "Debug"   -BuildDir $DebugBuildDir   -Clean $false }
        "2" { Invoke-Build -BuildType "Release" -BuildDir $ReleaseBuildDir -Clean $false }
        "3" { Invoke-Build -BuildType "Debug"   -BuildDir $DebugBuildDir   -Clean $true  }
        "4" { Invoke-Build -BuildType "Release" -BuildDir $ReleaseBuildDir -Clean $true  }
        "5" { Invoke-Backup }
        "6" { exit 0 }
        default { Write-Host "无效选择" -ForegroundColor Red }
    }
    Write-Host "`n按任意键继续..." -ForegroundColor Gray
    $null = $Host.UI.RawUI.ReadKey('NoEcho,IncludeKeyDown')
} while ($true)
