/**
 * SendEmbed Node - Send an embed to a channel
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <dpp/dpp.h>
#include <cstdlib>

static bool send_embed_execute(void* inst, ExecContext* ctx) {
    const char* channel_id_str = ctx->get_input_string(ctx, "ChannelID");
    const char* title = ctx->get_input_string(ctx, "Title");
    const char* description = ctx->get_input_string(ctx, "Description");
    int64_t color = ctx->get_input_int(ctx, "Color");

    if (!channel_id_str) {
        ctx->set_error(ctx, "ChannelID is required");
        return false;
    }

    dpp::snowflake channel_id = std::strtoull(channel_id_str, nullptr, 10);

    dpp::embed embed;
    if (title) embed.set_title(title);
    if (description) embed.set_description(description);
    embed.set_color(static_cast<uint32_t>(color));

    BotManager::instance().send_embed(channel_id, embed);

    ctx->trigger_output(ctx, "Done");
    return true;
}

static PinDesc send_embed_pins[] = {
    {"Exec", "execution", PIN_IN, PIN_KIND_EXECUTION, 0},
    {"ChannelID", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Title", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Description", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Color", "int", PIN_IN, PIN_KIND_DATA, 0},
    {"Done", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
};

static NodeVTable send_embed_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    send_embed_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc send_embed_desc = {
    "Send Embed",
    "Discord/Actions",
    "com.rune.discord.send_embed",
    send_embed_pins,
    6,
    NODE_FLAG_NONE,
    NULL, NULL,
    "Send a rich embed message to a Discord channel"
};

void register_send_embed_node(PluginNodeRegistry* reg) {
    reg->register_node(&send_embed_desc, &send_embed_vtable);
}

