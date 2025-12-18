/**
 * ReplyToMessage Node - Reply to a specific message
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <cstdlib>

static bool reply_execute(void* inst, ExecContext* ctx) {
    const char* channel_id_str = ctx->get_input_string(ctx, "ChannelID");
    const char* message_id_str = ctx->get_input_string(ctx, "MessageID");
    const char* content = ctx->get_input_string(ctx, "Content");

    if (!channel_id_str || !message_id_str || !content) {
        ctx->set_error(ctx, "ChannelID, MessageID, and Content are required");
        return false;
    }

    dpp::snowflake channel_id = std::strtoull(channel_id_str, nullptr, 10);
    dpp::snowflake message_id = std::strtoull(message_id_str, nullptr, 10);

    BotManager::instance().reply_to_message(channel_id, message_id, content);

    ctx->trigger_output(ctx, "Done");
    return true;
}

static PinDesc reply_pins[] = {
    {"Exec", "execution", PIN_IN, PIN_KIND_EXECUTION, 0},
    {"ChannelID", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"MessageID", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Content", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Done", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
};

static NodeVTable reply_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    reply_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc reply_desc = {
    "Reply To Message",
    "Discord/Actions",
    "com.rune.discord.reply_to_message",
    reply_pins,
    5,
    NODE_FLAG_NONE,
    NULL, NULL,
    "Reply to a specific Discord message"
};

void register_reply_to_message_node(PluginNodeRegistry* reg) {
    reg->register_node(&reply_desc, &reply_vtable);
}

