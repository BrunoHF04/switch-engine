#pragma once

#include <tesla.hpp>

#include <cstdint>
#include <functional>

#include "seng_ipc.hpp"

/**
 * ProcessListGui
 *
 * Mostra todos os processos vivos no sistema (chamada svcGetProcessList no
 * sysmod). Cada linha exibe:
 *
 *   - Titulo: "App"  (TID >= 0x0100000000000000)
 *           ou "Sysmod" (TID alto reservado, ex. 0x0100xxxxxxxxxxxx fora da
 *             faixa de apps), ou "Process" generico se nao tiver TID.
 *   - Subtitulo: "PID 0x... | TID 0x..."
 *
 * Ao clicar em um processo, chama o callback com (pid, tid) e da' goBack().
 *
 * Uso (do MainGui):
 *   tsl::changeTo<ProcessListGui>([this](u64 pid, u64 tid) {
 *       this->onProcessPicked(pid, tid);
 *   });
 */
class ProcessListGui : public tsl::Gui {
public:
    using PickCallback = std::function<void(uint64_t pid, uint64_t tid)>;

    explicit ProcessListGui(PickCallback onPick);
    ~ProcessListGui() override = default;

    tsl::elm::Element *createUI() override;

private:
    PickCallback m_onPick;
};
