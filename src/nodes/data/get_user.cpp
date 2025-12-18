/**
 * GetUser Node - Get user info by ID (pure data node)
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <cstdlib>

static bool get_user_execute(void* inst, ExecContext* ctx) {
    const char* user_id_str = ctx->get_input_string(ctx, "UserID");

    if (!user_id_str) {
        ctx->set_error(ctx, "UserID is required");
        return false;
    }

    dpp::snowflake user_id = std::strtoull(user_id_str, nullptr, 10);
    auto* cluster = BotManager::instance().get_cluster();

    if (!cluster) {
        ctx->set_error(ctx, "Discord bot not initialized");
        return false;
    }

    // For pure data nodes, we return cached data if available
    // This is a simplified implementation - real async would use jobs
    dpp::user* cached = dpp::find_user(user_id);
    if (cached) {
        ctx->set_output_string(ctx, "Username", cached->username.c_str());
        ctx->set_output_string(ctx, "Discriminator", "0");
        ctx->set_output_bool(ctx, "IsBot", cached->is_bot());
        return true;
    }

    ctx->set_output_string(ctx, "Username", "");
    ctx->set_output_string(ctx, "Discriminator", "0");
    ctx->set_output_bool(ctx, "IsBot", false);
    return true;
}

static PinDesc get_user_pins[] = {
    {"UserID", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Username", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"Discriminator", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"IsBot", "bool", PIN_OUT, PIN_KIND_DATA, 0},
};

static NodeVTable get_user_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    get_user_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc get_user_desc = {
    "Get User",
    "Discord/Data",
    "com.rune.discord.get_user",
    get_user_pins,
    4,
    NODE_FLAG_PURE_DATA,
    NULL, NULL,
    "Get user information from cache by User ID"
};

void register_get_user_node(PluginNodeRegistry* reg) {
    reg->register_node(&get_user_desc, &get_user_vtable);
}

