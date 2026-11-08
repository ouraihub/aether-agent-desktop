# Aether 工作台设计预览

**状态：未获视觉验收的设计预览，不是 GTK/Aether 产品实现。** 用户拒绝了第一版自创线框；旧工作台/设置/诊断 SVG 已撤回。当前以 `D:\workspace-ai\autorail` 的实际源码与主题文件为依据，不再用自创状态详情栏替换其工作台。

## 打开与查看

从仓库根运行：

```text
uv run --no-project python -m http.server 8766 --bind 127.0.0.1 --directory docs/design/ui
```

浏览器打开 <http://127.0.0.1:8766/>，或直接查看已导出的 [深色工作台 PNG](workbench-dark.png)、[浅色工作台 PNG](workbench-light.png)。这是 HTML/CSS 视觉预览，不是 GTK 应用；需要 HTTP 服务读取 token，不支持直接 file:// 双击运行模块。PNG 是浏览器实际渲染导出，不依赖聊天窗口对 SVG CSS 变量的支持。

## 逐项参考依据

| 设计区域 | Autorail 实际来源 | 当前预览 |
| --- | --- | --- |
| 顶栏 | `crates/app/src/workbench.rs` top_bar；`metrics.rs` TOP_BAR=40 | 40px，品牌、PROJECT/WORKSPACE/BRANCH、连接状态、设置、窗口按钮 |
| 活动栏/项目树 | workbench activity_bar；`panels/projects.rs` | 40px活动栏，280px项目栏，树选择/分支副标题；不另加会话/任务/设置重复导航 |
| 会话列 | workbench Render；`panels/chat/view.rs` | 460px，28px标签条、Runtime/模型头、任务折叠条、消息、工具卡及底部输入框 |
| 输入框 | `chat/view.rs` composer_box | 8px外边距、8px圆角、模型选择器/用量/发送位置；预览发送禁用 |
| 右侧 | `panels/files.rs`、`panels/changes.rs` | 上方文件/差异、下方260px Changes；不换成Core/generation/授权说明面板 |
| 底部 | workbench show_terminal=false，开启后240px | 默认收起，活动栏可展开；只回显示例，不启动Shell |
| 色板 | `crates/ui/themes/{dark,light}.theme.json` colors | 默认Autorail原色：深色#09090b、侧栏#101013、顶栏#18181b、强调#8dff4f；浅色逐值参考原色 |
| 语义角色 | `crates/ui/src/theme/roles.rs` | 调色板→语义→组件；primary/border/容器与输入分开，不让所有面板同色 |
| 字体/度量 | `metrics.rs`，详细设计§9.1 | UI14、code13、metadata12、header28、row22、radius4/8；未打包Geist/Orbitron/JetBrains Mono，当前系统字体回退，因此不是像素级复制 |

本机没有 `apps/web` 原版界面目录，也未找到文档提到的workbench-light/dark截图。`.omp/smoke.png`实际为浏览器/终端截图，**不是Autorail工作台**，不作为视觉依据。Autorail源码在本轮期间有更新（chat.rs拆分view/tabs等），以上按实际最新文件重读。没有运行其完整GUI，不宣称已经截图比对原版像素一致。

用户随后提供了真实Autorail模型服务截图，模型服务页面现在以该图为直接视觉基准：七牛云关闭、空密钥、api.qnaigc.com地址、未选择模型。它不是此前误认的smoke.png；设置导航/服务商栏/详情顺序按截图与源码共同核对。

### LumaKit共享组件来源

Autorail Cargo.toml/Cargo.lock将model-picker和usage-status固定到LumaKit提交`7c46b2b9fb21c9d0d8889775d599a5d10d6499d5`。本机对应组件在`D:\workspace-ai\LumaKit`（HEAD `b5c020725cb6f4f5777a489fcb3d233e9bd7deae`），不是desk-dev下旧目录，不能当作锁定版本的逐像素证据。已读model-picker/view.rs、rail.rs和usage-status/view.rs/presentation.rs及context-usage-status.md，确认分组选择/推理滑轨、上下文百分比/累计token/费用详情与宿主主题关系。模型服务主体仍在Autorail settings/providers，不与聊天模型选择器混为一谈。预览尚未复刻完整LumaKit模型选择/用量弹层，不声称已完成这些组件。


文件/差异/终端出现在本设计中是为了严格核对用户指定的Autorail布局，不代表Aether已批准或实现其业务。正式产品启用前必须有真实Backend入口、权限/路径/进程合同及对应任务，不伪造执行结果。示例新增行、变更统计、代码签名均为设计数据，不是实际工程产物。

## 主题 Token：唯一来源与GTK映射

- `theme-tokens.json`为当前设计唯一色值源；`default`选dark/autorail，light/dark与autorail/blue/violet配色独立选择。
- `palettes`存基础颜色，`semantic`只引用palette/accent，`components`只引用语义；`metrics`集中尺寸、字号、间距与圆角。界面CSS只消费解析后的`--aether-*`，不得写散落十六进制颜色。
- `theme.mjs`的`resolveTheme`校验并解析引用，`webProperties`生成Web CSS变量，`gtkCss`由同一解析结果生成GTK命名颜色和组件类。两份输出不是两套独立主题。
- GTK4.14兼容映射使用`@define-color aether_*`、`@aether_*`和GtkCssProvider；不假设Web `var()`能直接粘到所有GTK版本。详见[GTK CSS](https://docs.gtk.org/gtk4/css-overview.html)。GTK切换在UI线程替换同一provider样式，不重建Session/VM或丢草稿，不依赖libadwaita。
- 原色照录不是全局无障碍认证。正文/按钮/焦点/禁用状态须在真实控件和多配色中检查；不能只算一种对比度或用颜色作为唯一状态信号。高对比/系统外观和持久化须在UI-11/DEC-09正式实现，当前预览只内存切换，不写用户设置。

UI框架重新评估见[GTK以外的C界面候选](UI_OPTIONS.md)：原生C控件优先对比GTK4/NAppGUI；若允许独立HTML/CSS/JS前端，则C宿主WebView在Autorail视觉保真和token方面值得同页验证。当前没有引入或切换库，C核心与Backend边界不变。


## 模型服务配置

模型服务[浏览器预览](providers.html)、[深色PNG](providers-dark.png)、[浅色PNG](providers-light.png)与[源码/交互对照](providers-reference.md)严格参考Autorail：1040px设置壳、192px导航、208px服务商栏，右侧保持标题/启用、API密钥检测/管理、轮流使用、API地址/线路、模型分组。默认三项内置预设停用/无密钥/无模型；不增加原版没有的全局保存按钮。外部检测/保存未接Backend，不伪造成功。

## 验收边界

当前只验证浏览器视觉/布局开关和Token映射，不更新UI-01～14为已完成。DEC-09目标GTK版本、原生Windows/macOS、真实IME/焦点/DPI/AT与Backend关闭链仍缺实测；工程任务与本设计交付分开。模型服务中的显示/复制秘密如与Aether正式secret_ref合同冲突，保留位置但禁用并解释，不为视觉复制放宽秘密回显规则。
