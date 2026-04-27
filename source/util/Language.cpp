#include "Language.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace seng::i18n {

    namespace {
        Lang g_current = Lang::En;

        constexpr const char *kConfigDir  = "sdmc:/switch/switch-engine";
        constexpr const char *kConfigPath = "sdmc:/switch/switch-engine/config.ini";

        // Tabela de traducoes. Indice = static_cast<size_t>(S::xxx).
        // Colunas: [En, PtBr, Es, Ja].
        // ATENCAO: ordem TEM que bater com a enum em Language.hpp.
        constexpr const char *kStrings[static_cast<size_t>(S::Count)][4] = {
            // ---- MainGui categorias ----
            /* TargetProcessHeader */ { "Target Process",       "Processo Alvo",          "Proceso objetivo",        "対象プロセス" },
            /* SearchValueHeader   */ { "Search Value (uint32)","Valor a Buscar (uint32)","Valor (uint32)",          "検索値 (uint32)" },
            /* ScanHeader          */ { "Scan",                  "Busca",                  "Escanear",                "スキャン" },
            /* ResultsHeader       */ { "Results",               "Resultados",             "Resultados",              "結果" },
            /* StatusHeader        */ { "Status",                "Status",                 "Estado",                  "ステータス" },
            /* SettingsHeader      */ { "Settings",              "Configuracoes",          "Ajustes",                 "設定" },

            // ---- MainGui itens ----
            /* DetectForeground    */ { "Detect foreground",     "Detectar em primeiro plano", "Detectar primer plano", "前面アプリを検出" },
            /* PickProcess         */ { "Pick process...",       "Escolher processo...",   "Elegir proceso...",       "プロセスを選択..." },
            /* NoTarget            */ { "(no target)",           "(sem alvo)",             "(sin objetivo)",          "(ターゲットなし)" },
            /* SetValue            */ { "Set value",             "Definir valor",          "Establecer valor",        "値を設定" },
            /* FirstScan           */ { "First Scan",            "Primeira Busca",         "Primer escaneo",          "初回スキャン" },
            /* NextScan            */ { "Next Scan",             "Proxima Busca",          "Siguiente escaneo",       "次のスキャン" },
            /* Reset               */ { "Reset",                 "Reiniciar",              "Restablecer",             "リセット" },
            /* ShowResults         */ { "Show Results",          "Ver Resultados",         "Ver resultados",          "結果を表示" },
            /* LastAction          */ { "Last action",           "Ultima acao",            "Ultima accion",           "直近の操作" },
            /* Idle                */ { "(idle)",                "(ocioso)",               "(inactivo)",              "(待機)" },
            /* Language            */ { "Language",              "Idioma",                 "Idioma",                  "言語" },

            // ---- MainGui status messages ----
            /* StatusError                   */ { "error",                                "erro",                                  "error",                                  "エラー" },
            /* StatusDetectFailedFmt         */ { "detect failed (rc=0x%08X)",            "deteccao falhou (rc=0x%08X)",           "deteccion fallida (rc=0x%08X)",          "検出に失敗 (rc=0x%08X)" },
            /* StatusTargetDetected          */ { "target detected",                      "alvo detectado",                        "objetivo detectado",                    "ターゲットを検出" },
            /* StatusScanning                */ { "scanning...",                          "buscando...",                           "escaneando...",                         "スキャン中..." },
            /* StatusFirstScanFailedFmt      */ { "first scan failed (rc=0x%08X)",        "primeira busca falhou (rc=0x%08X)",     "primer escaneo fallido (rc=0x%08X)",    "初回スキャン失敗 (rc=0x%08X)" },
            /* StatusFirstScanResultFmt      */ { "first scan: %zu hits",                 "primeira busca: %zu acertos",           "primer escaneo: %zu coincidencias",     "初回スキャン: %zu 件" },
            /* StatusNoTargetRunDetect       */ { "no target (run Detect first)",         "sem alvo (rode Detectar primeiro)",     "sin objetivo (usa Detectar primero)",   "ターゲットなし（先に検出）" },
            /* StatusFiltering               */ { "filtering...",                         "filtrando...",                          "filtrando...",                          "絞り込み中..." },
            /* StatusNextScanFailedFmt       */ { "next scan failed (rc=0x%08X)",         "proxima busca falhou (rc=0x%08X)",      "siguiente escaneo fallido (rc=0x%08X)", "次スキャン失敗 (rc=0x%08X)" },
            /* StatusNextScanResultFmt       */ { "next scan: %zu hits",                  "proxima busca: %zu acertos",            "siguiente escaneo: %zu coincidencias",  "次のスキャン: %zu 件" },
            /* StatusNoPreviousResults       */ { "no previous results (run First Scan)", "sem resultados (rode Primeira Busca)",  "sin resultados (ejecuta Primer escaneo)", "結果なし（初回スキャンを実行）" },
            /* StatusResultsCleared          */ { "results cleared",                      "resultados limpos",                     "resultados borrados",                   "結果をクリア" },
            /* StatusNoResultsToShow         */ { "no results to show",                   "nenhum resultado para mostrar",         "nada que mostrar",                      "表示する結果がありません" },
            /* StatusReopenToApply           */ { "language saved (reopen to apply)",     "idioma salvo (reabra para aplicar)",    "idioma guardado (reabre para aplicar)",  "言語を保存（反映は再オープン）" },
            /* StatusProcessPickedFmt        */ { "target set (PID %llu)",               "alvo definido (PID %llu)",              "objetivo (PID %llu)",                   "ターゲット設定 (PID %llu)" },

            // ---- ListItem suffixes ----
            /* HitsSuffixFmt       */ { "%zu hits",              "%zu acertos",            "%zu coincidencias",       "%zu 件" },

            // ---- ResultsListGui ----
            /* ResultsTitle        */ { "Results",               "Resultados",             "Resultados",              "結果" },
            /* PageInfoFmt         */ { "%zu hits | page %zu/%zu","%zu acertos | pag %zu/%zu", "%zu | pag. %zu/%zu", "%zu 件 | %zu/%zu ページ" },
            /* NoResults           */ { "(no results)",          "(sem resultados)",       "(sin resultados)",        "(結果なし)" },
            /* RunFirstScan        */ { "run First Scan",        "rode Primeira Busca",    "ejecuta Primer escaneo",  "初回スキャンを実行" },
            /* Navigation          */ { "Navigation",            "Navegacao",              "Navegacion",              "移動" },
            /* PreviousPage        */ { "Previous Page",         "Pagina Anterior",        "Pagina anterior",         "前へ" },
            /* NextPage            */ { "Next Page",             "Proxima Pagina",         "Pagina siguiente",        "次へ" },
            /* PokeValueTitle      */ { "Poke value (uint32)",   "Alterar valor (uint32)", "Escribir valor (uint32)", "書き込み値 (uint32)" },
            /* SearchValueTitle    */ { "Search value (uint32)", "Valor a buscar (uint32)","Valor a buscar (uint32)", "検索する値 (uint32)" },

            // ---- NumericInputGui ----
            /* ValueDecimalHeader  */ { "Value (decimal)",       "Valor (decimal)",        "Valor (decimal)",         "値（10進）" },
            /* ValueLabel          */ { "Value",                 "Valor",                  "Valor",                   "値" },
            /* DigitsHeader        */ { "Digits",                "Digitos",                "Digitos",                 "数字" },
            /* EditHeader          */ { "Edit",                  "Editar",                 "Editar",                  "編集" },
            /* Backspace           */ { "Backspace",             "Apagar",                 "Retroceso",               "退格" },
            /* Clear               */ { "Clear",                 "Limpar",                   "Borrar",                  "クリア" },
            /* ActionHeader        */ { "Action",                "Acao",                   "Accion",                  "操作" },
            /* Confirm             */ { "Confirm",               "Confirmar",              "Confirmar",               "確定" },
            /* Cancel              */ { "Cancel",                "Cancelar",               "Cancelar",                "キャンセル" },

            // ---- Language tela ----
            /* LanguageTitle       */ { "Language",              "Idioma",                 "Idioma",                  "言語" },
            /* LanguageEnglish     */ { "English",               "Ingles (English)",       "Ingles",                  "英語" },
            /* LanguagePortugueseBr*/ { "Portuguese (Brazil)",   "Portugues (Brasil)",     "Portugues (Brasil)",      "ポルトガル語（ブラジル）" },
            /* LanguageSpanish     */ { "Spanish",               "Espanhol",               "Espanol",                 "スペイン語" },
            /* LanguageJapanese    */ { "Japanese",              "Japones",                "Japones",                 "日本語" },

            // ---- ProcessList tela ----
            /* ProcessListTitle    */ { "Process List",          "Lista de Processos",     "Lista de procesos",       "プロセス一覧" },
            /* ProcessListEmpty    */ { "(no processes)",        "(sem processos)",        "(sin procesos)",          "(プロセスなし)" },
            /* ProcessListCountFmt */ { "%zu processes",         "%zu processos",          "%zu procesos",            "%zu プロセス" },
            /* ProcessListErrorFmt */ { "error (rc=0x%08X)",     "erro (rc=0x%08X)",       "error (rc=0x%08X)",       "エラー (rc=0x%08X)" },

            // ---- Creditos ----
            /* CreditsMenu           */ { "Credits",               "Creditos",               "Creditos",                "クレジット" },
            /* CreditsScreenTitle    */ { "Credits",               "Creditos",               "Creditos",                "クレジット" },
            /* CreditsAppName        */ { "Switch Engine",         "Switch Engine",          "Switch Engine",           "Switch Engine" },
            /* CreditsAppTagline     */ { "Memory scanner",        "Buscador de memoria",    "Escaneador de memoria",   "メモリスキャナ" },
            /* CreditsDeveloperName  */ { "Bruno Fernandes",       "Bruno Fernandes",        "Bruno Fernandes",         "Bruno Fernandes" },
            /* CreditsDeveloperRole  */ { "Developer",             "Desenvolvedor",          "Desarrollador",           "開発者" },
            /* CreditsPortfolioUrl   */ { "bruno-fernandes.online","bruno-fernandes.online", "bruno-fernandes.online",  "bruno-fernandes.online" },
            /* CreditsPortfolioRole  */ { "Portfolio / website",   "Portfolio / site",       "Portfolio / sitio web",   "ポートフォリオ／サイト" },

            // ---- Subtitulo do overlay ----
            /* OverlaySubtitle     */ { "Memory Scanner v0.2",   "Buscador de Memoria v0.2", "Escaneador de Memoria v0.2", "メモリスキャナ v0.2" },
        };

        static_assert(sizeof(kStrings) / sizeof(kStrings[0])
                          == static_cast<size_t>(S::Count),
                      "Lang table size mismatch -- some StringId nao foi traduzido");

        bool parseLangCode(const char *value, Lang *out) {
            if (!value || !out) return false;

            char buf[24]{};
            size_t j = 0;
            for (size_t i = 0; value[i] && j + 1 < sizeof(buf); ++i) {
                char c = value[i];
                if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                buf[j++] = c;
            }

            if (std::strncmp(buf, "ja", 2) == 0 || std::strncmp(buf, "jp", 2) == 0) {
                *out = Lang::Ja;
                return true;
            }
            if (std::strncmp(buf, "es", 2) == 0) {
                *out = Lang::Es;
                return true;
            }
            if (std::strncmp(buf, "pt", 2) == 0) {
                *out = Lang::PtBr;
                return true;
            }
            if (std::strncmp(buf, "en", 2) == 0) {
                *out = Lang::En;
                return true;
            }
            return false;
        }
    } // namespace

    Lang current() { return g_current; }

    void setCurrent(Lang l) { g_current = l; }

    const char *tr(S id) {
        const auto idx = static_cast<size_t>(id);
        if (idx >= static_cast<size_t>(S::Count)) return "?";
        return kStrings[idx][static_cast<size_t>(g_current)];
    }

    const char *currentCode() {
        switch (g_current) {
            case Lang::PtBr: return "pt-BR";
            case Lang::Es:   return "es";
            case Lang::Ja:   return "ja";
            default:         return "en";
        }
    }

    void load() {
        FILE *fp = std::fopen(kConfigPath, "r");
        if (!fp) return;

        char line[64];
        while (std::fgets(line, sizeof(line), fp)) {
            for (char *p = line; *p; ++p) {
                if (*p == '\n' || *p == '\r') { *p = 0; break; }
            }
            if (std::strncmp(line, "lang=", 5) == 0) {
                Lang parsed = Lang::En;
                if (parseLangCode(line + 5, &parsed)) {
                    g_current = parsed;
                }
                break;
            }
        }
        std::fclose(fp);
    }

    void save() {
        mkdir("sdmc:/switch", 0777);
        mkdir(kConfigDir,     0777);

        FILE *fp = std::fopen(kConfigPath, "w");
        if (!fp) return;
        std::fprintf(fp, "lang=%s\n", currentCode());
        std::fflush(fp);
        std::fclose(fp);
    }

} // namespace seng::i18n
