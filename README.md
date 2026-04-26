# Switch Engine

Memory Scanner para Nintendo Switch como **Tesla Overlay**, usando `libnx` + `libtesla`.
Resultados sao gravados de forma paginada em `sdmc:/switch/switch-engine/results.bin`
para nao estourar o heap reduzido do overlay.

## Status

Esqueleto inicial. Ja temos:

- Estrutura de projeto compilavel pelo `Makefile` do devkitPro.
- `SwitchEngineOverlay : tsl::Overlay` como root.
- `MainGui : tsl::Gui` com botoes (Detect Target / First Scan / Next Scan / Reset).
- `MemoryScanner` envolvendo `svcQueryDebugProcessMemory` e `svcReadDebugProcessMemory`.
- `ProcessUtils` para obter PID + TitleID do `Application` em primeiro plano.
- `ResultsStore` para persistencia paginada no SD.

## Layout

```
switch-engine/
|- Makefile
|- README.md
|- source/
|  |- main.cpp                 # entry point, define TESLA_INIT_IMPL
|  |- Overlay.hpp / .cpp       # tsl::Overlay raiz
|  |- gui/
|  |  |- MainGui.hpp / .cpp    # tela inicial
|  |- scanner/
|     |- MemoryScanner.hpp / .cpp
|     |- ProcessUtils.hpp / .cpp
|     |- ResultsStore.hpp / .cpp
|- lib/
   |- libtesla/                # adicionar como submodulo
```

## Build

Pre-requisitos:

- devkitPro com `switch-dev` instalado.
- libtesla em `lib/libtesla` (submodulo Git).

```bash
git submodule add https://github.com/WerWolv/libtesla lib/libtesla
make
```

A saida e `switch-engine.ovl`. Copie para `sdmc:/switch/.overlays/`.

## Importante: capabilities de kernel

`svcReadDebugProcessMemory`, `svcDebugActiveProcess`, `svcQueryDebugProcessMemory`
**so funcionam se o NPDM do processo chamador autorizar esses SVCs**.

Tesla overlays sao carregados pelo `ovlloader`/`Tesla-Menu` e **herdam** as
capabilities do loader. O loader padrao **nao** concede SVCs de debug.

Voce tem 3 caminhos:

1. **Patch o `ovlloader`** para incluir os SVCs 0x60, 0x65, 0x69, 0x6A, 0x6E, 0x6F
   no NPDM. Funciona, mas afeta TODOS os overlays.

2. **Hibrido (recomendado)** -> mantenha o overlay so como UI e crie um SysModule
   companion (`switch-engine-mod`) com NPDM proprio que tenha os SVCs. O overlay
   conversa com ele via servico IPC custom (ex: `seng:`). E como o EdiZon-SE
   funciona com o `dmnt:cht`.

3. **Reusar `dmnt:cht`** do Atmosphere -> nao chama SVCs de debug direto, usa
   o servico de cheat manager. Mais limitado mas funciona em overlay puro.

A camada `ProcessUtils` + `MemoryScanner` foi desenhada para ser portada para
caminho (2) sem alterar a UI: basta substituir as chamadas SVC por chamadas
IPC para o sysmod companion.

## Proximos passos

- [ ] Adicionar selecao de tipo de valor (u8/u16/u32/u64/f32/f64) na UI.
- [ ] Adicionar comparadores (>=, <=, between, changed, unchanged).
- [ ] Lista de resultados scrollavel (paginada via `ResultsStore::readPage`).
- [ ] Editor de valor inline (svcWriteDebugProcessMemory).
- [ ] Mover scan para thread separada com progresso na UI.
- [ ] Sysmod companion para uso real (ver secao acima).
