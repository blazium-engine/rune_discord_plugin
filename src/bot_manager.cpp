/**
 * Bot Manager - Implementation
 */

#include "bot_manager.h"
#include "discord_plugin.h"
#include <thread>

BotManager& BotManager::instance() {
    static BotManager instance;
    return instance;
}

BotManager::~BotManager() {
    shutdown();
}

bool BotManager::initialize(const std::string& token) {
    if (m_running) {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_INFO,
                "Discord plugin: BotManager::initialize called but bot is already running");
        }
        return false;
    }

    if (token.empty()) {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_ERROR,
                "Discord plugin: BotManager::initialize called with empty token");
        }
        return false;
    }

    const DiscordPluginConfig& cfg = GetDiscordPluginConfig();

    uint64_t intents = 0;
    bool usingOverride = false;

    if (cfg.gateway_intents != 0) {
        intents = cfg.gateway_intents;
        usingOverride = true;
    } else {
        intents = dpp::i_default_intents;
    }

    if (cfg.enable_message_content_intent) {
        intents |= dpp::i_message_content;
    }

    if (g_host) {
        std::string msg = "Discord plugin: initializing DPP cluster (token_length=";
        msg += std::to_string(token.size());
        msg += ", intents=";
        msg += std::to_string(intents);
        msg += usingOverride ? ", source=override)" : ", source=defaults)";
        g_host->log(PLUGIN_LOG_LEVEL_INFO, msg.c_str());
    }

    m_token = token;
    m_bot = std::make_unique<dpp::cluster>(token, intents);

    // Forward DPP logs into the unified plugin log (at DEBUG level) if enabled
    if (g_host && m_bot && cfg.enable_dpp_logging) {
        m_bot->on_log([](const dpp::log_t& event) {
            if (!g_host) {
                return;
            }
            std::string msg = "DPP[" + std::to_string(static_cast<int>(event.severity)) + "]: " + event.message;
            g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
        });
    }

    setup_event_handlers();

    // Start bot in background thread
    m_bot->start(dpp::st_return);
    m_running = true;

    if (g_host) {
        g_host->log(PLUGIN_LOG_LEVEL_INFO,
            "Discord plugin: DPP cluster start requested (BotManager running=true)");
    }

    return true;
}

void BotManager::shutdown() {
    if (!m_running || !m_bot) {
        return;
    }

    m_running = false;
    m_readyFired = false;
    m_bot.reset();

    // Clear event queue
    {
        std::lock_guard<std::mutex> lock(m_event_mutex);
        std::queue<QueuedEvent> empty;
        std::swap(m_event_queue, empty);
    }
}

bool BotManager::is_running() const {
    return m_running;
}

void BotManager::setup_event_handlers() {
    if (!m_bot) return;

    m_bot->on_ready([this](const dpp::ready_t& event) {
        (void)event;
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
                "Discord plugin: DPP on_ready received; queuing Ready event");
        }
        QueuedEvent qe;
        qe.type = DiscordEventType::Ready;
        {
            std::lock_guard<std::mutex> lock(m_event_mutex);
            m_event_queue.push(qe);
        }

        // Once all shards are ready, set a default presence so the bot appears online.
        // Users can override this at any time using the Set Presence node.
        set_presence(dpp::presence(dpp::ps_online, dpp::at_game, "online"));
    });

    m_bot->on_message_create([this](const dpp::message_create_t& event) {
        // Ignore bot messages
        if (event.msg.author.is_bot()) return;

        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
                "Discord plugin: DPP on_message_create received; queuing Message event");
        }

        QueuedEvent qe;
        qe.type = DiscordEventType::Message;
        qe.message_data.author_id = event.msg.author.id;
        qe.message_data.author_name = event.msg.author.username;
        qe.message_data.content = event.msg.content;
        qe.message_data.channel_id = event.msg.channel_id;
        qe.message_data.guild_id = event.msg.guild_id;
        qe.message_data.message_id = event.msg.id;
        {
            std::lock_guard<std::mutex> lock(m_event_mutex);
            m_event_queue.push(qe);
        }
    });

    m_bot->on_message_reaction_add([this](const dpp::message_reaction_add_t& event) {
        (void)event;
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
                "Discord plugin: DPP on_message_reaction_add received; queuing ReactionAdd event");
        }
        QueuedEvent qe;
        qe.type = DiscordEventType::ReactionAdd;
        qe.reaction_data.user_id = event.reacting_user.id;
        qe.reaction_data.emoji = event.reacting_emoji.name;
        qe.reaction_data.message_id = event.message_id;
        qe.reaction_data.channel_id = event.channel_id;
        qe.reaction_data.guild_id = event.reacting_guild.id;
        {
            std::lock_guard<std::mutex> lock(m_event_mutex);
            m_event_queue.push(qe);
        }
    });
}

