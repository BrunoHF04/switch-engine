#pragma once

#include <cstdint>

/**
 * Sistema de internacionalizacao (i18n) do Switch Engine.
 *
 * Filosofia:
 *   - Sem libs (gettext, fmt, etc.) -- alocacao zero, lookup O(1).
 *   - Dois idiomas embutidos: English (default) e Portugues-BR.
 *   - Cada string tem um StringId; a tabela de traducoes e' um array
 *     paralelo no .cpp.
 *   - tr(id) retorna const char* (ponteiro para tabela estatica). Strings
 *     de format ficam com seus %s/%d -- quem chama faz o snprintf.
 *
 * Persistencia:
 *   - Arquivo: sdmc:/switch/switch-engine/config.ini
 *   - Formato: linha unica "lang=en" ou "lang=pt-BR".
 *   - load() chamado uma vez no boot do overlay (apos fsdevMountSdmc).
 *   - save() chamado quando o usuario muda na tela de Language.
 *
 * Reload de UI:
 *   - Trocar o idioma NAO altera ListItems ja' criados (titulo nao muda
 *     em runtime). O usuario precisa fechar+reabrir o overlay, ou voltar
 *     para o MainGui (que e' rebuildado a cada changeTo).
 */

namespace seng::i18n {

    enum class Lang : uint8_t {
        En   = 0,
        PtBr = 1,
    };

    enum class S : uint16_t {
        // ---- MainGui categorias ----
        TargetProcessHeader,
        SearchValueHeader,
        ScanHeader,
        ResultsHeader,
        StatusHeader,
        SettingsHeader,

        // ---- MainGui itens ----
        DetectForeground,
        PickProcess,
        NoTarget,
        SetValue,
        FirstScan,
        NextScan,
        Reset,
        ShowResults,
        LastAction,
        Idle,
        Language,

        // ---- MainGui status messages (alguns sao formats) ----
        StatusError,
        StatusDetectFailedFmt,         // "detect failed (rc=0x%08X)"
        StatusTargetDetected,
        StatusScanning,
        StatusFirstScanFailedFmt,      // "first scan failed (rc=0x%08X)"
        StatusFirstScanResultFmt,      // "first scan: %zu hits"
        StatusNoTargetRunDetect,
        StatusFiltering,
        StatusNextScanFailedFmt,       // "next scan failed (rc=0x%08X)"
        StatusNextScanResultFmt,       // "next scan: %zu hits"
        StatusNoPreviousResults,
        StatusResultsCleared,
        StatusNoResultsToShow,
        StatusReopenToApply,
        StatusProcessPickedFmt,        // "process selected (PID %llu)"

        // ---- ListItem suffixes ----
        HitsSuffixFmt,                 // "%zu hits"

        // ---- ResultsListGui ----
        ResultsTitle,
        PageInfoFmt,                   // "%zu hits | page %zu/%zu"
        NoResults,
        RunFirstScan,
        Navigation,
        PreviousPage,
        NextPage,
        PokeValueTitle,
        SearchValueTitle,

        // ---- NumericInputGui ----
        ValueDecimalHeader,
        ValueLabel,
        DigitsHeader,
        EditHeader,
        Backspace,
        Clear,
        ActionHeader,
        Confirm,
        Cancel,

        // ---- Language tela ----
        LanguageTitle,
        LanguageEnglish,
        LanguagePortugueseBr,

        // ---- ProcessList tela ----
        ProcessListTitle,
        ProcessListEmpty,
        ProcessListCountFmt,           // "%zu processes"
        ProcessListErrorFmt,           // "error (rc=0x%08X)"

        // ---- Creditos (MainGui rodape) ----
        CreditsHeader,
        CreditsDeveloperLabel,
        CreditsDeveloperName,
        CreditsWebLabel,
        CreditsWebUrl,

        // ---- Subtitulo do overlay ----
        OverlaySubtitle,

        // SENTINEL
        Count,
    };

    void load();

    void save();

    Lang current();

    void setCurrent(Lang l);

    const char *tr(S id);

    // Formata "lang=xx" tag persistida. Util para UIs que mostrem o codigo.
    const char *currentCode();

} // namespace seng::i18n
