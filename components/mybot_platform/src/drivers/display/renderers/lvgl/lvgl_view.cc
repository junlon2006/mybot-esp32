/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2026 Project Contributors */
/* Status/content/notification layout adapted from the MIT-licensed display
 * reference identified in LVGL_VIEW_LICENSE.txt. */
#include "display/lvgl_view.h"
#include "display/lvgl_assets.h"
#include "network/wifi_control.h"
#include "board_config.h"

#include "sdkconfig.h"

#include <cstring>

LV_FONT_DECLARE(mybot_lvgl_font_20);
LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(lv_font_montserrat_10);

namespace {

constexpr uint32_t kTimerPeriodMs = 200;
constexpr uint32_t kNotificationMs = 2000;

struct Theme {
    uint32_t background;
    uint32_t surface;
    uint32_t border;
    uint32_t text;
    uint32_t muted;
    uint32_t blue;
    uint32_t green;
    uint32_t amber;
    uint32_t red;
};

#if CONFIG_MYBOT_LVGL_UI_LIGHT_THEME
constexpr Theme kTheme{0xeff3f8, 0xffffff, 0xdce4ee, 0x172637, 0x566477,
                       0x1766ae, 0x177849, 0x8b5000, 0xbc3542};
#else
constexpr Theme kTheme{0x0e1520, 0x192433, 0x2a3b50, 0xedf2fa, 0xa5b2c7,
                       0x69b5ff, 0x69dba8, 0xffcc7c, 0xff8e91};
#endif

struct Presentation {
    const char *title;
    const char *status;
    const char *notice;
    const char *icon;
    uint32_t accent;
};

#if CONFIG_MYBOT_LANGUAGE_ZH_CN
#if defined(MYBOT_BOARD_HAS_TOUCH) && MYBOT_BOARD_HAS_TOUCH
constexpr const char *kReadyPrompt = "轻触开始对话";
#else
constexpr const char *kReadyPrompt = "短按开始对话";
#endif
constexpr Presentation kScreens[] = {
    {"启动中", "启动", "正在初始化", LV_SYMBOL_POWER, kTheme.blue},
    {"Wi-Fi 配网", "配网", "请连接设备热点", LV_SYMBOL_WIFI, kTheme.amber},
    {"网络已断开", "离线", "正在重新连接", LV_SYMBOL_WIFI, kTheme.red},
    {"连接服务中", "连接", "正在连接服务", LV_SYMBOL_REFRESH, kTheme.blue},
    {"等待配对", "配对", "正在获取配对码", LV_SYMBOL_SETTINGS, kTheme.amber},
    {"配对码", "配对", "在应用中输入配对码", LV_SYMBOL_SETTINGS, kTheme.amber},
    {"准备就绪", "就绪", kReadyPrompt, LV_SYMBOL_OK, kTheme.green},
    {"对话中", "对话", "声纹注册中", LV_SYMBOL_CALL, kTheme.blue},
    {"启动失败", "异常", "请重启后重试", LV_SYMBOL_WARNING, kTheme.red},
    {"正在退出", "退出", "正在结束服务", LV_SYMBOL_POWER, kTheme.muted},
};
constexpr const char *kListening = "聆听中";
constexpr const char *kThinking = "思考中";
constexpr const char *kSpeaking = "正在回复";
constexpr const char *kVoiceprintSaved = "声纹已注册";
constexpr const char *kPairedNotification = "配对成功";
constexpr const char *kVoiceprintNotification = "声纹注册成功";
constexpr const char *kNetworkNotification = "网络已恢复";
#else
#if defined(MYBOT_BOARD_HAS_TOUCH) && MYBOT_BOARD_HAS_TOUCH
constexpr const char *kReadyPrompt = "Tap to start";
#else
constexpr const char *kReadyPrompt = "Press to start";
#endif
constexpr Presentation kScreens[] = {
    {"Starting", "Starting", "Please wait", LV_SYMBOL_POWER, kTheme.blue},
    {"Wi-Fi setup", "Setup", "Connect to device Wi-Fi", LV_SYMBOL_WIFI, kTheme.amber},
    {"Wi-Fi disconnected", "Offline", "Reconnecting to Wi-Fi", LV_SYMBOL_WIFI, kTheme.red},
    {"Connecting", "Connecting", "Connecting to services", LV_SYMBOL_REFRESH, kTheme.blue},
    {"Pairing", "Pairing", "Getting pairing code", LV_SYMBOL_SETTINGS, kTheme.amber},
    {"Pairing code", "Pairing", "Enter code in the app", LV_SYMBOL_SETTINGS, kTheme.amber},
    {"Ready", "Ready", kReadyPrompt, LV_SYMBOL_OK, kTheme.green},
    {"In conversation", "Talking", "VP registering", LV_SYMBOL_CALL, kTheme.blue},
    {"Startup failed", "Error", "Restart to retry", LV_SYMBOL_WARNING, kTheme.red},
    {"Stopping", "Stopping", "Closing services", LV_SYMBOL_POWER, kTheme.muted},
};
constexpr const char *kListening = "Listening";
constexpr const char *kThinking = "Thinking";
constexpr const char *kSpeaking = "Speaking";
constexpr const char *kVoiceprintSaved = "VP registered";
constexpr const char *kPairedNotification = "Paired successfully";
constexpr const char *kVoiceprintNotification = "Voiceprint registered";
constexpr const char *kNetworkNotification = "Wi-Fi reconnected";
#endif

static_assert(sizeof(kScreens) / sizeof(kScreens[0]) == MYBOT_LCD_SCREEN_COUNT,
              "Every SDK workflow screen needs a presentation");

enum class Activity {
    None,
    Listening,
    Thinking,
    Speaking,
};

enum class Notification {
    None,
    Pairing,
    Voiceprint,
    Network,
};

struct View {
    lv_obj_t *root;
    lv_obj_t *brand;
    lv_obj_t *status;
    lv_obj_t *notification;
    lv_obj_t *card;
    lv_obj_t *badge;
    lv_obj_t *icon;
    lv_obj_t *emoji;
    lv_obj_t *title;
    lv_obj_t *code;
    lv_obj_t *notice;
    lv_obj_t *activity;
    lv_obj_t *bars[5];
    lv_timer_t *timer;
    const lv_image_dsc_t *emoji_source;
    mybot_lcd_content_t previous;
    char provisioning_ssid[MYBOT_WIFI_PROVISIONING_SSID_CAPACITY];
    bool has_content;
    bool notification_visible;
    bool vp_notification_shown;
    Notification notification_kind;
    uint32_t notification_started;
    uint32_t accent;
    uint32_t notice_color;
    Activity activity_mode;
    unsigned animation_frame;
    int width;
    int height;
};

View s_view{};
int view_width();

lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int width, int height, uint32_t color) {
    lv_obj_t *panel = lv_obj_create(parent);
    if (!panel) {
        return nullptr;
    }
    lv_obj_remove_style_all(panel);
    lv_obj_remove_flag(panel,
                       static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    return panel;
}

lv_obj_t *make_label(lv_obj_t *parent, int x, int y, int width, int height) {
    lv_obj_t *label = lv_label_create(parent);
    if (!label) {
        return nullptr;
    }
    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_size(label, width, height);
    lv_obj_set_style_text_font(label, &mybot_lvgl_font_20, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(kTheme.text), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text_static(label, "");
    return label;
}

void set_text(lv_obj_t *label, const char *text) {
    if (std::strcmp(lv_label_get_text(label), text) != 0) {
        lv_label_set_text(label, text);
    }
}

void set_hidden(lv_obj_t *object, bool hidden) {
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) == hidden) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

uint32_t conversation_indicators(uint32_t indicators) {
    uint32_t state = 0;
    if (indicators & MYBOT_LCD_INDICATOR_LISTENING) {
        state = MYBOT_LCD_INDICATOR_LISTENING;
    } else if (indicators & MYBOT_LCD_INDICATOR_THINKING) {
        state = MYBOT_LCD_INDICATOR_THINKING;
    } else if (indicators & MYBOT_LCD_INDICATOR_SPEAKING) {
        state = MYBOT_LCD_INDICATOR_SPEAKING;
    }
    return state | (indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED);
}

void clear_notification() {
    if (!s_view.notification_visible) {
        return;
    }
    s_view.notification_visible = false;
    s_view.notification_kind = Notification::None;
    set_hidden(s_view.notification, true);
    set_hidden(s_view.brand, false);
    set_hidden(s_view.status, false);
}

void show_notification(Notification kind, const char *text, uint32_t color) {
    set_text(s_view.notification, text);
#if !CONFIG_MYBOT_LANGUAGE_ZH_CN
    if (view_width() < 240) {
        lv_obj_set_style_text_font(s_view.notification, &lv_font_montserrat_10, 0);
    }
#endif
    lv_obj_set_style_text_color(s_view.notification, lv_color_hex(color), 0);
    s_view.notification_visible = true;
    s_view.notification_kind = kind;
    s_view.notification_started = lv_tick_get();
    set_hidden(s_view.brand, true);
    set_hidden(s_view.status, true);
    set_hidden(s_view.notification, false);
}

/* These bars express an activity state, not measured microphone amplitude.
 * All objects and their style properties exist before the first timer tick. */
void draw_activity_frame() {
    constexpr uint8_t heights[][5] = {
        {4, 8, 14, 8, 4},   {6, 12, 8, 14, 6}, {10, 6, 12, 6, 10},
        {14, 8, 4, 10, 14}, {8, 14, 10, 4, 8}, {6, 10, 14, 10, 6},
    };
    unsigned phase = s_view.animation_frame;
    for (unsigned i = 0; i < 5; ++i) {
        const bool thinking = s_view.activity_mode == Activity::Thinking;
        if (thinking) {
            set_hidden(s_view.bars[i], i >= 3);
            if (i >= 3) {
                continue;
            }
            lv_obj_set_pos(s_view.bars[i], 10 + static_cast<int>(i) * 17, 5);
            lv_obj_set_size(s_view.bars[i], 6, 6);
            lv_obj_set_style_bg_opa(s_view.bars[i], i == phase % 3 ? LV_OPA_COVER : LV_OPA_30, 0);
        } else {
            set_hidden(s_view.bars[i], false);
            const unsigned column = s_view.activity_mode == Activity::Speaking ? 4 - i : i;
            const int height = heights[phase % 6][column];
            lv_obj_set_pos(s_view.bars[i], 7 + static_cast<int>(i) * 11, (16 - height) / 2);
            lv_obj_set_size(s_view.bars[i], 6, height);
            lv_obj_set_style_bg_opa(s_view.bars[i], LV_OPA_COVER, 0);
        }
    }
}

bool animation_running() {
#if CONFIG_MYBOT_LVGL_UI_ANIMATIONS
    return s_view.activity_mode != Activity::None;
#else
    return false;
#endif
}

void update_timer() {
    const bool needed = s_view.notification_visible || animation_running();
    if (needed && lv_timer_get_paused(s_view.timer)) {
        lv_timer_reset(s_view.timer);
        lv_timer_resume(s_view.timer);
    } else if (!needed && !lv_timer_get_paused(s_view.timer)) {
        lv_timer_pause(s_view.timer);
    }
}

void timer_callback(lv_timer_t *) {
    if (s_view.notification_visible &&
        lv_tick_elaps(s_view.notification_started) >= kNotificationMs) {
        clear_notification();
    }
    if (animation_running()) {
        s_view.animation_frame = (s_view.animation_frame + 1) % 6;
        draw_activity_frame();
    }
    update_timer();
}

bool online_screen(mybot_lcd_screen_t screen) {
    return screen == MYBOT_LCD_SCREEN_STARTING_SERVICES || screen == MYBOT_LCD_SCREEN_PAIRING ||
           screen == MYBOT_LCD_SCREEN_PAIR_CODE || screen == MYBOT_LCD_SCREEN_READY ||
           screen == MYBOT_LCD_SCREEN_IN_CONVERSATION;
}

void update_notifications(const mybot_lcd_content_t &content) {
    const bool conversation = content.screen == MYBOT_LCD_SCREEN_IN_CONVERSATION;
    const bool screen_changed = !s_view.has_content || content.screen != s_view.previous.screen;
    if (screen_changed) {
        /* A workflow transition invalidates a toast from the previous screen. Speaking and
         * listening updates stay within the same conversation and keep its toast's deadline. */
        clear_notification();
        if (!conversation || s_view.previous.screen != MYBOT_LCD_SCREEN_IN_CONVERSATION) {
            s_view.vp_notification_shown = false;
        }
    }
    if (s_view.notification_kind == Notification::Voiceprint &&
        !(content.indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED)) {
        clear_notification();
    }
    if (conversation && (content.indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED) &&
        !s_view.vp_notification_shown) {
        s_view.vp_notification_shown = true;
        show_notification(Notification::Voiceprint, kVoiceprintNotification, kTheme.green);
    } else if (screen_changed && s_view.has_content &&
               (s_view.previous.screen == MYBOT_LCD_SCREEN_PAIRING ||
                s_view.previous.screen == MYBOT_LCD_SCREEN_PAIR_CODE) &&
               (content.screen == MYBOT_LCD_SCREEN_READY || conversation)) {
        show_notification(Notification::Pairing, kPairedNotification, kTheme.green);
    } else if (screen_changed && s_view.has_content &&
               s_view.previous.screen == MYBOT_LCD_SCREEN_WIFI_DISCONNECTED &&
               online_screen(content.screen)) {
        show_notification(Notification::Network, kNetworkNotification, kTheme.blue);
    }
}

int view_width() {
    return s_view.width;
}
} // namespace

