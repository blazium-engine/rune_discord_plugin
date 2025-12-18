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

    const DiscordPluginConfig& cfg = GetDiscordPluginConfig();

    // Log current configuration and bot state when this node begins listening
    if (g_host) {
        std::string msg = "Discord On Ready: start_listening (auto_connect=";
        msg += cfg.auto_connect ? "true" : "false";
        msg += ", bot_running=";
        msg += BotManager::instance().is_running() ? "true" : "false";
        msg += ", gateway_intents=";
        msg += std::to_string(cfg.gateway_intents);
        msg += ", enable_message_content_intent=";
        msg += cfg.enable_message_content_intent ? "true" : "false";
        msg += ", enable_dpp_logging=";
        msg += cfg.enable_dpp_logging ? "true" : "false";
        msg += ", ready_already_fired=";
        msg += BotManager::instance().has_ready_fired() ? "true" : "false";
        msg += ")";
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
    }

    // In auto-connect mode, ensure the bot is running when this node starts listening.
    if (cfg.auto_connect && !BotManager::instance().is_running()) {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_INFO,
                "Discord On Ready: auto_connect is true and bot is not running; requesting Discord auto-connect");
        }
        Discord_EnsureAutoConnectFromConfig();
    }
    else if (!cfg.auto_connect && !BotManager::instance().is_running()) {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_WARN,
                "Discord On Ready: auto_connect is false and bot is not running; use Connect Discord node before On Ready will fire");
        }
    }

    BotManager::instance().add_ready_listener([inst]() {
        if (inst && inst->listening && inst->ctx) {
            if (g_host) {
                g_host->log(PLUGIN_LOG_LEVEL_INFO,
                    "Discord On Ready: Ready event received, triggering OnReady output");
            }
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

