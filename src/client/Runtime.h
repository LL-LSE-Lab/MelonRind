#pragma once

#include "model/Preview.h"
#include "model/SaturationObservation.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>

class IClientInstance;
class UpdateAttributesPacket;
class HudHungerRenderer;
class HudHeartRenderer;
class MinecraftUIRenderContext;
class UIControl;
namespace ll::io { class Logger; }

namespace melonrind {
struct Config {
    bool showSaturation{true};
    bool showFoodPreview{true};
    bool showRegeneration{true};
    bool pulse{true};
    float opacity{0.8f};
    bool estimateUnknownRule{true};
    bool diagnostics{false};
    bool diagnosticMarkers{false};
    // Calibration controls, retained only while target-version verification is pending.
    bool positionsAreLocal{true};
    int renderPass{0};
    SaturationCap saturationCap{SaturationCap::HungerAfterEating};
};

struct Snapshot {
    std::uint64_t runtimeId{};
    Nutrition nutrition;
    std::optional<Food> food;
    std::string itemName;
    RegenContext regen;
};

class Runtime {
public:
    static Runtime& instance();
    Config config;
    std::atomic_bool enabled{false};
    ll::io::Logger* logger{};

    void reset();
    void observe(IClientInstance&, UpdateAttributesPacket const&);
    [[nodiscard]] std::optional<Snapshot> sample(IClientInstance&);
    void hunger(HudHungerRenderer&, MinecraftUIRenderContext&, IClientInstance&, UIControl&, int);
    void hearts(HudHeartRenderer&, MinecraftUIRenderContext&, IClientInstance&, UIControl&, int);
private:
    std::mutex mutex;
    SaturationObservation observation;
};

bool installHooks();
bool removeHooks();
} // namespace melonrind
