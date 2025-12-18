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
void register_send_message_node(PluginNodeRegistry* reg);
void register_send_embed_node(PluginNodeRegistry* reg);
void register_reply_to_message_node(PluginNodeRegistry* reg);
void register_get_user_node(PluginNodeRegistry* reg);
void register_get_channel_node(PluginNodeRegistry* reg);
void register_build_embed_node(PluginNodeRegistry* reg);

// Plugin configuration (from settings)
struct DiscordPluginConfig
{
    bool auto_connect;
    std::string token;
};

const DiscordPluginConfig& GetDiscordPluginConfig();
std::string ResolveDiscordToken(ExecContext* ctx);

#endif // RUNE_DISCORD_PLUGIN_H

