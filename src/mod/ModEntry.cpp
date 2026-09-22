#include "client/Runtime.h"
#include "ll/api/Versions.h"
#include "ll/api/mod/RegisterHelper.h"
#include "ll/api/io/FileSink.h"
#include "ll/api/io/PatternFormatter.h"
#include "ll/api/memory/Symbol.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/client/ClientCancelJoinLevelEvent.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <ranges>
#include <windows.h>

namespace melonrind {
namespace {
bool supportedExecutable() {
    wchar_t path[32768];
    if (!GetModuleFileNameW(nullptr, path, 32768)) return false;
    DWORD unused{};
    auto size = GetFileVersionInfoSizeW(path, &unused);
    if (!size) return false;
    std::vector<char> data(size);
    if (!GetFileVersionInfoW(path, 0, size, data.data())) return false;
    VS_FIXEDFILEINFO* info{};
    UINT length{};
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void**>(&info), &length) || length < sizeof(*info)) return false;
    return info->dwFileVersionMS == ((1u << 16) | 26u) && info->dwFileVersionLS == ((10u << 16) | 4u);
}
}

class ModEntry {
    ll::mod::NativeMod& self = *ll::mod::NativeMod::current();
    std::vector<ll::event::ListenerPtr> listeners;
    bool installed{};
public:
    static ModEntry& instance() { static ModEntry mod; return mod; }
    bool load() {
        auto& runtime = Runtime::instance();
        runtime.logger = &self.getLogger();
        std::filesystem::create_directories(self.getModDir() / "logs");
        self.getLogger().addSink(std::make_shared<ll::io::FileSink>(
            self.getModDir() / "logs" / "MelonRind.log",
            ll::makePolymorphic<ll::io::PatternFormatter>("[{3:.3%F %T.} {2}] {0}", false), std::ios::app));
        self.getLogger().setFlushLevel(ll::io::LogLevel::Info);
        auto version = ll::getLoaderVersion();
        if (!ll::isClient() || version.major != 26 || version.minor != 10 || version.patch != 14 || !supportedExecutable()) {
            self.getLogger().error("Requires LeviLamina client 26.10.14 and Minecraft 1.26.10.4; no hooks installed.");
            return false;
        }
        try {
            std::filesystem::create_directories(self.getConfigDir());
            auto path = self.getConfigDir() / "config.json";
            nlohmann::json j = nlohmann::json::object();
            if (std::ifstream input{path}; input.good()) input >> j;
            auto& c = runtime.config;
            c.showSaturation = j.value("showSaturation", true);
            c.showFoodPreview = j.value("showFoodPreview", true);
            c.showRegeneration = j.value("showRegeneration", true);
            c.pulse = j.value("pulse", true);
            c.opacity = j.value("opacity", .8f);
            if (!std::isfinite(c.opacity) || c.opacity < 0 || c.opacity > 1) throw std::runtime_error("opacity must be in [0,1]");
            c.estimateUnknownRule = j.value("estimateUnknownRule", true);
            c.diagnostics = j.value("diagnostics", false);
            c.diagnosticMarkers = j.value("diagnosticMarkers", false);
            c.positionsAreLocal = j.value("positionsAreLocal", true);
            c.renderPass = j.value("renderPass", 0);
            auto cap = j.value("saturationCap", std::string("hungerAfterEating"));
            if (cap != "attributeMaximum" && cap != "hungerAfterEating") throw std::runtime_error("invalid saturationCap");
            c.saturationCap = cap == "attributeMaximum" ? SaturationCap::AttributeMaximum : SaturationCap::HungerAfterEating;
            if (!std::filesystem::exists(path)) {
                j = {{"showSaturation", c.showSaturation}, {"showFoodPreview", c.showFoodPreview},
                    {"showRegeneration", c.showRegeneration}, {"pulse", c.pulse}, {"opacity", c.opacity},
                    {"estimateUnknownRule", c.estimateUnknownRule}, {"diagnostics", c.diagnostics},
                    {"diagnosticMarkers", c.diagnosticMarkers}, {"positionsAreLocal", c.positionsAreLocal},
                    {"renderPass", c.renderPass}, {"saturationCap", cap}};
                std::ofstream output(path); output << j.dump(2) << '\n';
                if (!output) throw std::runtime_error("cannot write config");
            }
        } catch (std::exception const& e) {
            self.getLogger().error("Configuration error: {}", e.what());
            return false;
        }
        return true;
    }
    bool enable() {
        if (installed) return true;
        if (Runtime::instance().config.diagnostics) {
            auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
            for (auto name : {
                "?eat@Player@@QEAAXHM@Z",
                "?render@HudHungerRenderer@@UEAAXAEAVMinecraftUIRenderContext@@AEAVIClientInstance@@AEAVUIControl@@H@Z",
                "?render@HudHeartRenderer@@UEAAXAEAVMinecraftUIRenderContext@@AEAVIClientInstance@@AEAVUIControl@@H@Z",
                "?tick@HungerAttributeDelegate@@UEAAXAEAVAttributeInstance@@AEAUAttributeModificationContext@@@Z",
                "?tick@ExhaustionAttributeDelegate@@UEAAXAEAVAttributeInstance@@AEAUAttributeModificationContext@@@Z"
            }) {
                if (auto* address = ll::memory::SymbolView(name).resolve(true)) {
                    self.getLogger().info("symbol {} address={:#x} gameBase={:#x}", name, reinterpret_cast<std::uintptr_t>(address), base);
                }
            }
        }
        auto& bus = ll::event::EventBus::getInstance();
        auto reset = [](auto&) { Runtime::instance().reset(); };
        listeners.push_back(bus.emplaceListener<ll::event::ClientStartJoinLevelEvent>(reset));
        listeners.push_back(bus.emplaceListener<ll::event::ClientExitLevelEvent>(reset));
        listeners.push_back(bus.emplaceListener<ll::event::ClientCancelJoinLevelEvent>(reset));
        if (std::ranges::find(listeners, nullptr) != listeners.end() || !installHooks()) {
            for (auto const& listener : listeners) if (listener) bus.removeListener(listener);
            listeners.clear();
            self.getLogger().error("Could not install HUD hooks or session listeners.");
            return false;
        }
        installed = true;
        Runtime::instance().enabled = true;
        self.getLogger().info("MelonRind development HUD enabled. Geometry and nutrition rules await game validation.");
        return true;
    }
    bool disable() {
        Runtime::instance().enabled = false;
        bool removed = !installed || removeHooks();
        if (removed) installed = false;
        for (auto const& listener : listeners) if (listener) ll::event::EventBus::getInstance().removeListener(listener);
        listeners.clear();
        Runtime::instance().reset();
        return removed;
    }
    // No hot unloading: code may still be on a rendering thread's stack.
};
}
LL_REGISTER_MOD(melonrind::ModEntry, melonrind::ModEntry::instance());
