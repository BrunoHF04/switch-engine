#pragma once

#include <cstdint>

/**
 * Sistema de internacionalizacao (i18n) do Switch Engine.
 *
 * Filosofia:
 *   - Sem libs (gettext, fmt, etc.) -- alocacao zero, lookup O(1).
 *   - Idiomas embutidos: English (default) e Portugues-BR.
 *   - Cada string tem um StringId; a tabela de traducoes e' um array
 *     paralelo no .cpp ([En, PtBr]).
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
 *   - Trocar o idioma NAO altera ListItems ja' construidos. Reabra o overlay
 *     ou volte ao MainGui (rebuildado a cada changeTo).
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
        StatusDetectFailedFmt,
        StatusTargetDetected,
        StatusScanning,
        StatusFirstScanFailedFmt,
        StatusFirstScanResultFmt,
        StatusNoTargetRunDetect,
        StatusFiltering,
        StatusNextScanFailedFmt,
        StatusNextScanResultFmt,
        StatusNoPreviousResults,
        StatusResultsCleared,
        StatusNoResultsToShow,
        StatusReopenToApply,
        StatusProcessPickedFmt,

        HitsSuffixFmt,

        // ---- ResultsListGui ----
        ResultsTitle,
        PageInfoFmt,
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
        ProcessListCountFmt,
        ProcessListErrorFmt,

        // ---- Creditos ----
        CreditsMenu,
        CreditsScreenTitle,
        CreditsAppName,
        CreditsAppTagline,
        CreditsDeveloperName,
        CreditsDeveloperRole,
        CreditsPortfolioUrl,
        CreditsPortfolioRole,

        OverlaySubtitle,

        Count,
    };

    void load();

    void save();

    Lang current();

    void setCurrent(Lang l);

    const char *tr(S id);

    const char *currentCode();

} // namespace seng::i18n
