/**
 * OnMessage Node - Fires when a message is received
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <cstring>

struct OnMessageInstance {
    ExecContext* ctx;
    bool listening;
    // Cached data for output
    std::string author_id;
    std::string author_name;
    std::string content;
    std::string channel_id;
    std::string guild_id;
    std::string message_id;
};

static void* on_message_create() {
    auto* inst = new OnMessageInstance();
    inst->ctx = nullptr;
    inst->listening = false;
    return inst;
}

static void on_message_destroy(void* inst_ptr) {
    delete static_cast<OnMessageInstance*>(inst_ptr);
}

static bool on_message_start_listening(void* inst_ptr, ExecContext* ctx) {
    auto* inst = static_cast<OnMessageInstance*>(inst_ptr);
    inst->ctx = ctx;
    inst->listening = true;

    BotManager::instance().add_message_listener([inst](const MessageEventData& data) {
        if (inst && inst->listening && inst->ctx) {
            inst->author_id = std::to_string(static_cast<uint64_t>(data.author_id));
            inst->author_name = data.author_name;
            inst->content = data.content;
            inst->channel_id = std::to_string(static_cast<uint64_t>(data.channel_id));
            inst->guild_id = std::to_string(static_cast<uint64_t>(data.guild_id));
            inst->message_id = std::to_string(static_cast<uint64_t>(data.message_id));

            inst->ctx->set_output_string(inst->ctx, "AuthorID", inst->author_id.c_str());
            inst->ctx->set_output_string(inst->ctx, "AuthorName", inst->author_name.c_str());
            inst->ctx->set_output_string(inst->ctx, "Content", inst->content.c_str());
            inst->ctx->set_output_string(inst->ctx, "ChannelID", inst->channel_id.c_str());
            inst->ctx->set_output_string(inst->ctx, "GuildID", inst->guild_id.c_str());
            inst->ctx->set_output_string(inst->ctx, "MessageID", inst->message_id.c_str());
            inst->ctx->trigger_output(inst->ctx, "OnMessage");
        }
    });

    return true;
}

static void on_message_stop_listening(void* inst_ptr) {
    auto* inst = static_cast<OnMessageInstance*>(inst_ptr);
    inst->listening = false;
    inst->ctx = nullptr;
}

static PinDesc on_message_pins[] = {
    {"OnMessage", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
    {"AuthorID", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"AuthorName", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"Content", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"ChannelID", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"GuildID", "string", PIN_OUT, PIN_KIND_DATA, 0},
    {"MessageID", "string", PIN_OUT, PIN_KIND_DATA, 0},
};

static NodeVTable on_message_vtable = {
    on_message_create,
    on_message_destroy,
    NULL, NULL,
    NULL, NULL,
    NULL,
    NULL, NULL,
    on_message_start_listening,
    on_message_stop_listening,
    NULL
};

static NodeDesc on_message_desc = {
    "On Message",
    "Discord/Events",
    "com.rune.discord.on_message",
    on_message_pins,
    7,
    NODE_FLAG_TRIGGER_EVENT,
    NULL, NULL,
    "Fires when a message is received in any channel the bot can see"
};

void register_on_message_node(PluginNodeRegistry* reg) {
    reg->register_node(&on_message_desc, &on_message_vtable);
}

