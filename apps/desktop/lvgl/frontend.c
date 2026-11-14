#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#include <lvgl/lvgl.h>
#include <lvgl/drivers/windows/lv_windows_display.h>
#include <lvgl/drivers/windows/lv_windows_input.h>

#define WINDOW_WIDTH 1040
#define WINDOW_HEIGHT 700
#define TOP_BAR_HEIGHT 40
#define NAV_WIDTH 192
#define PROVIDER_WIDTH 208
#define SPACE 16
#define ROW 28

typedef struct Theme {
    lv_color_t window;
    lv_color_t sidebar;
    lv_color_t topbar;
    lv_color_t panel;
    lv_color_t input;
    lv_color_t text;
    lv_color_t muted;
    lv_color_t border;
    lv_color_t accent;
    lv_color_t warning;
} Theme;

typedef struct App {
    lv_display_t *display;
    lv_obj_t *screen;
    bool dark;
    Theme theme;
} App;

static lv_color_t color(uint32_t value)
{
    return lv_color_hex(value);
}

static void set_theme(App *app)
{
    if (app->dark) {
        app->theme = (Theme){
            color(0x09090b), color(0x101013), color(0x18181b),
            color(0x111114), color(0x18181b), color(0xf4f4f5),
            color(0xa1a1aa), color(0x27272a), color(0x8dff4f),
            color(0xfacc15)};
    } else {
        app->theme = (Theme){
            color(0xf4f4f5), color(0xffffff), color(0xffffff),
            color(0xffffff), color(0xf4f4f5), color(0x18181b),
            color(0x71717a), color(0xe4e4e7), color(0x3f7d20),
            color(0xa16207)};
    }
}