int mybot_lvgl_view_create_sized(lv_display_t *display, int width, int height) {
    if (!display || s_view.root || width <= 0 || height <= 0 ||
        lv_display_get_horizontal_resolution(display) != width ||
        lv_display_get_vertical_resolution(display) != height) {
        return -1;
    }
    s_view.width = width;
    s_view.height = height;
    const int margin = width >= 240 ? 12 : 4;
    const int card_width = width - margin * 2;
    const int card_x = margin;
    const int card_y = 46;
    const int card_height = height >= 220 ? 138 : (height - 102);
    const int center_x = (card_width - 64) / 2;
    const int footer_y = height - 44;
    s_view.root =
        make_panel(lv_display_get_screen_active(display), 0, 0, width, height, kTheme.background);
    if (!s_view.root) {
        return -1;
    }
    lv_obj_t *header = make_panel(s_view.root, 0, 0, width, 38, kTheme.background);
    lv_obj_t *footer = make_panel(s_view.root, margin, footer_y, card_width, 34, kTheme.surface);
    s_view.card = make_panel(s_view.root, card_x, card_y, card_width, card_height, kTheme.surface);
    if (!header || !footer || !s_view.card) {
        mybot_lvgl_view_destroy();
        return -1;
    }
    lv_obj_set_style_radius(s_view.card, 18, 0);
    lv_obj_set_style_border_color(s_view.card, lv_color_hex(kTheme.border), 0);
    lv_obj_set_style_border_width(s_view.card, 1, 0);
    lv_obj_set_style_radius(footer, 12, 0);
    s_view.badge = make_panel(s_view.card, center_x, 8, 64, 64, kTheme.surface);
    s_view.activity = make_panel(s_view.card, center_x, card_height - 26, 64, 16, kTheme.surface);
    if (!s_view.badge || !s_view.activity) {
        mybot_lvgl_view_destroy();
        return -1;
    }
    lv_obj_set_style_radius(s_view.badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_view.badge, 2, 0);
    lv_obj_set_style_bg_opa(s_view.activity, LV_OPA_TRANSP, 0);

    const int brand_width = width < 240 ? width / 2 : width / 3;
    s_view.brand = make_label(header, 8, 4, brand_width, 30);
    s_view.status = make_label(header, brand_width, 4, width - brand_width - 8, 30);
    s_view.notification = make_label(header, 4, 4, width - 8, 30);
    s_view.icon = make_label(s_view.badge, 0, 0, 60, 40);
    s_view.emoji = lv_image_create(s_view.card);
    s_view.title = make_label(s_view.card, 4, 81, card_width - 8, 30);
    s_view.code = make_label(s_view.card, 2, 63, card_width - 4, 45);
    s_view.notice = make_label(footer, 2, 3, card_width - 4, 28);
    if (!s_view.brand || !s_view.status || !s_view.notification || !s_view.icon || !s_view.emoji ||
        !s_view.title || !s_view.code || !s_view.notice) {
        mybot_lvgl_view_destroy();
        return -1;
    }
    lv_obj_set_pos(s_view.emoji, center_x, 8);
    lv_obj_set_size(s_view.emoji, 64, 64);
    lv_obj_set_style_text_align(s_view.brand, LV_TEXT_ALIGN_LEFT, 0);
    if (width < 240) {
        lv_obj_set_style_text_font(s_view.brand, &lv_font_montserrat_14, 0);
#if CONFIG_MYBOT_LANGUAGE_ZH_CN
        lv_obj_set_style_text_font(s_view.status, &mybot_lvgl_font_20, 0);
        lv_obj_set_style_text_font(s_view.notification, &mybot_lvgl_font_20, 0);
        lv_obj_set_style_text_font(s_view.title, &mybot_lvgl_font_20, 0);
        lv_obj_set_style_text_font(s_view.notice, &mybot_lvgl_font_20, 0);
#else
        lv_obj_set_style_text_font(s_view.status, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_font(s_view.notification, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_font(s_view.title, &lv_font_montserrat_10, 0);
        lv_obj_set_style_text_font(s_view.notice, &lv_font_montserrat_10, 0);
#endif
    }
    lv_label_set_text_static(s_view.brand, "mybot");
    lv_obj_set_style_text_align(s_view.status, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(s_view.status, lv_color_hex(kTheme.muted), 0);
    lv_obj_set_style_text_color(s_view.notification, lv_color_hex(kTheme.green), 0);
    set_hidden(s_view.notification, true);
    lv_obj_set_style_text_font(s_view.icon, &lv_font_montserrat_32, 0);
    lv_obj_center(s_view.icon);
    lv_obj_set_style_text_letter_space(s_view.code, 0, 0);
    for (unsigned i = 0; i < 5; ++i) {
        s_view.bars[i] =
            make_panel(s_view.activity, 7 + static_cast<int>(i) * 11, 4, 6, 8, kTheme.blue);
        if (!s_view.bars[i]) {
            mybot_lvgl_view_destroy();
            return -1;
        }
        lv_obj_set_style_radius(s_view.bars[i], 3, 0);
    }
    s_view.timer = lv_timer_create(timer_callback, kTimerPeriodMs, nullptr);
    if (!s_view.timer) {
        mybot_lvgl_view_destroy();
        return -1;
    }
    lv_timer_pause(s_view.timer);

    const mybot_lcd_content_t initial = {MYBOT_LCD_SCREEN_STARTING, {}, 0};
    mybot_lvgl_view_update(&initial);
    return 0;
}

int mybot_lvgl_view_create(lv_display_t *display) {
    if (!display) {
        return -1;
    }
    return mybot_lvgl_view_create_sized(display, lv_display_get_horizontal_resolution(display),
                                        lv_display_get_vertical_resolution(display));
}

void mybot_lvgl_view_update(const mybot_lcd_content_t *content) {
    if (!s_view.root || !content || content->screen < MYBOT_LCD_SCREEN_STARTING ||
        content->screen >= MYBOT_LCD_SCREEN_COUNT) {
        return;
    }
    mybot_lcd_content_t normalized{};
    normalized.screen = content->screen;
    const bool pairing_code = content->screen == MYBOT_LCD_SCREEN_PAIR_CODE;
    const bool conversation = content->screen == MYBOT_LCD_SCREEN_IN_CONVERSATION;
    const bool provisioning = content->screen == MYBOT_LCD_SCREEN_WIFI_PROVISIONING;
    char provisioning_ssid[MYBOT_WIFI_PROVISIONING_SSID_CAPACITY]{};
    if (provisioning) {
        (void)mybot_wifi_get_provisioning_ssid(provisioning_ssid, sizeof(provisioning_ssid));
    }
    if (pairing_code) {
        std::memcpy(normalized.pair_code, content->pair_code, sizeof(normalized.pair_code) - 1);
    }
    if (conversation) {
        normalized.indicators = conversation_indicators(content->indicators);
    }
    if (s_view.has_content && normalized.screen == s_view.previous.screen &&
        normalized.indicators == s_view.previous.indicators &&
        std::strcmp(normalized.pair_code, s_view.previous.pair_code) == 0 &&
        std::strcmp(provisioning_ssid, s_view.provisioning_ssid) == 0) {
        return;
    }

    Presentation presentation = kScreens[content->screen];
    if (provisioning && provisioning_ssid[0]) {
        presentation.title = provisioning_ssid;
    }
    if (!s_view.has_content ||
        provisioning != (s_view.previous.screen == MYBOT_LCD_SCREEN_WIFI_PROVISIONING)) {
        /* LVGL starts a marquee only when the text exceeds the label width.
         * Restore clipping on exit to remove the provisioning scroll animations. */
        const auto mode = provisioning ? LV_LABEL_LONG_SCROLL_CIRCULAR : LV_LABEL_LONG_CLIP;
        lv_label_set_long_mode(s_view.title, mode);
        lv_label_set_long_mode(s_view.notice, mode);
        const lv_font_t *title_font = &mybot_lvgl_font_20;
#if !CONFIG_MYBOT_LANGUAGE_ZH_CN
        if (view_width() < 240 && !provisioning) {
            title_font = &lv_font_montserrat_10;
        }
#endif
        lv_obj_set_style_text_font(s_view.title, title_font, 0);
    }
    uint32_t notice_color = kTheme.muted;
    Activity activity = Activity::None;
    auto emoji = MYBOT_UI_EMOJI_NEUTRAL;
    const bool show_emoji = conversation || content->screen == MYBOT_LCD_SCREEN_READY;
    if (content->screen == MYBOT_LCD_SCREEN_READY) {
        emoji = MYBOT_UI_EMOJI_HAPPY;
    }
    if (conversation) {
        if (normalized.indicators & MYBOT_LCD_INDICATOR_LISTENING) {
            presentation.title = kListening;
            presentation.accent = kTheme.green;
            emoji = MYBOT_UI_EMOJI_RELAXED;
            activity = Activity::Listening;
        } else if (normalized.indicators & MYBOT_LCD_INDICATOR_THINKING) {
            presentation.title = kThinking;
            presentation.accent = kTheme.amber;
            emoji = MYBOT_UI_EMOJI_THINKING;
            activity = Activity::Thinking;
        } else if (normalized.indicators & MYBOT_LCD_INDICATOR_SPEAKING) {
            presentation.title = kSpeaking;
            emoji = MYBOT_UI_EMOJI_HAPPY;
            activity = Activity::Speaking;
        }
        const bool registered = normalized.indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED;
        if (registered) {
            presentation.notice = kVoiceprintSaved;
        }
        notice_color = registered ? kTheme.green : kTheme.amber;
    }
    set_text(s_view.title, presentation.title);
    set_text(s_view.status, presentation.status);
    set_text(s_view.icon, presentation.icon);
    set_text(s_view.notice, presentation.notice);
    if (!s_view.has_content || s_view.accent != presentation.accent) {
        const lv_color_t accent = lv_color_hex(presentation.accent);
        lv_obj_set_style_border_color(s_view.badge, accent, 0);
        lv_obj_set_style_text_color(s_view.icon, accent, 0);
        lv_obj_set_style_text_color(s_view.code, accent, 0);
        for (auto *bar : s_view.bars) {
            lv_obj_set_style_bg_color(bar, accent, 0);
        }
        s_view.accent = presentation.accent;
    }
    if (!s_view.has_content || s_view.notice_color != notice_color) {
        lv_obj_set_style_text_color(s_view.notice, lv_color_hex(notice_color), 0);
        s_view.notice_color = notice_color;
    }
    set_hidden(s_view.badge, pairing_code || show_emoji);
    set_hidden(s_view.emoji, !show_emoji);
    set_hidden(s_view.code, !pairing_code);
    set_hidden(s_view.activity, activity == Activity::None);
    if (show_emoji) {
        const lv_image_dsc_t *source = mybot_lvgl_ui_emoji(emoji);
        if (s_view.emoji_source != source) {
            lv_image_set_src(s_view.emoji, source);
            s_view.emoji_source = source;
        }
    }
    if (!s_view.has_content ||
        pairing_code != (s_view.previous.screen == MYBOT_LCD_SCREEN_PAIR_CODE)) {
        lv_obj_set_y(s_view.title, pairing_code ? 20 : 81);
    }
    if (pairing_code && (!s_view.has_content || normalized.screen != s_view.previous.screen ||
                         std::strcmp(normalized.pair_code, s_view.previous.pair_code) != 0)) {
        lv_point_t extent{};
        lv_text_get_size(&extent, normalized.pair_code, &lv_font_montserrat_32, 0, 0, LV_COORD_MAX,
                         LV_TEXT_FLAG_NONE);
        const int available = view_width() - (view_width() >= 240 ? 28 : 12);
        const lv_font_t *font = &lv_font_montserrat_32;
        if (extent.x > available) {
            lv_text_get_size(&extent, normalized.pair_code, &mybot_lvgl_font_20, 0, 0, LV_COORD_MAX,
                             LV_TEXT_FLAG_NONE);
            if (extent.x <= available) {
                font = &mybot_lvgl_font_20;
            } else {
                font = &lv_font_montserrat_10;
            }
        }
        lv_obj_set_style_text_font(s_view.code, font, 0);
        lv_obj_set_style_text_letter_space(
            s_view.code, font == &lv_font_montserrat_10 && view_width() < 240 ? -4 : 0, 0);
        set_text(s_view.code, normalized.pair_code);
    }
    if (!s_view.has_content || s_view.activity_mode != activity) {
        s_view.activity_mode = activity;
        s_view.animation_frame = 0;
        if (activity != Activity::None) {
            draw_activity_frame();
        }
    }
    update_notifications(normalized);
    s_view.previous = normalized;
    std::memcpy(s_view.provisioning_ssid, provisioning_ssid, sizeof(s_view.provisioning_ssid));
    s_view.has_content = true;
    update_timer();
}

void mybot_lvgl_view_destroy(void) {
    /* The same LVGL owner serializes timer callbacks and destruction. */
    if (s_view.timer) {
        lv_timer_delete(s_view.timer);
    }
    if (s_view.root) {
        lv_obj_delete(s_view.root);
    }
    s_view = View{};
}
