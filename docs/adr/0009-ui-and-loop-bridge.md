# ADR 0009：C17 桌面 UI 框架与循环桥接

- 状态：提议（GTK4已做部分实窗验证，未最终选定；按用户要求重新比较C界面候选，见UI_OPTIONS；DEC-09完整门槛未齐）。
- 记录日期：2026-11-08
- 依据：DEC-09；`docs/ARCHITECTURE.md` §2、§5、§9；
  `docs/CODING_STANDARDS.md` §2；`docs/INTERFACE_CONTRACTS.md` §10；
  `docs/design/detail/01-frontend.md`；GTK 官方文档与源码发布目录；NAppGUI 官方文档与
  `frang75/nappgui_src` 的 MIT 许可证/CMake 文件。
- 关系：落实 ADR-0002、ADR-0004、ADR-0005 的 UI 边界；不改变 Backend/Core
  契约，不选定 libuv、线程数量、渲染后端或 Runtime。
- 本轮追加：用户强调Autorail视觉保真与Token主题并要求评估GTK以外方案；[候选评估](../design/ui/UI_OPTIONS.md)保留GTK4、NAppGUI及C宿主WebView的实际取舍，不因HTML预览或GTK烟测自动改变技术栈。Web UI需独立界面语言/桥接/安全决策，尚未批准引入。

## 背景与硬门槛

Aether 的自研核心必须为 ISO C17，UI 只能通过独立 `ui_adapter` 边界接入。控件和
ViewModel 归 UI 线程；后台结果必须复制为有界载荷，按 receiver/view-generation 投递，
不得把窗口指针或第三方控件类型放进公共 Backend/Core 头。UI 不得同步等待 Runtime、IO
或 join；关闭必须先使 receiver 失效、停止新交互、退订并处理晚到载荷，直到
`close-complete` 及本地引用归零才释放 callback context。

DEC-09 的硬门槛是：

1. C17 可调用的稳定 C API；UI 依赖不能穿透 Core/Backend。
2. Windows、macOS、Linux 均有官方/可复核的构建与窗口支持说明，且许可可接受。
3. 有明确的主循环投递机制；后台投递不能直接触碰控件，销毁时能撤销/丢弃投递。
4. 真窗口必须验证中文输入、焦点/键盘、DPI 缩放和可访问性；还必须验证后台
   Command/Query/Event 交接、快速销毁和无悬空回调。

Pi/OMP 是 TUI/RPC 参考，不能作为 C 桌面 UI 框架或三平台输入/可访问性证据。
LumaDesk/Autorail 的 gpui-kit/Rust 事件模式只证明“UI 投递意图、后台结果回 UI、状态
与 UI 分离”的边界，不构成 Aether 依赖选择。

## 候选比较

| 候选 | C/平台/构建 | 循环、投递与销毁 | 中文/焦点/DPI/可访问性 | 许可与结论 |
| --- | --- | --- | --- | --- |
| **GTK 4.24.1** | GTK 的公开 API 是 C；GNOME 官方源目录在本记录日期列出 4.24.1（2026-10-01）。官方提供 Linux、Windows（MSYS2 或 gvsbuild）和 macOS（Quartz、jhbuild/bundler）安装/构建路径。 | GLib `g_main_context_invoke_full()` 将函数安排在指定 MainContext 所拥有的主循环执行，且支持销毁通知；GTK 官方线程文档明确控件通常只能在主线程访问，后台工作应以 idle/invoke 回主线程。Aether 必须在 adapter 中再加 receiver 代次、有限 queue、关闭门和载荷所有权，不能把 invoke 当成 close-complete。 | GTK 标准控件默认实现 `GtkAccessible`；官方文档列出 Linux AT-SPI、macOS NSAccessibility、Windows Active Accessibility。Widget 可取得 scale factor。中文 IME、实际焦点遍历、DPI 热切换和 AT 工具连接本仓库尚未实测。 | GTK API/源码为 LGPL-2.1-or-later；应用分发仍须核实各依赖及源代码/许可证履行。**推荐**：C17 边界清晰、投递和可访问性文档最完整，但验收前不是已接受依赖。 |
| **NAppGUI 1.6.2（官网页脚版本）** | 官方定位为跨平台 C SDK，覆盖 Windows/macOS/Linux；CMake 文档给出三平台构建/安装命令，Linux 图形后端是 GTK3，Windows 是 Win32，macOS 是 AppKit。SDK 主要 C，但官方明确含少量 C++/Objective-C，必须隔离在 UI adapter。 | `osmain` 管理 message cycle；`osapp_task` 在后台线程执行 `task_main`，再在 GUI 线程执行 `task_update/task_end`；`window_OnClose`、`window_destroy` 提供窗口关闭/销毁。文档没有足够的“已排队 task 在销毁后如何取消、callback context 何时安全释放”的契约，Aether 仍需自己包裹生命周期。 | 官网明确 HiDPI、Unicode/资源翻译、焦点/tabstop；但本次官方资料未发现跨平台 screen-reader/accessibility API 或 AT 映射说明。中文输入和可访问性必须实窗验证，缺少可审查的前置证据。 | 官方 MIT 许可证，CMake 源码也带 MIT；**不选作当前目标**：accessibility 证据缺口是 DEC-09 硬门槛，且内部 C++/Objective-C 使绑定隔离和工具链矩阵更复杂。若 GTK4 不能通过真实门槛，NAppGUI 只能作为重新调查候选，不是隐式 fallback。 |

