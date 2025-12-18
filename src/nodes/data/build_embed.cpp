/**
 * BuildEmbed Node - Construct embed JSON (pure data node)
 */

#include "discord_plugin.h"
#include <string>
#include <sstream>

static bool build_embed_execute(void* inst, ExecContext* ctx) {
    const char* title = ctx->get_input_string(ctx, "Title");
    const char* description = ctx->get_input_string(ctx, "Description");
    int64_t color = ctx->get_input_int(ctx, "Color");
    const char* footer = ctx->get_input_string(ctx, "Footer");
    const char* image_url = ctx->get_input_string(ctx, "ImageURL");

    // Build JSON string for embed
    std::ostringstream json;
    json << "{";
    
    bool first = true;
    
    if (title && title[0]) {
        json << "\"title\":\"" << title << "\"";
        first = false;
    }
    
    if (description && description[0]) {
        if (!first) json << ",";
        json << "\"description\":\"" << description << "\"";
        first = false;
    }
    
    if (color != 0) {
        if (!first) json << ",";
        json << "\"color\":" << color;
        first = false;
    }
    
    if (footer && footer[0]) {
        if (!first) json << ",";
        json << "\"footer\":{\"text\":\"" << footer << "\"}";
        first = false;
    }
    
    if (image_url && image_url[0]) {
        if (!first) json << ",";
        json << "\"image\":{\"url\":\"" << image_url << "\"}";
    }
    
    json << "}";

    // Store in static buffer (simplified - real impl would use proper memory)
    static std::string result;
    result = json.str();
    ctx->set_output_json(ctx, "EmbedJSON", result.c_str());

    return true;
}

static PinDesc build_embed_pins[] = {
    {"Title", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Description", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"Color", "int", PIN_IN, PIN_KIND_DATA, 0},
    {"Footer", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"ImageURL", "string", PIN_IN, PIN_KIND_DATA, 0},
    {"EmbedJSON", "json", PIN_OUT, PIN_KIND_DATA, 0},
};

static NodeVTable build_embed_vtable = {
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    build_embed_execute,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static NodeDesc build_embed_desc = {
    "Build Embed",
    "Discord/Data",
    "com.rune.discord.build_embed",
    build_embed_pins,
    6,
    NODE_FLAG_PURE_DATA,
    NULL, NULL,
    "Build a Discord embed JSON object"
};

void register_build_embed_node(PluginNodeRegistry* reg) {
    reg->register_node(&build_embed_desc, &build_embed_vtable);
}

