#include "client/Runtime.h"

#include "ll/api/io/Logger.h"
#include "mc/client/game/IClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/client/options/IOptions.h"
#include "mc/network/packet/UpdateAttributesPacket.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/attribute/AttributeData.h"
#include "mc/world/attribute/SharedAttributes.h"
#include "mc/world/effect/MobEffect.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/components/IFoodItemComponent.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/storage/GameRules.h"
#include "mc/world/level/storage/GameRuleId.h"

#include <cmath>

namespace melonrind {
Runtime& Runtime::instance() { static Runtime runtime; return runtime; }

void Runtime::reset() {
    std::lock_guard lock(mutex);
    observation.clear();
}

void Runtime::observe(IClientInstance& client, UpdateAttributesPacket const& packet) {
    auto* player = client.getLocalPlayer();
    if (!player || player->getRuntimeID() != packet.mRuntimeId.get()) return;
    std::lock_guard lock(mutex);
    auto const id = static_cast<std::uint64_t>(packet.mRuntimeId.get());
    auto const tick = packet.mTick->mValue;
    auto const identity = reinterpret_cast<std::uintptr_t>(player);
    if (!player->isAlive()) {
        observation.forget(identity);
        return;
    }
    // Attribute packets are deltas. A packet without S must not erase the last S.
    if (!observation.receive(identity, id, tick, {})) return;
    for (auto const& attribute : packet.mAttributeData.get()) {
        auto const& name = attribute.mName->getString();
        bool const saturation = name == "minecraft:player.saturation";
        bool const relevant = saturation || name == "minecraft:player.hunger" || name == "minecraft:health";
        if (!relevant) continue;
        auto const value = attribute.mCurrentValue;
        if (saturation) {
            observation.receive(identity, id, tick, value);
        }
        if (config.diagnostics && logger) logger->info("attribute id={} tick={} {}={} max={}", id, tick, name, value, attribute.mMaxValue);
    }
}

std::optional<Snapshot> Runtime::sample(IClientInstance& client) {
    auto* player = client.getLocalPlayer();
    if (!player) return {};
    if (!player->isAlive()) {
        std::lock_guard lock(mutex);
        observation.forget(reinterpret_cast<std::uintptr_t>(player));
        return {};
    }
    if (player->isCreative() || player->isSpectator() || client.getOptions().getHideHud()) return {};
    auto hunger = player->getAttribute(Player::HUNGER()).mPtr;
    auto health = player->getAttribute(SharedAttributes::HEALTH()).mPtr;
    if (!hunger || !health) return {};
    Snapshot result;
    result.runtimeId = static_cast<std::uint64_t>(player->getRuntimeID());
    result.nutrition.hunger = hunger->mCurrentValue;
    result.nutrition.maxHunger = hunger->mCurrentMaxValue;
    // The native current saturation maximum follows CURRENT hunger (observed
    // 0 -> 8 -> 16 after porkchops). It is not the cap for the NEXT meal.
    result.nutrition.maxSaturation = 20;
    {
        std::lock_guard lock(mutex);
        result.nutrition.saturation = observation.get(reinterpret_cast<std::uintptr_t>(player), result.runtimeId);
    }
    result.regen.health = health->mCurrentValue;
    result.regen.maxHealth = health->mCurrentMaxValue;
    result.regen.peaceful = static_cast<int>(player->getLevel().getDifficulty()) == 0;
    result.regen.allowUnknownRule = config.estimateUnknownRule;
    // A false client rule suppresses prediction; true is still provisional until
    // the remote rule synchronization path has been verified for this version.
    auto const& rules = player->getLevel().getGameRules();
    auto id = rules.nameToGameRuleIndex("naturalregeneration");
    if (!rules.getBool(id, true)) result.regen.naturalRegeneration = RuleState::Disabled;
    for (auto* effect : {MobEffect::REGENERATION(), MobEffect::POISON(), MobEffect::WITHER(), MobEffect::HUNGER()}) {
        if (effect && player->getEffect(*effect)) result.regen.suppressed = true;
    }
    auto const& stack = player->getSelectedItem();
    if (auto* item = stack.getItem()) {
        if (auto* food = item->getFood()) {
            result.itemName = stack.getTypeName();
            bool uncertain = result.itemName == "minecraft:golden_apple"
                || result.itemName == "minecraft:enchanted_golden_apple"
                || result.itemName == "minecraft:appleenchanted"
                || result.itemName == "minecraft:suspicious_stew"
                || result.itemName == "minecraft:rotten_flesh"
                || result.itemName == "minecraft:pufferfish"
                || result.itemName == "minecraft:poisonous_potato"
                || result.itemName == "minecraft:spider_eye"
                || result.itemName == "minecraft:chicken"
                || !result.itemName.starts_with("minecraft:");
            result.food = Food{food->getNutrition(), food->getSaturationModifier(), food->canAlwaysEat(), uncertain};
            result.regen.suppressed |= uncertain;
        }
    }
    return result;
}
} // namespace melonrind
