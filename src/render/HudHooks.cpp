#include "client/Runtime.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/renderers/HudHungerRenderer.h"
#include "mc/client/gui/controls/renderers/HudHeartRenderer.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/packet/UpdateAttributesPacket.h"

namespace melonrind {
LL_TYPE_INSTANCE_HOOK(HungerHook, ll::memory::HookPriority::Normal, HudHungerRenderer, &HudHungerRenderer::$render,
    void, MinecraftUIRenderContext& context, IClientInstance& client, UIControl& owner, int pass) {
    origin(context, client, owner, pass);
    auto& runtime = Runtime::instance();
    if (runtime.enabled.load(std::memory_order_relaxed)) runtime.hunger(*this, context, client, owner, pass);
}
LL_TYPE_INSTANCE_HOOK(HeartHook, ll::memory::HookPriority::Normal, HudHeartRenderer, &HudHeartRenderer::$render,
    void, MinecraftUIRenderContext& context, IClientInstance& client, UIControl& owner, int pass) {
    origin(context, client, owner, pass);
    auto& runtime = Runtime::instance();
    if (runtime.enabled.load(std::memory_order_relaxed)) runtime.hearts(*this, context, client, owner, pass);
}
using AttributeHandler = void (ClientNetworkHandler::*)(NetworkIdentifier const&, std::shared_ptr<UpdateAttributesPacket>);
LL_TYPE_INSTANCE_HOOK(AttributeHook, ll::memory::HookPriority::Normal, ClientNetworkHandler,
    static_cast<AttributeHandler>(&ClientNetworkHandler::$handle), void,
    NetworkIdentifier const& network, std::shared_ptr<UpdateAttributesPacket> packet) {
    origin(network, packet);
    auto& runtime = Runtime::instance();
    if (runtime.enabled.load(std::memory_order_relaxed) && packet) {
        if (auto client = ll::service::getClientInstance()) runtime.observe(*client, *packet);
    }
}

bool installHooks() {
    if (HungerHook::hook() != 0) return false;
    if (HeartHook::hook() != 0) { HungerHook::unhook(); return false; }
    if (AttributeHook::hook() != 0) { HeartHook::unhook(); HungerHook::unhook(); return false; }
    return true;
}
bool removeHooks() {
    bool a = AttributeHook::unhook();
    bool b = HeartHook::unhook();
    bool c = HungerHook::unhook();
    return a && b && c;
}
} // namespace melonrind
