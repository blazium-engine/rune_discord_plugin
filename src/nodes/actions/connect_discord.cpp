/**
 * ConnectDiscord Node - Explicitly connect the Discord bot
 */

#include "discord_plugin.h"
#include "bot_manager.h"

static bool connect_discord_execute(void* inst, ExecContext* ctx) {
    // Resolve token from input, env, or plugin settings
    const char* explicitTokenCStr = ctx->get_input_string(ctx, "Token");
    std::string explicitToken = (explicitTokenCStr && explicitTokenCStr[0] != '\0')
        ? std::string(explicitTokenCStr)
        : std::string();

    std::string token = ResolveDiscordToken(ctx);

    if (g_host) {
        std::string msg = "Discord Connect Discord: execute (source=";
        msg += !explicitToken.empty() ? "Token pin" : "env/settings";
        msg += ", resolved_length=";
        msg += std::to_string(token.size());
        msg += ", bot_running=";
        msg += BotManager::instance().is_running() ? "true" : "false";
        msg += ")";
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
    }
    if (token.empty()) {
        ctx->set_error(ctx, "Discord bot token is required to connect");
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_ERROR,
                "Discord plugin: Connect Discord node could not resolve a token");
        }
        return false;
    }

    if (BotManager::instance().is_running()) {
        // Already running; treat as success and just continue the flow
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_INFO,
                "Discord plugin: Connect Discord node called but bot is already running");
        }
        ctx->trigger_output(ctx, "Connected");
        return true;
    }

    if (!BotManager::instance().initialize(token)) {
        ctx->set_error(ctx, "Failed to initialize Discord bot from Connect Discord node");
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_ERROR,
                "Discord plugin: Connect Discord node failed to initialize bot (check token and network)");
        }
        return false;
    }

    if (g_host) {
        g_host->log(PLUGIN_LOG_LEVEL_INFO,
            "Discord plugin: bot connected via Connect Discord node");
    }

    ctx->trigger_output(ctx, "Connected");
    return true;
}

static PinDesc connect_discord_pins[] = {
    {"Exec", "execution", PIN_IN,  PIN_KIND_EXECUTION, 0},
    {"Token", "string",   PIN_IN,  PIN_KIND_DATA,      0},
    {"Connected", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
};

static NodeVTable connect_discord_vtable = {
    NULL, NULL,   // create, destroy
    NULL, NULL,   // draw
    NULL, NULL,   // serialize
    connect_discord_execute,
    NULL, NULL,   // pre/post execute
    NULL, NULL,   // start/stop listening
    NULL          // is_complete
};

static NodeDesc connect_discord_desc = {
    "Connect Discord",
    "Discord/Actions",
    "com.rune.discord.connect",
    connect_discord_pins,
    3,
    NODE_FLAG_NONE,
    NULL, NULL,
    "Connect the Discord bot using the configured or provided token"
};

void register_connect_discord_node(PluginNodeRegistry* reg) {
    reg->register_node(&connect_discord_desc, &connect_discord_vtable);
}


