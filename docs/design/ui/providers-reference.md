# 模型服务：Autorail 源码对照预览

入口：`providers.html`。从与工作台相同的本地 HTTP 服务打开，不依赖 GTK、外网字体、图标库或 CDN。默认 `dark / autorail`；所有颜色经 `theme.mjs` 从 `theme-tokens.json` 解析。预览与原生技术选型无关。

已读用户提供截图（会话附件`pasted-image-149febca4be8bf01.png`，原始1651×1083、显示1568×1029），核对默认状态、灰绿配色及区域比例。当前页面已由父任务浏览器实际运行，导出明暗PNG；没有做原生GPUI截图逐像素比较，系统缩放/字体/控件仍有差异。未把HTML预览当GTK产品验收。

## 当前源码依据

以下路径相对 `D:/workspace-ai/autorail/`，行号来自本轮最新读取的源码，不把设计文档中过时的导航描述覆盖实际实现。

| 区域 | 源文件与行号 | 保留内容 |
| --- | --- | --- |
| 设置外框、导航、滚动 | `crates/app/src/panels/settings/mod.rs:115–121, 183–226` | 1040 宽、192 导航、内容 padding 16；模型 → 模型服务 → 编码搭档 → MCP 服务器 → 后台日志；界面分组下外观；主面板无保存按钮 |
| 双栏、头像 | `crates/app/src/panels/settings/providers/mod.rs:324–359` | 左右间距 16；24 方形首字头像 |
| 服务商列表 | `providers/rail.rs:29–107, 111–181`（上表同级目录下） | 名称搜索、全部/启用/停用、36 高行、更多菜单、新建；有启用密钥时绿点、无密钥黄点、停用无点；预设删除禁用 |
| 详情顺序 | `providers/detail.rs:62–105, 119–195` | 标题/设置/启用 → API 密钥 → 轮流使用说明 → 地址/线路 → 模型；线路 picker 仅多线路显示；添加仅未使用协议 |
| 密钥行 | `providers/detail/key.rs:31–156, 258–278` | 空密码输入；已有密钥标签/掩码/+N；眼睛左侧 hover 显示复制/编辑；管理按钮在检测前；空输入 blur/Enter 为真实添加入口 |
| 模型列表 | `providers/models.rs:116–259, 262–378` | 160 宽搜索、能力过滤、同步/添加；分组/全部折叠、40 高组头、44 高模型行、能力图标、设置/移除 |
| 新建/编辑服务商 | `providers/dialogs.rs:63–110, 140–146` | 440 宽；名称；新建另含协议、API 地址；直连/网关、网关说明 |
| 线路对话框 | `providers/dialogs/line.rs:79–189, 212–253` | 520 宽；协议、地址、POST 预览、# 说明、请求头名称/取值及增删、共享说明、默认/删除；最后线路不能删；删除二次确认 |
| 密钥管理 | `providers/keys.rs:71–83, 163–288` | 520 宽；说明；标签/掩码/复制/编辑/删除/启停；120 宽标签输入与新值密码框；编辑留空说明；启用数量；关闭页脚 |
| 手动模型与设置 | `providers/models/editor.rs:43–50, 68–74, 135–141, 200–299` | 添加 420 宽，仅模型 ID；设置 520 宽；ID、名称/类型、线路（首项默认）、上下文/最大输出、视觉/推理/工具、七档推理、下一消息说明 |
| 同步挑选结构 | `providers/models/picker.rs:35–91, 153–170, 200–325, 329–409` | 真版先联网再挑选；预览明确未联网，额外静态字段展示，不假装探测到模型 |
| 文案/协议/类型 | `crates/app/src/strings/settings_providers.rs:10–121, 123–151` | 源码中文、三种协议全名、八种模型类型；推理档中文见 `strings/settings_models.rs:26–34` |
| 尺寸、字体 | `crates/ui/src/metrics.rs:7–58, 112–135` | 间距 4/6/8/12/16/24；14 UI、12 metadata/标签、16 标题、13 code、11 probe；圆角 4/6/8 |
| 规范 | `docs/design/agent-settings/04-detail.md:614–633` | §7.6 模型服务与 §7.7 密钥；实际控件顺序以当前源码为准 |
| URL 拼接 | `crates/routing/src/url.rs:6–49` | OpenAI 自动补版本、Anthropic 去末尾版本、# 禁止规范化；GET/POST 仅文本预览 |

