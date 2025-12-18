/**
 * SendMessage Node - Send a message to a channel
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <cstdlib>

static bool send_message_execute(void* inst, ExecContext* ctx) {
    (void)inst;

    const char* channel_id_str = ctx->get_input_string(ctx, "ChannelID");
    const char* content = ctx->get_input_string(ctx, "Content");

    if (!channel_id_str || channel_id_str[0] == '\0' ||
        !content || content[0] == '\0') {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_ERROR,
                "Discord Send Message: ChannelID and Content are required");
        }
        ctx->set_error(ctx, "ChannelID and Content are required");
        return false;
    }

    dpp::snowflake channel_id = std::strtoull(channel_id_str, nullptr, 10);

    if (g_host) {
        std::string msg = "Discord Send Message: sending to channel ";
        msg += channel_id_str;
        msg += " (bot_running=";
        msg += BotManager::instance().is_running() ? "true" : "false";
        msg += ")";
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
    }

    BotManager::instance().send_message(channel_id, content);

    if (g_host) {
        g_host->log(PLUGIN_LOG_LEVEL_INFO,
            "Discord Send Message: Done output triggered");
    }

    ctx->trigger_output(ctx, "Done");
    return true;
}

static PinDesc send_message_pins[] = {
    {"Exec", "execution", PIN_IN, PIN_KIND_EXECUTION, 0},
    {"ChannelID", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Content", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Done", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
};

static NodeVTable send_message_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    send_message_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc send_message_desc = {
    "Send Message",
    "Discord/Actions",
    "com.rune.discord.send_message",
    send_message_pins,
    4,
    NODE_FLAG_NONE,
    NULL, NULL,
    "Send a text message to a Discord channel"
};

void register_send_message_node(PluginNodeRegistry* reg) {
    reg->register_node(&send_message_desc, &send_message_vtable);
}

