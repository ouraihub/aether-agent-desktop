#include <nappgui.h>

typedef struct _app_t App;

struct _app_t
{
    Window *window;
    Panel *root;
    Layout *content;
    Layout *detail;
    Label *title;
    Label *status;
    Label *url;
    Edit *search;
    Edit *api_key;
    ListBox *providers;
    Button *enabled;
    Button *theme;
    Button *detect;
    Button *sync;
    color_t bg;
    color_t panel;
    color_t text;
    color_t muted;
    color_t accent;
    bool_t dark;
};

static const char_t *i_provider_name(const uint32_t index)
{
    static const char_t *names[] = {"七牛云", "硅基流动", "Fenno"};
    return index < 3 ? names[index] : names[0];
}

static const char_t *i_provider_url(const uint32_t index)
{
    static const char_t *urls[] = {
        "https://api.qnaigc.com", "https://api.siliconflow.cn/v1",
        "https://api.fenno.ai/v1"};
    return index < 3 ? urls[index] : urls[0];
}

static void i_set_theme(App *app)
{
    if (app->dark)
    {
        app->bg = color_html("#15171a");
        app->panel = color_html("#202329");
        app->text = color_html("#f3f4f6");
        app->muted = color_html("#9ca3af");
        app->accent = color_html("#6ee7b7");
    }
    else
    {
        app->bg = color_html("#f4f5f7");
        app->panel = color_html("#ffffff");
        app->text = color_html("#20242b");
        app->muted = color_html("#68707d");
        app->accent = color_html("#15803d");
    }
    layout_bgcolor(app->content, app->bg);
    layout_bgcolor(app->detail, app->panel);
    label_color(app->title, app->text);
    label_color(app->status, app->muted);
    label_color(app->url, app->muted);
    edit_color(app->search, app->text);
    edit_bgcolor(app->search, app->panel);
    edit_color(app->api_key, app->text);
    edit_bgcolor(app->api_key, app->panel);
    label_text(app->status, app->dark ? "原生控件 · 深色主题 · 仅内存演示"
                                      : "原生控件 · 浅色主题 · 仅内存演示");
    button_text(app->theme, app->dark ? "切换浅色" : "切换深色");
    panel_update(app->root);
}

static void i_select_provider(App *app, Event *event)
{
    const EvButton *params = event_params(event, EvButton);
    const uint32_t index = params->index;
    label_text(app->title, i_provider_name(index));
    label_text(app->url, i_provider_url(index));
    edit_text(app->api_key, "");
    label_text(app->status, "未连接 · 密钥为空 · 不保存、不联网");
    unref(event);
}

static void i_toggle_theme(App *app, Event *event)
{
    app->dark = app->dark ? FALSE : TRUE;
    i_set_theme(app);
    unref(event);
}

static void i_demo_action(App *app, Event *event)
{
    const uint32_t tag = button_get_tag(event_sender(event, Button));
    if (tag == 1)
        label_text(app->status, "演示：未接 Backend，未发起检测请求");
    else
        label_text(app->status, "演示：未接 Backend，未同步模型列表");
    unref(event);
}

static Layout *i_nav(App *app)
{
    Layout *layout = layout_create(1, 7);
    const char_t *items[] = {"模型", "模型服务", "编码搭档", "MCP", "后台日志",
                             "外观", "关于"};
    uint32_t index;
    for (index = 0; index < 7; ++index)
    {
        Button *item = button_flat();
        button_text(item, items[index]);
        layout_button(layout, item, 0, index);
        cell_padding(layout_cell(layout, 0, index), 8);
    }
    layout_vmargin(layout, 1, 3);
    layout_vmargin(layout, 2, 3);
    layout_vmargin(layout, 3, 3);
    layout_vmargin(layout, 4, 3);
    layout_vmargin(layout, 5, 3);
    layout_vexpand(layout, 6);
    unref(app);
    return layout;
}

static Layout *i_provider_rail(App *app)
{
    Layout *layout = layout_create(1, 5);
    Label *heading = label_create();
    Edit *search = edit_create();
    ListBox *list = listbox_create();
    Button *new_provider = button_push();
    label_text(heading, "模型服务");
    edit_phtext(search, "搜索服务商");
    listbox_add_elem(list, "七牛云", NULL);
    listbox_add_elem(list, "硅基流动", NULL);
    listbox_add_elem(list, "Fenno", NULL);
    listbox_select(list, 0, TRUE);
    listbox_OnSelect(list, listener(app, i_select_provider, App));
    button_text(new_provider, "+ 新建服务商");
    layout_label(layout, heading, 0, 0);
    layout_edit(layout, search, 0, 1);
    layout_listbox(layout, list, 0, 2);
    layout_button(layout, new_provider, 0, 4);
    layout_vmargin(layout, 0, 8);
    layout_vmargin(layout, 1, 6);
    layout_vexpand(layout, 3);
    layout_margin(layout, 12);
    app->providers = list;
    app->search = search;
    return layout;
}

