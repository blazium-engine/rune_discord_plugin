/**
 * Discord Plugin - Main entry point
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

// Global host services pointer
HostServices* g_host = nullptr;

// Plugin configuration (backed by plugin settings)
static DiscordPluginConfig g_DiscordConfig{
    true,              // auto_connect
    std::string(),     // token
    0,                 // gateway_intents (0 = use DPP defaults)
    false,             // enable_message_content_intent
    true               // enable_dpp_logging
};

static void Discord_UpdateConfigFromJson(const char* settings_json)
{
    // Defaults
    g_DiscordConfig.auto_connect = true;
    g_DiscordConfig.token.clear();
    g_DiscordConfig.gateway_intents = 0;
    g_DiscordConfig.enable_message_content_intent = false;
    g_DiscordConfig.enable_dpp_logging = true;

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

        if (j.contains("gateway_intents") && j["gateway_intents"].is_number_unsigned())
        {
            g_DiscordConfig.gateway_intents = j["gateway_intents"].get<uint64_t>();
        }

        if (j.contains("enable_message_content_intent") && j["enable_message_content_intent"].is_boolean())
        {
            g_DiscordConfig.enable_message_content_intent =
                j["enable_message_content_intent"].get<bool>();
        }

        if (j.contains("enable_dpp_logging") && j["enable_dpp_logging"].is_boolean())
        {
            g_DiscordConfig.enable_dpp_logging = j["enable_dpp_logging"].get<bool>();
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
    // 1. Node input pin "Token" (runtime value)
    if (ctx)
    {
        const char* inputToken = ctx->get_input_string(ctx, "Token");
        if (inputToken && inputToken[0] != '\0')
        {
            return std::string(inputToken);
        }
    }

    // 2. Node property "Token" (static/default value on the node)
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
        // 3. Flow environment DISCORD_TOKEN
        const char* flowToken = RUNE_FLOW_ENV_GET(g_host, "DISCORD_TOKEN");
        if (flowToken && flowToken[0] != '\0')
        {
            return std::string(flowToken);
        }

        // 4. Application environment DISCORD_TOKEN
        const char* appToken = RUNE_APP_ENV_GET(g_host, "DISCORD_TOKEN");
        if (appToken && appToken[0] != '\0')
        {
            return std::string(appToken);
        }
    }

    // 5. Plugin settings token
    if (!g_DiscordConfig.token.empty())
    {
        return g_DiscordConfig.token;
    }

    return std::string();
}

// Ensure the Discord bot is connected based on current configuration and environment
static void EnsureDiscordBotConnectedFromConfig()
{
    if (!g_host)
        return;

    if (!g_DiscordConfig.auto_connect)
    {
        // Respect manual mode; do not auto-start the bot
        return;
    }

    if (BotManager::instance().is_running())
        return;

    std::string token = ResolveDiscordToken(nullptr);
    if (token.empty())
    {
        g_host->log(PLUGIN_LOG_LEVEL_ERROR,
            "Discord plugin: auto_connect is enabled but no Discord token is configured (settings/env)");
        return;
    }

    {
        std::string msg = "Discord plugin: auto-connect helper resolved token (length=";
        msg += std::to_string(token.size());
        msg += ")";
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
    }

    if (BotManager::instance().initialize(token))
    {
        g_host->log(PLUGIN_LOG_LEVEL_INFO,
            "Discord plugin: bot connected via auto_connect using configured token");
    }
    else
    {
        g_host->log(PLUGIN_LOG_LEVEL_ERROR,
            "Discord plugin: failed to initialize bot via auto_connect (check token and network)");
    }
}

void Discord_EnsureAutoConnectFromConfig()
{
    EnsureDiscordBotConnectedFromConfig();
}

// Helper: check if a given flow JSON contains any Discord On Ready nodes
static bool FlowHasDiscordOnReadyNode(const std::string& flowsDir, const std::string& flowId)
{
    if (flowsDir.empty() || flowId.empty())
        return false;

    std::string path = flowsDir;
    if (!path.empty() && path.back() != '\\' && path.back() != '/')
    {
#if defined(_WIN32)
        path += '\\';
#else
        path += '/';
#endif
    }
    path += flowId;
    path += "/flow.json";

    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            if (g_host)
            {
                std::string msg = "Discord_OnFlowLoaded: could not open flow.json at '" + path + "'";
                g_host->log(PLUGIN_LOG_LEVEL_WARN, msg.c_str());
            }
            return false;
        }

        json j;
        file >> j;

        if (!j.contains("nodes") || !j["nodes"].is_array())
            return false;

        for (const auto& node : j["nodes"])
        {
            if (node.contains("type") && node["type"].is_string())
            {
                const std::string type = node["type"].get<std::string>();
                if (type == "com.rune.discord.on_ready")
                {
                    return true;
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        if (g_host)
        {
            std::string msg = std::string("Discord_OnFlowLoaded: exception while parsing flow.json: ") + e.what();
            g_host->log(PLUGIN_LOG_LEVEL_ERROR, msg.c_str());
        }
    }

    return false;
}

static void Discord_OnFlowLoaded(const char* flow_id)
{
    if (!g_host)
        return;

    std::string flowId = flow_id ? flow_id : "";
    if (flowId.empty())
    {
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
            "Discord_OnFlowLoaded: called with empty flow id, skipping");
        return;
    }

    if (!g_DiscordConfig.auto_connect)
    {
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
            "Discord_OnFlowLoaded: auto_connect is false, skipping auto-connect for flow");
        return;
    }

    if (BotManager::instance().is_running())
    {
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
            "Discord_OnFlowLoaded: bot already running, skipping auto-connect");
        return;
    }

    const char* flowsDirC = RUNE_GET_SETTING(g_host, "flows_directory");
    if (!flowsDirC || !flowsDirC[0])
    {
        g_host->log(PLUGIN_LOG_LEVEL_WARN,
            "Discord_OnFlowLoaded: flows_directory setting not available; cannot inspect flow for On Ready nodes");
        return;
    }

    std::string flowsDir(flowsDirC);

    if (!FlowHasDiscordOnReadyNode(flowsDir, flowId))
    {
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
            "Discord_OnFlowLoaded: flow has no com.rune.discord.on_ready nodes; skipping auto-connect");
        return;
    }

    g_host->log(PLUGIN_LOG_LEVEL_INFO,
        "Discord_OnFlowLoaded: flow contains Discord On Ready node; requesting auto-connect");

    Discord_EnsureAutoConnectFromConfig();
}

// Node registration wrapper functions
void register_event_nodes(PluginNodeRegistry* reg) {
    register_on_ready_node(reg);
    register_on_message_node(reg);
    register_on_reaction_node(reg);
}

void register_action_nodes(PluginNodeRegistry* reg) {
    register_connect_discord_node(reg);
    register_send_message_node(reg);
    register_send_embed_node(reg);
    register_reply_to_message_node(reg);
    register_send_direct_message_node(reg);
    register_set_presence_node(reg);
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
    static bool s_LoggedSchema = false;
    if (!s_LoggedSchema && g_host)
    {
        g_host->log(PLUGIN_LOG_LEVEL_INFO, "Discord plugin: providing settings schema to host");
        s_LoggedSchema = true;
    }

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
            "},"
            "\"gateway_intents\":{"
                "\"type\":\"integer\","
                "\"description\":\"Override Discord gateway intents bitmask; 0 uses DPP defaults\""
            "},"
            "\"enable_message_content_intent\":{"
                "\"type\":\"boolean\","
                "\"description\":\"Request the Message Content intent (requires it to be enabled for the bot)\""
            "},"
            "\"enable_dpp_logging\":{"
                "\"type\":\"boolean\","
                "\"description\":\"Forward internal DPP log messages to the RUNE log\""
            "}"
        "}"
        "}";

    static const char* defaults =
        "{"
        "\"auto_connect\":true,"
        "\"token\":\"\","
        "\"gateway_intents\":0,"
        "\"enable_message_content_intent\":false,"
        "\"enable_dpp_logging\":true"
        "}";

    static PluginSettingsSchema s_Schema{ schema, defaults };
    return &s_Schema;
}

static void Discord_OnSettingsChanged(const char* settings_json)
{
    Discord_UpdateConfigFromJson(settings_json);

    if (g_host)
    {
        std::string msg = "Discord plugin: settings changed (auto_connect=";
        msg += g_DiscordConfig.auto_connect ? "true" : "false";
        msg += ", token=";
        msg += g_DiscordConfig.token.empty() ? "empty" : "set";
        msg += ")";
        g_host->log(PLUGIN_LOG_LEVEL_INFO, msg.c_str());
    }
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
    Discord_OnFlowLoaded,      // on_flow_loaded
    NULL,                      // on_flow_unloaded
    Discord_GetSettingsSchema, // get_settings_schema
    Discord_OnSettingsChanged, // on_settings_changed
    NULL                       // get_menus
};

NODEPLUG_EXPORT const PluginAPI* NodePlugin_GetAPI(void) {
    return &g_api;
}

