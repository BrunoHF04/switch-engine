# Switch Engine

`Switch Engine` e um scanner de memoria para Nintendo Switch dividido em dois componentes:

- **Overlay Tesla** (`switch-engine.ovl`): interface do usuario para selecionar processo, escanear, refinar resultados e editar valores em tempo real.
- **SysModule Atmosphere** (`switch-engine-mod.nsp`): backend privilegiado com acesso as SVCs de debug, expondo um servico IPC (`seng`) para o overlay.

Esse desenho em duas partes e necessario porque overlays Tesla nao possuem permissao para executar chamadas de debug diretamente no processo alvo.

## O que o programa faz

O Switch Engine permite:

- Detectar o processo do jogo automaticamente via PGL/pm ou selecionar manualmente pela lista de processos.
- Realizar **First Scan** por valor numerico (`u32`) em regioes de memoria R/W (heap, code mutable, mapped memory).
- Realizar **Next Scan** para refinar resultados com um novo valor.
- Navegar resultados paginados e aplicar **poke** (escrita de memoria) em enderecos encontrados — o valor muda em tempo real no jogo.
- Persistir resultados e configuracoes no SD para manter estado entre aberturas.

## Arquitetura

```
+---------------------------+         +---------------------------+
|  switch-engine.ovl        |  IPC    |  switch-engine-mod.nsp   |
|  (Tesla Overlay)          | <-----> |  (Atmosphere SysModule)  |
|  - UI (libtesla)          |  seng:  |  - svcDebugActiveProcess |
|  - fluxo de scan          |  CMIF   |  - svcReadDebugProcess   |
|  - resultados no SD       |  HIPC   |  - svcWriteDebugProcess  |
|  - i18n EN/PT-BR          |         |  - svcGetProcessList     |
+---------------------------+         +---------------------------+
```

### Fluxo IPC

O overlay usa `serviceDispatch*` (libnx) para enviar pedidos ao sysmod via protocolo CMIF/HIPC. O sysmod:

1. Recebe mensagens no TLS via `svcReplyAndReceive`.
2. Copia o frame IPC para buffer estatico (o TLS e reutilizado por qualquer chamada FS/log subsequente).
3. Distingue **Control commands** (ex.: `QueryPointerBufferSize`) de **Request commands** (servico `seng`).
4. Despacha para o handler adequado e escreve a resposta SFCO alinhada de volta no TLS.

### Ciclo de debug

Para evitar congelar o jogo, o sysmod segue o padrao **attach → opera → detach** em cada operacao:

- **Scan**: attach ao PID, percorre todas as regioes R/W, grava hits em `results.bin`, detach.
- **Poke**: attach ao PID, escreve o valor no endereco, detach imediato.

### Componentes principais

- `source/gui/`: telas do overlay (`MainGui`, `ProcessListGui`, `ResultsListGui`, `NumericInputGui`, `LanguageGui`).
- `source/scanner/`: cliente IPC (`SengClient`), scanner de memoria e armazenamento de resultados.
- `source/util/`: logger e sistema de idioma (EN/PT-BR).
- `sysmod/source/`: servidor IPC (`IpcServer`), camada de debug (`Debugger`), scan no sysmod (`ScanRunner`) e logs.
- `include/seng_ipc.hpp`: contrato IPC compartilhado entre overlay e sysmod.

## Tecnologias

- Linguagem principal: **C++20**.
- SDK/Ferramentas: **devkitPro** com `switch-dev` (libnx 4.x).
- UI: **libtesla** (submodulo em `lib/libtesla`).
- Ambiente alvo: Nintendo Switch com **Atmosphere CFW**.
- Script de deploy: **PowerShell** (`scripts/install-to-sd.ps1`).

## Requisitos

Antes de compilar, garanta:

- devkitPro instalado corretamente.
- Pacote `switch-dev` instalado (`pacman -S switch-dev`).
- Ferramentas `npdmtool`, `elf2nro` e `build_pfs0` disponiveis.
- Submodulo `lib/libtesla` presente no projeto (`git submodule update --init`).

## Build

```bash
# Compilar tudo (overlay + sysmod)
make

# Ou separadamente:
make -C sysmod   # sysmod -> sysmod/exefs.nsp
make              # overlay -> switch-engine.ovl
```

Artefatos principais:

- `sysmod/exefs.nsp` — sysmodule (backend)
- `switch-engine.ovl` — overlay Tesla (frontend)

## Instalacao no Nintendo Switch

### Metodo recomendado (Windows / PowerShell)