static Layout *i_detail(App *app)
{
    Layout *layout = layout_create(2, 13);
    Label *heading = label_create();
    Label *key_label = label_create();
    Label *rotation = label_create();
    Label *rotation_desc = label_create();
    Label *line = label_create();
    Label *models = label_create();
    Label *address_label = label_create();
    Label *address = label_create();
    Label *empty = label_create();
    Combo *route = combo_create();
    Button *manage = button_push();
    Button *detect = button_push();
    Button *sync = button_push();
    Button *enabled = button_check();
    Button *theme = button_push();
    Edit *key = edit_create();
    Edit *model_search = edit_create();
    Button *add = button_flat();
    cassert_no_null(app);
    label_text(heading, "七牛云");
    label_text(key_label, "API 密钥");
    label_text(rotation, "轮流使用");
    label_text(line, "API 地址 · GET /v1/models");
    label_text(models, "模型");
    label_text(empty, "未选择模型\n\n模型列表为空；同步需要 Backend。\n"
                     "此演示不会保存密钥或发出网络请求。");
    button_text(enabled, "启用");
    button_state(enabled, ekGUI_OFF);
    button_text(manage, "管理密钥");
    button_text(detect, "检测");
    button_text(sync, "同步");
    button_text(theme, "切换深色");
    button_text(add, "+");
    combo_add_elem(route, "OpenAI Chat", NULL);
    combo_selected(route, 0);
    edit_passmode(key, TRUE);
    edit_phtext(key, "输入密钥（演示不保存）");
    edit_phtext(model_search, "搜索模型");
    button_tag(detect, 1);
    button_tag(sync, 2);
    button_OnClick(theme, listener(app, i_toggle_theme, App));
    button_OnClick(detect, listener(app, i_demo_action, App));
    button_OnClick(sync, listener(app, i_demo_action, App));
    layout_label(layout, heading, 0, 0);
    layout_button(layout, enabled, 1, 0);
    layout_label(layout, key_label, 0, 1);
    layout_edit(layout, key, 0, 2);
    layout_button(layout, manage, 1, 2);
    layout_label(layout, rotation, 0, 3);
    label_text(rotation_desc, "依次使用已启用的模型服务");
    layout_label(layout, rotation_desc, 1, 3);
    layout_label(layout, line, 0, 4);
    layout_combo(layout, route, 0, 5);
    layout_button(layout, detect, 1, 5);
    label_text(address_label, "地址");
    label_text(address, i_provider_url(0));
    layout_label(layout, address_label, 0, 6);
    layout_label(layout, address, 1, 6);
    layout_label(layout, models, 0, 7);
    layout_edit(layout, model_search, 0, 8);
    layout_button(layout, add, 1, 8);
    layout_label(layout, empty, 0, 10);
    layout_button(layout, sync, 1, 11);
    layout_button(layout, theme, 1, 12);
    layout_hmargin(layout, 0, 8);
    layout_vmargin(layout, 0, 16);
    layout_vmargin(layout, 2, 8);
    layout_vmargin(layout, 4, 6);
    layout_vmargin(layout, 7, 6);
    layout_vexpand(layout, 10);
    layout_margin4(layout, 20, 20, 16, 20);
    app->detail = layout;
    app->title = heading;
    app->status = rotation;
    app->url = layout_get_label(layout, 1, 6);
    app->api_key = key;
    app->enabled = enabled;
    app->theme = theme;
    app->detect = detect;
    app->sync = sync;
    return layout;
}

static Panel *i_panel(App *app)
{
    Layout *layout = layout_create(3, 1);
    Panel *panel = panel_create();
    app->content = layout;
    layout_layout(layout, i_nav(app), 0, 0);
    layout_layout(layout, i_provider_rail(app), 1, 0);
    layout_layout(layout, i_detail(app), 2, 0);
    layout_hsize(layout, 0, 192);
    layout_hsize(layout, 1, 208);
    layout_hexpand(layout, 2);
    layout_vexpand(layout, 0);
    layout_bgcolor(layout, color_html("#f4f5f7"));
    panel_layout(panel, layout);
    return panel;
}

static void i_close(App *app, Event *event)
{
    osapp_finish();
    unref(event);
    unref(app);
}

static App *i_create(void)
{
    App *app = heap_new0(App);
    app->dark = FALSE;
    app->root = i_panel(app);
    app->window = window_create(ekWINDOW_STDRES);
    window_panel(app->window, app->root);
    window_title(app->window, "Aether NAppGUI Demo");
    window_client_size(app->window, s2df(1040, 700));
    window_OnClose(app->window, listener(app, i_close, App));
    i_set_theme(app);
    window_show(app->window);
    return app;
}

static void i_destroy(App **app)
{
    window_destroy(&(*app)->window);
    heap_delete(app, App);
}

#include <osapp/osmain.h>
osmain(i_create, i_destroy, "", App)
