$ErrorActionPreference = 'Stop'
$taskRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$buildRoot = "$taskRoot/out/build/windows-x64-release"
$probeRoot = "$taskRoot/out/current-audit-20261117"
New-Item -ItemType Directory -Path $probeRoot -Force | Out-Null
$vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
$vsInstallation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vcvars = Join-Path $vsInstallation 'VC/Auxiliary/Build/vcvars64.bat'
$envFile = Join-Path $probeRoot 'compiler-environment.txt'
cmd /c "`"$vcvars`" > nul 2>&1 && set > `"$envFile`""
if ($LASTEXITCODE -ne 0) { throw 'VS environment initialization failed' }
Get-Content -LiteralPath $envFile | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}
Remove-Item -LiteralPath $envFile
$dependencies = "$buildRoot/vcpkg_installed/x64-windows"
$includes = @("$taskRoot/src", "$dependencies/include/Qt6", "$dependencies/include/Qt6/QtCore", "$dependencies/include/Qt6/QtGui", "$dependencies/include/Qt6/QtWidgets", "$dependencies/include/Qt6/QtSql", "$dependencies/share/Qt6/mkspecs/win32-msvc")
$arguments = @('/nologo', '/std:c++20', '/MD', '/EHsc', '/utf-8', '/Zc:__cplusplus', '/permissive-', '/DQT_NO_DEBUG', '/DQT_CORE_LIB', '/DQT_GUI_LIB', '/DQT_WIDGETS_LIB', '/DQT_SQL_LIB')
$arguments += $includes | ForEach-Object { "/I$_" }
$arguments += @("$taskRoot/docs/logic/evidence/current-audit-20261117-probe.cpp", "/Fo$probeRoot/probe.obj", "/Fe$probeRoot/probe.exe", '/link', "/LIBPATH:$buildRoot/lib/Release", "/LIBPATH:$dependencies/lib")
$arguments += @('FuseVisionDeepLearning.lib', 'FuseVisionDeepLearningData.lib', 'FuseVisionUiCommon.lib', 'FuseVisionCore.lib', 'Qt6Widgets.lib', 'Qt6Gui.lib', 'Qt6Core.lib', 'Qt6Sql.lib', 'spdlog.lib', 'fmt.lib', 'user32.lib', 'gdi32.lib', 'shell32.lib', 'ole32.lib', 'advapi32.lib', 'dwmapi.lib')
& cl.exe @arguments
if ($LASTEXITCODE -ne 0) { throw 'Audit probe compilation failed' }
$env:PATH = "$buildRoot/bin/Release;$env:PATH"
$env:QT_QPA_PLATFORM = 'offscreen'
$env:QT_PLUGIN_PATH = "$buildRoot/bin/Release"
& "$probeRoot/probe.exe"
exit $LASTEXITCODE
