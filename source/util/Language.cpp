#include "Language.hpp"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

namespace seng::i18n {

    namespace {
        Lang g_current = Lang::En;

        constexpr const char *kConfigDir  = "sdmc:/switch/switch-engine";
        constexpr const char *kConfigPath = "sdmc:/switch/switch-engine/config.ini";

        constexpr const char *kStrings[static_cast<size_t>(S::Count)][2] = {
            // ---- MainGui categorias ----
            /* TargetProcessHeader */ { "Target Process",       "Processo Alvo"          },
            /* SearchValueHeader   */ { "Search Value",         "Valor a Buscar"         },
            /* ScanHeader          */ { "Scan",                  "Busca"                  },
            /* ResultsHeader       */ { "Results",               "Resultados"             },
            /* StatusHeader        */ { "Status",                "Status"                 },
            /* SettingsHeader      */ { "Settings",              "Configuracoes"          },

            // ---- MainGui itens ----
            /* DetectForeground    */ { "Auto: game (PGL/pm)",   "Auto: jogo (PGL/pm)"   },
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

            // ---- MainGui status messages ----
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

            // ---- ListItem suffixes ----
            /* HitsSuffixFmt       */ { "%zu hits",              "%zu acertos"            },

            // ---- ResultsListGui ----
            /* ResultsTitle        */ { "Results",               "Resultados"             },
            /* PageInfoFmt         */ { "%zu hits | page %zu/%zu","%zu acertos | pag %zu/%zu" },
            /* NoResults           */ { "(no results)",          "(sem resultados)"       },
            /* RunFirstScan        */ { "run First Scan",        "rode Primeira Busca"    },
            /* Navigation          */ { "Navigation",            "Navegacao"              },
            /* PreviousPage        */ { "Previous Page",         "Pagina Anterior"        },
            /* NextPage            */ { "Next Page",             "Proxima Pagina"         },
            /* PokeValueTitle      */ { "Poke value",            "Alterar valor"          },
            /* SearchValueTitle    */ { "Search value",          "Valor a buscar"         },

            // ---- NumericInputGui ----
            /* ValueDecimalHeader  */ { "Value (decimal)",       "Valor (decimal)"        },
            /* ValueLabel          */ { "Value",                 "Valor"                  },
            /* DigitsHeader        */ { "Digits",                "Digitos"                },
            /* EditHeader          */ { "Edit",                  "Editar"                 },
            /* Backspace           */ { "Backspace",             "Apagar"                 },
            /* Clear               */ { "Clear",                 "Limpar"                 },
            /* ActionHeader        */ { "Action",                "Acao"                   },
            /* Confirm             */ { "Confirm",               "Confirmar"              },
            /* Cancel              */ { "Cancel",                "Cancelar"               },

            // ---- Language tela ----
            /* LanguageTitle       */ { "Language",              "Idioma"                 },
            /* LanguageEnglish     */ { "English",               "Ingles (English)"       },
            /* LanguagePortugueseBr*/ { "Portuguese (Brazil)",   "Portugues (Brasil)"     },

            // ---- ProcessList tela ----
            /* ProcessListTitle    */ { "Process List",          "Lista de Processos"     },
            /* ProcessListEmpty    */ { "(no processes)",        "(sem processos)"        },
            /* ProcessListCountFmt */ { "%zu processes",         "%zu processos"          },
            /* ProcessListErrorFmt */ { "error (rc=0x%08X)",     "erro (rc=0x%08X)"       },

            // ---- Creditos ----
            /* CreditsHeader       */ { "Credits",               "Creditos"               },
            /* CreditsDeveloperLabel */ { "Developer",           "Desenvolvedor"          },
            /* CreditsDeveloperName */ { "Bruno Fernandes",      "Bruno Fernandes"        },
            /* CreditsWebLabel     */ { "Portfolio",             "Portfolio"              },
            /* CreditsWebUrl       */ { "bruno-fernandes.online","bruno-fernandes.online" },

            // ---- Sysmod ausente ----
            /* SysmodMissingTitle  */ { "Sysmodule",               "Sysmodule"              },
            /* SysmodMissingBody   */ { "Error: Sysmodule not detected",
                                        "Erro: Sysmodule n\u00E3o detectado" },
            /* SysmodMissingHint   */ { "Install TID 0x420000000053454E + reboot",
                                        "Instale TID 0x420000000053454E + reinicie" },

            // ---- Subtitulo do overlay ----
            /* OverlaySubtitle     */ { "Memory Scanner v0.3",   "Buscador de Memoria v0.3" },

            // ---- Novos (v3) ----
            /* ValueTypeHeader     */ { "Value Type",            "Tipo de Valor"          },
            /* CompareOpHeader     */ { "Comparator",            "Comparador"             },
            /* FreezeHeader        */ { "Freeze",                "Congelar"               },
            /* FreezeAdd           */ { "Freeze this address",   "Congelar este endereco" },
            /* FreezeRemove        */ { "Unfreeze",              "Descongelar"            },
            /* FreezeClear         */ { "Clear all freezes",     "Limpar todos congelados"},
            /* FreezeSlotFmt       */ { "Frozen: 0x%010llX = %llu",
                                        "Congelado: 0x%010llX = %llu"                    },
            /* FreezeAdded         */ { "address frozen",        "endereco congelado"     },
            /* FreezeRemoved       */ { "freeze removed",        "congelamento removido"  },
            /* FreezeCleared       */ { "all freezes cleared",   "todos descongelados"    },
            /* FreezeFullFmt       */ { "freeze full (max %zu)", "cheio (max %zu)"        },
            /* UndoPoke            */ { "Undo last poke",        "Desfazer ultimo poke"   },
            /* UndoEmpty           */ { "nothing to undo",       "nada para desfazer"     },
            /* UndoAppliedFmt      */ { "undo: 0x%010llX",       "desfeito: 0x%010llX"    },
            /* ExportCheats        */ { "Export cheats (Atmo.)",  "Exportar cheats (Atmo.)"},
            /* ExportDoneFmt       */ { "exported %zu cheats",    "exportou %zu cheats"   },
            /* ExportFailedFmt     */ { "export failed (rc=0x%08X)", "exportacao falhou (rc=0x%08X)"},
            /* VersionMismatchFmt  */ { "sysmod v%u != overlay v%u",
                                        "sysmod v%u != overlay v%u"                      },
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
                Lang parsed;
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
