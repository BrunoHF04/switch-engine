#pragma once

#include <tesla.hpp>

/**
 * Tela exibida quando o servico IPC "seng" (switch-engine-mod) nao esta
 * disponivel — evita crash e informa o usuario.
 */
class SysmodMissingGui : public tsl::Gui {
public:
    SysmodMissingGui() = default;
    ~SysmodMissingGui() override = default;

    tsl::elm::Element *createUI() override;
};
