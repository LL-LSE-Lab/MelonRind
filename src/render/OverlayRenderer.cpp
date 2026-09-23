#include "client/Runtime.h"
#include "ll/api/io/Logger.h"
#include "mc/client/gui/VisibilityFlag.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/options/IOptions.h"
#include "mc/client/gui/controls/renderers/HudHungerRenderer.h"
#include "mc/client/gui/controls/renderers/HudHeartRenderer.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"
#include "mc/deps/core/file/PathView.h"
#include "mc/deps/core/math/Color.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/core/resource/ResourceLocation.h"
#include "mc/deps/input/RectangleArea.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <span>
#include <windows.h>

namespace melonrind {
namespace {
using Clock = std::chrono::steady_clock;
float pulseAlpha(Config const& config) {
    if (!config.pulse) return config.opacity * .65f;
    auto time = std::chrono::duration<double>(Clock::now().time_since_epoch()).count();
    return config.opacity * static_cast<float>(.5 + .3 * std::sin(time * 4));
}
bool isHostHudVisible(UICustomRenderer const& renderer, UIControl const& owner) {
    if (!owner.mIsVisibleInTree || owner.mVisible == ui::VisibilityFlag::None) return false;
    float propAlpha = renderer.mPropagatedAlpha;
    if (std::isfinite(propAlpha) && propAlpha <= 0.001f) return false;
    return true;
}
float hudAlpha(UICustomRenderer const& renderer, IClientInstance& client) {
    float alpha = client.getOptions().getInterfaceOpacity();
    if (!std::isfinite(alpha) || alpha <= 0.0f) return 0.0f;
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    float propAlpha = renderer.mPropagatedAlpha;
    if (std::isfinite(propAlpha)) {
        if (propAlpha <= 0.001f) return 0.0f;
        alpha *= std::clamp(propAlpha, 0.0f, 1.0f);
    }
    return alpha;
}
glm::vec2 position(glm::vec3 const& icon, UIControl& owner, Config const& config) {
    glm::vec2 result{icon.x, icon.y};
    if (config.positionsAreLocal) result += owner.getPosition();
    return result;
}
bool validPosition(glm::vec2 const& p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::abs(p.x) < 100000 && std::abs(p.y) < 100000;
}

// Texture is already cropped to an individual 9x9 icon. Fractional U slicing
// shows only the new part of a half-filled icon, without touching the old part.
void drawRange(MinecraftUIRenderContext& context, mce::TexturePtr const& texture,
               std::span<glm::vec3 const> positions, UIControl& owner, Config const& config,
               float before, float after, bool reverseFill, float alpha, mce::Color const& color) {
    if (alpha <= 0 || positions.empty() || positions.size() > 1000) return;
    auto slices = iconSlices(before, after, static_cast<int>(positions.size()));
    for (auto const& slice : slices) {
        auto p = position(positions[slice.index], owner, config);
        if (!validPosition(p)) continue;
        float left = reverseFill ? 1 - slice.end : slice.start;
        float width = slice.end - slice.start;
        p.x += left * 9;
        context.drawImage(texture.getClientTexture(), p, {width * 9, 9}, {left, 0}, {width, 1}, false);
    }
    if (!slices.empty()) context.flushImages(color, alpha, HashedString("ui_textured_and_glcolor"));
}

void diagnostics(char const* kind, UICustomRenderer const& renderer, std::span<glm::vec3 const> positions, MinecraftUIRenderContext& context,
                 IClientInstance& client, UIControl& owner, int pass, Snapshot const* state, Config const& config, ll::io::Logger* logger) {
    if (config.diagnostics && logger) {
        // Per render thread, no UI object addresses retained after this callback.
        static thread_local Clock::time_point lastHunger{};
        static thread_local Clock::time_point lastHeart{};
        auto& last = kind[0] == 'h' && kind[1] == 'u' ? lastHunger : lastHeart;
        auto now = Clock::now();
        if (now - last >= std::chrono::seconds(2)) {
            last = now;
            auto anchor = owner.getPosition();
            auto first = positions.empty() ? glm::vec3{} : positions[0];
            auto second = positions.size() < 2 ? glm::vec3{} : positions[1];
            auto topScreen = client.getTopScreenName();
            auto inWorldNoMenu = client.isInWorldAndNotShowingAnyMenuScreens();
            auto isExiting = context.mCurrentScene.isExiting();
            auto isVisibleTree = owner.mIsVisibleInTree;
            float propAlpha = renderer.mPropagatedAlpha;
            float alpha = hudAlpha(renderer, client);
            logger->info("HUD {} pass={} owner=({},{}) first=({},{},{}) count={} alpha={} propAlpha={} inWorld={} topScreen={} exiting={} visibleTree={} thread={}",
                         kind, pass, anchor.x, anchor.y, first.x, first.y, first.z,
                         positions.size(), alpha, propAlpha, inWorldNoMenu, topScreen, isExiting, isVisibleTree, GetCurrentThreadId());
            if (state) logger->info("state id={} H={} S={} HP={}/{} item={} N={} M={}", state->runtimeId,
                state->nutrition.hunger, state->nutrition.saturation.value_or(-1), state->regen.health, state->regen.maxHealth,
                state->itemName, state->food ? state->food->nutrition : 0, state->food ? state->food->saturationModifier : 0);
        }
    }
    if (config.diagnosticMarkers && state && pass == config.renderPass && hudAlpha(renderer, client) > 0) {
        for (auto const& icon : positions) {
            auto p = position(icon, owner, config);
            if (validPosition(p)) context.drawRectangle(RectangleArea(p.x, p.y, p.x + 9, p.y + 9, true), mce::Color(0, 255, 255), .6f, 1);
        }
    }
}
} // namespace

void Runtime::hunger(HudHungerRenderer& renderer, MinecraftUIRenderContext& context,
                     IClientInstance& client, UIControl& owner, int pass) {
    if (!renderer.mShouldRender) return;
    auto state = sample(client);
    auto const& positions = renderer.mIconPosition.get();
    diagnostics("hunger", renderer, positions, context, client, owner, pass, state ? &*state : nullptr, config, logger);
    if (!isHostHudVisible(renderer, owner)) return;
    if (!state || pass != config.renderPass || !renderer.mHasLoadedTextures) return;
    if (state->nutrition.hunger > 0 && renderer.mNumFullIcons == 0 && renderer.mNumHalfIcons == 0) return;
    float alpha = hudAlpha(renderer, client);
    if (alpha <= 0.001f) return;
    auto gold = context.getTexture(ResourceLocation(Core::PathView("textures/melonrind/saturation")), false);
    auto const& foodTexture = renderer.mHungerTextures.get()[renderer.mUseHungerEffect ? 3 : 2];
    std::optional<MealPreview> meal;
    if (config.showFoodPreview && state->food) meal = previewMeal(state->nutrition, *state->food, config.saturationCap);
    if (meal) drawRange(context, foodTexture, positions, owner, config, meal->hungerBefore, meal->hungerAfter,
                        true, alpha * pulseAlpha(config), mce::Color(255, 255, 255));
    if (config.showSaturation && state->nutrition.saturation) {
        drawRange(context, gold, positions, owner, config, 0, *state->nutrition.saturation,
                  true, alpha * config.opacity, mce::Color(255, 255, 255));
    }
    if (meal && meal->saturationBefore && meal->saturationAfter) {
        drawRange(context, gold, positions, owner, config, *meal->saturationBefore, *meal->saturationAfter,
                  true, alpha * pulseAlpha(config), mce::Color(255, 255, 255));
    }
}

void Runtime::hearts(HudHeartRenderer& renderer, MinecraftUIRenderContext& context,
                     IClientInstance& client, UIControl& owner, int pass) {
    if (!renderer.mShouldRender) return;
    auto state = sample(client);
    auto const& positions = renderer.mIconPosition.get();
    diagnostics("heart", renderer, positions, context, client, owner, pass, state ? &*state : nullptr, config, logger);
    if (!isHostHudVisible(renderer, owner)) return;
    if (renderer.mBackgroundIcon.get().mCount == 0) return;
    if (!state || !config.showRegeneration || pass != config.renderPass
        || !renderer.mHasLoadedTextures || !state->food) return;
    auto meal = previewMeal(state->nutrition, *state->food, config.saturationCap);
    if (!meal) return;
    auto regen = previewRegeneration(*meal, state->regen);
    if (!regen || regen->highHp <= 0) return;
    float alpha = hudAlpha(renderer, client) * pulseAlpha(config);
    if (alpha <= 0.001f) return;
    // Only normal-health icon slots, excluding trailing absorption icons.
    auto count = std::min(positions.size(), static_cast<std::size_t>(std::ceil(state->regen.maxHealth / 2)));
    std::span<glm::vec3 const> healthPositions{positions.data(), count};
    auto texture = context.getTexture(ResourceLocation(Core::PathView("textures/melonrind/regen")), false);
    float hp = state->regen.health;
    // Keep uncertain extra healing dimmer, but visible over native empty hearts.
    // The former .65/.3 multipliers compounded with the pulse to near invisibility.
    drawRange(context, texture, healthPositions, owner, config, hp, hp + regen->lowHp, false, alpha, mce::Color(255, 255, 255));
    drawRange(context, texture, healthPositions, owner, config, hp + regen->lowHp, hp + regen->highHp, false, alpha * .6f, mce::Color(255, 255, 255));
}
} // namespace melonrind
