#include "Language.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace seng::i18n {

    namespace {
        Lang g_current = Lang::En;

        constexpr const char *kConfigDir  = "sdmc:/switch/switch-engine";
        constexpr const char *kConfigPath = "sdmc:/switch/switch-engine/config.ini";

        // Tabela [En, PtBr]. Ordem = enum S.
        constexpr const char *kStrings[static_cast<size_t>(S::Count)][2] = {
            /* TargetProcessHeader */ { "Target Process",       "Processo Alvo"          },
            /* SearchValueHeader   */ { "Search Value (uint32)","Valor a Buscar (uint32)"},
            /* ScanHeader          */ { "Scan",                  "Busca"                  },
            /* ResultsHeader       */ { "Results",               "Resultados"             },
            /* StatusHeader        */ { "Status",                "Status"                 },
            /* SettingsHeader      */ { "Settings",              "Configuracoes"          },

            /* DetectForeground    */ { "Detect foreground",     "Detectar em primeiro plano" },
            /* PickProcess         */ { "Pick process...",       "Escolher processo..."   },
            /* NoTarget            */ { "(no target)",           "(sem alvo)"             },
            /* SetValue            */ { "Set value",             "Definir valor"          },
            /* FirstScan           */ { "First Scan",            "Primeira Busca"         },
            /* NextScan            */ { "Next Scan",             "Proxima Busca"          },
            /* Reset               */ { "Reset",                 "Reiniciar"              },
            /* ShowResults         */ { "Show Results",          "Ver Resultados"         },
            /* LastAction          */ { "Last action",           "Ultima acao"            },
            /* Idle                */ { "(idle)",                "(ocioso)"               },
            /* Language            */ { "Language",              "Idioma"                 },

            /* StatusError                   */ { "error",                                "erro"                                  },
            /* StatusDetectFailedFmt         */ { "detect failed (rc=0x%08X)",            "deteccao falhou (rc=0x%08X)"           },
            /* StatusTargetDetected          */ { "target detected",                      "alvo detectado"                        },
            /* StatusScanning                */ { "scanning...",                          "buscando..."                           },
            /* StatusFirstScanFailedFmt      */ { "first scan failed (rc=0x%08X)",        "primeira busca falhou (rc=0x%08X)"     },
            /* StatusFirstScanResultFmt      */ { "first scan: %zu hits",                 "primeira busca: %zu acertos"           },
            /* StatusNoTargetRunDetect       */ { "no target (run Detect first)",         "sem alvo (rode Detectar primeiro)"     },
            /* StatusFiltering               */ { "filtering...",                         "filtrando..."                          },
            /* StatusNextScanFailedFmt       */ { "next scan failed (rc=0x%08X)",         "proxima busca falhou (rc=0x%08X)"      },
            /* StatusNextScanResultFmt       */ { "next scan: %zu hits",                  "proxima busca: %zu acertos"            },
            /* StatusNoPreviousResults       */ { "no previous results (run First Scan)", "sem resultados (rode Primeira Busca)"  },
            /* StatusResultsCleared          */ { "results cleared",                      "resultados limpos"                     },
            /* StatusNoResultsToShow         */ { "no results to show",                   "nenhum resultado para mostrar"         },
            /* StatusReopenToApply           */ { "language saved (reopen to apply)",     "idioma salvo (reabra para aplicar)"    },
            /* StatusProcessPickedFmt        */ { "target set (PID %llu)",               "alvo definido (PID %llu)"              },

            /* HitsSuffixFmt       */ { "%zu hits",              "%zu acertos"            },

            /* ResultsTitle        */ { "Results",               "Resultados"             },
            /* PageInfoFmt         */ { "%zu hits | page %zu/%zu","%zu acertos | pag %zu/%zu" },
            /* NoResults           */ { "(no results)",          "(sem resultados)"       },
            /* RunFirstScan        */ { "run First Scan",        "rode Primeira Busca"    },
            /* Navigation          */ { "Navigation",            "Navegacao"              },
            /* PreviousPage        */ { "Previous Page",         "Pagina Anterior"        },
            /* NextPage            */ { "Next Page",             "Proxima Pagina"         },
            /* PokeValueTitle      */ { "Poke value (uint32)",   "Alterar valor (uint32)" },
            /* SearchValueTitle    */ { "Search value (uint32)", "Valor a buscar (uint32)"},

            /* ValueDecimalHeader  */ { "Value (decimal)",       "Valor (decimal)"        },
            /* ValueLabel          */ { "Value",                 "Valor"                  },
            /* DigitsHeader        */ { "Digits",                "Digitos"                },
            /* EditHeader          */ { "Edit",                  "Editar"                 },
            /* Backspace           */ { "Backspace",             "Apagar"                 },
            /* Clear               */ { "Clear",                 "Limpar"                 },
            /* ActionHeader        */ { "Action",                "Acao"                   },
            /* Confirm             */ { "Confirm",               "Confirmar"              },
            /* Cancel              */ { "Cancel",                "Cancelar"               },

            /* LanguageTitle       */ { "Language",              "Idioma"                 },
            /* LanguageEnglish     */ { "English",               "Ingles (English)"       },
            /* LanguagePortugueseBr*/ { "Portuguese (Brazil)",   "Portugues (Brasil)"     },

            /* ProcessListTitle    */ { "Process List",          "Lista de Processos"     },
            /* ProcessListEmpty    */ { "(no processes)",        "(sem processos)"        },
            /* ProcessListCountFmt */ { "%zu processes",         "%zu processos"          },
            /* ProcessListErrorFmt */ { "error (rc=0x%08X)",     "erro (rc=0x%08X)"       },

            /* CreditsMenu           */ { "Credits",               "Creditos"               },
            /* CreditsScreenTitle    */ { "Credits",               "Creditos"               },
            /* CreditsAppName        */ { "Switch Engine",         "Switch Engine"          },
            /* CreditsAppTagline     */ { "Memory scanner",        "Buscador de memoria"    },
            /* CreditsDeveloperName  */ { "Bruno Fernandes",       "Bruno Fernandes"        },
            /* CreditsDeveloperRole  */ { "Developer",             "Desenvolvedor"          },
            /* CreditsPortfolioUrl   */ { "bruno-fernandes.online","bruno-fernandes.online" },
            /* CreditsPortfolioRole  */ { "Portfolio / website",   "Portfolio / site"     },

            /* OverlaySubtitle     */ { "Memory Scanner v0.2",   "Buscador de Memoria v0.2" },
        };

        static_assert(sizeof(kStrings) / sizeof(kStrings[0])
                          == static_cast<size_t>(S::Count),
                      "Lang table size mismatch");

        bool parseLangCode(const char *value, Lang *out) {
            if (!value || !out) return false;
            if (std::strncmp(value, "pt", 2) == 0) {
                *out = Lang::PtBr;
                return true;
            }
            if (std::strncmp(value, "en", 2) == 0) {
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
        return g_current == Lang::PtBr ? "pt-BR" : "en";
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
                } else {
                    // lang=es, lang=ja, ou valor invalido -> volta ao ingles
                    g_current = Lang::En;
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
