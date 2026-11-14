# 04 桌面交互与产品接线任务

执行规则见[总看板](README.md)。设计入口：[Frontend详细设计](../design/detail/01-frontend.md)、[LVGL候选Frontend设计](../design/ui/lvgl-frontend.md)、[Autorail对齐工作台与主题Token](../design/ui/README.md)、[Backend](../design/detail/02-backend.md)、[验收场景](../design/detail/08-implementation-validation.md)。模型服务配置严格参考Autorail实际UI，主题经共享token/适配器映射；静态预览不算UI-01完成，绑定实施仍受DEC-09约束。

| ID | 任务 | 状态 | 硬依赖 | 交付物与验收条件 |
| --- | --- | --- | --- | --- |
| UI-01 | 应用壳与UI Adapter | 进行中 | ENG-01, ENG-04, DEC-09 | NAppGUI应用壳、导航、模型服务页面控件与独立UI Adapter原型已落位；Backend Client/真实DTO交接、输入法/焦点/DPI/AT和关闭链继续作为本任务未完成验收，不以静态图代替完整UI-01 |
| UI-01-L | LVGL候选Frontend技术切片 | 进行中 | ENG-01, ENG-04, DEC-09 | 固定LVGL版本、独立ui_adapter target、Windows真实窗口/布局/主题/缩放/关闭烟测；不替代NAppGUI，不包含Backend Client、真实DTO、IME/AT或三平台正式验收；设计见[LVGL设计](../design/ui/lvgl-frontend.md) |
| UI-02 | Backend Client与视图接收器 | 未开始 | UI-01, APP-06, APP-08 | receiver/generation、快照安装、事件队列、分页关联、异步退订；FE-04/06/15、INT-05/06实窗制造竞态，无悬空VM |
| UI-03 | Workspace与Agent配置界面 | 未开始 | UI-02, APP-02, APP-09 | 真实创建/选择/配置、路径失败/无权/空状态、Agent与Runtime身份区别；FE-01/17，UI不直接业务读写 |
| UI-04 | Runtime管理与能力呈现 | 未开始 | UI-02, APP-09, RUN-01 | 列表/版本/状态/受管借用、启动连接停止、未知/不支持能力；FE-02，按能力而非名称；先可控源测试，真实验收交UI-13 |
| UI-05 | Session列表与历史/恢复 | 未开始 | UI-02, APP-09, DATA-03 | 会话创建/选择/关闭、消息历史分页、恢复能力反馈；FE-03/06，打开历史不宣称Resume成功 |
| UI-06 | 消息编辑器与结构块呈现 | 未开始 | UI-05, APP-04 | 文本/图像/文件/工具块安全呈现、草稿/受理/失败、滚动锚点；FE-05/07/16，未知块不执行、不静默丢用户输入 |
| UI-07 | 任务与取消反馈 | 未开始 | UI-06, APP-07, APP-12 | 请求状态、取消处理中、execution/cleanup/persistence差异；FE-08/18，等待超时不伪报底层停止，无盲重发 |
| UI-08 | 权限请求与失效处理 | 未开始 | UI-02, APP-10 | 目标/作用域/来源、批准拒绝、过期/重同步禁用、迟到确认反馈；FE-09、INT-07，不自动加入永久授权选项 |
| UI-09 | MCP与Tools管理界面 | 未开始 | UI-02, TOOL-03 | 服务连接发现、工具输入/来源/执行者、允许调用及只读映射；FE-10，真实可控MCP操作和错误可见 |
| UI-10 | Skills管理与有效配置 | 未开始 | UI-02, TOOL-05 | 发现/校验/版本/启停、desired/effective/RequiresNewSession；FE-11，无空按钮冒充安装能力 |
| UI-11 | 设置/凭据与配置冲突 | 未开始 | UI-02, DATA-04, APP-09 | 草稿/保存期间编辑、revision冲突、有效来源、秘密保留替换清除；FE-12/13，失败不清新草稿或回显秘密 |
| UI-12 | 日志诊断与级别设置 | 未开始 | UI-02, UI-11, APP-11 | 错误Trace ID复制跳转、有界筛选/分页/缺失提示、有效级别来源及重启恢复；FE-14、OBS-08/09、INT-11实窗验证 |
| UI-13 | 全桌面真实业务联调 | 未开始 | UI-03, UI-04, UI-07, UI-08, UI-09, UI-10, UI-11, UI-12, RUN-11 | 对全部已承诺Runtime与页面运行完整操作矩阵，消息/任务/权限/工具/配置/诊断连通；FE-01至18适用项、INT-01至11，不把替身通过当真实接入 |
| UI-14 | 页面销毁与应用退出体验 | 未开始 | UI-13, APP-12 | 快速切页、在途OpenView、晚到结果、未保存草稿和关闭截止；FE-15/18、INT-12，UI不阻塞，不误杀借用进程 |

页面任务可以用明确标识的可控数据源制造错误/竞态，但必须调用真实Backend合同；每个页面的本地验收不等于对应厂商的实际功能完成。UI-13统一补齐真实联调，任何未实现区域保留未完成状态。
