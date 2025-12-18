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

    // Legacy integer override (0 = use DPP defaults or per-intent flags)
    0,                 // gateway_intents

    // Per-intent flags (default to DPP's i_default_intents set)
    true,  // intent_guilds
    false, // intent_guild_members (privileged)
    true,  // intent_guild_bans
    true,  // intent_guild_emojis
    true,  // intent_guild_integrations
    true,  // intent_guild_webhooks
    true,  // intent_guild_invites
    true,  // intent_guild_voice_states
    false, // intent_guild_presences (privileged)
    true,  // intent_guild_messages
    true,  // intent_guild_message_reactions
    true,  // intent_guild_message_typing
    true,  // intent_direct_messages
    true,  // intent_direct_message_reactions
    true,  // intent_direct_message_typing
    false, // intent_message_content (privileged; controlled via enable_message_content_intent)
    true,  // intent_guild_scheduled_events
    true,  // intent_auto_moderation_configuration
    true,  // intent_auto_moderation_execution

    // Convenience toggles
    false, // enable_message_content_intent
    true   // enable_dpp_logging
};

static void Discord_UpdateConfigFromJson(const char* settings_json)
{
    // Defaults
    g_DiscordConfig.auto_connect = true;
    g_DiscordConfig.token.clear();
    g_DiscordConfig.gateway_intents = 0;

    // Reset per-intent flags to match DPP default intents by default
    g_DiscordConfig.intent_guilds = true;
    g_DiscordConfig.intent_guild_members = false;
    g_DiscordConfig.intent_guild_bans = true;
    g_DiscordConfig.intent_guild_emojis = true;
    g_DiscordConfig.intent_guild_integrations = true;
    g_DiscordConfig.intent_guild_webhooks = true;
    g_DiscordConfig.intent_guild_invites = true;
    g_DiscordConfig.intent_guild_voice_states = true;
    g_DiscordConfig.intent_guild_presences = false;
    g_DiscordConfig.intent_guild_messages = true;
    g_DiscordConfig.intent_guild_message_reactions = true;
    g_DiscordConfig.intent_guild_message_typing = true;
    g_DiscordConfig.intent_direct_messages = true;
    g_DiscordConfig.intent_direct_message_reactions = true;
    g_DiscordConfig.intent_direct_message_typing = true;
    g_DiscordConfig.intent_message_content = false;
    g_DiscordConfig.intent_guild_scheduled_events = true;
    g_DiscordConfig.intent_auto_moderation_configuration = true;
    g_DiscordConfig.intent_auto_moderation_execution = true;

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

        // New structured intent flags (preferred UI representation)
        if (j.contains("gateway_intent_flags") && j["gateway_intent_flags"].is_object())
        {
            const json& flags = j["gateway_intent_flags"];

            auto read_bool = [&flags](const char* key, bool& target) {
                auto it = flags.find(key);
                if (it != flags.end() && it->is_boolean())
                {
                    target = it->get<bool>();
                }
            };

            read_bool("guilds", g_DiscordConfig.intent_guilds);
            read_bool("guild_members", g_DiscordConfig.intent_guild_members);
            read_bool("guild_bans", g_DiscordConfig.intent_guild_bans);
            read_bool("guild_emojis", g_DiscordConfig.intent_guild_emojis);
            read_bool("guild_integrations", g_DiscordConfig.intent_guild_integrations);
            read_bool("guild_webhooks", g_DiscordConfig.intent_guild_webhooks);
            read_bool("guild_invites", g_DiscordConfig.intent_guild_invites);
            read_bool("guild_voice_states", g_DiscordConfig.intent_guild_voice_states);
            read_bool("guild_presences", g_DiscordConfig.intent_guild_presences);
            read_bool("guild_messages", g_DiscordConfig.intent_guild_messages);
            read_bool("guild_message_reactions", g_DiscordConfig.intent_guild_message_reactions);
            read_bool("guild_message_typing", g_DiscordConfig.intent_guild_message_typing);
            read_bool("direct_messages", g_DiscordConfig.intent_direct_messages);
            read_bool("direct_message_reactions", g_DiscordConfig.intent_direct_message_reactions);
            read_bool("direct_message_typing", g_DiscordConfig.intent_direct_message_typing);
            read_bool("message_content", g_DiscordConfig.intent_message_content);
            read_bool("guild_scheduled_events", g_DiscordConfig.intent_guild_scheduled_events);
            read_bool("auto_moderation_configuration", g_DiscordConfig.intent_auto_moderation_configuration);
            read_bool("auto_moderation_execution", g_DiscordConfig.intent_auto_moderation_execution);
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

// Normalize a Discord bot token before use.
// - Strips a leading "Bot " prefix (case-insensitive) if present.
// - Logs a warning if the token looks unusually short.
static std::string NormalizeDiscordToken(const std::string& raw)
{
    std::string token = raw;

    if (!token.empty())
    {
        // Strip optional "Bot " prefix (case-insensitive)
        const char prefix[] = "Bot ";
        const size_t prefixLen = sizeof(prefix) - 1;

        if (token.size() >= prefixLen)
        {
            bool match = true;
            for (size_t i = 0; i < prefixLen; ++i)
            {
                char c1 = token[i];
                char c2 = prefix[i];

                // tolower without <cctype> locale complications
                if (c1 >= 'A' && c1 <= 'Z')
                    c1 = static_cast<char>(c1 - 'A' + 'a');
                if (c2 >= 'A' && c2 <= 'Z')
                    c2 = static_cast<char>(c2 - 'A' + 'a');

                if (c1 != c2)
                {
                    match = false;
                    break;
                }
            }

            if (match)
            {
                token.erase(0, prefixLen);
                if (g_host)
                {
                    g_host->log(PLUGIN_LOG_LEVEL_WARN,
                        "Discord plugin: token started with 'Bot ' prefix; prefix has been stripped before connecting");
                }
            }
        }

        // Discord bot tokens are typically long; warn if the resolved token is suspiciously short.
        if (token.size() > 0 && token.size() < 30)
        {
            if (g_host)
            {
                g_host->log(PLUGIN_LOG_LEVEL_WARN,
                    "Discord plugin: resolved Discord token appears unusually short; verify that it is correct");
            }
        }
    }

    return token;
}

std::string ResolveDiscordToken(ExecContext* ctx)
{
    // 1. Node input pin "Token" (runtime value)
    if (ctx)
    {
        const char* inputToken = ctx->get_input_string(ctx, "Token");
        if (inputToken && inputToken[0] != '\0')
        {
            return NormalizeDiscordToken(std::string(inputToken));
        }
    }

    // 2. Node property "Token" (static/default value on the node)
    if (ctx)
    {
        const char* propToken = ctx->get_property(ctx, "Token");
        if (propToken && propToken[0] != '\0')
        {
            return NormalizeDiscordToken(std::string(propToken));
        }
    }

    if (g_host)
    {
        // 3. Flow environment DISCORD_TOKEN
        const char* flowToken = RUNE_FLOW_ENV_GET(g_host, "DISCORD_TOKEN");
        if (flowToken && flowToken[0] != '\0')
        {
            return NormalizeDiscordToken(std::string(flowToken));
        }

        // 4. Application environment DISCORD_TOKEN
        const char* appToken = RUNE_APP_ENV_GET(g_host, "DISCORD_TOKEN");
        if (appToken && appToken[0] != '\0')
        {
            return NormalizeDiscordToken(std::string(appToken));
        }
    }

    // 5. Plugin settings token
    if (!g_DiscordConfig.token.empty())
    {
        return NormalizeDiscordToken(g_DiscordConfig.token);
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
                "\"description\":\"Discord bot token (raw token from Discord Developer Portal, without the 'Bot ' prefix; optional if provided via DISCORD_TOKEN env or node property)\""
            "},"
            "\"gateway_intent_flags\":{"
                "\"type\":\"object\","
                "\"description\":\"Primary control surface for Discord gateway intents; each checkbox maps to a specific intent bit. If any flags are set, they are combined into the effective intents bitmask.\","
                "\"properties\":{"
                    "\"guilds\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILDS intent (guild information)\""
                    "},"
                    "\"guild_members\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_MEMBERS intent (privileged)\""
                    "},"
                    "\"guild_bans\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_BANS intent\""
                    "},"
                    "\"guild_emojis\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_EMOJIS intent\""
                    "},"
                    "\"guild_integrations\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_INTEGRATIONS intent\""
                    "},"
                    "\"guild_webhooks\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_WEBHOOKS intent\""
                    "},"
                    "\"guild_invites\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_INVITES intent\""
                    "},"
                    "\"guild_voice_states\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_VOICE_STATES intent\""
                    "},"
                    "\"guild_presences\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_PRESENCES intent (privileged)\""
                    "},"
                    "\"guild_messages\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_MESSAGES intent\""
                    "},"
                    "\"guild_message_reactions\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_MESSAGE_REACTIONS intent\""
                    "},"
                    "\"guild_message_typing\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_MESSAGE_TYPING intent\""
                    "},"
                    "\"direct_messages\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"DIRECT_MESSAGES intent\""
                    "},"
                    "\"direct_message_reactions\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"DIRECT_MESSAGE_REACTIONS intent\""
                    "},"
                    "\"direct_message_typing\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"DIRECT_MESSAGE_TYPING intent\""
                    "},"
                    "\"message_content\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"MESSAGE_CONTENT intent (privileged; also controlled by the Message Content toggle)\""
                    "},"
                    "\"guild_scheduled_events\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"GUILD_SCHEDULED_EVENTS intent\""
                    "},"
                    "\"auto_moderation_configuration\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"AUTO_MODERATION_CONFIGURATION intent\""
                    "},"
                    "\"auto_moderation_execution\":{"
                        "\"type\":\"boolean\","
                        "\"description\":\"AUTO_MODERATION_EXECUTION intent\""
                    "}"
                "}"
            "},"
            "\"gateway_intents\":{"
                "\"type\":\"integer\","
                "\"description\":\"Advanced: raw Discord gateway intents bitmask. When non-zero and no flags are set, this overrides the computed flags-based bitmask; 0 uses defaults/flags.\""
            "},"
            "\"enable_message_content_intent\":{"
                "\"type\":\"boolean\","
                "\"description\":\"Request the Message Content intent (requires it to be enabled for the bot). This always ORs the MESSAGE_CONTENT bit into the effective intents.\""
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
        "\"gateway_intent_flags\":{"
            "\"guilds\":true,"
            "\"guild_members\":false,"
            "\"guild_bans\":true,"
            "\"guild_emojis\":true,"
            "\"guild_integrations\":true,"
            "\"guild_webhooks\":true,"
            "\"guild_invites\":true,"
            "\"guild_voice_states\":true,"
            "\"guild_presences\":false,"
            "\"guild_messages\":true,"
            "\"guild_message_reactions\":true,"
            "\"guild_message_typing\":true,"
            "\"direct_messages\":true,"
            "\"direct_message_reactions\":true,"
            "\"direct_message_typing\":true,"
            "\"message_content\":false,"
            "\"guild_scheduled_events\":true,"
            "\"auto_moderation_configuration\":true,"
            "\"auto_moderation_execution\":true"
        "},"
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

