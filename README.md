# Switch Engine

Memory Scanner para Nintendo Switch dividido em dois componentes:

- **switch-engine.ovl** &mdash; Tesla Overlay (UI), em `./Makefile`.
- **switch-engine-mod.nsp** &mdash; Atmosphere SysModule com capabilities de debug, em `./sysmod/`.

```
+---------------------------+         +---------------------------+
|  switch-engine.ovl        |  IPC    |  switch-engine-mod.nsp    |
|  (Tesla Overlay)          | <-----> |  (Atmosphere SysModule)   |
|  - libtesla UI            |  seng:  |  - svcDebugActiveProcess  |
|  - i18n EN / PT-BR        |         |  - svcReadDebugProcMemory |
|  - SengClient (cmif)      |         |  - svcWriteDebugProcMem   |
|  - ResultsStore (sdmc)    |         |  - svcGetProcessList      |
|  - ProcessListGui         |         |  - sysmod log + crash log |
+---------------------------+         +---------------------------+
                                              |
                                              v
                                       sdmc:/atmosphere/contents/
                                         420000000053454E/
                                           exefs.nsp
                                           toolbox.json
                                           flags/boot2.flag
```

Por que dois componentes? Tesla overlays sao carregados pelo `nx-ovlloader` e
**herdam** as capabilities do loader, que **nao** liberam SVCs de debug. A
unica forma legitima de chamar `svcDebugActiveProcess` e amigos e ter um NPDM
proprio &mdash; o que so o sysmod tem.

## Estrutura

```
switch-engine/
|- Makefile                    # builda o overlay (.ovl via elf2nro)
|- README.md
|- include/
|  |- seng_ipc.hpp             # contrato IPC (compartilhado overlay+sysmod)
|- scripts/
|  |- install-to-sd.ps1        # PowerShell deploy (overlay + sysmod, com -CleanLogs)
|- source/                     # codigo do OVERLAY
|  |- main.cpp                 # __appInit, fsdevMountSdmc, i18n::load, tsl::loop
|  |- Overlay.hpp / .cpp       # frame wrapper, dispatcher inicial
|  |- gui/
|  |  |- MainGui.hpp / .cpp           # tela principal (categorias + status)
|  |  |- NumericInputGui.hpp / .cpp   # keypad numerico
|  |  |- ResultsListGui.hpp / .cpp    # paginacao + poke value
|  |  |- LanguageGui.hpp / .cpp       # picker EN / PT-BR
|  |  |- ProcessListGui.hpp / .cpp    # picker manual de processo
|  |- scanner/
|  |  |- MemoryScanner.hpp / .cpp
|  |  |- ProcessUtils.hpp / .cpp      # delega tudo p/ SengClient
|  |  |- ResultsStore.hpp / .cpp
|  |  |- SengClient.hpp / .cpp        # cmif client p/ servico "seng"
|  |- util/
|     |- Logger.hpp / .cpp            # log persistente sdmc:/switch/switch-engine/log.txt
|     |- Language.hpp / .cpp          # i18n (EN / PT-BR), config.ini
|- sysmod/                     # codigo do SYSMODULE
   |- Makefile                       # gera exefs.nsp, injeta SENG_BUILD_TAG
   |- switch-engine-mod.json         # NPDM (capabilities de debug)
   |- toolbox.json                   # metadata p/ sysmod-manager
   |- source/
      |- main.cpp                    # __appInit, heap, server loop, exception handler
      |- Debugger.hpp / .cpp         # wrapper svcDebug*, listProcesses
      |- IpcServer.hpp / .cpp        # cmif server (commands 0-9)
      |- SysmodLog.hpp / .cpp        # log persistente + crash log com build tag
```

## Title ID e configuracao do Atmosphere

O sysmod usa o TID **`0x420000000053454E`** (faixa de homebrew, terminado em
`SEN` em ASCII para "Switch ENgine"). Esse mesmo TID aparece em 3 lugares e
**precisa ser identico nos tres**:

