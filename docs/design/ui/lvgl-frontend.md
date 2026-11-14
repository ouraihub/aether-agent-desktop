# LVGL 候选 Frontend 设计

## 1. 状态与范围

状态：候选实现设计，Windows 技术切片实施中；不替换已登记的 NAppGUI
Frontend 入口，不代表 UI-01 或 DEC-09 已完成。

本设计验证一个可运行的 LVGL 桌面 Frontend Adapter：固定 LVGL 版本、Windows
真窗口、Autorail 风格工作台布局、主题切换、窗口缩放和关闭。它不实现业务
Command/Query/Event，不读取 Core/Backend/数据库/Runtime，不使用测试 Mock 冒充
业务连接。

后续若通过中文 IME、焦点、DPI、辅助功能、Backend DTO 交接和关闭链验收，才可
在 `ui_adapter` 边界选择 LVGL 作为正式 Frontend。若失败，删除或停用本 target
不影响 Backend、Core 或 NAppGUI。

## 2. 目标与非目标

### 目标

- 以 `aether_frontend_lvgl` 作为独立、默认关闭的桌面技术切片。
- 以 vendored LVGL 9.6.0 和官方 Windows driver 创建 Win32 窗口。
- 验证多栏工作台、导航、会话列表、消息区、权限卡片、模型服务设置和深浅主题。
- 保持 Frontend 只能通过未来的 Backend Client/DTO/Event 边界取得业务事实。
- 将 LVGL 类型、Windows 句柄和绘制资源限制在 `apps/desktop/lvgl/`。

### 非目标

- 不实现 Backend Client、ViewModel、OpenView、订阅、UI Queue 或真实业务 DTO。
- 不把 LVGL 加入默认构建，不改 NAppGUI target，不创建第二套业务状态机。
- 不宣称中文系统 IME、屏幕阅读器、三平台构建、生产打包或业务联调已经通过。
- 不引入 LVGL Pro 编辑器或其单独许可的工具链。

## 5. 视觉切片（Autorail 对标）

切片严格对标仓库现有 Autorail 模型服务页面，而不是自创聊天工作台。窗口使用
1040x700 起始尺寸，内容结构固定为：

1. 40px 顶栏：AETHER、PROJECT、WORKSPACE、BRANCH、连接状态和外观入口。
2. 192px 设置导航：模型、模型服务、编码搭档、MCP 服务器、后台日志、外观、关于。
3. 208px 服务商栏：搜索、全部/启用/停用筛选、七牛云/硅基流动/Fenno 三个默认预设、
   新建服务商。
4. 剩余详情区：七牛云标题/启用、API 密钥、轮流使用、API 地址与线路、模型搜索/能力/
   同步/添加、未选择模型空态。

尺寸遵循 `docs/design/ui/providers-reference.md`：导航192、服务商栏208、
详情间距16、模型工具栏高度28、模型空态上下内边距24、控件圆角4/6/8。

颜色直接对应 `theme-tokens.json` 的 Autorail 默认角色：深色窗口#09090b、侧栏#101013、
顶栏#18181b、强调#8dff4f；不得在 LVGL 实现中恢复旧的#15171a/三栏聊天草图色板。
中文文案、三个默认服务商、地址、空密钥和“未选择模型”状态与 providers-reference
保持一致。API 密钥只显示空输入/安全提示，不回显或复制秘密。

演示所有按钮均是未接 Backend 的视觉入口，不显示保存成功、检测成功或模型同步成功。
不得把模型服务设置页改造成聊天消息页、权限状态页或自创全局作用域卡片。

## 6. 交互范围

切片只验证窗口创建、Autorail 页面布局、中文字体、可编辑空密钥/搜索输入、窗口缩放
和关闭。正式实现仍需 Backend Client、真实 DTO/Event、UI Queue、IME、焦点、DPI、AT
和关闭链。
收束晚到载荷，之后才释放 LVGL 对象和 Adapter context。
## 7. 依赖与构建

- 版本固定为 LVGL `v9.6.0`，MIT；保留原始 release archive、源码和
  `LICENCE.txt`，不修改上游文件。
- 项目包装位于 `third_party/lvgl_integration/`，原始源码位于 `third_party/lvgl/`。
  只启用所需静态库并隔离第三方警告；不依赖系统安装的 LVGL。
- Windows 使用官方 Windows driver 与 GDI framebuffer，不加入 SDL、OpenGL、WebView。
- `AETHER_BUILD_LVGL_FRONTEND` 默认 `OFF`；打开后才创建 `aether_frontend_lvgl`。
- 目标只在 Windows 创建；非 Windows 明确未实现，不创建假成功 target。

## 8. 验收

### 本切片必须通过

- Windows MSVC 严格编译；关闭 LVGL 选项时现有工程保持原行为。
- 实际创建并关闭 LVGL 窗口，输出明确 `preview` 标识。
- 可观察到 Autorail 对标顶栏、192px 导航、208px 服务商栏、详情顺序、中文字体和空态。
- 窗口调整大小后布局不越界崩溃；关闭后无残留 demo 线程或资源。
- 项目质量检查覆盖新增手写文件；上游源码按第三方原样规则处理。

### 后续正式 Frontend 门槛

- Windows 中文 IME 组合输入、候选框、焦点、剪贴板和快捷键。
- Windows per-monitor DPI；Linux/macOS 构建、窗口和输入行为。
- UIA、VoiceOver、AT-SPI 实际工具验证。
- 真实 Backend Client/DTO/Event、有限 UI Queue、快切、晚到事件和 `close-complete`。

## 9. 文件落位

```text
third_party/lvgl/                 # 固定原始依赖
third_party/lvgl_integration/     # 项目CMake包装
apps/desktop/lvgl/                # LVGL绑定和视觉技术切片
```

NAppGUI 保持在 `apps/desktop/nappgui/`。两个 target 不共享第三方控件对象或业务状态。

