/**
 * Discord Plugin - Main entry point
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Global host services pointer
HostServices* g_host = nullptr;

// Plugin configuration (backed by plugin settings)
static DiscordPluginConfig g_DiscordConfig{ true, std::string() };

static void Discord_UpdateConfigFromJson(const char* settings_json)
{
    // Defaults
    g_DiscordConfig.auto_connect = true;
    g_DiscordConfig.token.clear();

    if (!settings_json || !*settings_json)
        return;

    try
    {
        json j = json::parse(settings_json);

        if (j.contains("auto_connect") && j["auto_connect"].is_boolean())
        {
            g_DiscordConfig.auto_connect = j["auto_connect"].get<bool>();
        }

        if (j.contains("token") && j["token"].is_string())
        {
            g_DiscordConfig.token = j["token"].get<std::string>();
        }
    }
    catch (const std::exception& e)
    {
        if (g_host)
        {
            g_host->log(PLUGIN_LOG_LEVEL_ERROR, "Discord plugin: failed to parse settings JSON");
        }
    }
}

const DiscordPluginConfig& GetDiscordPluginConfig()
{
    return g_DiscordConfig;
}

std::string ResolveDiscordToken(ExecContext* ctx)
{
    // 1. Node property "Token"
    if (ctx)
    {
        const char* propToken = ctx->get_property(ctx, "Token");
        if (propToken && propToken[0] != '\0')
        {
            return std::string(propToken);
        }
    }

    if (g_host)
    {
        // 2. Flow environment DISCORD_TOKEN
        const char* flowToken = RUNE_FLOW_ENV_GET(g_host, "DISCORD_TOKEN");
        if (flowToken && flowToken[0] != '\0')
        {
            return std::string(flowToken);
        }

        // 3. Application environment DISCORD_TOKEN
        const char* appToken = RUNE_APP_ENV_GET(g_host, "DISCORD_TOKEN");
        if (appToken && appToken[0] != '\0')
        {
            return std::string(appToken);
        }
    }

    // 4. Plugin settings token
    if (!g_DiscordConfig.token.empty())
    {
        return g_DiscordConfig.token;
    }

    return std::string();
}

// Node registration wrapper functions
void register_event_nodes(PluginNodeRegistry* reg) {
    register_on_ready_node(reg);
    register_on_message_node(reg);
    register_on_reaction_node(reg);
}

void register_action_nodes(PluginNodeRegistry* reg) {
    register_send_message_node(reg);
    register_send_embed_node(reg);
    register_reply_to_message_node(reg);
}

void register_data_nodes(PluginNodeRegistry* reg) {
    register_get_user_node(reg);
    register_get_channel_node(reg);
    register_build_embed_node(reg);
}

static bool on_load(HostServices* host) {
    g_host = host;
    host->log(PLUGIN_LOG_LEVEL_INFO, "Discord plugin loaded");

    // Load initial settings into config (if available)
    const char* settings_json = RUNE_GET_PLUGIN_SETTINGS(host, "com.rune.discord");
    Discord_UpdateConfigFromJson(settings_json);

    return true;
}

static void on_register(PluginNodeRegistry* reg, LuauRegistry* luau) {
    register_event_nodes(reg);
    register_action_nodes(reg);
    register_data_nodes(reg);

    if (g_host) {
        g_host->log(PLUGIN_LOG_LEVEL_INFO, "Discord nodes registered");
    }
}

static void on_unload(void) {
    BotManager::instance().shutdown();
    g_host = nullptr;
}

static void on_tick(float delta_time) {
    BotManager::instance().tick();
}

static const PluginSettingsSchema* Discord_GetSettingsSchema(void)
{
    // Compact JSON strings to avoid compiler issues with raw multi-line strings
    static const char* schema =
        "{"
        "\"type\":\"object\","
        "\"properties\":{"
            "\"auto_connect\":{"
                "\"type\":\"boolean\","
                "\"description\":\"Automatically connect the Discord bot when needed\""
            "},"
            "\"token\":{"
                "\"type\":\"string\","
                "\"description\":\"Discord bot token (optional if provided via env or node property)\""
            "}"
        "}"
        "}";

    static const char* defaults =
        "{"
        "\"auto_connect\":true,"
        "\"token\":\"\""
        "}";

    static PluginSettingsSchema s_Schema{ schema, defaults };
    return &s_Schema;
}

static void Discord_OnSettingsChanged(const char* settings_json)
{
    Discord_UpdateConfigFromJson(settings_json);
}

static PluginAPI g_api = {
    {
        "com.rune.discord",          // id
        "Discord Bot",               // name
        "1.0.0",                     // version
        "RUNE Team",                 // author
        "Discord bot integration",   // description
        RUNE_PLUGIN_API_VERSION      // api_version
    },
    on_load,
    on_register,
    on_unload,
    on_tick,
    NULL,                      // on_flow_loaded
    NULL,                      // on_flow_unloaded
    Discord_GetSettingsSchema, // get_settings_schema
    Discord_OnSettingsChanged, // on_settings_changed
    NULL                       // get_menus
};

NODEPLUG_EXPORT const PluginAPI* NodePlugin_GetAPI(void) {
    return &g_api;
}