1. `sysmod/switch-engine-mod.json` -> `title_id` / `title_id_range_min/max`.
2. `sysmod/Makefile` -> `TARGET_TID := 420000000053454E`.
3. `include/seng_ipc.hpp` -> `kSysmodTitleId`.

O Atmosphere reconhece um sysmod homebrew quando ele aparece em:

```
sdmc:/atmosphere/contents/<TID>/
    exefs.nsp                 # binario do sysmod (gerado pelo Makefile)
    toolbox.json              # opcional, p/ sysmod-manager listar
    flags/
        boot2.flag            # ARQUIVO VAZIO; presenca = autostart no boot
```

Sem `flags/boot2.flag` o sysmod nao roda. Com ele, o Atmosphere o inicia logo
apos o `boot2` do firmware (antes mesmo do menu HOME aparecer), e nesse ponto
ele ja registra o servico `seng` em `sm:`.

Voce pode trocar o TID se ja tiver outro sysmod usando esse. Use a faixa
`0x420000xxxxxxxxxx` ou `0x430000xxxxxxxxxx` para nao colidir com sysmods
oficiais nem outros homebrew populares (sys-clk, sys-ftpd, sys-botbase).

## Build

Pre-requisitos:

- devkitPro com `switch-dev` instalado (`pacman -S switch-dev`).
- `npdmtool`, `elf2nro`, `build_pfs0` (ja vem com o `switch-tools` do devkitPro).
- libtesla em `lib/libtesla` (submodulo Git).

```bash
git submodule add https://github.com/WerWolv/libtesla lib/libtesla

# 1) Sysmod (gera sysmod/exefs.nsp)
make -C sysmod

# 2) Overlay (gera switch-engine.ovl como NRO + NACP)
make
```

**Nota sobre formato de overlay:** o `nx-ovlloader+` espera **NRO** (com NACP
embutido), nao NSO. O Makefile usa `elf2nro` + `NROFLAGS` para gerar o `.ovl`
corretamente; tentar carregar um NSO renomeado faz o overlay sumir
silenciosamente do menu Tesla.

**Build tag do sysmod:** o Makefile do sysmod injeta `SENG_BUILD_TAG=<epoch>`
em cada compilacao. Esse tag aparece no header de
`sdmc:/switch-engine_mod.log` para confirmar qual binario esta rodando
(util quando o sysmod parece nao recarregar apos uma atualizacao).

## Instalacao no console

Via script PowerShell (Windows):

```powershell
# Copia overlay + sysmod e limpa logs antigos da TID 420000000053454E
.\scripts\install-to-sd.ps1 -SDDrive F: -CleanLogs
```

Via shell manual:

```bash
SD=/run/media/$USER/SDCARD

# 1) Sysmod
mkdir -p $SD/atmosphere/contents/420000000053454E/flags
cp sysmod/exefs.nsp     $SD/atmosphere/contents/420000000053454E/exefs.nsp
cp sysmod/toolbox.json  $SD/atmosphere/contents/420000000053454E/toolbox.json
touch                   $SD/atmosphere/contents/420000000053454E/flags/boot2.flag

# 2) Overlay
mkdir -p $SD/switch/.overlays
cp switch-engine.ovl    $SD/switch/.overlays/

# 3) Reboot necessario para o boot2 carregar o sysmod pela primeira vez.
```

Ou simplesmente: `make -C sysmod install SDMOUNT=$SD`.

## Verificacao

Apos reboot:

1. Abra um jogo qualquer.
2. Pressione `L + dpad-down + RS` (atalho default do Tesla-Menu).
3. Selecione "Switch Engine".
4. **"Pick process..."** -> escolha o jogo manualmente, ou **"Detect foreground"**
   -> sysmod tenta `pmdmntGetApplicationProcessId`.
5. "First Scan" -> deve gerar `sdmc:/switch/switch-engine/results.bin`.

## Idioma

A tela principal tem uma categoria **Settings -> Language** com EN / PT-BR.
A escolha e gravada em `sdmc:/switch/switch-engine/config.ini` e carregada no
proximo open do overlay (`seng::i18n::load()` em `main.cpp`). Reabrir o
overlay e necessario para os widgets ja construidos pegarem as novas strings.

