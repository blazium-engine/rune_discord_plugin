/**
 * OnReaction Node - Fires when a reaction is added
 */

#include "discord_plugin.h"
#include "bot_manager.h"

struct OnReactionInstance {
    ExecContext* ctx;
    bool listening;
    std::string user_id;
    std::string emoji;
    std::string message_id;
    std::string channel_id;
    std::string guild_id;
};

static void* on_reaction_create() {
    auto* inst = new OnReactionInstance();
    inst->ctx = nullptr;
    inst->listening = false;
    return inst;
}

static void on_reaction_destroy(void* inst_ptr) {
    delete static_cast<OnReactionInstance*>(inst_ptr);
}

static bool on_reaction_start_listening(void* inst_ptr, ExecContext* ctx) {
    auto* inst = static_cast<OnReactionInstance*>(inst_ptr);
    inst->ctx = ctx;
    inst->listening = true;

    BotManager::instance().add_reaction_listener([inst](const ReactionEventData& data) {
        if (inst && inst->listening && inst->ctx) {
            inst->user_id = std::to_string(static_cast<uint64_t>(data.user_id));
            inst->emoji = data.emoji;
            inst->message_id = std::to_string(static_cast<uint64_t>(data.message_id));
            inst->channel_id = std::to_string(static_cast<uint64_t>(data.channel_id));
            inst->guild_id = std::to_string(static_cast<uint64_t>(data.guild_id));

            inst->ctx->set_output_string(inst->ctx, "UserID", inst->user_id.c_str());
            inst->ctx->set_output_string(inst->ctx, "Emoji", inst->emoji.c_str());
            inst->ctx->set_output_string(inst->ctx, "MessageID", inst->message_id.c_str());
            inst->ctx->set_output_string(inst->ctx, "ChannelID", inst->channel_id.c_str());
            inst->ctx->set_output_string(inst->ctx, "GuildID", inst->guild_id.c_str());
            inst->ctx->trigger_output(inst->ctx, "OnReaction");
        }
    });

    return true;
}

static void on_reaction_stop_listening(void* inst_ptr) {
    auto* inst = static_cast<OnReactionInstance*>(inst_ptr);
    inst->listening = false;
    inst->ctx = nullptr;
}

static PinDesc on_reaction_pins[] = {
    {"OnReaction", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
    {"UserID", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"Emoji", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"MessageID", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"ChannelID", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"GuildID", "string", PIN_OUT, PIN_KIND_DATA, 0},
};

static NodeVTable on_reaction_vtable = {
    on_reaction_create,
    on_reaction_destroy,
    NULL, NULL,
    NULL, NULL,
    NULL,
    NULL, NULL,
    on_reaction_start_listening,
    on_reaction_stop_listening,
    NULL
};

static NodeDesc on_reaction_desc = {
    "On Reaction",
    "Discord/Events",
    "com.rune.discord.on_reaction",
    on_reaction_pins,
    6,
    NODE_FLAG_TRIGGER_EVENT,
    NULL, NULL,
    "Fires when a reaction is added to a message"
};

void register_on_reaction_node(PluginNodeRegistry* reg) {
    reg->register_node(&on_reaction_desc, &on_reaction_vtable);
}

