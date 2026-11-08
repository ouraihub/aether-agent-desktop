# 04 Runtime、协议与工具生态详细设计

## 1. 范围与约束来源

本文细化 [Runtime 概要](../04-runtime-tools.md)，遵循 [共享基线](00-contract-baseline.md)、[接口契约](../../INTERFACE_CONTRACTS.md) §5–13、[架构](../../ARCHITECTURE.md) §6–7 及 [工程规范](../../CODING_STANDARDS.md)。**下述字段、操作和状态均是待评审推荐，不是已接受 ADR、已冻结 ABI 或已实现能力。**

Core拥有Agent/Session/Message/Permission/Tool领域语义，Backend裁决请求终态，Adapter只拥有连接/外部关联/协议状态。Agent配置不等于Runtime连接；权限正式语义见[接口§11.1](../../INTERFACE_CONTRACTS.md#111-已收敛的权限模式与范围dec-07)，配置/存储见[数据设计](05-data-config.md)，进程及IO见[执行平台](06-platform-execution.md)。

不预建插件/协议引擎/P7编排。DEC-12接入顺序及精确首目标见[ADR-0011](../../adr/0011-runtime-protocol-order.md)，DEC-11容量见[ADR-0006](../../adr/0006-bounded-capacity-baseline.md)；首选OMP18.18.0 stdio RPCv2，其余逐RUN核实，不把下述建议字段当已发布ABI。内部状态在Adapter串行域修改，IO只交接带代次的有界事实，不直接改Core/Backend。

## 2. Provider 操作与所有权建议

### 2.1 通用调用规则

Provider 的描述符以 `provider_id` 标识实现类别，实例以 `runtime_id` 标识，二者不能混用。组合根通过已声明接口注入 Process、Transport、Clock、日志及事实接收器，不把 Backend 私有指针传入 Adapter。

业务参数和只读 RequestContext 分开传递；关联到应用请求时传入 Backend 已生成的 `request_id`。Provider 内部 `operation_id` 仅用于启动、握手、关闭等子操作，不能对外冒充应用请求身份。TraceContext 按值传播或受明确寿命保护，不修改父上下文，也不向不支持的外部协议强加追踪字段。

推荐公开入口为 Thread Compatible：同一实例的操作由调用者串行投递到拥有者执行域；IO 完成先转入该域。入口不等待网络/进程，回调不在锁内调用。回调内允许排队取消/退订，禁止同步等待自己收尾。只读查询也经拥有者取快照，不开放可变结构指针。

| 操作意图 | 必需性与前置条件 | 输入 → 输出及寿命 |
| --- | --- | --- |
| create / initialize | 所有 Provider 必需；创建后初始化一次 | Provider 描述、注入依赖、已解析配置 → 实例；复制需长期保存的配置，共享服务借用至 close-complete |
| start / connect | 必需；Initialized 或 Stopped | 连接目标、受控启动参数、上下文 → 拒绝或本地受理 operation_id；异步报告连接/能力事实，受理不是 Ready |
| get_status | 必需；实例未 Destroyed | runtime_id → 独立状态快照，含连接状态、代次、清理状态、最后安全错误；调用方用配套释放函数回收 |
| get_capabilities | 必需；允许尚未 Ready | 适用 Session/配置范围 → 独立能力快照；未完成探测为 Unknown，查询失败单独返回错误 |
| create_session | 入口必需；实际能力可选 | 内部 session_id、Workspace 范围、配置 revision → 受理/拒绝，再报告外部绑定结果；没有管理能力时 NotSupported，不伪造远端会话 |
| close_session | 入口必需；绑定存在 | session_id、关闭原因 → 受理/拒绝及关闭事实；明确是解除本地绑定还是远端关闭，能力缺失不得暗示远端已删除 |
| send_message | 可用对话 Provider 的必需能力；Ready 且会话绑定有效 | session_id、message_id、有界内容块、RequestContext → 受理/拒绝；消息内容及关联在返回前取得异步所有权，后续输出为结构化事实 |
| cancel_request | 可选；能力支持且请求仍在途 | request_id → 取消已送出/尚未送出/不支持/未知；真实停止事实异步报告，返回 OK 不是 Cancelled |
| subscribe_events / unsubscribe | 必需；启动前可订阅 | 回调、存活上下文、过滤范围 → 订阅句柄；回调载荷借用至返回，退订 close-complete 后才释放上下文 |
| stop | 必需；可重复 | 停止原因、清理期限 → 停止已发起及停止事实；拒绝新执行，停止当前连接活动，保留配置以便显式再次 start |
| shutdown（Close）/ destroy | 必需；Close 可重复、Destroy 仅一次 | shutdown 返回关闭已发起，close-complete 才表示再无回调；destroy 释放已关闭/无异步资源的部分初始化实例，不代替 stop |
| 配置 Skills/MCP、模型选择、权限回送、恢复会话 | 可选且逐项声明 | 类型化操作参数 → 类型化实际应用结果；缺少入口或能力一律 NotSupported，不返回空成功 |

同步拒绝不登记在途映射、不随后发送该操作的完成事实，失败输出初始化为空。受理前准备映射、必要输入副本和队列额度；发送失败发生在受理后则异步报告。输入仅同步借用；若 Provider 与上游已经约定显式所有权转移则避免再次深复制，失败时所有权是否转移必须固定。

能力项包含 `support=Supported/Unsupported/Unknown`、版本依据、适用范围、connection_generation、配置 revision 和失效原因。连接更替立即失效；配置改变先失效受影响项，再显式确认。查询失败不改成 Unsupported。Streaming、Cancellation、并发会话数和单会话在途限制独立报告；仅支持单会话时明确拒绝额外创建，不偷换当前绑定。

## 3. Adapter 内部对象与外部关联

| 内部对象 | 推荐字段与唯一职责 |
| --- | --- |
| AdapterInstance | provider_id、runtime_id、state、generation_counter、配置快照、依赖借用、当前连接、close_requested；不保存领域状态副本 |
| ConnectionEpoch | connection_generation、transport_handle、owned_process_handle（可空）、managed/borrowed、协商版本、能力快照、读写额度、握手 deadline、pending_callbacks、cleanup_status |
| ProtocolDecoder | framing 状态、未消费字节、scan_offset、声明长度、已消费量、frame/aggregate 限制、错误位置；只识别选定协议 |
| SessionBinding | 内部 session_id、外部 session key、connection_generation、已应用配置 revision、可用/失效标记；恢复必须重建绑定 |
| InFlightBinding | request_id、operation_id、内部 session/message/tool_call ID、外部请求键、发送阶段、cancel_sent、执行观察、只读 TraceContext、待释放计数 |
| EventSink | subscription_id、active/closing/closed、排队额度、执行中回调计数、close-complete 接收器；没有 UI 指针 |

外部映射键推荐为 `(runtime_id, connection_generation, channel_namespace, external_session_scope, external_request_id)`。`channel_namespace` 区分控制请求、消息与工具调用；只有厂商证明请求 ID 在连接内全局唯一时才省去 session scope。外部 ID 的类型和值都参与比较，禁止把数字 1 和字符串 "1" 强行折叠。无请求 ID 的事件只按协议已证明的 Session/Stream 关联处理，不能归给“最近请求”。

内部 request_id 反向索引定位取消；外部键索引定位响应。外部消息/内容块/工具 ID 若作用域不同，分别以会话、消息或工具流作为父键。插入时检查冲突，冲突拒绝新绑定并报告协议错误，不覆盖原请求。一个请求可关联多个内容块或工具调用，不为每个 Delta 复制完整输入。

发送阶段为 Prepared → Sent → ObservedComplete；取消是独立标记，不创造第二份 Backend 请求状态机。终态后保留最小关联直到所有 IO/回调引用解除；释放正文等大载荷，有限迟到窗口内仅保存必要墓碑。若协议会在同一连接重用外部 ID 且无法区分迟到响应，应禁止复用或结束该连接，不能靠短超时假定安全。

新连接分配新代次，所有 IO 回调捕获原代次。旧代次回调只能更新旧连接清理事实并释放数据，不进入当前映射表。代次耗尽显式失败；旧 Epoch 未 close-complete 前不能释放回调上下文。断连不自动重连、重放消息或恢复外部 Session；明确恢复成功才把持久 Session 绑定到新代次。

## 4. connect、握手、停止与关闭

### 4.1 连接状态表

以下是 Adapter 连接状态，不取代 [Core](03-core.md) 的 Runtime/Session 领域状态。

| 前态 | 输入或事实 | 后态与动作 |
| --- | --- | --- |
| Created | initialize 成功 | Initialized；依赖和配置完整，未创建 IO |
| Initialized/Stopped | start 受理 | Connecting；创建新 Epoch，资源创建失败转 Failed 并回收部分资源 |
| Connecting | Transport 已连接 | Handshaking；仅接收握手及协议允许的控制信息 |
| Handshaking | 版本、身份/认证要求、能力确认通过 | Ready；发布同代次能力和可用事实，才允许消息发送 |
| Connecting/Handshaking | 超时、拒绝或格式损坏 | Failed；报告启动失败并进入清理，不发布可用事实 |
| Ready | 断连/异常退出/致命协议错误 | Failed；能力失效，逐一报告在途受影响事实并关闭 IO |
| Connecting/Handshaking/Ready/Failed | stop | Stopping；关闭受理闸门，停止生产，完成该 Epoch 清理后 Stopped |
| 任一未关闭态 | shutdown | Closing；包含停止与依赖解除；pending_callbacks=0 且所有资源关闭后 Closed |
| Stopping/Closing | 重复 stop/shutdown | 复用现有关闭过程；shutdown 可把最终目的升级为 Closed，不重新发送终止命令 |
| Closed | destroy | Destroyed；只释放内存，不访问外部服务 |

stop 完成需要连接活动确已停止，不能仅因应用请求已超时就报告 Stopped。Failed 的清理未完成时拒绝 start；安全释放后才允许显式 stop 到 Stopped 再 start。关闭失败保留 `cleanup_status=failed` 和仍存活的资源所有者，不发虚假 close-complete，不以 free 消除错误。

### 4.2 建连算法

1. 读取不可变有效配置，检查实际执行权限、目标及能力需求；向 Process Service 提交可执行文件、参数数组、工作目录和环境白名单，既有服务只建立连接。
2. 预留连接/握手额度及接收上下文，设置单调截止，生成代次，再创建 Transport；失败按反向取得顺序释放已成功取得的资源。
3. 按选定版本实际协议执行握手；记录协商版本、认证结果与能力依据。无需握手的协议也必须完成官方定义的就绪确认，不能因为进程已创建就称 Ready。
4. 校验版本兼容性及必需能力；不兼容返回明确错误，不自动降级到 CLI 或另一协议。凭据经授权按需从 ADR-0012 的普通 SQLite credentials 数据取得，不进入描述符/日志/事件/UI DTO；临时密钥不持久化，不自动读取 Pi/OMP 等外部凭据，读取失败不以空值、旧缓存或 ENV 回退。明文存储不授权 UI 显示/复制秘密。
5. 原子提交该 Epoch 的 Ready 与能力快照事实。连接完成晚于取消/关闭时只清理新到资源，不回到 Ready。

### 4.3 cancel 与 close 算法

cancel 先按 request_id 查反向绑定并重查代次/能力。尚未发送且已安全移除待发送项时报告停止事实；已发送则仅发真实协议取消并标记 cancel_sent。重复取消不重复副作用；无取消能力报告 NotSupported，不杀共享 Runtime，也不把关闭整个连接包装成单请求取消。

完成、取消确认、断连与截止的事实都交给 Backend；由 [共享基线](00-contract-baseline.md) 的首次有效终态及截止规则裁决。Adapter 不先宣告应用 Cancelled。成功可能胜过尚未确认的取消；已超时调用仍须跟踪执行/清理，外部晚到成功不能改写应用终态。

关闭顺序：关受理闸门 → 报告各在途调用的已知执行事实 → 停止新业务事件生产 → 按真实协议停止/解绑 → 关闭读写及受管资源 → 等待 IO 回调收尾 → 清空映射/解析缓冲 → 完成订阅关闭 → close-complete。关闭控制信号不依赖已满业务队列；在途失败事实仍须交给拥有者，不能退订后丢掉唯一的完成责任。

受管进程的升级终止由 Process Service 按已批准策略执行，借用服务绝不终止；远端连接关闭不能证明远端执行停止，执行状态报告 unknown。订阅停止后不启动新用户回调，已进入回调允许结束，排队未执行载荷释放。回调上下文及共享服务必须活到 close-complete。

## 5. 有界读帧与 normalize

### 5.1 帧解析抽象

只定义 `feed(bytes, end_of_stream)` 的本地抽象：输出 NeedMore、FrameReady（已消费字节及临时帧视图）或 FatalError。具体分隔符、长度字段、压缩与消息结构必须来自目标官方资料；本文不假设所有协议都是逐行 JSON，也不杜撰厂商 wire 字段。

1. 读入前检查连接代次和读额度，限制单次读块及累计缓存；使用检查溢出的长度运算。长度前缀协议在分配正文前验证长度；分隔符协议在有界扫描范围内找边界，不反复扫描已扫描前缀。
2. 半帧保留未消费字节和解析游标；多帧输入逐帧消费，每个调度回合限定处理量以让取消/关闭可运行。帧完成前不将分块 UTF-8 或 JSON 片段误判为完整对象。
3. 帧完整后执行结构、嵌套深度、字段数、字符串/数组长度及版本校验；FrameReady 的视图仅有效到下一次修改缓冲，normalize 在此前完成或显式取得所有权。
4. 允许的空白/保活按真实规范处理；EOF 有残留不完整帧为协议截断，没有残留也只代表传输结束，不凭 EOF 推断请求成功。
5. 超长、整数溢出、无法恢复的坏帧立即使连接 Failed；不跳过坏字节后猜测下一个响应。失败定位保留偏移/错误码，默认不记录正文、凭据及工具参数。

发送队列同样有界，部分写保留 offset；字节完整写出只表示已交给 Transport，不表示外部执行成功。读缓存、写缓存、在途请求、内容块、工具参数及下游事实队列分别有上限，归 D-LIMIT 批准，不允许任一环节以无界缓存补偿另一环节。

### 5.2 归一化与背压

normalize 顺序为：验证帧类型/版本 → 解析外部键 → 查代次与关联 → 校验协议顺序 → 转换内容块/工具/权限事实 → 向状态拥有者交接有界载荷。统一事实携带 runtime/session/message/block/tool_call 关联及必要上下文，不携带厂商解析树、进程句柄或 UI 指针。

文本 Delta 必须绑定具体块；Tool Call 的参数增量先在有界缓冲完成并校验，不能把半段参数送执行器。重复抑制仅使用协议明确提供的事件 ID/顺序信息；没有这类保证时不按相同文本去重。遇到明确重复结束报告重复事实，不生成第二份完成；缺失必要起始/关联属于协议错误，不自行补成成功对话。

外部帧顺序不冒充 Backend 订阅 sequence；Adapter 可持有本地接收序号用于诊断，领域 revision 和最终订阅序号仍由各自拥有者产生。未知可忽略扩展必须有协议依据；未知必需事件/内容种类报告不支持或协议错误，不能静默转成文本。

下游满载先暂停可暂停的读取，并保留有限待交接事实；关键终态和权限事实不能丢弃。持续无法交接达到声明期限则关闭故障连接，通过独立健康/控制状态让拥有者查询失败并逐项收尾。读取暂停可能阻塞同连接其他 Session，应暴露影响范围；不声称具备未实现的跨会话公平性。

## 6. MCP：连接、发现与调用

MCP Server 连接建议记录 `server_id`、配置 revision、受管/借用、connection_generation、协商协议版本、能力、发现 revision、在途调用和清理状态。Server ID 是配置身份，不是 runtime_id；由 Runtime 自己管理的 MCP 与 Aether MCP Client 连接不能合并为同一执行者。

生命周期为 Registered → Connecting → Negotiating → Discovering → Ready；连接/协商失败进入 Failed，关闭进入 Closing → Closed。发现某类别失败可保留连接，但该类别明确 unavailable/error，不能将空列表当成“发现成功且没有工具”。仅发布已经完整校验的类别快照。

发现算法：读取有效配置与凭据引用 → 按授权连接/启动 → 按协商版本初始化 → 分别发现支持的 Tool/Resource/Prompt → 有界分页校验 → 提交描述 revision。限制页数、条目数、累计字节；重复游标、重复冲突名称、变更导致快照不一致必须失败或明确重新发现，不无限循环。刷新期间旧描述标记 stale，不能拿旧 schema 受理新调用；若版本支持变更通知，则按其能力刷新，不假定任何版本都有推送。

| 类别 | 描述及操作输入输出 | 执行边界 |
| --- | --- | --- |
| Tool | server/name/schema/revision；调用输入为校验后的参数，输出结构化结果或明确工具错误 | 有副作用可能，必须经 Core 权限判断和执行前复核；协议成功携带工具错误时仍是工具失败 |
| Resource | URI/模板、媒体类型及访问说明；读取输入为受允许标识，输出有界内容或读取错误 | 不是 Tool 调用；URI 及内容访问受权限和大小限制，不默认下载引用资源 |
| Prompt | 名称、参数描述；取用输入为参数，输出消息/内容模板 | 不是执行脚本或授权；返回内容仍是不可信上下文，不自动运行其所述命令 |

调用固定 `(server_id, generation, discovery_revision, tool_identity)`，执行前重查描述、参数和权限，再分配协议请求 ID 并登记映射。结果保留 tool_call 关联、内容类型、业务失败与协议失败区别；权限拒绝不发外部请求。断连使描述和新调用路由失效，已发送调用报告执行不确定性，不自动重试。取消按协商版本真实能力执行；不预设 Aether 作为 MCP Server。

## 7. ToolRegistry 的来源、执行者和寿命

Core 维护 Registry 领域规则；Adapter 发布描述与失效事实，不自行扩展权限算法。建议条目含 `tool_id`、source_kind/source_id、外部名称、schema revision、executor_kind、executor_binding、scope、availability、permission_control_point、provenance；显示名不作为身份。

| 来源/执行者 | 注册与调用路线 | 失效及审计声明 |
| --- | --- | --- |
| Aether 本地工具 / 本地执行器 | 组合根注册可用实现；Core 授权后由执行器访问受控服务 | 关闭先禁止新调用，等在途清理再卸载；审计限于真实执行范围 |
| Aether MCP Client / 指定 Server 连接 | 发现完整类别快照后发布，调用绑定 server/generation/schema | 断连或 schema 变化使路由失效；不得把旧描述转发到另一个同名 Server |
| Runtime 原生工具 / Runtime 自身 | Adapter 登记已观察描述；仅在真实 API 支持且有执行权限时提供主动调用 | 默认 observed-only，不能因登记而启用调用；内部调用的审计完整性如实声明 |

Registry 生命周期建议为 Available → Stale/Unavailable → Removed。调用取得描述 revision 和执行绑定的受控使用权；移除禁止新调用，不释放在途执行上下文。名称冲突用来源与作用域区分，不“最后注册覆盖”。工具取消、断连和迟到授权均按 [Core](03-core.md) 的授权/操作身份复核；运行时原生权限交互只有真实支持暂停/回送时才可桥接，无法强制限制的工作流明确不满足条件。

## 8. Skills：内容版本、启用意图与实际生效

加载器拥有发现、读取及格式校验；Core 拥有 scope、启用/禁用与依赖规则，Adapter 只做目标 Runtime 配置映射。Skill 建议记录 `skill_id`、source_id、来源版本、内容指纹、元数据、依赖、校验结果和内容定位；版本标签相同但指纹变化仍视为内容变化，不悄悄替换会话已使用版本。

配置分为 `desired_revision`（用户已保存意图）与 `effective_revision`（目标已证实生效），另有 `apply_status=Pending/Applied/Failed/RequiresNewSession/Unknown`、适用 runtime/session/generation、错误及证据。Applied 不由“保存配置成功”推导。磁盘持久化、启用意图和 Runtime 生效是不同结果，保存边界引用 [数据与配置](05-data-config.md)。

1. 从已批准来源有界发现并读取；校验格式、依赖闭包和循环、来源/作用域及内容大小。正文是不可信上下文，不授予文件、命令或网络权限。
2. 同一来源/标识/version 若内容冲突则拒绝含糊版本；不同来源同名不覆盖，用来源身份消歧。文件变更生成待评估的新版本，不修改已加载快照。
3. 用户启用固定版本/指纹及 scope；Adapter 报告不支持、支持动态应用或仅新会话生效。后者标记 RequiresNewSession，当前会话保持旧快照。
4. 动态应用发送目标差异并等待真实确认；成功推进 effective_revision。失败仅在协议证明旧配置仍有效时保留旧版本，否则 Unknown，禁止声称已回滚。
5. 禁用改变 desired，不删除正被会话引用的内容；当前会话是否卸载由真实能力决定。断连、进程重启或外部配置变化使生效证据失效，重新确认后才恢复 Applied。

Runtime 自有 Skills 只暴露其真实查询/配置范围，不承诺 Aether 文件版本就等同其内部加载版本。内容保存及权限规则不在 Adapter 重写。

## 9. Native Runtime 的真实执行边界

Native 同样实现 Provider，不是为架构占位的固定回复或模拟器。最低真实闭环是：合法模型配置 → 实际模型请求 → 结构化/流式响应 → 真实工具调用（若报告支持）→ 工具结果回送模型 → 明确结束或失败。只接通 UI 与 Mock 不满足此定义。

推荐内部对象 `NativeTurn` 包含 session/message/request 关联、固定会话配置/工具 schema revision、有界上下文、模型请求关联、待处理 tool_call 集合、步骤计数、累计用量、deadline、取消标记；不包含可变全局“当前会话”。状态为 Preparing → AwaitingModel → CollectingResponse → WaitingPermission/ExecutingTool → AwaitingModel，最终报告 Completed/Failed/Stopped 的执行事实，由 Backend 裁决请求终态。

1. 根据 Session 的明确上下文策略构建输入，检查模型内容类型与上下文限制；无法容纳时明确失败或执行已批准压缩策略，不能悄悄截断系统约束，也不提前建设长期 Memory。
2. 通过实际模型 Adapter、凭据引用和 HTTP/Transport 发出请求；校验状态、流结束及模型结束原因，不凭 socket EOF 声称回答完成。
3. 聚合结构化内容与完整工具参数；普通最终回答结束本轮。工具调用只能解析到本轮已发布 schema 的 Registry 身份，未知工具返回明确失败，不执行模型生成的任意命令。
4. 工具权限由 Core/Backend 流程决定；推荐首个实现按模型声明次序串行执行工具，逐项关联真实结果再回送模型。拒绝/工具错误以该模型协议允许的工具结果表达，无法表达则终止失败，不伪装工具成功。
5. 每轮前检查单调截止、取消及已批准的模型轮数/工具次数/总输出额度；达到边界报告明确原因。不得无限自动纠错或重试有副作用工具。
6. 取消停止发起下一模型/工具步骤，并尝试真实在途取消；部分结果保留，工具可能已产生副作用。资源关闭后才报告真实停止；模型断连、工具执行和保存失败分别报告，不混为同一“生成失败”。

实施前必须取得真实模型端点/版本、认证方式、请求与内容 schema、流式 framing、工具参数/结果格式、结束及错误语义、取消能力、限额/用量、平台/TLS要求和合规条款。缺少这些资料时仍可评审内部循环，但不能填写虚构字段、选一个“兼容”协议当已验证，或拿模拟器代替真实 Native 验收。Native 接入顺序由实施决策确定，P7 仍为演进目标。

## 10. 建议文件职责与厂商接入记录

以下为实施时按实际需要落位的职责，不要求立即创建这些文件；Provider 公共契约在 Core，具体实现不反向泄漏到领域层。手写代码遵循 500 行硬上限、超过 300 行审查职责、80 列目标/100 列常规上限，不按文件名机械拆片。

| 建议落位 | 单一职责 |
| --- | --- |
| `core/runtime/provider.h` | 评审后的 Provider 抽象、能力及所有权契约，不含厂商头文件 |
| `adapters/runtime/<target>/lifecycle.c` | 实例/连接状态、建连、握手和关闭协调 |
| `adapters/runtime/<target>/mapping.c` | 外部关联键、代次隔离和映射寿命 |
| `adapters/runtime/<target>/normalize.c` | 厂商语义到统一内容/工具/错误事实 |
| `adapters/runtime/<target>/codec.c` | 实际协议编码和有界半帧解析；证明复用后再移入 `adapters/protocol/` |
| `adapters/protocol/mcp/` 中的连接/发现/调用文件 | 已批准版本的 MCP Client、描述快照与请求映射，权限策略仍在 Core |
| `infrastructure/skills/` 中的读取/校验文件 | 内容来源加载及格式边界，不处理 Runtime 状态 |
| `adapters/runtime/native/` 中的 turn/model 文件 | 真实执行循环与具体模型映射分离，不加入 P7 空接口 |

每个真实厂商接入必须填写：产品/精确版本/平台；官方资料 URL 与核实日期；实际启动/连接和所有权；Bridge 版本/来源/许可/维护责任；协议版本/帧结构/最大包；外部 ID 作用域；握手就绪与能力证据；会话并发/恢复；流结束/重复/取消；Tool/MCP/Skills/权限可控范围；错误/退出/关闭；配置实际生效判据；实测场景及未覆盖限制。不得把架构候选协议表复制成已支持清单。

## 11. Given / When / Then 验收与决策

下列是实施验收要求，不表示本次已执行。纯解析和竞态可用明确标识的 Simulator；支持声明还须记录真实版本/平台证据。

| 编号 | Given / When / Then |
| --- | --- |
| RT-01 | Given 精确版本及合法配置；When 真实启动、握手并发送消息；Then Ready 仅在确认后出现，收到真实关联回答，且凭据未进入业务事件/日志 |
| RT-02 | Given 握手拒绝、超时或资源分配失败；When start 失败；Then 没有可用能力假象，所有已取得资源被关闭，部分初始化可安全销毁 |
| RT-03 | Given 帧在任意字节处分割或多个帧同读；When feed；Then 输出与完整输入一致；超长、溢出、截断和错误类型进入明确失败且缓存受限 |
| RT-04 | Given 两 Runtime/多 Session 使用相同外部 ID；When 交错输出并取消一个请求；Then 只命中完整复合键，不干扰其他调用 |
| RT-05 | Given 重连产生新代次；When 旧 IO/权限/取消回调晚到；Then 新请求不受影响，旧载荷与上下文在安全点释放 |
| RT-06 | Given 成功、截止与取消确认竞争；When 事实交回 Backend；Then 按共享基线唯一裁决，取消受理不等于停止，终态后仅更新执行/清理事实 |
| RT-07 | Given 借用服务及同进程多 Session；When 不支持的单请求取消或关闭本地连接；Then 不误杀服务、不伪称远端已停止，不支持能力可观察 |
| RT-08 | Given 慢消费者/满队列及回调内退订；When stop/shutdown；Then 无关键事实静默丢失、无同步自等，close-complete 后无回调且内存有界 |
| RT-09 | Given 真实 MCP Server 的 Tool/Resource/Prompt；When 分页发现、读取/取用及工具调用；Then 类别与权限路径分离，工具业务错误不被当作成功，断连使描述失效 |
| RT-10 | Given MCP schema 改变、重复游标或同名来源；When 刷新/调用；Then 不无限发现、不覆盖身份、不用旧 schema 向新代次发送请求 |
| RT-11 | Given Skill desired 新版本且应用失败/仅新会话支持；When 保存/启用/禁用；Then desired 与 effective 分别可见，现有会话不被虚假标记已更新 |
| RT-12 | Given Runtime 原生工具仅可观察；When Registry 展示；Then 不出现虚假的 Aether 主动调用、授权或完整审计承诺 |
| RT-13 | Given Native 的真实模型和获授权工具；When 模型产生工具调用再接收工具结果；Then 发生真实执行与后续模型回答，调用关联完整，无固定回复代替 |
| RT-14 | Given Native 权限拒绝、坏工具参数、模型断流或执行额度耗尽；When 进入相应步骤；Then 不执行未经授权副作用、不无限循环，真实错误及部分结果可查询 |
| RT-15 | Given Runtime 只支持单会话或不支持 Skills/恢复；When 请求超出能力；Then 明确拒绝，不重绑已有会话、不把空结果包装为成功 |

评审归属：D-PROTOCOL 收敛真实协议/模型资料、Provider 必需面和配置生效证据；D-ID 收敛外部键及迟到代次；D-DOMAIN 收敛会话并发/恢复；D-EXEC 收敛执行域、清理及 close-complete；D-REQUEST 收敛取消事实；D-PERMISSION 收敛原生执行可控边界；D-LIMIT 给解析/映射/发现/Native 循环有限值；D-DATA 收敛 Skill 版本及配置保存。所有编号均引用 [共享决策索引](00-contract-baseline.md)，不另建已接受决策。
