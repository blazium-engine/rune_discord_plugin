/**
 * SendDirectMessage Node - Send a private message (DM) to a user
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <cstdlib>

static bool send_direct_message_execute(void* inst, ExecContext* ctx) {
    (void)inst;

    const char* user_id_str = ctx->get_input_string(ctx, "UserID");
    const char* content = ctx->get_input_string(ctx, "Content");

    if (!user_id_str || user_id_str[0] == '\0' ||
        !content || content[0] == '\0') {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_ERROR,
                "Discord Send Direct Message: UserID and Content are required");
        }
        ctx->set_error(ctx, "UserID and Content are required");
        return false;
    }

    dpp::snowflake user_id = std::strtoull(user_id_str, nullptr, 10);

    if (g_host) {
        std::string msg = "Discord Send Direct Message: sending DM to user ";
        msg += user_id_str;
        msg += " (bot_running=";
        msg += BotManager::instance().is_running() ? "true" : "false";
        msg += ")";
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
    }

    BotManager::instance().send_direct_message(user_id, content);

    if (g_host) {
        g_host->log(PLUGIN_LOG_LEVEL_INFO,
            "Discord Send Direct Message: Done output triggered");
    }

    ctx->trigger_output(ctx, "Done");
    return true;
}

static PinDesc send_direct_message_pins[] = {
    {"Exec", "execution", PIN_IN, PIN_KIND_EXECUTION, 0},
    {"UserID", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Content", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Done", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
};

static NodeVTable send_direct_message_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    send_direct_message_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc send_direct_message_desc = {
    "Send Direct Message",
    "Discord/Actions",
    "com.rune.discord.send_direct_message",
    send_direct_message_pins,
    4,
    NODE_FLAG_NONE,
    NULL, NULL,
    "Send a private message (DM) to a Discord user by ID"
};

void register_send_direct_message_node(PluginNodeRegistry* reg) {
    reg->register_node(&send_direct_message_desc, &send_direct_message_vtable);
}


