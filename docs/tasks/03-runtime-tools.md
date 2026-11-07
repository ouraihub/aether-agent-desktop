# 03 Runtime接入与工具生态任务

执行规则与索引见 [总看板](README.md)。设计入口：[Runtime/工具详细设计](../design/detail/04-runtime-tools.md)、[平台执行](../design/detail/06-platform-execution.md)、[Core权限](../design/detail/03-core.md)。RT-*为设计用例，RUN-*为本清单任务。

| ID | 任务 | 状态 | 硬依赖 | 交付物与验收条件 |
| --- | --- | --- | --- | --- |
| RUN-01 | Provider宿主与接入公共边界 | 未开始 | APP-01, ENG-06, ENG-07, DEC-12 | 实例/连接代次、能力快照、受管/借用资源、事件交接及关闭；可控Provider验证RT-02/04/05/07/08；不预建无需求协议引擎 |
| TOOL-01 | Tool Registry及执行路由 | 未开始 | APP-05, APP-10, RUN-01 | 描述来源/schema/执行者/权限控制点、路由及在途寿命；本地和observed-only区分，RT-12、INT-07拒绝后无副作用 |
| TOOL-02 | MCP连接与分类发现 | 未开始 | RUN-01, DATA-04, DEC-12 | 实际协商版本的连接/分页发现Tool、Resource、Prompt、代次失效；RT-09/10，重复游标有界，空列表不冒充发现失败 |
| TOOL-03 | MCP工具/资源/Prompt操作 | 未开始 | TOOL-01, TOOL-02 | 参数校验、授权、实际调用/读取/取用、取消与断连；真实可控MCP服务验证INT-10，工具业务错误不当协议成功 |
| TOOL-04 | Skills发现与版本校验 | 未开始 | DATA-04, APP-05, DEC-12 | 来源/元数据/版本指纹、依赖循环、范围与内容校验；恶意/无效/超大内容不执行，未启用不生效 |
| TOOL-05 | Skills配置应用与失效 | 未开始 | TOOL-04, RUN-01, APP-09 | desired/effective revision、生效证据、禁用/变更/新会话要求；RT-11/15，保存成功不等于Runtime已加载 |
| RUN-02 | OpenAI Codex真实适配 | 未开始 | RUN-01, APP-09, APP-10, APP-12 | 核实精确版本/官方接口后实现启动或连接、会话/消息、能力/取消/异常；实际运行RT-01/03/04/06/07及INT-01/02/08，保存兼容性证据，不默认候选协议已支持 |
| RUN-03 | Claude Code真实适配 | 未开始 | RUN-01, APP-09, APP-10, APP-12 | 同完整Provider闭环；所需Bridge单独记录版本/来源/许可/维护责任，真实运行成功/失败/取消和权限控制边界 |
| RUN-04 | Pi真实适配 | 未开始 | RUN-01, APP-09, APP-10, APP-12 | 按实际版本完成Provider闭环、能力与流式结构映射，真实验证多会话/不支持能力/退出，不照搬其他Runtime协议 |
| RUN-05 | OpenCode真实适配 | 未开始 | RUN-01, APP-09, APP-10, APP-12 | 实际协议握手、内容/工具事件、请求关联及生命周期；真实闭环和坏帧/迟到/取消，记录版本限制 |
| RUN-06 | oh-my-pi/OMP真实适配 | 未开始 | RUN-01, APP-09, APP-10, APP-12 | 依据真实支持接口接入；实际会话/消息/能力/权限/取消和清理，不凭产品名称推断ACP或其他协议 |
| RUN-07 | DeepSeek Harness真实适配 | 未开始 | RUN-01, APP-09, APP-10, APP-12 | 核实实际版本、Bridge/SDK或Headless路径后实现完整Provider；真实消息/错误/关闭验证，不编造兼容wire字段 |
| RUN-08 | Native真实模型执行链 | 未开始 | RUN-01, APP-09, APP-10, APP-12, TOOL-01, DEC-12 | 真实模型端点、认证、流式/结构化结果、受控上下文、截止及取消；Native不是固定回复或Simulator，RT-13/14的模型部分实际验证 |
| RUN-09 | Native模型与工具循环 | 未开始 | RUN-08, TOOL-03, TOOL-05 | 模型提出工具调用、授权后真实执行、结果回送模型并结束；额度/坏参数/拒绝/断流均有结果，RT-13/14全场景，不无限重试副作用 |
| RUN-10 | 逐Runtime工具与Skills能力联调 | 未开始 | RUN-02, RUN-03, RUN-04, RUN-05, RUN-06, RUN-07, RUN-09, TOOL-03, TOOL-05 | 对每个目标核实MCP/Skills/工具/模型/恢复真实支持，区分可调用和只读映射；无能力明确标不支持，不能让一项通过代表全体 |
| RUN-11 | 多Runtime隔离与兼容性验收 | 未开始 | RUN-10 | 实际交错运行、多实例同外部ID、重启代次、单请求取消和借用连接关闭；INT-08及RT-04至08，更新逐版本/平台矩阵，未知结果不归到最近请求 |

RUN-02至RUN-08可按DEC-12已确认的顺序或独立并行推进，不要求按表逐个串行。每一目标必须有真实版本资料、实现、测试和运行证据；受环境/凭据阻塞应保留阻塞，不以“不支持”替代尚未调查或未实现。未授权自动下载安装或变更用户Runtime环境。

如果需细拆某个Adapter，可在本文件给稳定子编号并替换其聚合依赖；旧任务必须保留为聚合验收，不能只完成解析器就把整个Adapter勾完。