未纳入 GPUI/gpui-kit、SDL、Qt：GPUI/gpui-kit 是 Rust；Qt 的主要应用 API 是 C++；SDL
是窗口/输入/渲染底层而非本项目所需的可访问标准控件集合。它们不能在本次 C17 UI
硬门槛下减少验证工作。

## 决策与边界

1. **推荐 GTK 4.24.1 作为 Frontend 的首选 UI 实现。** 版本必须在工程锁定时记录
   GTK、GLib、Pango、GDK 等实际依赖的精确版本、来源、构建器和许可证。GTK 类型只能
   出现在 `ui_adapter`/绑定实现，不能出现在 Core、Backend 公共头或 DTO。
2. **采用单一 GTK 主线程。** `gtk_application_run()`/默认 `GMainContext` 驱动控件；
   后台 Client 回调只复制 DTO/Event 到有界 Aether UI Queue。主线程用受控的 idle/source
   或 `g_main_context_invoke_full()` 消费 queue，再按 receiver_id、view_generation、
   subscription_id 校验。队列满必须让 receiver 进入健康失效/ResyncRequired 或拒绝
   载荷，不同步堵塞主循环、不无限增长。
3. **投递不是对象寿命保证。** 每个载荷带自己的释放函数和 receiver/generation；关闭
   先从注册表移除接收关系、撤销可取消 source、释放尚未消费载荷，再调用 Backend
   Unsubscribe。正在运行的主线程 callback 结束前，callback context 保持存活；收到
   `close-complete` 且本地 queue 引用为零后才释放。不得在 GTK `destroy` 信号里仅靠
   “is_destroyed” 标志访问页面。
4. **事件循环不承担 Backend 状态拥有者职责。** GTK callback 只提交类型化意图，Backend
   仍是请求/事件终态权威；不在 UI 线程等待 Runtime、磁盘、网络或大解析。应用关闭
   遵循前端设计的禁交互、失效 receiver、退订、载荷收尾和有限进度显示顺序。
5. **默认控件优先。** Entry/Button/List 等标准控件保留 GTK accessibility role、label、
   relation 和键盘语义；自绘控件必须显式实现 accessible role/state/property，并在
   AT 工具中检查。中文文案使用 UTF-8 资源，不把输入法候选或组合文本当作普通按键序列
   自行重写。

## 最小真实验证（选择证据，不等于功能实现）

本轮已使用WSL Ubuntu现有GTK **4.14.5**（pkg-config实查）运行一次性C17窗口探针；不将现有版本当成4.24.1验证。真实WSLg显示环境DISPLAY=:0、WAYLAND_DISPLAY=wayland-0。探针创建GtkApplication/Window/Entry/Label，worker通过显式`GSource` attach到UI main context，回调断言当前线程是UI线程后更新文本；不依赖`g_main_context_invoke_full()`在某些所有权条件下可能直接在调用线程执行的行为。

实际输出：`GTK runtime 4.14.5`、`worker event applied on UI thread`、`window mapped=1 scale=1 accessible-role=68`，程序化设置并读取中文UTF-8相等，退出0。首次严格编译因GTK第三方头额外分号产生pedantic错误，改将pkg-config include以-isystem隔离后自研源仍保留-Wall/-Wextra/-Wpedantic/-Werror，构建通过。运行出现EGL/MESA驱动告警，但窗口mapped与回投成功；未据此宣称渲染后端无问题。

这只证明实际主循环/窗口映射及线程交接，不是完整Backend/UI Queue或产品验收。中文值是程序设置，不是输入法测试；scale=1不是高DPI切换；GtkAccessible角色存在不是AT连接验证。原生Windows未发现pkg-config/GTK SDK；macOS运行环境当前不可用；未擅自安装系统GUI工具链。实验源码/二进制交付后清理，不留下未验收永久UI脚本。

