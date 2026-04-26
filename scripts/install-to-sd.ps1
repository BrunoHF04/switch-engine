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
#   . .\scripts\install-to-sd.ps1 -SdRoot E:\ -CleanLogs
#
# -CleanLogs: apaga sdmc:/switch-engine.log e sdmc:/switch-engine_debug.log
#             E o .ovl antigo em sdmc:/switch/.overlays/ ANTES de copiar.
#             Use isso quando estiver desconfiado de "build velho no SD".
# -DryRun: nao copia, so imprime o que faria.
# =============================================================================

[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$SdRoot,

    [switch]$OverlayOnly,
    [switch]$SysmodOnly,
    [switch]$CleanLogs,
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
# Cleanup opcional (logs antigos + .ovl antigo)
# -----------------------------------------------------------------------------
if ($CleanLogs) {
    Write-Host "`n[clean]" -ForegroundColor Yellow
    $toDelete = @(
        (Join-Path $SdRoot "switch-engine.log"),
        (Join-Path $SdRoot "switch-engine_debug.log"),
        (Join-Path $SdRoot "switch-engine_mod.log"),
        (Join-Path $SdRoot "switch-engine_mod_crash.log"),
        (Join-Path $SdRoot "switch\.overlays\switch-engine.ovl")
    )
    foreach ($p in $toDelete) {
        if (Test-Path $p) {
            $sz = (Get-Item $p).Length
            Write-Host ("  remove {0,-50}  ({1} bytes)" -f $p, $sz)
            if (-not $DryRun) { Remove-Item -Path $p -Force }
        } else {
            Write-Host "  (nao existe) $p"
        }
    }
    # Crash reports antigos do Atmosphere atrapalham o diagnostico do
    # proximo crash; limpa todos os do nosso TID.
    $crashDir = Join-Path $SdRoot "atmosphere\crash_reports"
    if (Test-Path $crashDir) {
        $tidLower = "420000000053454e"
        Get-ChildItem $crashDir -Filter "*_$tidLower.*" -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host ("  remove {0,-50}  ({1} bytes)" -f $_.FullName, $_.Length)
            if (-not $DryRun) { Remove-Item -Path $_.FullName -Force }
        }
    }
}

# -----------------------------------------------------------------------------
# Overlay
# -----------------------------------------------------------------------------
if (-not $SysmodOnly) {
    Write-Host "`n[overlay]" -ForegroundColor Cyan
    $overlayDir   = Join-Path $SdRoot "switch\.overlays"
    $overlayDest  = Join-Path $overlayDir "switch-engine.ovl"

    # Compara tamanho atual no SD com o que vamos copiar (para detectar build velho).
    if ((Test-Path $OvlPath) -and (Test-Path $overlayDest)) {
        $srcSize = (Get-Item $OvlPath).Length
        $dstSize = (Get-Item $overlayDest).Length
        if ($srcSize -eq $dstSize) {
            Write-Host ("  [warn] tamanho identico no SD ({0} bytes). Verifique se voce rodou 'make' apos editar o codigo." -f $srcSize) -ForegroundColor Yellow
        } else {
            Write-Host ("  [info] substituindo .ovl: SD={0} bytes -> novo={1} bytes" -f $dstSize, $srcSize) -ForegroundColor Green
        }
    }

    Copy-Verbose -Src $OvlPath -Dst $overlayDest
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
