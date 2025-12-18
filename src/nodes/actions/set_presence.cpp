/**
 * SetPresence Node - Update the bot's presence/status
 */

#include "discord_plugin.h"
#include "bot_manager.h"
#include <string>
#include <algorithm>
#include <cctype>

static dpp::presence_status map_status_string(const std::string& status_raw) {
    std::string s = status_raw;
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (s == "idle") {
        return dpp::ps_idle;
    }
    if (s == "dnd" || s == "donotdisturb" || s == "do_not_disturb") {
        return dpp::ps_dnd;
    }
    if (s == "invisible") {
        return dpp::ps_invisible;
    }
    if (s == "offline") {
        return dpp::ps_offline;
    }

    // Default
    return dpp::ps_online;
}

static bool set_presence_execute(void* inst, ExecContext* ctx) {
    (void)inst;

    const char* status_cstr = ctx->get_input_string(ctx, "Status");
    const char* activity_cstr = ctx->get_input_string(ctx, "ActivityText");

    std::string status = (status_cstr && status_cstr[0] != '\0')
        ? std::string(status_cstr)
        : std::string("online");
    std::string activity = activity_cstr ? std::string(activity_cstr) : std::string();

    dpp::cluster* cluster = BotManager::instance().get_cluster();
    if (!cluster) {
        ctx->set_error(ctx, "Discord bot not initialized");
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_ERROR,
                "Discord Set Presence: bot not initialized (cluster is null)");
        }
        return false;
    }

    dpp::presence_status ps = map_status_string(status);
    dpp::presence presence(ps, dpp::at_game, activity);

    if (g_host) {
        std::string msg = "Discord Set Presence: status=" + status +
            ", activity=\"" + activity + "\"";
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
    }

    BotManager::instance().set_presence(presence);

    ctx->trigger_output(ctx, "Done");
    return true;
}

static PinDesc set_presence_pins[] = {
    {"Exec", "execution", PIN_IN, PIN_KIND_EXECUTION, 0},
    {"Status", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"ActivityText", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Done", "execution", PIN_OUT, PIN_KIND_EXECUTION, 0},
};

static NodeVTable set_presence_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    set_presence_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc set_presence_desc = {
    "Set Presence",
    "Discord/Actions",
    "com.rune.discord.set_presence",
    set_presence_pins,
    4,
    NODE_FLAG_NONE,
    NULL, NULL,
    "Set the Discord bot's presence/status and activity text"
};

void register_set_presence_node(PluginNodeRegistry* reg) {
    reg->register_node(&set_presence_desc, &set_presence_vtable);
}