## 默认预设与截图状态

`crates/core/src/preset.rs:18–39` 精确有三个内置预设，顺序为：

| ID | 名称 | 公共 API 地址 | 默认协议 |
| --- | --- | --- | --- |
| qiniu | 七牛云 | `https://api.qnaigc.com` | OpenaiChat |
| siliconflow | 硅基流动 | `https://api.siliconflow.cn/v1` | OpenaiChat |
| fenno | Fenno | `https://api.fenno.ai/v1` | OpenaiChat |

`crates/settings-store/src/presets.rs:3–5, 12–24, 27–49`：种子均停用、直连、无请求头、单线路、无模型、无密钥；按 `preset_id` 幂等插入，已有行不覆写用户编辑；同名用户服务商阻止对应种子插入，不重命名。内置预设删除禁用规则见 `rail.rs:143–146`。

默认预览与截图一样是三个停用行、选中七牛云、空密钥、无模型；地址显示公开真实预设 URL（无凭据），GET 显示 `/v1/models`。这些 URL **只作为字符串，不会被 fetch**；页面唯一 fetch 是同目录主题 JSON。截图状态恰与源码种子默认一致，并非从截图推断数据库状态，也不代表读取了用户持久化配置。

预览工具的“交互示例”仅给七牛云装入内存中的虚构模型/两条线路/密钥掩码；额外线路使用 `example.invalid`。它不是 Backend 返回数据。刷新或选择“截图空配置”还原空配置。

## 区域尺寸与边界

均为 CSS px，不直接等同截图的屏幕像素。

| 区域 | 本页尺寸 |
| --- | --- |
| 设置框 | 外宽 1040（含 1 边框）、圆角 8；Web 标题栏 48（适配值，不冒称 GPUI Dialog 内部常量） |
| 设置内容高度 | `80vh - 72px`，对应源码 shell 高度公式 |
| 左导航 | 宽 192、padding 12、间距 4、行高 28、左右 padding 8、图标 14、界面分组上距 8 |
| 内容滚动区 | padding 16；剩余宽度由 flex 分配，不人为增加“作用域”卡片 |
| provider rail | 固定 208；搜索行 gap 6；列表 gap 4；provider 行 36、padding-x 8、gap 8；头像 24；点 6 |
| rail/detail | gap 16；详情 flex 1/min-width 0；详情区块 gap 16 |
| 密钥 | 行高 32；控件 gap 8；标签区 gap 6；掩码动作 24，copy/edit gap 2 |
| 轮流使用 | 标签/标题/说明 gap 4 |
| URL | 标签/选择器/地址/GET gap 6；地址/设置 gap 8 |
| 模型工具栏 | gap 4；搜索 160；Web 小控件高 28；源码 IconButton 24 |
| 模型列表 | 外圆角 8；组头 40、padding-x 12、gap 8；模型行 44、padding-x 12、gap 8；能力 24、gap 4 |
| 空模型 | padding-y 24、虚线边框、圆角 6；精确空文案“未选择模型” |
| 子对话框 | 新建/服务商设置 440；线路/密钥/模型设置 520；手动添加 420；静态挑选字段 640 |
| 子表单 | 字段间距 12；标签/控件 gap 4；密钥管理行 36、标签宽 120 |

### 组件来源与尚未验证的内部样式

`Cargo.toml:34` 依赖 `gpui-kit = 0.9.0`。已读 Autorail 自有 `crates/ui/src/components/buttons.rs:60–92`（IconButton 24、icon 16、圆角 4）与 `components/tabs.rs:195–228`（Segment 高 24、padding/gap 2、项目 padding-x 6）。不是把所有按钮一律当通用 32 高控件。

模型服务目录当前没有直接使用 `lumakit-model-picker` 或 `lumakit-usage-status`，不会强行引入另一套布局。Autorail `Cargo.toml:35–37` 的两个共享包锁定 LumaKit `7c46b2b9fb21c9d0d8889775d599a5d10d6499d5`；父任务核对的新源码位于 `D:/workspace-ai/LumaKit`，不是旧 `D:/workspace-ai/desk-dev/LumaKit`，且当前本地版本不能冒称与锁定版本相同。

