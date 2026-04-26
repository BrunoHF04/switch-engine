# Switch Engine

Memory Scanner para Nintendo Switch dividido em dois componentes:

- **switch-engine.ovl** &mdash; Tesla Overlay (UI), em `./Makefile`.
- **switch-engine-mod.nsp** &mdash; Atmosphere SysModule com capabilities de debug, em `./sysmod/`.

```
+---------------------------+         +---------------------------+
|  switch-engine.ovl        |  IPC    |  switch-engine-mod.nsp    |
|  (Tesla Overlay)          | <-----> |  (Atmosphere SysModule)   |
|  - libtesla UI            |  seng:  |  - svcDebugActiveProcess  |
|  - SengClient (cmif)      |         |  - svcReadDebugProcMemory |
|  - ResultsStore (sdmc)    |         |  - svcWriteDebugProcMem   |
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
|- Makefile                    # builda o overlay (.ovl)
|- README.md
|- include/
|  |- seng_ipc.hpp             # contrato IPC (compartilhado overlay+sysmod)
|- source/                     # codigo do OVERLAY
|  |- main.cpp
|  |- Overlay.hpp / .cpp
|  |- gui/
|  |  |- MainGui.hpp / .cpp
|  |- scanner/
|     |- MemoryScanner.hpp / .cpp
|     |- ProcessUtils.hpp / .cpp     # delega tudo p/ SengClient
|     |- ResultsStore.hpp / .cpp
|     |- SengClient.hpp / .cpp        # cmif client p/ servico "seng"
|- sysmod/                     # codigo do SYSMODULE
|  |- Makefile                       # gera exefs.nsp
|  |- switch-engine-mod.json         # NPDM (capabilities de debug)
|  |- toolbox.json                   # metadata p/ sysmod-manager
|  |- source/
|     |- main.cpp                    # __appInit, heap, server loop
|     |- Debugger.hpp / .cpp         # wrapper svcDebug*
|     |- IpcServer.hpp / .cpp        # cmif server (commands 0-7)
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
- `npdmtool`, `elf2nso`, `build_pfs0` (ja vem com o `switch-tools` do devkitPro).
- libtesla em `lib/libtesla` (submodulo Git).

```bash
git submodule add https://github.com/WerWolv/libtesla lib/libtesla

# 1) Sysmod (gera sysmod/exefs.nsp)
make -C sysmod

# 2) Overlay (gera switch-engine.ovl)
make
```

## Instalacao no console

```bash
# Caminho do SD card (ajuste para sua maquina)
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
4. "Detect foreground" -> mostra `TID xxxxxxxxxxxxxxxx` -> sysmod respondeu OK.
5. "First Scan" -> deve gerar `sdmc:/switch/switch-engine/results.bin`.

Se "Detect foreground" mostrar `error`, o sysmod nao esta rodando. Cheque:

```bash
# No PC, depois do reboot, com SD montado:
ls $SD/atmosphere/logs/                       # logs de crash do sysmod
cat $SD/atmosphere/contents/420000000053454E/flags/boot2.flag    # deve existir
```

## Contrato IPC (servico `seng`)

| Cmd | Nome              | In                | Out / Buffer        |
|---:-|-------------------|-------------------|---------------------|
| 0   | GetVersion        | -                 | u32 version         |
| 1   | GetForegroundPid  | -                 | u64 pid             |
| 2   | GetTitleId        | u64 pid           | u64 tid             |
| 3   | AttachProcess     | u64 pid           | -                   |
| 4   | DetachProcess     | -                 | -                   |
| 5   | QueryMemory       | u64 addr          | MemoryRegion (32 B) |
| 6   | ReadMemory        | u64 addr, u64 sz  | Type-B out buffer   |
| 7   | WriteMemory       | u64 addr, u64 sz  | Type-A in  buffer   |
| 8   | IsAttached        | -                 | u8                  |

Buffers limitados a `seng::kMaxChunkBytes = 64 KB` por chamada.

## Proximos passos

- [ ] UI: input numerico para o valor de busca, picker de tipo (u8/u16/u32/u64/f32/f64).
- [ ] UI: lista scrollavel de resultados via `ResultsStore::readPage()`.
- [ ] UI: editor de valor inline (chama `SengClient::writeMemory`).
- [ ] Scanner: comparadores `>=`, `<=`, `between`, `changed`, `unchanged`.
- [ ] Scanner: thread separada para nao travar redraw do Tesla.
- [ ] Sysmod: log circular em `sdmc:/switch/switch-engine/sysmod.log` p/ debugar.