static void background(lv_obj_t *obj, lv_color_t fill, lv_color_t border)
{
    lv_obj_set_style_bg_color(obj, fill, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(obj, border, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_radius(obj, 4, 0);
}

static lv_obj_t *text(lv_obj_t *parent, const char *value,
                      lv_color_t foreground)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_label_set_text(obj, value);
    lv_obj_set_style_text_color(obj, foreground, 0);
    lv_obj_set_style_text_font(obj, &lv_font_source_han_sans_sc_16_cjk, 0);
    return obj;
}

static lv_obj_t *flat_button(lv_obj_t *parent, const char *value,
                             lv_color_t fill, lv_color_t foreground)
{
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_width(button, LV_PCT(100));
    lv_obj_set_height(button, ROW);
    lv_obj_set_style_bg_color(button, fill, 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(button, 4, 0);
    lv_obj_set_style_pad_left(button, 8, 0);
    lv_obj_set_style_pad_right(button, 8, 0);
    lv_obj_set_style_text_color(button, foreground, 0);
    lv_obj_set_style_text_font(button, &lv_font_source_han_sans_sc_16_cjk, 0);
    lv_obj_t *caption = text(button, value, foreground);
    lv_obj_center(caption);
    return button;
}

static lv_obj_t *field(lv_obj_t *parent, const char *placeholder,
                       Theme *theme, bool password)
{
    lv_obj_t *edit = lv_textarea_create(parent);
    lv_textarea_set_one_line(edit, true);
    lv_textarea_set_placeholder_text(edit, placeholder);
    lv_textarea_set_password_mode(edit, password);
    lv_obj_set_width(edit, LV_PCT(100));
    lv_obj_set_height(edit, 32);
    lv_obj_set_style_bg_color(edit, theme->input, 0);
    lv_obj_set_style_bg_opa(edit, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(edit, theme->border, 0);
    lv_obj_set_style_border_width(edit, 1, 0);
    lv_obj_set_style_radius(edit, 4, 0);
    lv_obj_set_style_text_color(edit, theme->text, 0);
    lv_obj_set_style_text_color(edit, theme->muted, LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_text_font(edit, &lv_font_source_han_sans_sc_16_cjk, 0);
    return edit;
}

static void add_section_label(lv_obj_t *parent, const char *value,
                              Theme *theme)
{
    lv_obj_t *heading = text(parent, value, theme->text);
    lv_obj_set_height(heading, 24);
    lv_obj_set_style_text_font(heading, &lv_font_source_han_sans_sc_16_cjk, 0);
}

static void add_nav(App *app, lv_obj_t *body)
{
    const char *items[] = {
        "模型", "模型服务", "编码搭档", "MCP 服务器", "后台日志",
        "外观", "关于"};
    lv_obj_t *nav = lv_obj_create(body);
    lv_obj_set_size(nav, NAV_WIDTH, LV_PCT(100));
    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(nav, 12, 0);
    lv_obj_set_style_pad_row(nav, 4, 0);
    background(nav, app->theme.sidebar, app->theme.border);
    for (uint32_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        lv_obj_t *item = flat_button(nav, items[i],
                                     i == 1 ? app->theme.input :
                                              app->theme.sidebar,
                                     i == 1 ? app->theme.accent :
                                              app->theme.muted);
        lv_obj_set_height(item, ROW);
    }
    lv_obj_set_flex_grow(lv_obj_create(nav), 1);
    flat_button(nav, app->dark ? "切换浅色" : "切换深色",
                app->theme.sidebar, app->theme.muted);
}

static void add_provider_rail(App *app, lv_obj_t *body)
{
    const char *providers[] = {"七牛云", "硅基流动", "Fenno"};
    const char *urls[] = {
        "https://api.qnaigc.com", "https://api.siliconflow.cn/v1",
        "https://api.fenno.ai/v1"};
    lv_obj_t *rail = lv_obj_create(body);
    lv_obj_set_size(rail, PROVIDER_WIDTH, LV_PCT(100));
    lv_obj_set_flex_flow(rail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(rail, 12, 0);
    lv_obj_set_style_pad_row(rail, 6, 0);
    background(rail, app->theme.sidebar, app->theme.border);
    add_section_label(rail, "模型服务", &app->theme);
    field(rail, "搜索服务商", &app->theme, false);
    flat_button(rail, "全部  ·  启用  ·  停用", app->theme.input,
                app->theme.muted);
    lv_obj_t *list = lv_obj_create(rail);
    lv_obj_set_width(list, LV_PCT(100));
    lv_obj_set_flex_grow(list, 1);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 4, 0);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    for (uint32_t i = 0; i < 3; ++i) {
        lv_obj_t *row = flat_button(list, providers[i],
                                    i == 0 ? app->theme.input :
                                             app->theme.sidebar,
                                    app->theme.text);
        lv_obj_set_style_text_align(row, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_user_data(row, (void *)urls[i]);
    }
    flat_button(rail, "+ 新建服务商", app->theme.sidebar, app->theme.accent);
}

static void add_key_row(App *app, lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, 36);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_t *key = field(row, "输入密钥（不会回显）", &app->theme, true);
    lv_obj_set_flex_grow(key, 1);
    flat_button(row, "管理密钥", app->theme.input, app->theme.text);
}

static void add_model_section(App *app, lv_obj_t *parent)
{
    add_section_label(parent, "模型", &app->theme);
    lv_obj_t *tools = lv_obj_create(parent);
    lv_obj_set_width(tools, LV_PCT(100));
    lv_obj_set_height(tools, 32);
    lv_obj_set_flex_flow(tools, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(tools, 0, 0);
    lv_obj_set_style_pad_column(tools, 4, 0);
    lv_obj_set_style_bg_opa(tools, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tools, 0, 0);
    lv_obj_t *search = field(tools, "搜索模型", &app->theme, false);
    lv_obj_set_flex_grow(search, 1);
    flat_button(tools, "能力", app->theme.input, app->theme.muted);
    flat_button(tools, "同步", app->theme.input, app->theme.muted);
    flat_button(tools, "+", app->theme.accent, app->theme.window);
    lv_obj_t *empty = lv_obj_create(parent);
    lv_obj_set_width(empty, LV_PCT(100));
    lv_obj_set_height(empty, 130);
    lv_obj_set_flex_flow(empty, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(empty, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(empty, 24, 0);
    background(empty, app->theme.input, app->theme.border);
    text(empty, "未选择模型", app->theme.text);
    text(empty, "模型列表为空；同步需要 Backend。\n"
                "此预览不会保存密钥或发出网络请求。", app->theme.muted);
}

static void add_detail(App *app, lv_obj_t *body)
{
    lv_obj_t *detail = lv_obj_create(body);
    lv_obj_set_flex_grow(detail, 1);
    lv_obj_set_height(detail, LV_PCT(100));
    lv_obj_set_flex_flow(detail, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(detail, SPACE, 0);
    lv_obj_set_style_pad_row(detail, SPACE, 0);
    background(detail, app->theme.window, app->theme.border);

    lv_obj_t *heading = lv_obj_create(detail);
    lv_obj_set_width(heading, LV_PCT(100));
    lv_obj_set_height(heading, 32);
    lv_obj_set_flex_flow(heading, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(heading, 0, 0);
    lv_obj_set_style_bg_opa(heading, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(heading, 0, 0);
    lv_obj_t *name = text(heading, "七牛云", app->theme.text);
    lv_obj_set_flex_grow(name, 1);
    flat_button(heading, "启用", app->theme.input, app->theme.muted);
    text(detail, "模型服务 · 预览未接 Backend", app->theme.muted);

    add_section_label(detail, "API 密钥", &app->theme);
    add_key_row(app, detail);
    add_section_label(detail, "轮流使用", &app->theme);
    text(detail, "依次使用已启用的模型服务", app->theme.muted);

    add_section_label(detail, "API 地址 · GET /v1/models", &app->theme);
    lv_obj_t *route = flat_button(detail, "OpenAI Chat", app->theme.input,
                                  app->theme.text);
    lv_obj_set_style_text_align(route, LV_TEXT_ALIGN_LEFT, 0);
    text(detail, "地址", app->theme.muted);
    text(detail, "https://api.qnaigc.com", app->theme.text);
    add_model_section(app, detail);
}

static void build_page(App *app)
{
    set_theme(app);
    lv_obj_clean(app->screen);
    lv_obj_set_style_bg_color(app->screen, app->theme.window, 0);
    lv_obj_set_style_bg_opa(app->screen, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(app->screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(app->screen, 0, 0);
    lv_obj_set_style_pad_row(app->screen, 0, 0);

    lv_obj_t *top = lv_obj_create(app->screen);
    lv_obj_set_size(top, LV_PCT(100), TOP_BAR_HEIGHT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_left(top, 16, 0);
    lv_obj_set_style_pad_right(top, 8, 0);
    lv_obj_set_style_pad_column(top, 16, 0);
    background(top, app->theme.topbar, app->theme.border);
    lv_obj_t *brand = text(top, "AETHER", app->theme.accent);
    lv_obj_set_style_text_font(brand, &lv_font_montserrat_16, 0);
    text(top, "PROJECT", app->theme.muted);
    text(top, "WORKSPACE", app->theme.muted);
    text(top, "BRANCH", app->theme.muted);
    lv_obj_t *connected = text(top, "● 未连接", app->theme.warning);
    lv_obj_set_flex_grow(connected, 1);
    flat_button(top, app->dark ? "浅色" : "深色", app->theme.topbar,
                app->theme.muted);

    lv_obj_t *body = lv_obj_create(app->screen);
    lv_obj_set_width(body, LV_PCT(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_pad_column(body, 0, 0);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    add_nav(app, body);
    add_provider_rail(app, body);
    add_detail(app, body);
}

int main(void)
{
    App app = {0};
    app.dark = true;
    lv_init();
    app.display = lv_windows_create_display(
        L"Aether · 模型服务", WINDOW_WIDTH, WINDOW_HEIGHT, 100, false, false);
    if (app.display == NULL) return 1;
    lv_lock();
    if (lv_windows_acquire_pointer_indev(app.display) == NULL ||
        lv_windows_acquire_keypad_indev(app.display) == NULL) {
        lv_unlock();
        return 1;
    }
    app.screen = lv_screen_active();
    build_page(&app);
    lv_obj_invalidate(app.screen);
    lv_unlock();
    (void)puts("aether_frontend_lvgl: Autorail provider preview ready");
    while (lv_display_get_next(NULL) != NULL) {
        lv_lock();
        uint32_t wait_ms = lv_timer_handler();
        lv_unlock();
        if (wait_ms == LV_NO_TIMER_READY) wait_ms = 33;
        lv_sleep_ms(wait_ms);
    }
    return 0;
}