本地 Cargo registry 搜索未取得 gpui-kit 源码，因此 **Dialog、Input、Button、Switch、Checkbox、PopupMenu 的内部默认 padding/边框/尺寸尚未源码核验**。本页对这部分采用明确的 Web 适配值；浏览器原生 select/menu 焦点表现与 GPUI 不同。图标为 inline SVG/currentColor 的同义图标，并非声称复制上游图标资产。

## 保留的本地交互与安全差异

- 服务商选择、名称搜索与启停筛选；更多菜单的编辑；预设删除禁用；新建打开准确字段。
- 线路切换仅改变视图，并重置模型搜索/能力/折叠状态；模型搜索同时匹配 ID/名称，能力过滤、分组折叠、全部折叠。
- 新建、线路、密钥管理、模型添加/设置均可打开、输入、取消、关闭；请求头行本地增删；推理开关控制七档显示；推理档按钮可本地切换。
- 启停开关仅演示内存状态并明确提示没有保存；所有检测/同步/写入/移除/外部链接都显示“设计预览未接Backend”，不伪造成功、不调用 Backend、不改动模型配置。
- URL blur/Enter 和首个密钥 blur/Enter 只报告未执行；不会把输入冒充已保存；密码输入提交/离开后清空。各子对话框关闭即丢弃输入。
- 同步不会声称发现可用模型；只打开标明“静态示例（非网络结果）”的字段展示，不具备真实同步后挑选保存语义。
- Autorail 原版允许 reveal/copy：见 `detail/key.rs:233–255`、`keys.rs:87–99`。Aether 合同禁止 secret Query/DTO 回显，因此位置与顺序保留，但**显示原密钥/复制明文禁用**，解释安全限制；新值字段不预填原 key。没有剪贴板 API、秘密存储或真实密钥读操作。
- 本页不是完整产品设置：其他导航项不会假装加载空壳产品页，只提示本切片范围。没有全局大 Save，也没有自创作用域卡片。

## 父任务浏览器检查 selectors

1. 初始 `#settings-dialog` / `#settings-nav` / `#provider-rail` / `#provider-name`；三个 `[data-provider]`；`#provider-enabled[aria-checked="false"]`；`#provider-first-key`；`#provider-url`；`#provider-probe`；`.empty-model-list`。
2. 搜索 `#provider-search`；筛选 `#providers-filter` → `#popup-menu button`；选中 `[data-select="fenno"]`；菜单 `[data-provider-menu="qiniu"]`。
3. 新建 `#providers-add` → `#provider-modal[open]`、`#new-name` / `#new-protocol` / `#new-url` / `#provider-access`；提交后 `#modal-status` 应明确未执行且对话框保持打开。
4. 线路 `#provider-line-settings` → `#line-protocol` / `#line-url` / `#line-request` / `#line-header-add` / `#header-fields`；初始 `#line-remove:disabled`；`#provider-add-line` 只提供未用协议。
5. 密钥 `#provider-keys` → `#keys-label` / `#keys-value[type="password"]` / `#keys-submit`；原配置无假 key。
6. `#fixture` 选 `populated` 后：`#provider-line` 切 Responses；`#model-search`、`#models-cap`、`[data-fold]`、`#models-fold`；密钥 `#provider-key-reveal:disabled`；hover `.key-actions` 显示 copy/edit，copy 仍 disabled。
7. `[data-model-settings="deepseek-reasoner"]` → `#model-name` / `#model-type` / `#model-line-chat` / `#model-context` / `#model-output` / `#model-cap-reasoning` / `#model-efforts`；`#models-add` → `#add-model-id`。
8. `#provider-check` / `#models-sync` 后查看 `#preview-status` / `#modal-status`，不得出现“连接正常”或保存成功；`#appearance` / `#accent` 对所有区域切换共享主题；`#modal-close`、`[data-close]`、Escape 可关闭子对话框。

代码文件均低于300物理行，视图渲染与本地事件路由属于此独立设计切片，未建立第二套主题。父任务已实际验证三预设选择/地址与空态、搜索、删除禁用、线路请求头添加、新建/密钥/模型对话框、未接Backend提示、六组主题；其余selector供后续完善检查，不声明所有原版业务已实现。
