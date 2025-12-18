/**
 * Discord Plugin - Main header
 */

#ifndef RUNE_DISCORD_PLUGIN_H
#define RUNE_DISCORD_PLUGIN_H

#ifndef NODEPLUG_BUILDING
#define NODEPLUG_BUILDING
#endif
#include <rune_plugin.h>
#include <string>

// Shared host services pointer
extern HostServices* g_host;

// Node registration functions (called from discord_plugin.cpp)
void register_event_nodes(PluginNodeRegistry* reg);
void register_action_nodes(PluginNodeRegistry* reg);
void register_data_nodes(PluginNodeRegistry* reg);

// Individual node registrations
void register_on_ready_node(PluginNodeRegistry* reg);
void register_on_message_node(PluginNodeRegistry* reg);
void register_on_reaction_node(PluginNodeRegistry* reg);
void register_connect_discord_node(PluginNodeRegistry* reg);
void register_send_message_node(PluginNodeRegistry* reg);
void register_send_embed_node(PluginNodeRegistry* reg);
void register_reply_to_message_node(PluginNodeRegistry* reg);
void register_send_direct_message_node(PluginNodeRegistry* reg);
void register_set_presence_node(PluginNodeRegistry* reg);
void register_get_user_node(PluginNodeRegistry* reg);
void register_get_channel_node(PluginNodeRegistry* reg);
void register_build_embed_node(PluginNodeRegistry* reg);

// Plugin configuration (from settings)
struct DiscordPluginConfig
{
    bool auto_connect;
    std::string token;

    // 0 means \"use DPP defaults\"; non-zero overrides gateway intents bitmask.
    // This is preserved for backward compatibility and advanced overrides.
    uint64_t gateway_intents;

    // Per-intent flags (derived from/stored in plugin settings).
    // These map directly to dpp::intents bits and are combined into
    // the effective gateway intents bitmask when connecting.
    bool intent_guilds;
    bool intent_guild_members;
    bool intent_guild_bans;
    bool intent_guild_emojis;
    bool intent_guild_integrations;
    bool intent_guild_webhooks;
    bool intent_guild_invites;
    bool intent_guild_voice_states;
    bool intent_guild_presences;
    bool intent_guild_messages;
    bool intent_guild_message_reactions;
    bool intent_guild_message_typing;
    bool intent_direct_messages;
    bool intent_direct_message_reactions;
    bool intent_direct_message_typing;
    bool intent_message_content;
    bool intent_guild_scheduled_events;
    bool intent_auto_moderation_configuration;
    bool intent_auto_moderation_execution;

    // Convenience toggles
    bool enable_message_content_intent;
    bool enable_dpp_logging;
};

const DiscordPluginConfig& GetDiscordPluginConfig();
std::string ResolveDiscordToken(ExecContext* ctx);
void Discord_EnsureAutoConnectFromConfig();

#endif // RUNE_DISCORD_PLUGIN_H