void BotManager::tick() {
    // Process queued events on main thread
    std::queue<QueuedEvent> events_to_process;
    {
        std::lock_guard<std::mutex> lock(m_event_mutex);
        std::swap(events_to_process, m_event_queue);
    }

    const auto eventCount = events_to_process.size();
    if (g_host && eventCount > 0) {
        std::string msg = "Discord plugin: BotManager::tick processing " +
            std::to_string(eventCount) + " event(s)";
        g_host->log(PLUGIN_LOG_LEVEL_DEBUG, msg.c_str());
    }

    std::vector<ReadyCallback> ready_cbs;
    std::vector<MessageCallback> message_cbs;
    std::vector<ReactionCallback> reaction_cbs;
    {
        std::lock_guard<std::mutex> lock(m_listener_mutex);
        ready_cbs = m_ready_listeners;
        message_cbs = m_message_listeners;
        reaction_cbs = m_reaction_listeners;
    }

    while (!events_to_process.empty()) {
        const auto& event = events_to_process.front();

        switch (event.type) {
            case DiscordEventType::Ready:
                for (auto& cb : ready_cbs) {
                    cb();
                }
                m_readyFired = true;
                break;

            case DiscordEventType::Message:
                for (auto& cb : message_cbs) {
                    cb(event.message_data);
                }
                break;

            case DiscordEventType::ReactionAdd:
                for (auto& cb : reaction_cbs) {
                    cb(event.reaction_data);
                }
                break;
        }

        events_to_process.pop();
    }
}

void BotManager::add_ready_listener(ReadyCallback callback) {
    {
        std::lock_guard<std::mutex> lock(m_listener_mutex);
        m_ready_listeners.push_back(callback);
    }

    // If the bot is already ready, immediately invoke this listener once so
    // late subscribers (e.g., flows opened after connect) still see a Ready.
    if (m_readyFired) {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
                "Discord plugin: add_ready_listener called after ready; invoking callback immediately");
        }
        callback();
    }
}

void BotManager::add_message_listener(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_message_listeners.push_back(callback);
}

void BotManager::add_reaction_listener(ReactionCallback callback) {
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_reaction_listeners.push_back(callback);
}

void BotManager::clear_listeners() {
    std::lock_guard<std::mutex> lock(m_listener_mutex);
    m_ready_listeners.clear();
    m_message_listeners.clear();
    m_reaction_listeners.clear();
}

void BotManager::send_message(dpp::snowflake channel_id, const std::string& content) {
    if (!m_bot || !m_running) return;
    m_bot->message_create(dpp::message(channel_id, content));
}

void BotManager::send_embed(dpp::snowflake channel_id, const dpp::embed& embed) {
    if (!m_bot || !m_running) return;
    dpp::message msg(channel_id, "");
    msg.add_embed(embed);
    m_bot->message_create(msg);
}

void BotManager::add_reaction(dpp::snowflake channel_id, dpp::snowflake message_id, const std::string& emoji) {
    if (!m_bot || !m_running) return;
    m_bot->message_add_reaction(message_id, channel_id, emoji);
}

void BotManager::reply_to_message(dpp::snowflake channel_id, dpp::snowflake message_id, const std::string& content) {
    if (!m_bot || !m_running) return;
    dpp::message msg(channel_id, content);
    msg.set_reference(message_id);
    m_bot->message_create(msg);
}

void BotManager::send_direct_message(dpp::snowflake user_id, const std::string& content) {
    if (!m_bot || !m_running) {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
                "Discord plugin: send_direct_message called but bot is not running");
        }
        return;
    }

    dpp::message msg;
    msg.set_content(content);

    m_bot->direct_message_create(user_id, msg, [](const dpp::confirmation_callback_t& cb) {
        if (!g_host) {
            return;
        }

        if (cb.is_error()) {
            const auto& error = cb.get_error();

            std::string err = "Discord plugin: direct_message_create failed: ";
            err += error.message;
            g_host->log(PLUGIN_LOG_LEVEL_ERROR, err.c_str());

            const std::string& emsg = error.message;
            const bool has401 = (emsg.find("401") != std::string::npos);
            const bool hasUnauthorized =
                (emsg.find("Unauthorized") != std::string::npos) ||
                (emsg.find("unauthorized") != std::string::npos);

            if (has401 || hasUnauthorized) {
                g_host->log(PLUGIN_LOG_LEVEL_ERROR,
                    "Discord plugin: Discord returned 401 Unauthorized for direct_message_create. "
                    "This usually means the bot token is invalid, includes the 'Bot ' prefix, or has been reset. "
                    "Update the DISCORD_TOKEN environment variable or plugin settings token with a valid raw bot token and reconnect.");
            }
        }
    });
}

void BotManager::set_presence(const dpp::presence& presence) {
    if (!m_bot || !m_running) {
        if (g_host) {
            g_host->log(PLUGIN_LOG_LEVEL_DEBUG,
                "Discord plugin: set_presence called but bot is not running");
        }
        return;
    }

    m_bot->set_presence(presence);

    if (g_host) {
        g_host->log(PLUGIN_LOG_LEVEL_INFO,
            "Discord plugin: presence updated via BotManager::set_presence");
    }
}

