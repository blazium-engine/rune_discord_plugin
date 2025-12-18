/**
 * Bot Manager - Manages DPP cluster lifecycle
 */

#ifndef RUNE_DISCORD_BOT_MANAGER_H
#define RUNE_DISCORD_BOT_MANAGER_H

#include <dpp/dpp.h>
#include <string>
#include <mutex>
#include <functional>
#include <vector>
#include <queue>

// Forward declaration
struct ExecContext;

// Event types for queuing
enum class DiscordEventType {
    Ready,
    Message,
    ReactionAdd
};

// Event data structures
struct MessageEventData {
    dpp::snowflake author_id;
    std::string author_name;
    std::string content;
    dpp::snowflake channel_id;
    dpp::snowflake guild_id;
    dpp::snowflake message_id;
};

struct ReactionEventData {
    dpp::snowflake user_id;
    std::string emoji;
    dpp::snowflake message_id;
    dpp::snowflake channel_id;
    dpp::snowflake guild_id;
};

// Queued event structure
struct QueuedEvent {
    DiscordEventType type;
    MessageEventData message_data;
    ReactionEventData reaction_data;
};

// Listener callback types
using ReadyCallback = std::function<void()>;
using MessageCallback = std::function<void(const MessageEventData&)>;
using ReactionCallback = std::function<void(const ReactionEventData&)>;

/**
 * BotManager - Singleton that manages the DPP cluster
 */
class BotManager {
public:
    static BotManager& instance();

    // Lifecycle
    bool initialize(const std::string& token);
    void shutdown();
    bool is_running() const;

    // Called on main thread to process events
    void tick();

    // Event listener registration
    void add_ready_listener(ReadyCallback callback);
    void add_message_listener(MessageCallback callback);
    void add_reaction_listener(ReactionCallback callback);

    void clear_listeners();

    // Actions (thread-safe, can be called from any thread)
    void send_message(dpp::snowflake channel_id, const std::string& content);
    void send_embed(dpp::snowflake channel_id, const dpp::embed& embed);
    void add_reaction(dpp::snowflake channel_id, dpp::snowflake message_id, const std::string& emoji);
    void reply_to_message(dpp::snowflake channel_id, dpp::snowflake message_id, const std::string& content);
    void send_direct_message(dpp::snowflake user_id, const std::string& content);
    void set_presence(const dpp::presence& presence);

    // Getters
    dpp::cluster* get_cluster() { return m_bot.get(); }
    bool has_ready_fired() const { return m_readyFired; }

private:
    BotManager() = default;
    ~BotManager();
    BotManager(const BotManager&) = delete;
    BotManager& operator=(const BotManager&) = delete;

    void setup_event_handlers();

    std::unique_ptr<dpp::cluster> m_bot;
    bool m_running = false;
    std::string m_token;

    // Event queue for main thread processing
    std::mutex m_event_mutex;
    std::queue<QueuedEvent> m_event_queue;

    // Listeners
    std::mutex m_listener_mutex;
    std::vector<ReadyCallback> m_ready_listeners;
    std::vector<MessageCallback> m_message_listeners;
    std::vector<ReactionCallback> m_reaction_listeners;

    bool m_readyFired = false;
};

#endif // RUNE_DISCORD_BOT_MANAGER_H

