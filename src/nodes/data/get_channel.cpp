/**
 * GetChannel Node - Get channel info by ID (pure data node)
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <cstdlib>

static bool get_channel_execute(void* inst, ExecContext* ctx) {
    const char* channel_id_str = ctx->get_input_string(ctx, "ChannelID");

    if (!channel_id_str) {
        ctx->set_error(ctx, "ChannelID is required");
        return false;
    }

    dpp::snowflake channel_id = std::strtoull(channel_id_str, nullptr, 10);
    auto* cluster = BotManager::instance().get_cluster();

    if (!cluster) {
        ctx->set_error(ctx, "Discord bot not initialized");
        return false;
    }

    dpp::channel* cached = dpp::find_channel(channel_id);
    if (cached) {
        ctx->set_output_string(ctx, "Name", cached->name.c_str());
        ctx->set_output_string(ctx, "Topic", cached->topic.c_str());
        ctx->set_output_int(ctx, "Type", static_cast<int64_t>(cached->get_type()));
        return true;
    }

    ctx->set_output_string(ctx, "Name", "");
    ctx->set_output_string(ctx, "Topic", "");
    ctx->set_output_int(ctx, "Type", 0);
    return true;
}

static PinDesc get_channel_pins[] = {
    {"ChannelID", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Name", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"Topic", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"Type", "int", PIN_OUT, PIN_KIND_DATA, 0},
};

static NodeVTable get_channel_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    get_channel_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc get_channel_desc = {
    "Get Channel",
    "Discord/Data",
    "com.rune.discord.get_channel",
    get_channel_pins,
    4,
    NODE_FLAG_PURE_DATA,
    NULL, NULL,
    "Get channel information from cache by Channel ID"
};

void register_get_channel_node(PluginNodeRegistry* reg) {
    reg->register_node(&get_channel_desc, &get_channel_vtable);
}

