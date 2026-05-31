<#
FuseVision 构建脚本（Windows + Ninja）
自动检测 VS 环境，无需手动打开开发者命令行
#>
chcp 65001 > $null
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

$ProjectRoot = $PWD.Path
$DebugBuildDir  = Join-Path $ProjectRoot "out/build/windows-x64-debug"
$ReleaseBuildDir = Join-Path $ProjectRoot "out/build/windows-x64-release"
$ExeName = "FuseVision.exe"

$VSBase = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
$VcVars64 = Join-Path $VSBase "VC\Auxiliary\Build\vcvars64.bat"

function Initialize-VSEnv {
    if (-not (Test-Path $VcVars64)) {
        Write-Host "错误: 未找到 VS 环境脚本: $VcVars64" -ForegroundColor Red
        exit 1
    }
    Write-Host "初始化 VS x64 编译环境..." -ForegroundColor Cyan
    
    $tempFile = [System.IO.Path]::GetTempFileName()
    try {
        cmd /c "`"$VcVars64`" > nul 2>&1 && set > `"$tempFile`""
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
        Write-Host "警告: 未找到 x64 编译器路径" -ForegroundColor Yellow
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
    
    Initialize-VSEnv
    
    if ($Clean -and (Test-Path $BuildDir)) {
        Write-Host "清理: $BuildDir" -ForegroundColor Yellow
        Remove-Item -Path $BuildDir -Recurse -Force -ErrorAction SilentlyContinue
        $rootCache = Join-Path $ProjectRoot "CMakeCache.txt"
        if (Test-Path $rootCache) { Remove-Item $rootCache -Force }
    }
    
    $configPreset = "windows-x64-$($BuildType.ToLower())"
    $buildPreset = "windows-$($BuildType.ToLower())"
    
    Write-Host "配置: $configPreset" -ForegroundColor Cyan
    cmake --preset $configPreset
    if ($LASTEXITCODE -ne 0) { Write-Host "配置失败" -ForegroundColor Red; return }
    
    Write-Host "构建: $buildPreset" -ForegroundColor Cyan
    cmake --build --preset $buildPreset
    if ($LASTEXITCODE -ne 0) { Write-Host "构建失败" -ForegroundColor Red; return }
    
    $exePath = Join-Path $BuildDir "bin\$BuildType\$ExeName"
    if (-not (Test-Path $exePath)) {
        Write-Host "未找到可执行文件: $exePath" -ForegroundColor Red; return
    }
    
    Write-Host "运行: $BuildType" -ForegroundColor Cyan
    & $exePath
}

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
