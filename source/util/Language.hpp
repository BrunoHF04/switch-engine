#pragma once

#include <cstdint>

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

        // ---- MainGui status messages ----
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

        // ---- ListItem suffixes ----
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
        CreditsHeader,
        CreditsDeveloperLabel,
        CreditsDeveloperName,
        CreditsWebLabel,
        CreditsWebUrl,

        // ---- Sysmod ausente ----
        SysmodMissingTitle,
        SysmodMissingBody,
        SysmodMissingHint,

        // ---- Subtitulo do overlay ----
        OverlaySubtitle,

        // ---- Novos (v3) ----
        ValueTypeHeader,
        CompareOpHeader,
        FreezeHeader,
        FreezeAdd,
        FreezeRemove,
        FreezeClear,
        FreezeSlotFmt,
        FreezeAdded,
        FreezeRemoved,
        FreezeCleared,
        FreezeFullFmt,
        UndoPoke,
        UndoEmpty,
        UndoAppliedFmt,
        ExportCheats,
        ExportDoneFmt,
        ExportFailedFmt,
        VersionMismatchFmt,

        // SENTINEL
        Count,
    };

    void load();

    void save();

    Lang current();

    void setCurrent(Lang l);

    const char *tr(S id);

    const char *currentCode();

} // namespace seng::i18n
