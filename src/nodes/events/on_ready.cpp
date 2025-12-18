/**
 * OnReady Node - Fires when Discord bot connects
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <cstring>
#include <string>

// Instance data
struct OnReadyInstance {
    ExecContext* ctx;
    bool listening;
};

static void* on_ready_create() {
    auto* inst = new OnReadyInstance();
    inst->ctx = nullptr;
    inst->listening = false;
    return inst;
}

static void on_ready_destroy(void* inst_ptr) {
    delete static_cast<OnReadyInstance*>(inst_ptr);
}

static bool on_ready_start_listening(void* inst_ptr, ExecContext* ctx) {
    auto* inst = static_cast<OnReadyInstance*>(inst_ptr);
    inst->ctx = ctx;
    inst->listening = true;

    // Resolve token from property, flow/app env, or plugin settings
    std::string token = ResolveDiscordToken(ctx);
    if (token.empty()) {
        ctx->set_error(ctx, "Discord bot token is required");
        return false;
    }

    const DiscordPluginConfig& cfg = GetDiscordPluginConfig();

    // Initialize bot if not running and auto_connect is enabled
    if (cfg.auto_connect && !BotManager::instance().is_running()) {
        if (!BotManager::instance().initialize(token)) {
            ctx->set_error(ctx, "Failed to initialize Discord bot");
            return false;
        }
    }

    BotManager::instance().add_ready_listener([inst]() {
        if (inst && inst->listening && inst->ctx) {
            inst->ctx->trigger_output(inst->ctx, "OnReady");
        }
    });

    return true;
}

static void on_ready_stop_listening(void* inst_ptr) {
    auto* inst = static_cast<OnReadyInstance*>(inst_ptr);
    inst->listening = false;
    inst->ctx = nullptr;
}

static PinDesc on_ready_pins[] = {
    {"Token", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"OnReady", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
};

static NodeVTable on_ready_vtable = {
    on_ready_create,
    on_ready_destroy,
    NULL, NULL,  // draw
    NULL, NULL,  // serialize
    NULL,        // execute
    NULL, NULL,  // pre/post execute
    on_ready_start_listening,
    on_ready_stop_listening,
    NULL         // is_complete
};

static NodeDesc on_ready_desc = {
    "On Ready",
    "Discord/Events",
    "com.rune.discord.on_ready",
    on_ready_pins,
    2,
    NODE_FLAG_TRIGGER_EVENT,
    NULL, NULL,
    "Fires when the Discord bot successfully connects"
};

// Registration called from discord_plugin.cpp
void register_on_ready_node(PluginNodeRegistry* reg) {
    reg->register_node(&on_ready_desc, &on_ready_vtable);
}