可复核构建方法：在GTK开发包环境由pkg-config取gtk4 cflags/libs，自研源用-std=c17和严格警告，第三方include用-isystem；显式attach idle source，验证UI线程身份并观察mapped/scale/text，最后退出。Windows可依官方MSYS2 UCRT64或gvsbuild，macOS依官方GTK-OSX/jhbuild；具体发行依赖及许可须锁定，不能把上述入口当已经构建。

观察记录必须至少包括：

- 真窗口出现；worker经显式附加到UI主循环的source投递，回调核对UI线程，再更新控件；不能假设invoke_full永远异步或自动保证对象寿命。
- 在 Entry 中输入并提交中文（含组合输入法候选），鼠标/Tab/Shift-Tab/Enter/Escape
  焦点行为符合预期；记录操作系统、桌面环境、输入法和 GTK/GLib 版本。
- 改变系统缩放或跨不同 DPI 显示器移动窗口；记录 `gtk_widget_get_scale_factor()`
  及文字/控件是否可用，不能只凭截图宣称 DPI 通过。
- 用平台实际 AT 工具检查窗口、Entry、Button、Label 的名称、角色、焦点和状态：
  Windows UIA/Accessibility Insights（GTK 文档称 Windows Active Accessibility，
  实际桥接需记录）；macOS VoiceOver/NSAccessibility；Linux Orca/AT-SPI。
- 关闭窗口或退出应用时，先停止提交并等待 worker；扩展实验需制造晚到事件、快速关闭、
  重复关闭和投递失败，确认无 callback 访问已销毁 VM、无泄漏、无借用 Runtime 被误杀。

本轮一次性探针没有Aether有界队列/OpenView/Unsubscribe/close-complete、真实中文IME/DPI切换或AT验证，DEC-09仍未完成。不把基本窗口通过当产品界面完成，也不把没有原生Windows SDK当所有平台均无法验证。

## 影响、风险与解除条件

正面影响：GTK 提供与 C17 兼容的公开 C API、成熟主循环/后台回投路径和标准无障碍
抽象；Frontend 可把 GTK 限制在单一 adapter，保留未来替换框架的 DTO/VM 边界。

风险：GTK Windows/macOS 的打包与依赖源链比 NAppGUI 静态库复杂；GTK 标准控件并不
自动使自绘消息块可访问；`g_main_context_invoke_full()` 的调用成功不代表载荷已执行，
也不提供 Aether 的退订语义；GTK/GLib 版本升级可能改变平台行为；本记录没有实测三平台。

解除“提议/阻塞”状态至少需要：

1. GTK 4.24.1（或工程锁定的后续补丁版）在 Windows、macOS、Linux 各有一次可复核真实
   窗口记录和可重复构建入口；缺失任一平台时明确记录平台阻塞，不得宣称三平台支持。
2. FE-17 的中文输入、键盘焦点、DPI 和 accessibility 在三平台均有实际结果；任何一项
   失败都须修复 adapter/控件或改选框架，不能用模拟器替代。
3. 一个真实 Backend Command/Query/Event 贯穿 UI Queue，并实测快切/关闭/晚到事件、
   queue 满、退订和 `close-complete`；记录资源释放和借用 Runtime 关闭边界。
4. 完成 GTK 及传递依赖的许可证、版本、构建和打包清单审查。

UI-01正式集成前须补齐上述证据及目标依赖清单；不需要用户再次选择相同方案。无法运行的平台/输入工具是具体环境缺口，保留阻塞并推进不依赖UI的工程任务，不能把静态窗口当完整UI交付。

## 官方来源

- [GTK线程与主循环](https://docs.gtk.org/gtk4/section-threading.html)、[可访问性与平台桥接](https://docs.gtk.org/gtk4/section-accessibility.html)、[GTK4发布目录](https://download.gnome.org/sources/gtk/4.24/)。
- [GTK Windows安装](https://www.gtk.org/docs/installations/windows/)、[macOS安装](https://www.gtk.org/docs/installations/macos/)、[Linux安装](https://www.gtk.org/docs/installations/linux/)、[GTK API与许可证入口](https://docs.gtk.org/gtk4/)。
- [NAppGUI C/平台定位](https://nappgui.com/en/start/start.html)、[构建](https://nappgui.com/en/guide/build.html)、[GUI/HiDPI](https://nappgui.com/en/gui/gui.html)、[后台任务与主循环](https://nappgui.com/en/osapp/osapp.html)、[许可](https://nappgui.com/en/legal/license.html)。
