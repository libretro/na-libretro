/*
 * "Not available" libretro core.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "libretro-common/include/libretro.h"

#define FB_WIDTH  640
#define FB_HEIGHT 480

static uint16_t framebuffer[FB_WIDTH * FB_HEIGHT];

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;
static retro_log_printf_t log_cb;

/* ---------------------------------------------------------------------
 * 5x7 bitmap font
 * ------------------------------------------------------------------- */
#define GLYPH_W 5
#define GLYPH_H 7

typedef struct { char ch; uint8_t rows[GLYPH_H]; } glyph_t;

static const glyph_t font[] = {
    { ' ', { 0x00,0x00,0x00,0x00,0x00,0x00,0x00 } },
    { 'A', { 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11 } },
    { 'B', { 0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E } },
    { 'C', { 0x0F,0x10,0x10,0x10,0x10,0x10,0x0F } },
    { 'E', { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F } },
    { 'F', { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x10 } },
    { 'H', { 0x11,0x11,0x11,0x1F,0x11,0x11,0x11 } },
    { 'I', { 0x1F,0x04,0x04,0x04,0x04,0x04,0x1F } },
    { 'L', { 0x10,0x10,0x10,0x10,0x10,0x10,0x1F } },
    { 'N', { 0x11,0x19,0x15,0x15,0x13,0x11,0x11 } },
    { 'O', { 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E } },
    { 'R', { 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11 } },
    { 'S', { 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E } },
    { 'T', { 0x1F,0x04,0x04,0x04,0x04,0x04,0x04 } },
    { 'U', { 0x11,0x11,0x11,0x11,0x11,0x11,0x0E } },
    { 'V', { 0x11,0x11,0x11,0x11,0x11,0x0A,0x04 } },
    { 'W', { 0x11,0x11,0x11,0x15,0x15,0x15,0x0A } },
    { 'X', { 0x11,0x11,0x0A,0x04,0x0A,0x11,0x11 } },
    { 'Y', { 0x11,0x11,0x0A,0x04,0x04,0x04,0x04 } },
};
#define FONT_COUNT (sizeof(font) / sizeof(font[0]))

static const uint8_t *glyph_rows(char c)
{
    if (c >= 'a' && c <= 'z')
        c -= 32;
    for (size_t i = 0; i < FONT_COUNT; i++)
        if (font[i].ch == c)
            return font[i].rows;
    return font[0].rows; /* fall back to space for unknown glyphs */
}

static void put_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT)
        return;
    framebuffer[y * FB_WIDTH + x] = color;
}

static void draw_char(int x, int y, char c, int scale, uint16_t color)
{
    const uint8_t *rows = glyph_rows(c);
    for (int row = 0; row < GLYPH_H; row++)
    {
        uint8_t bits = rows[row];
        for (int col = 0; col < GLYPH_W; col++)
        {
            if (bits & (1 << (GLYPH_W - 1 - col)))
            {
                for (int sy = 0; sy < scale; sy++)
                    for (int sx = 0; sx < scale; sx++)
                        put_pixel(x + col * scale + sx, y + row * scale + sy, color);
            }
        }
    }
}

static void draw_string_centered(const char *s, int y, int scale, uint16_t color)
{
    int len = (int)strlen(s);
    int char_w = (GLYPH_W + 1) * scale; /* +1 column of spacing */
    int total_w = len * char_w;
    int x = (FB_WIDTH - total_w) / 2;
    for (int i = 0; i < len; i++)
    {
        draw_char(x + i * char_w, y, s[i], scale, color);
    }
}

static void draw_wrapped_centered(const char *msg, int scale, uint16_t color)
{
    char buf[256];
    strncpy(buf, msg, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int char_w = (GLYPH_W + 1) * scale;
    int max_chars_per_line = FB_WIDTH / char_w;

    char *lines[16];
    int line_count = 0;

    char *cursor = buf;
    while (*cursor && line_count < 16)
    {
        int len = (int)strlen(cursor);
        if (len <= max_chars_per_line)
        {
            lines[line_count++] = cursor;
            break;
        }

        int break_at = -1;
        for (int i = max_chars_per_line; i > 0; i--)
        {
            if (cursor[i] == ' ')
            {
                break_at = i;
                break;
            }
        }
        if (break_at < 0)
            break_at = max_chars_per_line;

        cursor[break_at] = '\0';
        lines[line_count++] = cursor;
        cursor += break_at + 1;
    }

    int line_h = (GLYPH_H + 2) * scale;
    int total_h = line_count * line_h;
    int y = (FB_HEIGHT - total_h) / 2;

    for (int i = 0; i < line_count; i++)
        draw_string_centered(lines[i], y + i * line_h, scale, color);
}

static void render_frame(void)
{
    memset(framebuffer, 0x00, sizeof(framebuffer));
    draw_wrapped_centered(
        "This core is not currently available for this architecture",
        3, 0xFFFF /* white in RGB565 */
    );
}

/* ----------------------------- libretro API ---------------------------- */

void retro_init(void) {}
void retro_deinit(void) {}

unsigned retro_api_version(void) { return RETRO_API_VERSION; }

void retro_set_environment(retro_environment_t cb)
{
    environ_cb = cb;
    bool no_content = true;
    cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);
}

void retro_set_video_refresh(retro_video_refresh_t cb) { video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { input_state_cb = cb; }

void retro_get_system_info(struct retro_system_info *info)
{
    memset(info, 0, sizeof(*info));
    info->library_name     = "Not Available";
    info->library_version  = "1.0";
    info->valid_extensions = NULL;
    info->need_fullpath    = false;
    info->block_extract    = false;
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
    info->geometry.base_width   = FB_WIDTH;
    info->geometry.base_height  = FB_HEIGHT;
    info->geometry.max_width    = FB_WIDTH;
    info->geometry.max_height   = FB_HEIGHT;
    info->geometry.aspect_ratio = (float)FB_WIDTH / (float)FB_HEIGHT;
    info->timing.fps            = 60.0;
    info->timing.sample_rate    = 44100.0;
}

void retro_set_controller_port_device(unsigned port, unsigned device) { (void)port; (void)device; }

void retro_reset(void) {}

void retro_run(void)
{
    input_poll_cb();
    render_frame();
    video_cb(framebuffer, FB_WIDTH, FB_HEIGHT, FB_WIDTH * sizeof(uint16_t));
}

size_t retro_serialize_size(void) { return 0; }
bool retro_serialize(void *data, size_t size) { (void)data; (void)size; return false; }
bool retro_unserialize(const void *data, size_t size) { (void)data; (void)size; return false; }

void retro_cheat_reset(void) {}
void retro_cheat_set(unsigned index, bool enabled, const char *code) { (void)index; (void)enabled; (void)code; }

bool retro_load_game(const struct retro_game_info *game)
{
    (void)game;
    enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
    environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

    struct retro_log_callback logging;
    if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
        log_cb = logging.log;
    else
        log_cb = NULL;

    render_frame();
    return true;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
    (void)type; (void)info; (void)num;
    return retro_load_game(NULL);
}

void retro_unload_game(void) {}

unsigned retro_get_region(void) { return RETRO_REGION_NTSC; }

void *retro_get_memory_data(unsigned id) { (void)id; return NULL; }
size_t retro_get_memory_size(unsigned id) { (void)id; return 0; }
