#pragma once

#include <tesla.hpp>
#include <memory>

class SwitchEngineOverlay : public tsl::Overlay {
public:
    SwitchEngineOverlay();
    ~SwitchEngineOverlay() override;

    void initServices() override;
    void exitServices() override;

    void onShow() override;
    void onHide() override;

    std::unique_ptr<tsl::Gui> loadInitialGui() override;

private:
    bool     m_sengReady        = false;
    bool     m_versionMismatch  = false;
    uint32_t m_sysmodVersion    = 0;
};