Tabela de strings em `source/util/Language.cpp` (`kStrings[][2]`); para
adicionar um idioma novo, estenda `enum class Lang`, amplie a tabela para
`Lang::Count` colunas e atualize `parseLangCode` / `currentCode`.

## Logs

| Arquivo | Origem | Propossito |
|---------|--------|----------|
| `sdmc:/switch/switch-engine/log.txt` | Overlay (`util/Logger`) | Eventos da UI, IPC client, scan |
| `sdmc:/switch-engine_mod.log` | Sysmod (`SysmodLog`) | Boot do sysmod, IPC server, comandos atendidos |
| `sdmc:/switch-engine_mod_crash.log` | Sysmod (handler) | Contexto de crash (PC, LR, registers, build tag) |
| `sdmc:/atmosphere/crash_reports/` | Atmosphere | Crash dumps do firmware (cobre o sysmod tambem) |

Os logs do sysmod incluem `SENG_BUILD_TAG=<epoch>` no header para garantir
que voce esta lendo do binario certo apos um redeploy.

## Diagnostico rapido

Se "Detect foreground" mostrar `error` ou `pmdmnt` retornar PID 1 (kernel),
use **"Pick process..."** &mdash; o overlay chama
`SengClient::listProcesses()` que internamente faz `svcGetProcessList` no
sysmod e retorna ate 64 entradas `{pid, tid}`. Cada entrada e classificada
em "App" / "Sysmod" / "Process" pela faixa do TID.

Se a lista vier vazia ou so com `PID 0`, o sysmod **nao esta rodando**.
Cheque na ordem:

```bash
# 1) flag de boot2 existe?
ls $SD/atmosphere/contents/420000000053454E/flags/boot2.flag

# 2) hash do exefs.nsp bate com o build local?
sha256sum sysmod/exefs.nsp \
  $SD/atmosphere/contents/420000000053454E/exefs.nsp

# 3) sysmod logou alguma coisa?
head -30 $SD/switch-engine_mod.log

# 4) crash dumps do firmware?
ls $SD/atmosphere/crash_reports/
```

Sysmods so sao carregados em **boot completo** &mdash; sleep / wake do
console nao recarrega. Power-cycle obrigatorio depois de atualizar o
`exefs.nsp`.

## Contrato IPC (servico `seng`)

| Cmd | Nome              | In                | Out / Buffer        |
|----:|-------------------|-------------------|---------------------|
| 0   | GetVersion        | -                 | u32 version         |
| 1   | GetForegroundPid  | -                 | u64 pid             |
| 2   | GetTitleId        | u64 pid           | u64 tid             |
| 3   | AttachProcess     | u64 pid           | -                   |
| 4   | DetachProcess     | -                 | -                   |
| 5   | QueryMemory       | u64 addr          | MemoryRegion (32 B) |
| 6   | ReadMemory        | u64 addr, u64 sz  | Type-B out buffer   |
| 7   | WriteMemory       | u64 addr, u64 sz  | Type-A in  buffer   |
| 8   | IsAttached        | -                 | u8                  |
| 9   | ListProcesses     | u64 max           | u64 count + Type-B  |

Buffers limitados a `seng::kMaxChunkBytes = 64 KB` por chamada.
`ListProcesses` devolve ate `seng::kMaxProcessList = 64` entradas
`{u64 pid, u64 tid}` (16 B cada).

## Proximos passos

- [ ] Scanner: comparadores `>=`, `<=`, `between`, `changed`, `unchanged`.
- [ ] Scanner: thread separada para nao travar redraw do Tesla.
- [ ] UI: picker de tipo (u8/u16/u32/u64/f32/f64) na busca e no poke.
- [ ] UI: marcar processo "atual" na ProcessListGui com `[*]`.
- [ ] Sysmod: investigar `pmdmntGetApplicationProcessId` retornando PID 1
  em alguns titulos / firmwares (logs ja instrumentados).
- [ ] Sysmod: comando `ReadMemoryRange(start, end, stride)` para acelerar
  first-scan reduzindo ida-volta IPC.