```powershell
# Copiar tudo e limpar logs antigos
.\scripts\install-to-sd.ps1 -SdRoot F:\ -CleanLogs

# Somente overlay
.\scripts\install-to-sd.ps1 -SdRoot F:\ -OverlayOnly

# Somente sysmod
.\scripts\install-to-sd.ps1 -SdRoot F:\ -SysmodOnly
```

### Metodo manual

Copie os arquivos para o SD nesta estrutura:

```text
sdmc:/atmosphere/contents/420000000053454E/exefs.nsp
sdmc:/atmosphere/contents/420000000053454E/toolbox.json
sdmc:/atmosphere/contents/420000000053454E/flags/boot2.flag   (arquivo vazio)
sdmc:/switch/.overlays/switch-engine.ovl
```

Importante:

- O arquivo `boot2.flag` deve existir (arquivo vazio) para autostart do sysmod no boot.
- Apos instalar/atualizar o **sysmod**, faca **reboot completo** do console (nao apenas sleep/wake).
- A atualizacao do **overlay** nao requer reboot — basta fechar e reabrir o Tesla Menu.

## Como usar

1. Abra um jogo no Switch.
2. Abra o Tesla Menu (atalho padrao: `L + D-Pad Down + RS`).
3. Selecione **Switch Engine**.
4. Escolha o alvo:
   - **Auto: jogo (PGL/pm)** — detecta automaticamente o jogo em execucao.
   - **Escolher processo...** — lista todos os processos com PID e TID.
5. Defina o valor de busca (uint32).
6. Execute **Primeira Busca**.
7. Altere o valor no jogo e rode **Proxima Busca** para filtrar.
8. Abra **Ver Resultados** e clique num endereco para alterar o valor (poke).

## Idioma

Suporte a:

- **PT-BR** (padrao)
- **EN**

A selecao e salva em `sdmc:/switch/switch-engine/config.ini`. As strings ficam em `source/util/Language.cpp`.

## Logs e diagnostico

| Arquivo | Descricao |
|---------|-----------|
| `sdmc:/switch-engine.log` | Log do overlay (init, poke, erros IPC) |
| `sdmc:/switch-engine_debug.log` | Log detalhado do overlay (ciclo de vida) |
| `sdmc:/switch-engine_mod.log` | Log do sysmod (IPC, scan, debug, processos) |
| `sdmc:/switch-engine_mod_crash.log` | Dump de exception do sysmod (PC/LR/registros) |
| `sdmc:/atmosphere/crash_reports/` | Crash reports gerais do Atmosphere |

Se algo nao funcionar:

1. Verifique se `boot2.flag` esta presente.
2. Verifique se o `exefs.nsp` foi atualizado no SD.
3. Leia `sdmc:/switch-engine_mod.log` — ele mostra cada comando IPC recebido e o resultado.
4. Reinicie o console por power-cycle (nao apenas sleep/wake).

## Contrato IPC (`seng` v2)

| Cmd | Nome | Entrada | Saida |
|-----|------|---------|-------|
| 0 | `GetVersion` | — | `u32 version` |
| 1 | `GetForegroundPid` | — | `u64 pid` |
| 2 | `GetTitleId` | `u64 pid` | `u64 tid` |
| 3 | `AttachProcess` | `u64 pid` | — |
| 4 | `DetachProcess` | — | — |
| 5 | `QueryMemory` | `u64 addr` | `MemoryRegion` |
| 6 | `ReadMemory` | `u64 addr, u64 size` + buffer out | `u64 read` |
| 7 | `WriteMemory` | `u64 addr, u64 size` + buffer in | `u64 written` |
| 8 | `IsAttached` | — | `u8 attached` |
| 9 | `ListProcessesLegacy` | (alias de 10) | — |
| 10 | `ListProcesses` | `u64 max` + buffer out | `u64 count` |
| 11 | `StartMemoryScan` | `u64 pid, u32 value` | `u64 total_hits` |

Limitacoes:

- Buffer maximo por leitura/escrita: **64 KB**.
- Lista de processos: ate **64** entradas por chamada.
- Scan atual: somente **uint32** com comparador de igualdade.

## Roadmap

- [ ] Comparadores adicionais (`>=`, `<=`, `between`, `changed`, `unchanged`).
- [ ] Suporte a tipos numericos adicionais (u8, u16, u64, f32, f64).
- [ ] Freeze/lock de enderecos (escrita continua em loop).
- [ ] Scan em thread separada para melhor responsividade da UI.
- [ ] Nomes dos processos na lista (via NACP quando disponivel).
- [ ] Exportacao/importacao de cheat codes.

## Creditos

Desenvolvido por **Bruno Fernandes**.
Portfolio: [bruno-fernandes.online](https://bruno-fernandes.online)
