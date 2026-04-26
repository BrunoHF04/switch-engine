# Configura DEVKITPRO + PATH para a sessao atual do PowerShell.
# Uso: . .\scripts\setup-devkitpro.ps1   (dot-source para herdar as variaveis)

$DevkitPro = "C:\devkitPro"
if (-not (Test-Path $DevkitPro)) {
    Write-Error "devkitPro nao encontrado em $DevkitPro. Instale via https://devkitpro.org/wiki/Getting_Started"
    return
}

$env:DEVKITPRO = $DevkitPro -replace '\\','/'
$env:DEVKITA64 = "$($env:DEVKITPRO)/devkitA64"
$env:DEVKITARM = "$($env:DEVKITPRO)/devkitARM"

# Ordem importa: msys2/usr/bin tem make, sed, awk, etc.
$env:PATH = "$DevkitPro\msys2\usr\bin;$DevkitPro\devkitA64\bin;$DevkitPro\tools\bin;$env:PATH"

Write-Host "DEVKITPRO = $env:DEVKITPRO"
Write-Host "make:      $((Get-Command make -ErrorAction SilentlyContinue).Source)"
Write-Host "elf2nso:   $((Get-Command elf2nso -ErrorAction SilentlyContinue).Source)"
Write-Host "npdmtool:  $((Get-Command npdmtool -ErrorAction SilentlyContinue).Source)"
