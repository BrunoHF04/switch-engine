#pragma once

#include <tesla.hpp>

/**
 * Tela de creditos / sobre o Switch Engine.
 */
class CreditsGui : public tsl::Gui {
public:
    ~CreditsGui() override = default;

    tsl::elm::Element *createUI() override;
};
