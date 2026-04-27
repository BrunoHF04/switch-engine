# Switch Engine

`Switch Engine` e um scanner de memoria para Nintendo Switch dividido em dois componentes:

- **Overlay Tesla** (`switch-engine.ovl`): interface do usuario para selecionar processo, escanear, refinar resultados e editar valores.
- **SysModule Atmosphere** (`switch-engine-mod.nsp`): backend com acesso as SVCs de debug, expondo um servico IPC (`seng`) para o overlay.

Esse desenho em duas partes e necessario porque o overlay, isoladamente, nao possui permissao para executar chamadas de debug no processo alvo.

## O que o programa faz

O Switch Engine permite:

- Detectar o processo alvo automaticamente (foreground) ou manualmente por lista.
- Realizar **First Scan** por valor numerico (`u32`) em regioes relevantes de memoria.
- Realizar **Next Scan** para refinar resultados com um novo valor.
- Navegar resultados paginados e aplicar **poke** (escrita de memoria) em enderecos encontrados.
- Persistir resultados e configuracoes no SD para manter estado entre aberturas.

## Arquitetura

```
+---------------------------+         +---------------------------+
|  switch-engine.ovl        |  IPC    |  switch-engine-mod.nsp   |
|  (Tesla Overlay)          | <-----> |  (Atmosphere SysModule)  |
|  - UI (libtesla)          |  seng:  |  - svcDebugActiveProcess |
|  - fluxo de scan          |         |  - svcReadDebugProcess   |
|  - resultados no SD       |         |  - svcWriteDebugProcess  |
|  - i18n EN/PT-BR          |         |  - svcGetProcessList     |
+---------------------------+         +---------------------------+
```

### Componentes principais

- `source/gui/`: telas do overlay (`MainGui`, `ProcessListGui`, `ResultsListGui`, `NumericInputGui`, `LanguageGui`).
- `source/scanner/`: cliente IPC, scanner e armazenamento de resultados.
- `source/util/`: logger e sistema de idioma.
- `sysmod/source/`: servidor IPC, camada de debug e logs do sysmod.
- `include/seng_ipc.hpp`: contrato IPC compartilhado entre overlay e sysmod.

## Tecnologias e linguagem

- Linguagem principal: **C++**.
- SDK/Ferramentas: **devkitPro** com `switch-dev`.
- UI: **libtesla**.
- Ambiente alvo: Nintendo Switch com **Atmosphere CFW**.
- Script de deploy no Windows: **PowerShell** (`scripts/install-to-sd.ps1`).

## Requisitos

Antes de compilar, garanta:

- devkitPro instalado corretamente.
- Pacote `switch-dev` instalado (`pacman -S switch-dev`).
- Ferramentas `npdmtool`, `elf2nro` e `build_pfs0` disponiveis.
- Submodulo `lib/libtesla` presente no projeto.

## Build

```bash
# 1) Compilar sysmod
make -C sysmod

# 2) Compilar overlay
make
```

Artefatos principais:

- `sysmod/exefs.nsp`
- `switch-engine.ovl`

## Instalacao no Nintendo Switch

### Metodo recomendado (Windows / PowerShell)

```powershell
.\scripts\install-to-sd.ps1 -SDDrive F: -CleanLogs
```

Esse comando copia overlay + sysmod para o SD e limpa logs antigos quando solicitado.

### Metodo manual

Copie os arquivos para o SD nesta estrutura:

```text
sdmc:/atmosphere/contents/420000000053454E/exefs.nsp
sdmc:/atmosphere/contents/420000000053454E/toolbox.json
sdmc:/atmosphere/contents/420000000053454E/flags/boot2.flag
sdmc:/switch/.overlays/switch-engine.ovl
```

Importante:

- O arquivo `boot2.flag` deve existir (arquivo vazio) para autostart do sysmod.
- Apos instalar/atualizar o sysmod, faca **reboot completo** do console.

## Como usar

1. Abra um jogo.
2. Abra o Tesla Menu (atalho padrao: `L + D-Pad Down + RS`).
3. Selecione `Switch Engine`.
4. Escolha o alvo:
   - `Detect foreground` (automatico), ou
   - `Pick process` (manual).
5. Defina o valor de busca.
6. Execute `First Scan`.
7. Altere o valor no jogo e rode `Next Scan`.
8. Abra resultados e use poke quando necessario.

## Idioma

O projeto possui suporte a:

- `PT-BR`
- `EN`

A selecao de idioma e salva em:

- `sdmc:/switch/switch-engine/config.ini`

As strings ficam em:

- `source/util/Language.cpp`

## Logs e diagnostico

Arquivos importantes:

- `sdmc:/switch/switch-engine/log.txt` (overlay)
- `sdmc:/switch-engine_mod.log` (sysmod)
- `sdmc:/switch-engine_mod_crash.log` (crash do sysmod)
- `sdmc:/atmosphere/crash_reports/` (crash reports gerais do sistema)

Se o scanner nao listar processos ou falhar no alvo:

1. Verifique se `boot2.flag` esta presente.
2. Verifique se o `exefs.nsp` foi atualizado corretamente no SD.
3. Leia `sdmc:/switch-engine_mod.log`.
4. Reinicie o console por power-cycle (nao apenas sleep/wake).

## Contrato IPC atual (`seng`)

Comandos implementados:

- `GetVersion`
- `GetForegroundPid`
- `GetTitleId`
- `AttachProcess`
- `DetachProcess`
- `QueryMemory`
- `ReadMemory`
- `WriteMemory`
- `IsAttached`
- `ListProcesses`

Limitacoes atuais:

- Buffer maximo por operacao de leitura/escrita: `64 KB`.
- Lista de processos: ate `64` entradas por chamada.

## Roadmap tecnico

- Comparadores adicionais de scan (`>=`, `<=`, `between`, `changed`, `unchanged`).
- Execucao de scan em thread separada para melhor responsividade da UI.
- Picker de tipo numerico (u8/u16/u32/u64/f32/f64) para busca e poke.
- Marcacao de processo atual na lista de processos.
- Investigacao de cenarios onde foreground retorna PID 1 em alguns firmwares/titulos.
- Possivel comando de leitura por faixa para reduzir overhead de IPC no first scan.

## Creditos

Desenvolvido por **Bruno Fernandes**.  
Portfolio: [bruno-fernandes.online](https://bruno-fernandes.online)
