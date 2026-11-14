<#
.SYNOPSIS
    Run clang-tidy over CraftLang source files.

.DESCRIPTION
    Reads build/compile_commands.json, selects translation units under src/,
    and runs clang-tidy with the repository's .clang-tidy configuration.

    By default warnings are treated as errors, so the script exits with 1 when
    any lint finding exists. This script never applies fixes automatically.

.EXAMPLE
    pwsh -NoProfile -File tools/lint.ps1

.EXAMPLE
    pwsh -NoProfile -File tools/lint.ps1 -NoWarningsAsErrors
#>
#requires -Version 7.0

[CmdletBinding()]
param(
    [string]$BuildDir = 'build',
    [string]$ClangTidy = '',
    [string]$SourceRoot = 'src',
    [switch]$NoWarningsAsErrors,
    [switch]$Quiet
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-RepoPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $script:RepoRoot $Path))
}

function Convert-ToComparePath {
    param([Parameter(Mandatory = $true)][string]$Path)
    return [System.IO.Path]::GetFullPath($Path).Replace('\', '/')
}

try {
    $script:RepoRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
    $buildPath = Resolve-RepoPath $BuildDir
    $compileCommandsPath = Join-Path $buildPath 'compile_commands.json'

    if (-not (Test-Path -LiteralPath $compileCommandsPath -PathType Leaf)) {
        throw "找不到 compile_commands.json：$compileCommandsPath`n请先运行 cmake 配置和构建。"
    }

    # 优先使用与 CMake 编译器同目录的 clang-tidy，保证能找到 llvm-mingw 的 libc++ 头文件。
    if ([string]::IsNullOrWhiteSpace($ClangTidy)) {
        $cachePath = Join-Path $buildPath 'CMakeCache.txt'
        if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
            $compilerLine = Get-Content -LiteralPath $cachePath |
                Select-String -Pattern '^CMAKE_CXX_COMPILER:FILEPATH=' |
                Select-Object -First 1

            if ($null -ne $compilerLine) {
                $compilerPath = ($compilerLine.Line -split '=', 2)[1]
                $candidate = Join-Path (Split-Path -Parent $compilerPath) 'clang-tidy.exe'
                if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                    $ClangTidy = $candidate
                }
            }
        }

        if ([string]::IsNullOrWhiteSpace($ClangTidy)) {
            $command = Get-Command clang-tidy -ErrorAction SilentlyContinue
            if ($null -ne $command) {
                $ClangTidy = $command.Source
            }
        }
    }

    if ([string]::IsNullOrWhiteSpace($ClangTidy) -or -not (Test-Path -LiteralPath $ClangTidy -PathType Leaf)) {
        throw '找不到 clang-tidy。请通过 -ClangTidy 指定路径，或把 clang-tidy 加入 PATH。'
    }

    $commands = Get-Content -Raw -LiteralPath $compileCommandsPath | ConvertFrom-Json
    $sourceRootCompare = (Convert-ToComparePath (Resolve-RepoPath $SourceRoot)).TrimEnd('/') + '/'

    $files = @(
        $commands |
            ForEach-Object { [string]$_.file } |
            Where-Object {
                (Convert-ToComparePath $_).StartsWith($sourceRootCompare, [System.StringComparison]::OrdinalIgnoreCase)
            } |
            Sort-Object -Unique
    )

    if ($files.Count -eq 0) {
        throw "compile_commands.json 中没有 $SourceRoot 下的源文件。"
    }

    if (-not $Quiet) {
        Write-Host "clang-tidy：$ClangTidy"
        Write-Host "compile_commands：$compileCommandsPath"
        Write-Host "源文件：$($files.Count) 个"
        Write-Host ""
    }

    $failedFiles = 0
    $totalFindings = 0

    Push-Location $script:RepoRoot
    try {
        foreach ($file in $files) {
            $relative = $file.Replace($script:RepoRoot + '\', '').Replace('\', '/')
            if (-not $Quiet) {
                Write-Host "==> $relative" -ForegroundColor Cyan
            }

            $arguments = @('-p', $buildPath, $file, '--quiet')
            if (-not $NoWarningsAsErrors) {
                $arguments += '--warnings-as-errors=*'
            }

            $output = & $ClangTidy @arguments 2>&1
            $exitCode = $LASTEXITCODE
            $text = ($output | Out-String).TrimEnd()
            $findings = @($output | Select-String -Pattern ' (warning|error): ')

            $totalFindings += $findings.Count
            if ($exitCode -ne 0 -or $findings.Count -gt 0) {
                if (-not [string]::IsNullOrWhiteSpace($text)) {
                    Write-Host $text
                }
            }

            if ($exitCode -ne 0 -or ($findings.Count -gt 0 -and -not $NoWarningsAsErrors)) {
                $failedFiles++
            }
            elseif (-not $Quiet) {
                Write-Host "    通过" -ForegroundColor Green
            }
        }
    }
    finally {
        Pop-Location
    }

    Write-Host ""
    if ($failedFiles -gt 0) {
        Write-Host "lint 失败：$failedFiles 个文件，$totalFindings 条诊断" -ForegroundColor Red
        exit 1
    }

    if ($totalFindings -gt 0) {
        Write-Host "lint 通过：$($files.Count) 个文件，$totalFindings 条诊断（未按错误处理）" -ForegroundColor Yellow
    }
    else {
        Write-Host "lint 通过：$($files.Count) 个文件，无诊断" -ForegroundColor Green
    }
    exit 0
}
catch {
    Write-Host ""
    Write-Host "lint 执行失败：" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
