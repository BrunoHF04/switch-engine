# =============================================================================
# install-to-sd.ps1
#
# Copia o overlay (switch-engine.ovl) e o sysmod (sysmod/exefs.nsp +
# switch-engine-mod.npdm + toolbox.json) para o SD card do Switch nas pastas
# corretas:
#
#   <SD>/switch/.overlays/switch-engine.ovl
#   <SD>/atmosphere/contents/420000000053454E/exefs.nsp
#   <SD>/atmosphere/contents/420000000053454E/toolbox.json
#   <SD>/atmosphere/contents/420000000053454E/flags/boot2.flag
#
# A maior fonte de "overlay invisivel no menu Tesla/Ultrahand" e' justamente
# copiar para o lugar errado (faltando o ponto em ".overlays", colocando em
# "/switch/" raiz, etc). Esse script garante a estrutura.
#
# Uso:
#   . .\scripts\install-to-sd.ps1 -SdRoot D:\
#   . .\scripts\install-to-sd.ps1 -SdRoot E:\ -OverlayOnly
#   . .\scripts\install-to-sd.ps1 -SdRoot E:\ -SysmodOnly
#
# Com -DryRun nao copia, so imprime o que faria.
# =============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$SdRoot,

    [switch]$OverlayOnly,
    [switch]$SysmodOnly,
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$RepoRoot   = Split-Path -Parent $PSScriptRoot
$OvlPath    = Join-Path $RepoRoot "switch-engine.ovl"
$SysmodDir  = Join-Path $RepoRoot "sysmod"
$NsoPath    = Join-Path $SysmodDir "switch-engine-mod.nso"
$NspPath    = Join-Path $SysmodDir "exefs.nsp"
$NpdmPath   = Join-Path $SysmodDir "build\switch-engine-mod.npdm"
$ToolboxPath= Join-Path $SysmodDir "toolbox.json"

$Tid = "420000000053454E"

if (-not (Test-Path $SdRoot)) {
    throw "SdRoot '$SdRoot' nao existe. Aponte para a raiz do SD (ex: D:\)."
}

function Copy-Verbose {
    param([string]$Src, [string]$Dst)
    if (-not (Test-Path $Src)) {
        Write-Warning "FONTE NAO EXISTE: $Src (rode 'make' antes)"
        return
    }
    $dstDir = Split-Path -Parent $Dst
    if (-not (Test-Path $dstDir)) {
        if ($DryRun) { Write-Host "[dry] mkdir $dstDir" }
        else        { New-Item -ItemType Directory -Path $dstDir -Force | Out-Null }
    }
    $size = (Get-Item $Src).Length
    Write-Host ("  {0,-50} -> {1}  ({2} bytes)" -f (Split-Path -Leaf $Src), $Dst, $size)
    if (-not $DryRun) {
        Copy-Item -Path $Src -Destination $Dst -Force
    }
}

# -----------------------------------------------------------------------------
# Overlay
# -----------------------------------------------------------------------------
if (-not $SysmodOnly) {
    Write-Host "`n[overlay]" -ForegroundColor Cyan
    $overlayDir = Join-Path $SdRoot "switch\.overlays"
    Copy-Verbose -Src $OvlPath -Dst (Join-Path $overlayDir "switch-engine.ovl")
}

# -----------------------------------------------------------------------------
# Sysmod
# -----------------------------------------------------------------------------
if (-not $OverlayOnly) {
    Write-Host "`n[sysmod] TID=$Tid" -ForegroundColor Cyan
    $sysmodDst = Join-Path $SdRoot "atmosphere\contents\$Tid"
    Copy-Verbose -Src $NspPath     -Dst (Join-Path $sysmodDst "exefs.nsp")
    Copy-Verbose -Src $ToolboxPath -Dst (Join-Path $sysmodDst "toolbox.json")

    # boot2.flag faz o sysmod auto-iniciar no boot.
    $flagsDir = Join-Path $sysmodDst "flags"
    $flagFile = Join-Path $flagsDir "boot2.flag"
    if (-not (Test-Path $flagFile)) {
        if ($DryRun) {
            Write-Host "  [dry] criar $flagFile"
        } else {
            New-Item -ItemType Directory -Path $flagsDir -Force | Out-Null
            New-Item -ItemType File      -Path $flagFile -Force | Out-Null
            Write-Host "  flags\boot2.flag                                  -> $flagFile"
        }
    } else {
        Write-Host "  flags\boot2.flag (ja existe)"
    }
}

# -----------------------------------------------------------------------------
# Resumo + instrucoes
# -----------------------------------------------------------------------------
Write-Host "`n[ok] copia concluida." -ForegroundColor Green
Write-Host "Caminhos esperados no console:"
Write-Host "  sdmc:/switch/.overlays/switch-engine.ovl"
Write-Host "  sdmc:/atmosphere/contents/$Tid/exefs.nsp"
Write-Host "  sdmc:/atmosphere/contents/$Tid/flags/boot2.flag"
Write-Host ""
Write-Host "Apos plugar o SD: REINICIE o console (o sysmod so' inicia no boot)."
Write-Host "No primeiro uso do overlay, o sdmc:/switch-engine.log sera criado"
Write-Host "automaticamente -- copie ele de volta pra ca para diagnostico."
