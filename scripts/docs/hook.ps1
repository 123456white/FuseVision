$ErrorActionPreference = "Stop"
$OutputEncoding = [System.Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)
$docPayload = [Console]::In.ReadToEnd()
$docPython = if ($env:FUSEVISION_DOCS_PYTHON) { $env:FUSEVISION_DOCS_PYTHON } else { "python" }
$docPayload | & $docPython -X utf8 (Join-Path $PSScriptRoot "docflow.py") hook
exit $LASTEXITCODE
