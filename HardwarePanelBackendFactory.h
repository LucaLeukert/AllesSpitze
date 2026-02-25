#pragma once

#include <memory>

class HardwarePanelBackend;

std::unique_ptr<HardwarePanelBackend> createHardwarePanelBackend();
