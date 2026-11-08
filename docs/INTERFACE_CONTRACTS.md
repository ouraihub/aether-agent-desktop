# Aether 接口契约规范

## 1. 状态、来源与适用范围

**当前状态：文档设计阶段。** 仓库尚无接口实现、公开头文件或可执行契约测试。本文规定后续接口设计必须满足的约束，保留原规范的建议方向；不宣称已有可调用 API，也不冻结 C ABI、枚举布局、内存布局或线协议。

**迁移来源：** `设计2.0.md` 原 §3.2–3.5（通信、DTO、事件）、§5.2–5.3（组件及生命周期）、§6.1/6.3/6.4（Runtime 接口、能力与事件）、§7（业务模型）、§9.2–9.3（状态码与错误对象）、§10.4（字符串）、§11.3–11.4（线程与异步）、§16（接口设计及版本）。权限交互与验证要求同时承接入口规则及原 §12、§17 的相关约束。

适用于 Backend API、Core 公开接口、Runtime Provider Interface、事件 Schema、DTO 及组件接口。分层、依赖方向、Runtime 接入和权限执行边界见 [ARCHITECTURE.md](ARCHITECTURE.md)；C17 编码、错误传播与清理、资源管理及测试工程细则见 [CODING_STANDARDS.md](CODING_STANDARDS.md)。本文自含契约要求，不要求回读原章节。

- “必须／禁止”是硬约束；“应／建议／优先”保留默认方向，偏离须说明原因。
- 名称、字段、状态与操作列表用于保留业务语义；标为建议的模型不是强制实现全部功能的清单。
- 下文“发布前待落实”表示正式接口发布前必须完成定义和验证，不代表已接受某一种具体方案。
- libuv、NAppGUI、yyjson、SQLite 等仍为候选技术，不构成接口依赖、存储格式或协议承诺。

## 2. 接口优先与统一描述要求

开发顺序为：需求分析、接口设计、数据结构设计、接口评审、实现、单元测试、集成测试。任何模块开发前必须先定义或复用接口，不得先写实现再迫使其他模块大范围适配。

每个公开接口必须明确以下内容，不能仅提供函数签名：

| 类别 | 必须定义的内容 |
| --- | --- |
| 标识与版本 | API 名称、所属契约、版本、适用能力或前置状态 |
| 输入与输出 | 请求／响应结构、输入／输出参数、必填与可空规则、字段有效范围、失败时输出有效性 |
| 返回与错误 | 返回状态、业务错误码、错误语义及必要上下文 |
| 数据与资源 | 创建、借用、持有、转移、复制、释放责任以及有效期 |
| 生命周期 | 对象前置状态、调用后的状态、重复调用与销毁限制 |
| 执行方式 | 是否阻塞、是否异步、超时策略、取消支持及其语义 |
| 并发 | 线程安全属性、调用与回调线程、共享数据同步及并发访问限制 |
| 可追踪性 | 顶层操作或子操作身份、TraceContext 传入/返回位置、异步传播和终结责任；见 §13 |

接口不得依赖隐式全局状态。不得把公开 DTO、Core 内部对象、第三方协议对象或 UI 对象混为同一契约。

## 3. Backend API：Command、Query、Event 与 DTO

### 3.1 交互分类及边界

Frontend 的唯一业务入口是 Backend API Contract。Backend 不依赖 UI 框架，不持有控件、窗口或 ViewModel 指针，必须能够无 UI 独立运行和测试。初期采用进程内通信；逻辑边界不授权提前建设 IPC、网络或插件框架。

| 类型 | 作用 | 原规范中的语义示例，非已发布函数 |
| --- | --- | --- |
| Command | 修改系统状态 | StartAgent、StopAgent、SendMessage |
| Query | 查询数据 | GetSessions、GetRuntimeList |
| Event | 通知状态变化 | MessageReceived、RuntimeStatusChanged |

命令经 Backend 应用服务进入 Core，再由抽象接口调用 Adapter；Runtime 原始事件由 Adapter 转为统一事件，经 Backend 分发给 Frontend。此调用关系不改变源码依赖方向，具体边界见架构规范。

### 3.2 DTO 与响应语义

DTO 与 Core 领域对象必须分离，由应用映射边界转换。禁止直接向 Frontend 暴露 Core 内部结构体，使 Core 内部调整、内部对象生命周期和 API 版本相互解耦。

原 SendMessage 示例以 `session_id`、`content` 作为请求字段，以 `request_id`、`accepted` 作为响应字段。这只是说明请求、受理反馈与关联标识的形态，不是冻结的结构体；`content` 也不取代第 6 节的结构化消息模型。

正式接口必须明确受理响应与业务完成的区别，以及拒绝、受理后失败与结果通知的对应关系；不得让调用方从一个示意 `accepted` 字段猜测业务是否完成。Query 必须定义返回数据的有效期及其与异步更新的关系，不得把内部可变对象直接作为查询结果交给 UI。

请求字符串默认由调用方持有，Backend 不得在请求返回后继续引用调用方临时内存。异步使用必须复制或明确转移所有权；具体约束见第 8 节。

### 3.3 请求上下文与边界响应

业务输入 DTO 只表达业务参数。TraceContext、请求标识以及已验证调用者身份等请求元数据，通过独立 RequestContext 参数或外层请求信封传递，不逐个混入业务 DTO；上下文只携带当前边界确有必要的信息，不引入没有需求的 Actor 类型或通用框架。追踪 ID 不授予权限，调用者身份必须由可信边界验证。

Backend/传输边界负责输入解析（适用时）、字段/长度及请求总大小校验、敏感字段隔离和上下文创建/验证；不得仅因输入结构体具有正确 C 类型就认为外部数据可信。大小上限及超限错误须在具体 API 发布前确定。

Core/应用服务返回领域结果和统一状态/错误，不返回 UI 控件、HTTP 响应或 MCP 外层封装。Backend/对应 Adapter 将内部结果转换成各入口的响应；对用户可见的失败至少保留稳定错误码、安全提示和对应 Trace ID（生成失败时明确不可用）。内部错误诊断与用户文案分开，产品边界按错误码映射提示，不直接透传底层 SQL、路径、命令或异常原文。

成功输出和失败输出有效性必须明确，异步受理/完成区分仍遵守 §3.2。C 实现使用已有状态码、输出参数和明确错误对象契约，不为模仿 Rust `Result<T, E>` 而制造通用 `void*` 结果包装或让日志库拥有业务响应类型。

## 4. 统一事件契约

### 4.1 必须保留的元数据

Backend 必须提供统一事件分发机制。事件至少包含以下字段；原示意中的 C 类型仅说明设计意图，不冻结 ABI 或序列化表示。

| 字段 | 原示意类型 | 必须保留的语义 |
| --- | --- | --- |
| `schema_version` | `uint32_t` | 事件 Schema 版本 |
| `event_id` | `const char*` | 事件唯一标识 |
| `event_type` | `const char*` | 事件类型 |
| `session_id` | `const char*` | 关联 Session |
| `runtime_id` | `const char*` | 关联 Runtime |
| `sequence` | `uint64_t` | 顺序标识 |
| `timestamp_ms` | `uint64_t` | 毫秒时间戳 |

事件必须具有唯一标识、顺序标识、版本信息、会话与 Runtime 关联、明确的数据所有权及可取消的订阅生命周期。不得把临时缓冲区指针保存到异步事件中。

发布前必须定义：标识唯一性的范围；没有会话或 Runtime 关联时字段是否可空及表达方式；序号的作用域、分配及重置规则；时间戳的时钟来源和基准；载荷与元数据的版本关系；字段编码、长度和有效期。不得从字段名推导全局总序、持久化、重放或跨进程时钟保证。

由某次操作产生的事件还必须通过事件信封或明确关联字段携带该操作的追踪上下文，不能从日志或“当前会话”反推。追踪标识不替代上述事件字段，具体关系与无请求背景事件的处理见 §13。

### 4.2 建议事件种类与归一化

原规范建议的事件集合保留如下，正式 Schema 发布时须明确各类型、触发条件及载荷，并处理名称相近类型的关系，而非直接宣布以下全部可用：

- 消息：MessageStarted、MessageDelta、MessageCompleted、MessageReceived。
- 工具：ToolCallStarted、ToolCallCompleted。
- Runtime／Agent：RuntimeStarted、RuntimeStopped、RuntimeStatusChanged、AgentStarted、AgentStopped、RuntimeExited。
- Session：SessionCreated、SessionUpdated。
- 权限与错误：PermissionRequested、ErrorOccurred、RuntimeError。

Runtime 原始事件必须转换为 Aether 统一事件。原始事件可以保留用于诊断，但 Frontend 不得依赖各 Runtime 的原始事件结构。日志与业务事件分离，不得通过解析日志驱动业务。

### 4.3 订阅、顺序与流量待落实

| 发布前必须明确的项目 | 验收条件 |
| --- | --- |
| 订阅建立、句柄所有权、退订及重复退订行为 | 能演示从订阅到释放的完整流程，并覆盖建立失败和重复调用 |
| 退订返回时，正在执行和已经排队的回调如何处置 | 退订与回调并发、订阅方销毁期间均不访问已释放对象；测试结果符合已声明的返回后回调规则 |
| 回调线程、串行／并发保证、重入限制 | 从回调内调用 API 或退订的允许性有明文规定，并有对应测试 |
| `sequence` 顺序保证的范围、重复或缺口的处理、事件与 Query 结果的关系 | 并发消息、跨会话和状态查询场景不依赖未承诺的全局顺序；出现重复、缺口时行为符合已声明的处理 |
| 消息 Delta 与完成、错误、退出事件的关系 | 对完整、异常中断及迟到事件序列有确定处理结果，不依赖 Runtime 原始事件布局 |
| 慢消费者、队列容量边界、满载与背压行为 | 对已选的阻塞、拒绝、合并或其他策略说明适用事件和可观察结果；压力场景不静默违背交付保证，不无限增长而不声明资源边界 |

### 4.4 视图握手与回调交接（DEC-05）

当前语义版本1；正式DTO/二进制编码由APP-06发布，不支持的主版本拒绝。事件ID在Backend实例代次内唯一；没有Session/Runtime关联时使用显式缺省关联，不造ID。timestamp_ms为事实创建时的Unix毫秒，只供展示，不用于竞争排序。事件信封独立携带scope、对象revision及适用TraceContext，不把上下文写入业务DTO。

OpenView在同一状态操作中登记订阅、取得不可变快照及base_revision，返回subscription_id和sequence基线0；过滤后每条交付依次分配1、2……，序号只属于该订阅。重新OpenView取得新身份和新基线，不能复用旧订阅。快照/队列/映射失败整体撤销，不留半订阅；回调可能早于响应，Client须先登记接收上下文，有界缓存并在安装快照后消费增量。对象revision只在状态变化时递增，不替代sequence。

同订阅用户回调在非UI交付域串行执行，不持状态锁，不同步重入Core或状态处理；不同订阅不保证相同线程或全序。事件只借用至回调返回，Client异步持有须复制或正式转移。重复sequence不重复应用；gap、队列满或快照失效进入ResyncRequired，停止交付增量，释放无用积压，以独立健康状态可查询；终态和权限等待留在权威对象中。重新打开不重放副作用。容量数值归DEC-11，有限内存下不承诺无损持久投递。

Unsubscribe在线程安全关闭门线性化后不启动新用户回调；排队未进入载荷丢弃并释放，已经进入的回调可结束。回调内可提交非阻塞Command或发起退订，不得等待自己或同步处理状态。完成信号close-complete一次通知，表示无后续事件回调；Client在通知返回且本地UI Queue引用解除后回收context。重复退订关联原过程，未知/已释放身份返回NotFound；退订不取消业务请求。Backend销毁须等待所有订阅、回调和资源关闭，不因等待截止释放活跃引用。

## 5. Runtime 能力接口与可选功能

Aether 必须定义统一 Runtime 接口，由独立 Adapter 实现。接入方式与权限边界见架构规范，不得把第三方原始协议公开为 Frontend 契约。

### 5.1 建议接口面

| 职责 | 建议操作 |
| --- | --- |
| 生命周期 | initialize、start、stop、shutdown、destroy |
| 描述与状态 | get_capabilities、get_status |
| 会话 | create_session、close_session |
| 消息与请求 | send_message、cancel_request |
| 事件 | subscribe_events |

原描述符示意包含 `runtime_id`、`display_name`、`version`。原函数表示意只展示了上述操作的部分，不构成完整公开头文件。正式设计必须明确描述符字段语义、生命周期、字符串分配与释放、异步回调机制、请求 ID 作用域、错误与取消语义、并发限制。`shutdown` 与组件的 Stop／Close／Destroy 之间如何对应，也必须明确定义，不能凭名称推断。

### 5.2 能力发现

不能假设所有 Runtime 支持相同功能。能力必须由 Runtime Adapter 发现和报告，UI 按能力显示和启用功能，不允许按 Runtime 名称硬编码判断。

统一能力模型建议覆盖：Streaming、Session Management、Multi Turn、Tool Calling、MCP、Skills、Permission Request、File Operations、Image Input、Model Selection、Thinking Configuration、Cancellation、Resume Session。

以上为建议能力维度，不意味着每个 Adapter 必须支持全部功能。正式接口必须定义能力报告的有效期、查询失败或未知能力的表达、可选操作的调用前提和不支持时的可观察结果；不得将不支持的能力伪装为成功。需要动态能力更新时必须先定义更新和失效语义，不默认能力永久不变，也不预设动态更新实现。

可选功能是否具备能力，与是否获得权限是不同问题；能力存在不能替代权限检查。Runtime 实际支持必须按版本和官方资料验证，模拟能力不等于真实接入成功。

## 6. Session、Workspace 与结构化消息

### 6.1 Session

Session 是用户与 Agent Runtime 持续交互的业务对象，必须具有：

- Session ID、Runtime ID、Workspace ID。
- 创建时间、更新时间、当前状态。
- 模型信息、会话配置、上下文关联信息。

原正常生命周期为 Created → Initializing → Active → Idle → Closing → Closed；异常状态包括 Failed、Interrupted、RuntimeUnavailable。这是状态模型来源，不是完整状态转换表。具体转换必须通过明确状态机管理。

发布前必须列出允许的转换、触发条件、失败去向、重复命令行为、事件通知及关闭期间操作限制；Active／Idle 是否及如何往返、异常状态如何退出均不得从示意路径猜测。恢复会话受 Runtime 能力约束，状态模型本身不承诺恢复或持久化协议。

### 6.2 Workspace

Workspace 是 Agent 执行任务的工作空间，包含工作目录、项目配置、权限策略、Runtime 配置、环境变量策略、工具配置和 Skills 配置。

Workspace 不等于 Session，一个 Workspace 可以包含多个 Session。正式 DTO 必须保持 Session 到 Workspace 的关联，明确配置与权限读取的作用范围及变更影响，不能通过复制字段绕过 Workspace 权限。

### 6.3 Message

统一消息模型包含 Message ID、Session ID、Role、Content、Timestamp、Status、Metadata。消息角色包括 `system`、`user`、`assistant`、`tool`。

内容应支持多种内容块：Text、Image、File、Tool Call、Tool Result、Structured Data。不得将所有消息简单处理成一个字符串；不同 Runtime 的输入支持仍由能力发现约束。

发布前必须定义内容块类型及载荷、块的顺序、长度和所有权、消息状态及时间语义、流式增量如何关联消息／内容块、Tool Call 与 Tool Result 的关联方式，以及未知或不支持内容的明确处理。文件和图像的表示与访问范围必须受权限边界约束；本文不预设内联字节、路径或外部引用的编码方案。

## 7. 组件定义与生命周期

组件是具备独立生命周期、接口和依赖关系的可组合单元。每个组件必须明确 Component ID、Component Name、Component Version、Component Interface、Initialization、Start、Stop、Destroy、Dependencies、Configuration、Thread Safety、Error Handling，并遵守以下统一阶段：

| 阶段 | 职责 |
| --- | --- |
| Create | 创建对象 |
| Initialize | 初始化依赖和配置 |
| Start | 启动运行资源 |
| Running | 执行运行活动 |
| Stop | 停止运行活动 |
| Close | 关闭资源并解除外部连接 |
| Destroy | 释放对象内存 |

每个组件必须明确哪些阶段可以重复调用、哪些操作必须幂等及非法调用顺序如何返回。Start 失败必须执行必要清理；Destroy 必须安全处理部分初始化对象，不得假设初始化完整成功。

发布前必须定义停止、关闭、销毁对在途请求、排队事件、订阅、外部连接和回调的影响；明确何时调用方可以释放回调上下文及依赖对象。不得用释放对象替代停止运行活动，也不得让销毁期间回调访问已释放对象。

## 8. 数据所有权、字符串与输出资源

所有跨模块数据和资源必须明确创建者、所有者、借用者、转移时点、释放者及释放方式；覆盖 DTO、请求／响应、事件、错误对象、订阅句柄和 Runtime 返回资源。

字符串及缓冲区必须说明：编码、长度及其单位、是否以 NUL（`\0`）终止、空指针／空值是否允许、容量与有效数据的关系、是否拥有内存、是否允许修改及有效期。这里的 NUL 是原规范“NULL 结尾”所指的字符串终止符，不等于空指针。

跨模块优先使用明确长度的字符串视图或 DTO，但视图本身不延长底层存储寿命。不得默认字符串可以长期保存，不得保存调用方临时指针用于异步处理。异步持有必须复制或明确转移所有权，并说明失败时所有权是否已经转移。

正式接口必须指明成功及失败后的输出有效性、输出字符串的分配与释放方式、数据复制／保留方式、回调参数在返回后是否有效。资源配对、分配失败回滚、部分初始化清理和安全字符串操作的实现细则见编码规范。

## 9. 状态码与错误对象

### 9.1 统一状态类别

保留原 `AetherStatus` 的统一类别及名称，`AETHER_STATUS_OK` 表示成功且原示意取值为 `0`；其余类别的数值、枚举布局和传输编码不在本文冻结。

| 状态名 | 类别 |
| --- | --- |
| `AETHER_STATUS_OK` | 成功 |
| `AETHER_STATUS_INVALID_ARGUMENT` | 参数无效 |
| `AETHER_STATUS_OUT_OF_MEMORY` | 内存分配失败 |
| `AETHER_STATUS_NOT_FOUND` | 对象不存在 |
| `AETHER_STATUS_ALREADY_EXISTS` | 对象已存在 |
| `AETHER_STATUS_NOT_INITIALIZED` | 尚未初始化 |
| `AETHER_STATUS_NOT_SUPPORTED` | 不支持的功能或操作 |
| `AETHER_STATUS_TIMEOUT` | 超时 |
| `AETHER_STATUS_CANCELLED` | 已取消 |
| `AETHER_STATUS_IO_ERROR` | IO 错误 |
| `AETHER_STATUS_PROTOCOL_ERROR` | 协议错误 |
| `AETHER_STATUS_RUNTIME_ERROR` | Runtime 错误 |
| `AETHER_STATUS_PERMISSION_DENIED` | 权限拒绝 |
| `AETHER_STATUS_INTERNAL_ERROR` | 内部错误 |

状态码表示错误类别，业务错误需要进一步细分错误码以具体定位。每个接口必须说明可能返回的类别、具体错误码和上下文，以及同步返回错误与异步失败的对应关系。不得只用一段错误文本替代明确的错误语义。

### 9.2 建议错误对象及强制生命周期要求

建议统一错误对象包含 `status`、`code`、`message`、`module`、`function`；原示意中 `code` 为 `int32_t`，后三项为字符串指针，均不是冻结的 ABI。

正式实现必须解决字符串生命周期，禁止把局部变量中的字符串指针保存到长期错误对象中。建议明确选择一种方案：对象拥有字符串内存、固定长度缓冲区、引用计数的不可变对象，或调用方提供的错误缓冲区。不能将这些候选同时当作调用方可猜测的规则。

必须提供配套的错误创建、复制、释放接口，并定义跨异步边界和分配失败时的行为。错误须显式传播并保留必要上下文和底层原因；返回值检查、统一清理出口、日志去重及禁止吞错等实现细则见 [CODING_STANDARDS.md](CODING_STANDARDS.md)。

## 10. 线程、异步请求与取消

### 10.1 线程属性

每个公共接口必须声明 Thread Safe、Thread Compatible 或 Not Thread Safe，并具体说明保护范围、调用方同步责任和线程限制，不能只列标签。

共享数据必须明确同步策略。当前领域状态采用单一逻辑拥有者和串行事实提交；IO/工作线程通过有界消息交接，不直接修改领域对象。具体物理线程数、锁或平台机制仍由消费者和Platform任务声明。

UI只能在UI线程更新，后台事件经UI Queue返回；不得在UI线程等待Runtime、执行长时间IO或解析大型数据。契约必须声明回调所在执行上下文和阻塞限制。

### 10.2 所有异步操作必须覆盖的情形

成功、失败、超时、取消、重复请求、资源释放、回调线程、对象销毁期间回调均须定义。异步回调不得引用已释放对象。

取消必须明确：请求受理、底层停止、资源关闭和close-complete分别报告；请求取消不等于底层工作已停止，也不默认终止共享Runtime。关闭先停止新工作并执行有限优雅收尾，达到声明截止后才可按策略强制终止。

| 发布前必须明确的项目 | 验收条件 |
| --- | --- |
| `request_id` 的生成方、有效期、唯一性作用域、复用规则及与 Session／Runtime 的关联 | 同一 Runtime 的多会话、多个 Runtime、关闭或重启后的请求均能无歧义定位；不存在因 ID 冲突导致取消错误请求的情况 |
| Command 受理与完成、终态种类、每类通知次数、完成结果与错误的交付方式 | 调用方能按声明区分拒绝、在途与终结；受理后失败不会被误报为业务成功 |
| 完成、失败、超时、取消竞争以及迟到回调／事件的裁决 | 并发竞争测试产生符合已声明状态机的结果，调用方不会收到相互矛盾且无法判定的最终状态 |
| 超时起算点、截止范围、返回后底层任务是否继续及资源责任 | 超时前后边界和晚到结果有可重复验证的行为，不把等待超时自动等同于进程终止 |
| 重复提交、重复取消、未知／已结束请求的取消 | 对每种情形明确返回与副作用；不暗设幂等、自动重试或去重机制 |
| Stop／Close／Destroy 与在途请求、订阅及回调竞争 | 部分初始化、运行中关闭及取消清理路径均不泄漏资源、不访问释放后对象，且完成责任符合契约 |

### 10.3 Runtime Session领域边界

Runtime描述与Runtime实例分离；实例由`runtime_id`和`connection_generation`标识。Session默认只接受一个进行中的turn，只有Adapter能力和Session合同同时允许时才接受并发turn。普通turn结束回Idle；连接/进程失效进入RuntimeUnavailable或Closed的明确分支。历史记录不等于外部Runtime恢复，只有真实session locator经Adapter验证后才能报告恢复成功。未知ContentBlock不得静默转成Text。

| 发布前必须明确的项目 | 验收条件 |
| --- | --- |
| 队列容量、满载和关键事实处理 | 满载不阻塞UI、不无限增长；完成/权限事实不静默丢失，视图失效时可重新同步 |
| 关闭截止与遗留资源责任 | 优雅收尾、强制终止、回调关闭和资源释放分别可观察；close-complete前不释放回调上下文 |
| Session并发和恢复能力 | 无能力或Idle以外的非法turn明确拒绝；恢复只在真实Runtime locator和能力确认后报告 |
| 未知内容块 | Unsupported或ResyncRequired可观察，不转成Text或伪造成功 |

这些条目要求作出可验证的定义，不预先选择终态裁决算法、线程数、重试策略或持久化协议。

### 10.4 请求终态与等待交接（DEC-05）

本节约束未来APP-06/07/08异步业务合同；§14同步本地记录在明确迁移前仍按既有V1运行。受理入口验证版本/尺寸/能力，复制输入并取得队列和最小终态额度，原子登记/入队后返回request_id；任何失败撤销登记和副本，输出无ID且无完成事件。ID在Backend实例代次内唯一，Trace/Session/第三方ID都不能代替它。受理后可变前置在状态域重查；不承诺受理响应先于回调。每次独立提交是新请求，不隐式重试/幂等重放。

| 前态 | 有效事实 | 后态/行为 |
| --- | --- | --- |
| Accepted | 开始执行 | Running |
| Accepted/Running | 需要用户授权 | WaitingPermission |
| WaitingPermission | 有效批准 | Running，副作用前再复核 |
| WaitingPermission | 拒绝/失效 | Failed；若业务截止已到则TimedOut，不执行 |
| 任一非终态 | 有效业务成功/失败 | Succeeded/Failed，保存要求由用例合同决定 |
| 未开始执行的非终态 | 取消 | Cancelled，撤销等待/排队工作 |
| 已执行的非终态 | 取消意图 | cancel_requested=true；不支持则NotSupported，不杀共享Runtime |
| 任一非终态 | 真实取消完成 | Cancelled |
| 任一非终态 | 有限业务deadline到达 | TimedOut；底层可能仍运行 |
| 任一终态 | 重复/迟到事实 | 不改终态，仅更新执行/清理/保存事实并释放载荷 |

Backend为终态唯一拥有者；所有有效事实按其串行处理顺序进入。处理终态事实前先读单调时钟，now>=业务deadline则TimedOut，否则首个有效终态获胜，不比较来自不同线程/Runtime的墙钟完成时间。业务deadline从成功受理时起算，排队及等待权限计时；数值按具体Command/DEC-11发布。完成提交后只产生一次逻辑RequestCompleted，消息结束、Runtime yield、会话settled、保存完成和资源清理分别表达，不互相代替。

客户端自己的等待超时只撤销等待，不修改业务deadline/请求终态、不自动发送取消。取消为独立控制Command，有自己的request_id/trace和目标request_id；控制请求完成只说明意图已处理，已终态目标返回原结果且不再发底层取消，重复意图至多发一次，未知目标NotFound。传输失败保留未确认事实；成功可以胜过尚未得到确认的取消。超时/取消后保留底层关联直到回调可靠断开或实际关闭。

终态可查询保留期由不可变limits明确，回收须同时满足保留截止和全部回调/映射解除。到记录上限拒绝新受理，不驱逐活跃记录；不承诺跨崩溃完成通知重放或自动恢复在途副作用。错误映射使用现有AetherStatus和业务码，不按文案猜测；早到事件由request_id关联，迟到旧实例/generation不路由新请求。

| 给定/按拥有者处理的顺序 | 可判定结果 |
| --- | --- |
| 未到业务截止：成功→取消 | Succeeded，取消返回原结果，不再发底层取消 |
| 未到截止：已执行请求取消意图→成功→取消确认 | Succeeded；取消意图不是Cancelled，最后只补清理 |
| 未到截止：真实取消确认→成功 | Cancelled，成功不覆盖 |
| 业务截止已到→收到成功（底层称更早完成） | TimedOut，保留底层执行成功，不回写请求成功 |
| 客户端等待超时→Backend在业务截止前成功 | 客户端等待超时；QueryRequest仍为Succeeded，无隐式重发 |
| RequestCompleted早于受理响应 | Client按关联暂存，取得同一request_id后安装终态，不再回退Accepted |
| OpenView建立→更新入队→快照响应→增量交付 | 快照与基线后sequence连续应用，不用无关联Query+Subscribe |
| 事件队列满→请求终态提交 | 请求终态仍可查，订阅ResyncRequired，不伪称视图完整 |
| 回调已进入→退订→回调返回→close-complete | context一直存活；退订线性化后不开始新事件回调 |

## 11. 权限交互契约的发布前要求

权限执行职责、最小权限、Process Service、路径和环境变量约束见 [ARCHITECTURE.md](ARCHITECTURE.md)。接口必须支持按 Workspace、Session、Runtime、Tool 控制权限，策略至少包括 Allow、Deny、Ask、Inherit；权限请求必须具有明确用户交互流程。能力发现不得绕过权限策略。

PermissionRequested 是建议统一事件类型；如正式接口提供该事件及响应操作，必须完成以下定义，而不是仅显示确认对话框：

| 待明确项目 | 验收条件 |
| --- | --- |
| 请求与受保护操作、Workspace／Session／Runtime／Tool 的关联，以及展示信息与授权目标的一致性 | 用户确认指向可识别的具体请求和操作；不同会话、工具或 Runtime 的响应不能误用于另一请求 |
| 授权作用域、有效期、使用次数或其他适用边界；Allow／Deny／Ask／Inherit 的继承与冲突规则 | 按已定义边界测试允许与拒绝，作用域之外的操作不能借用已有授权 |
| 请求取消、会话关闭、Runtime 退出、策略变更等情况下请求和授权如何失效 | 迟到、重复、过期响应的结果明确，不会以无效授权执行操作 |
| 无响应、超时、拒绝及执行前权限改变的行为 | 各路径有确定状态和资源清理责任，未满足权限条件不得伪装成功或绕过检查 |

### 11.1 已收敛的权限模式与范围（DEC-07）

依据用户明确要求采用Pi/OMP既有做法，正常开发不反复确认；本节定义语义，不代表APP-05/APP-10已实现。采用OMP的`yolo`、`write`、`always-ask`、`always-ask-cached`四种模式。可信Workspace有效配置默认`yolo`：没有显式限制的普通工具调用直接执行，不产生PermissionRequested。缺失可信上下文不能借用默认配置，未知模式拒绝；不可用能力仍NotSupported。`yolo`不是OS授权、沙箱或绕过Workspace/执行服务校验的开关。

模式只解决未被显式规则决定的调用：`always-ask`允许read，write/exec询问；`write`允许read/write，exec询问；`yolo`允许三个等级；`always-ask-cached`有目标时全部目标命中有效缓存才允许，无目标read允许，无目标write/exec询问。等级缺失按exec处理。工具硬拒绝与有效显式Deny优先，显式Ask及提供者不可缓存安全检查不被模式/普通记忆覆盖；其余明确Allow可直接执行。这是Aether对OMP工具规则的统一映射，不声称两个分支逐项优先级完全相同。

Workspace/Session/Runtime/Tool是匹配维度，不串成虚构的继承链。每条规则声明父范围；Inherit只继续查该父范围，必须无环，缺失父规则不产生批准。同一适用集合先Deny、再显式Ask、再Allow，最后才按有效模式求值。配置覆盖不移除仍适用的禁止规则；更窄Allow不能覆盖宽Deny。模式取Session显式覆盖、Workspace配置、产品默认的首个值；Runtime/Tool仅提供各自匹配规则与能力，不偷偷修改整个Session模式。

仅模式产生的询问提供可记忆选项：单次；工具/子工具+等级的会话批准；有规范化目标时改为目标或同目录的会话/持久批准。目录仅自身直接条目，不自动覆盖子树；多个目标须全部被覆盖。能力更高的目标批准可满足较低等级，反向不成立；会话工具批准精确匹配等级。授权同时限定所属Workspace、主体及执行者身份，不能把Aether批准跨给另一个Runtime/Provider。持久批准保存目标/等级/范围，不保存一次性nonce；只来自明确“记住”选择，提供清除。会话结束清除会话批准，不因每次新request重复询问。

单次答复绑定等待项、request、工具、实际参数、Runtime代次及policy_revision；只能消费一次，不能误投或重放。执行前使用实际参数重新求值；新禁止规则优先，单次批准在版本/目标/代次变化时失效；会话/持久批准对新目标重新匹配而非全部强制失效。撤销立即禁止后续消费，已发生副作用不承诺撤回。批准不自动重试失败执行，不自动扩大到会话或目录。

只有确需Ask才等待：使用单调有效期并不超过业务deadline（默认见DEC-11）；无UI/通道关闭、拒绝、取消、到期或目标关闭均不执行。迟到/重复答复返回明确失效/冲突，不恢复原执行。权限等待是独立可查询状态，订阅丢失不会变成批准。UI关闭是否取消业务遵循具体关闭合同，但不能生成Allow。

Runtime原生工具沿其实际模式和协议执行，不人为逐工具再叠一层Aether确认。Adapter只桥接真实可暂停并回送的权限请求，展示原生决定实际覆盖范围；不支持的授权范围不得伪装支持。未提供OS隔离不自动禁用常规Runtime；用户明确要求强制限制的工作流若无法满足，返回NotSupported/权限拒绝并准确说明，而非声称已受控。路径强制须针对最终资源/链接与竞态核实，环境白名单和借用进程归属仍遵循架构§6。

## 12. 版本变更与契约发布检查

### 12.1 版本与迁移

公共 API 必须有版本管理，Backend API、Runtime Interface 和 Event Schema 都需要明确版本边界。原来的 “V1” 名称仅为示例，不表示这些契约已经发布。

不兼容修改必须升级主版本或提供明确迁移策略，并同步全部受影响调用方、Adapter、DTO 映射、测试和文档。兼容性评审不能只看函数签名，还必须覆盖错误、所有权、生命周期、线程、超时、取消和事件语义。不得默认新增字段、状态或事件一定兼容；未知值与版本不匹配的处理必须在发布前明确。

涉及重大架构、安全或公共契约取舍的决策须经项目负责人确认并按入口规则记录 ADR；建议方案或待落实条目不得写成已接受决策。

### 12.2 有限发布检查清单

以下检查针对即将发布的具体契约及其实现，不宣称本轮文档迁移已经执行这些验证：

1. **接口描述齐全：** 第 2 节的名称、版本、输入输出、错误、所有权、生命周期、阻塞、异步、超时、取消和线程属性逐项具备可审查定义；不再依赖示意结构猜测行为。
2. **模型与边界完整：** Session、Workspace、消息内容块及关联字段符合本文；DTO 不泄漏 Core、第三方协议或 UI 对象；事件七个元数据字段、载荷及空值规则有明确 Schema。
3. **资源生命周期可验证：** 正常路径、参数边界、分配失败、部分初始化、启动失败、关闭销毁、输出和错误对象释放均有检查；临时字符串不跨异步边界失效。
4. **请求与事件竞争有结果：** 第 4.3、10.2 节中的 ID 范围、顺序、终态、超时取消、重复调用、退订回调和背压条目均已定义，并有相应契约测试验证，而非仅列术语。
5. **能力与权限不混淆：** 支持、不支持和能力获取失败路径，以及第 11 节的授权关联、作用域、失效和迟到响应均有验收；UI 不按 Runtime 名称推断能力。
6. **跨模块与 Adapter 已验证：** 按影响覆盖 Backend—Core、Core—Adapter、事件分发—Frontend、Session—Workspace 及工具权限链；单元测试不强依赖真实 Runtime，使用明确标识的 Mock／Simulator，正式 Adapter 另行报告真实接入验证。
7. **版本迁移完整且报告真实：** 受影响调用方、测试和文档同步；不兼容变更有主版本或迁移说明；记录实际执行的检查、平台与结果，未执行或受阻检查明确列出，不将模拟验证当作真实 Runtime 或跨平台验证。
8. **链路关联可验证：** 涉及请求、任务、事件或诊断接口时，按 §13 验证跨队列传播、并发隔离、异步终态、外部关联及诊断 ID；真实日志验收按工程规范 §4.8 执行。

## 13. 可追踪日志与诊断契约

本节规定已采纳的追踪语义与DEC-10编码，不冻结C结构体布局、函数签名或远端协议。日志记录等级、输出与性能要求见[工程规范](CODING_STANDARDS.md) §4，模块边界见[架构](ARCHITECTURE.md) §3.3；设计依据和数值解释见[ADR-0010](adr/0010-local-diagnostics.md)。

### 13.1 TraceContext 与标识关系

- 每次独立用户操作、命令、后台任务或独立消息处理建立 root trace；同一次操作的请求、子任务和事件处理保持同一 `trace_id`。不同顶层操作不得共享标识，Session/Workspace/Runtime/请求 ID 都不能代替 Trace ID。
- 每个需要追踪的边界操作建立 span，使用 `span_id` 和 `parent_span_id` 表达父子关系；根 span 的父标识为空。并行子调用各有独立 span，不通过线程或执行先后猜测父级。
- TraceContext 是显式传递的只读上下文值，携带 trace 与当前 span 身份；创建子上下文不能修改父上下文。禁止全局可变“当前 Trace ID”，禁止依赖线程局部变量推断异步父级。Logger 句柄可注入共享，但不是当前请求上下文。
- trace_id为16字节非零系统随机值，span_id为8字节非零系统随机值，按值复制；对外严格32/16位小写hex，根parent_span_id为null。Platform使用Windows BCryptGenRandom、Linux getrandom或macOS SecRandomCopyBytes受检结果，不用Foundation进程计数ID/时间/PID替代；概率唯一不是授权token或数学全局唯一。全零至多重取一次，仍失败明确diagnostic unavailable并计健康故障，不借用其他请求身份。child生成失败保留父关联但不伪造子span；不改变业务结果。当前不接纳第三方父级。
- 同步调用可在声明的有效期内借用上下文；线程任务、队列项、回调上下文、计时器和进程读取任务必须按值复制，或采用明确且有界的持有方式，不能保存调用方栈地址。结束 span 不应使已复制 ID 失效，也不应为了追踪强制保活整个业务对象。
- 同一后台工作派生出的消息处理保留因果上下文；独立外来消息若无法关联已知请求，则建立新 trace 或标为无请求的系统记录，不能套用最近一次请求。独立后续用户操作建立新 trace；需要关联前序操作时使用明确的关联信息，不复用原 root。

### 13.2 日志记录字段

字段使用稳定snake_case，UTF-8无BOM JSON Lines，每条加LF，schema_version=1；统一yyjson 0.13.0严格编码/解析（仅已选型，未引入）。timestamp为UTC RFC3339固定毫秒，level为大写六级词；所有不透明64bit业务ID用字符串，数值计数为无符号整数、duration_ms为非负有限毫秒数。最多32顶层字段；module/component/layer/operation/function各≤128 UTF-8字节、message≤1024、其他可选文本≤512（均不含NUL），整条含LF≤16KiB。可选文本仅UTF-8边界截断并标记truncated；非法必填/UTF-8/重复键/非有限数/整条超限拒绝并计数。调用方不能覆盖保留键；本表是字段语义唯一维护处。

| 字段 | 适用性与含义 |
| --- | --- |
| `schema_version`、`process_instance_id`、`truncated` | 版本1；实例身份为16字节非零系统随机值输出32位hex，用于文件隔离；可选文本发生截断时为true |
| `timestamp`、`level`、`module` | 每条记录的时间、等级、所属模块；时间编码与时区必须明确 |
| `component`、`layer`、`operation`、`event` | 边界记录必填；组件、真实架构层、稳定操作名及生命周期事件，不假造 Repository 等不存在层 |
| `trace_id`、`span_id`、`parent_span_id` | 操作边界必填，根父标识为空；无请求系统记录明确不适用，不伪造身份 |
| `result` | 终结记录必填：`ok`、`error`、`cancelled` 或 `timeout`，不得把“已受理”记为成功完成 |
| `duration_ms` | 终结记录必填，使用本地单调时钟计算；入口不填，不用墙钟差值计算耗时 |
| `error_code` | 失败终结必填，来自稳定公共错误类别/代码；其他结果按其契约填充，不按错误文案分类 |
| `message`、`function`、`thread_id` | 按需的有界脱敏说明、实际函数定位和线程诊断信息；线程 ID 不能决定请求归属 |
| `request_id`、`session_id`、`runtime_id`、`workspace_id` | 有相关业务上下文时携带，作用域遵循对应领域契约；不会替代追踪身份 |
| `target_kind`、`status_code`、`exit_code` | 外部调用按实际类型记录目标类别及协议状态/进程退出码，不用未经处理的完整 URL 或命令 |
| `reason`、`timeout_ms`、`request_kind`、`model` | 适用时的有限、脱敏上下文，不默认记录正文或任意高基数输入 |

一次操作的 `operation` 必须稳定并能定位到实际边界函数；名称变化不能让既有诊断筛选悄悄失效。跨进程时间戳不保证全局顺序，本地耗时不能被用于推断未观测到的远端内部时序。

### 13.3 Span 终结、异步和错误

- 生命周期事件统一为 `entry`，以及终结事件 `exit`、`error`、`cancel`、`timeout`；分别对应 `ok`、`error`、`cancelled`、`timeout`。操作所有者对每个 span 负责一次终结，失败或取消后不得再记成功出口。
- C17 不依赖析构、异常展开或编译器 cleanup 扩展完成终结。同步路径使用明确的统一清理出口或等价控制流覆盖早退；异步路径将终结责任交给任务所有者，在实际完成时结束，不在提交函数返回时把整个任务标为完成。
- 提交动作与后台任务可分别有 span；受理后失败由后台操作报告。完成、超时、取消竞争必须与业务状态机的终态裁决一致，重复/迟到回调不能二次终结。
- 请求等待超时或取消反馈不等于进程已停或资源已回收。继续运行的底层工作保留自己的 span；后续清理用明确的相关 span/事件报告，清理失败独立记录且不能覆盖原始失败。
- 父操作与清理/子任务的结束先后由具体异步契约定义，发布前须说明哪些子操作在父操作结束后仍可运行；不能为补齐日志而阻塞 UI 或延长无关资源寿命。
- 各层可以记录自己的 span 失败终结，但同一根因的完整诊断由明确处理边界记录一次。上层传播仅记录自身结果、稳定错误码及关联 ID，不层层重复堆栈/原始正文。C 不保证跨平台自动堆栈，应提供实际可用的函数/操作定位与脱敏上下文，不能伪造堆栈。
- 业务错误状态和资源清理不依赖日志写入成功。日志关闭、过滤、队列满或进程崩溃可能使观测链路不完整，不能据此改变业务结果或补写虚假的成功终态。
- 在正常任务销毁/关闭路径，操作所有者须检查尚未终结的操作，发现遗漏时记录 `event=unfinished`，附已知 ID、已观察耗时及原因；它是未完整终结的诊断，不是第五种业务终态，不伪填 `result=ok/error/cancelled/timeout`。重复关闭不得反复记录，发现所有者遗漏属 ERROR，已知外部中断按故障性质定级。进程崩溃或未执行清理时无法保证生成该记录，缺失出口也不能单独证明操作仍在运行。

### 13.4 外部传播与诊断查询

- HTTP/RPC/Runtime 协议明确支持追踪元数据且符合安全策略时，Adapter 才通过该受支持通道传播。不得向任意第三方协议加未知字段、改 prompt 或把 ID 混入业务输出；新传输格式须版本化定义。
- 不支持传播的外部系统，由 Aether 本地 span 记录调用边界，使用带 Runtime 实例/连接作用域的请求关联映射处理响应。共享进程输出不能简单归到最近一个请求；无法确认归属时明确未知，不声称拥有外部内部 span。
- 外部传入的标识是诊断输入，不是授权。须校验格式、长度和信任边界；当前不接纳外部父级，远端身份仅按受支持字段另存关联，不能覆盖本地trace或导致日志注入。未来跨进程父级接纳须另定义协议。
- Backend 诊断接口向前端提供可复制的 Trace ID；错误响应/完成事件包含对应诊断 ID 或明确不可用状态。不得泄漏业务对象指针、日志路径或底层敏感错误。
- Backend支持trace精确及operation/layer/component/level过滤，默认当前实例，可显式查同用户历史实例；每页重授权。受信本机桌面Client可有应用诊断权限，Workspace限域Client只看该workspace_id记录，无关联/其他Workspace不可见，Runtime/MCP/插件无诊断能力；不接任意路径/SQL/正则。trace/游标不是凭证。
- Query默认100条、最多500且响应≤1MiB；每调用扫描≤8MiB或100ms先到、最多2并发，无无限等待队列。最多16个不透明游标，各≤1KiB、5分钟失效，绑定主体/范围/过滤及文件代次/快照长度；权限变化、过期、文件删除明确失效。预算耗尽可继续扫描，不能把空页当不存在；活动尾行pending，损坏/未知schema跳过并标incomplete。返回采集条件、保留及健康/扫描完成信息，不推测特定trace完整性。查询不逐次写日志，不递归记录故障。

### 13.5 发布前检查

必须明确 ID 生成/编码/失效、传播载体与所有权、公共记录字段类型/容量、单调时钟来源、同步/异步终结责任、外部映射清理、级别配置/查询 API，以及日志输出失败和丢弃的可观察行为。按工程规范 §4.8 运行实际成功、失败、并发和清理场景后，才可声称追踪链路已实现；本节不是已存在的 C 日志库。

## 14. 当前进程内请求记录 API V1

实现入口为 `include/aether/backend/request.h`，仅用于同步请求记录管理，不是已接通业务的Command/Runtime执行接口。Backend实例仍借用Core，调用方必须串行化同一实例上的全部操作；无内部线程、回调、IO等待、deadline或隐式重试。它不替代 §10、§13 中待实现的异步和追踪合同。

| 操作 | 输入/输出与状态 |
| --- | --- |
| submit_request | 运行中的Backend → 新进程内ID和Accepted记录；仅登记，不执行工作。有效输出参数在失败时清为无效ID。未start/已stop为NotInitialized，8条记录占满为OutOfMemory（本地额度不足，不表示再次malloc失败） |
| query_request | owning Backend与ID → 按值快照，不借用内部数据；失败时快照清零，request_id=0表示无结果，不能把零state误认为已受理 |
| finish_request | Accepted记录与终态/结果 → 一次提交；Succeeded仅配OK，Cancelled仅配Cancelled，Failed不能配OK/Cancelled。无效组合拒绝且保留原状态；合法重复提交返回AlreadyExists且不覆盖第一次结果 |
| release_request | 仅释放终态记录，允许槽位再用；Accepted返回InvalidArgument。未知/已释放ID返回NotFound，不驱逐仍需查询的记录 |

8个槽位在Backend创建时一次取得，submit/finish/query/release无额外堆分配。记录保留到显式release或destroy，跨Backend查询返回NotFound；ID不能跨进程重启使用，槽位复用不复用请求ID；进程内耗尽行为如下，跨进程/持久身份仍须独立合同，不能据本API宣称持久全局唯一。

Foundation身份生成器现按无锁CAS饱和递增：1至UINT64_MAX各最多发一次，耗尽后永久返回无效0、不回绕重用；所有请求/订阅/对象共用此进程内身份来源，无跨重启持久唯一或授权语义。调用方必须继续检查aether_id_is_valid并回滚尚未发布的记录。

Backend stop成功后将尚未终结的本地Accepted记录转Cancelled，已终态不变；仅因当前记录未关联外部工作，才可同步取消，不承诺未来Runtime停止语义。stop/close后仍可query和release至destroy；destroy释放所有记录并使句柄失效，不能继续访问。调用者若需要保留结果，应先按值查询。

当前尚未实现业务输入副本、执行器、Running/WaitingPermission/TimedOut、完成事件、TraceContext、TTL及自动回收，完整APP-07验收仍未满足。将来接入真实工作时必须先更新受理/停止/资源寿命合同及全部调用方，不能把本地finish成功当作外部业务成功。

## 15. Platform基础值接口 V1

以`include/aether/platform/platform.h`为签名和错误/所有权合同入口。单调时钟以本机任意起点的纳秒表示，仅用于耗时/deadline；墙钟统一为Unix纪元UTC毫秒，允许回拨；两字段不是原子同时采样，任一失败全部清零。

路径构造只复制非空、无嵌入NUL的有效UTF-8，不校验存在性、不授予权限、不执行规范化。对象一次分配，view借用不可变字节至destroy，destroy(NULL)安全，非NULL对象只能销毁一次。当前目录、TEMP和USER_DATA查询可能触发OS操作，不能用于UI耗时回调；进程cwd和环境修改须由调用方串行化。系统目录返回OS根位置而非创建Aether数据目录；Windows宽字符转UTF-8，POSIX非UTF-8路径显式拒绝。非法输入/分配失败/平台不存在/拒绝/其他IO错误分别返回统一状态，失败输出为空。

## 16. Core Runtime Registry V1（APP-01）

公开入口为`include/aether/core/runtime_registry.h`。Registry是纯内存领域对象，不是进程/协议宿主；同实例调用由调用方串行化，无线程、回调、等待IO、deadline或业务取消。capacity显式非零传入且校验数组乘法溢出，创建失败输出NULL并释放所有部分初始化资源；容量满拒绝，不自动驱逐。ProviderId/RuntimeId为不同C类型、进程内身份，不能当持久ID。

Provider复制非空UTF-8名称/版本（内容分别小于128/64字节，不含NUL），同Registry名称精确区分大小写且唯一；查询复制独立快照；版本字段是Provider报告的字符串，不是协议支持证据。Runtime必须引用本Registry的Provider，初始generation/revision=1、Registered/能力未知；Ready只来自当前generation显式事实。能力以known/supported掩码分开表示，supported不得超出known，未知位拒绝；目前仅stream/session/cancel/resume四个基础能力，不暗示其他能力不受支持，扩展须更新合同。

重连校验expected_generation，递增generation/revision并清除旧能力/可用状态；同实例的旧事实和旧RuntimeRef查询被拒绝，但Ref仍可安全release。运行时query输出复制快照，重复同一事实不推进revision。begin_unregister幂等进入Closing并阻止新acquire；保留不可用tombstone，finish_unregister只在Closing且所有Ref释放后移除。Ref持有者通过ref_query看Closing事实并停止新业务；Provider仍有Runtime时不得移除；Registry有任意Ref时destroy返回AlreadyExists且不清空对象。

ref_release接收**，成功释放后清NULL，NULL或已清空指针重复释放安全；禁止复制拥有型裸指针后双释放。移除不会关闭外部连接/杀进程；整个Registry销毁在无Ref时只释放内存登记。register/update/query/remove均无堆分配，acquire单独分配Ref；失败输出清零/NULL。Registered/Unavailable/Closing不允许新借用（NotInitialized），旧代次InvalidArgument，未知ID NotFound，名称重复/引用占用AlreadyExists，分配或容量耗尽OutOfMemory；revision/generation溢出InternalError且不改状态。

本接口支撑CORE-RT-01/02与共享Runtime关系引用；没有实现Session、Message、Provider执行函数表、外部协议、事件分发、持久化或UI，后续任务不得将登记为Ready等同真实Runtime握手成功。

## 17. 数据、配置与容量决策交接（DEC-08/11/12）

持久化选用SQLite3.54.1（Public Domain），Infrastructure窄C API适配，Core/Frontend不接触SQL/sqlite3句柄。统一受限JSON使用yyjson0.13.0（MIT），依赖尚未引入。正式存储/配置语义见[ADR-0008](adr/0008-data-and-configuration.md)：单写短事务、expected_revision CAS、独立快照，外部执行与保存分开；高版本拒绝写入、损坏保全不重建，一致备份使用SQLite在线backup或关闭并checkpoint后快照，不能分别复制活跃sidecar。保存失败不提升saved_revision；外部成功而保存失败不伪报回滚或自动重放。

普通可继承配置按Session→Workspace→应用持久偏好→内置默认，只有Schema明确允许的CLI/ENV才临时覆盖；权限按§11.1，不因普通配置合并覆盖禁止规则。返回saved/effective/source/apply_state分开，需重连的变更保持pending直至真实确认。秘密普通表只存secret_ref，安全存储不可用以稳定业务错误和真实状态类别拒绝依赖操作，不明文/掩码/空值回退；附件只清理受管资源，不删用户原文件。

新模块容量初始值及双限、满载、保留、关闭策略按[ADR-0006](adr/0006-bounded-capacity-baseline.md)实施，构造可降低容量供确定性测试；旧§14的8槽请求API不因此自动变成APP-07的业务执行器。总预算不是全进程RSS保证，正式工程验收仍需实际边界/故障/资源场景。

首Runtime及厂商资料目录见[ADR-0011](adr/0011-runtime-protocol-order.md)：先OMP18.18.0 stdio RPCv2，不混用Pi/OMP终态或帧协议；其他目标由各RUN核实精确版本。协议原始结构只在Adapter，不泄漏Frontend；MCP/Skills和Native保持独立真实执行验收。

## 18. 有界执行器与计时器 V1（ENG-05）

签名与完整错误/所有权说明在`include/aether/infrastructure/executor.h`；平台同步边界在`include/aether/platform/thread.h`。这是基础执行能力，不是Backend RequestRecord终态机，不执行Runtime协议或权限策略。

- 构造配置只可降低ADR-0006上限：排队32条/2MiB、单输入64KiB、默认1/最多4个worker、128个timer。每个worker原生栈1MiB；任务总槽为排队数+worker数，终结结果保留到take，不驱逐。固定输入存储和结果额度在create预留，submit无额外分配，不在调用线程执行工作。
- API内部monitor串行保护状态；worker回调在锁外执行，只有复制的输入、显式trace和受控借用context。输入属于调用者，受理复制；context至结果领取或close-complete前有效，调用者不能提前释放。回调不得操作Core/UI，只返回事实，状态拥有者主动take提交。回调可非阻塞提交/取消/关闭，不能同步等待自身结果或destroy。
- generation非零、trace有效；取消必须同时匹配ID/generation。排队取消不运行回调，保留Cancelled结果；运行取消只置受保护意图，由回调观测并报告实际结果，不把意图当底层停止，不强杀线程。未知ID NotFound，旧代次InvalidArgument，终结后取消AlreadyExists。
- request_close幂等停止接收，取消排队/通知运行工作，丢弃pending timers；控制面独立于满队列。is_closed只在worker循环实际退出后为真；此前destroy返回AlreadyExists且资源不变，之后由非UI线程join并释放。协作任务不返回时资源继续保管，无截止强制释放或假close-complete。
- timer无独立线程/用户回调；状态拥有者传Platform单调纳秒now，take_timer返回最早到期事实一次，包含timer/target/generation/trace。取消只移除尚未领取的项，不能回收已交付事实；接收者必须再次核对目标仍存在且代次匹配，EXEC-06不能依赖cancel侥幸消除旧事实。
- 队列条数/字节或保留槽用尽OutOfMemory、关闭中投递NotInitialized，无隐藏等待/重试；输出失败清零。create失败关闭并join已启动worker、回滚资源。公共头不暴露原生线程或锁，具体Win32/pthread实现只在Platform。

## 19. 结构化消息归并 V2（APP-04）

`include/aether/core/message.h`声明V2，替换未完成的Text-only开发接口；旧append_text/replace_text/finish及单字符串快照删除，调用方全部迁至`aether_message_apply`。无兼容别名或持久存储迁移（原接口没有持久化）。Core不解析Runtime wire、不发事件，不把消息结束映射为Backend请求成功。

- 同一状态拥有者串行调用，Registry借用Session/Runtime registry至销毁，无IO/回调/deadline。create固定Session及Runtime generation，仅Idle/Active且当前Runtime Ready可创建/编辑；精确重放不修改内容。中断/失败END可在关系不可用/关闭时收尾并保留部分内容，Completed必须仍有效。独立查询快照不借用内部可变数据；块快照一次分配，由专用destroy释放。
- 六种kind：Text为显式UTF-8；Image/File为非零不透明resource_id和MIME label，不存任意OS路径或内联附件；ToolCall为工具名label、归一化参数、call ID及调用代次；ToolResult为相同call身份/代次及归一化结果；StructuredData有格式label和UTF-8载荷。归一化载荷不是隐式JSON/可执行文本，资源ID不授予访问权限。label最多127字节，所有视图无内嵌NUL且无需终止符，未知kind NotSupported。
- 块由非零ID和ordinal定位，ID在保留消息中唯一。create cursor=1/expected_revision=0；append/replace/finalize使用连续cursor和当前块revision，append只限Text，metadata不可暗改。finalize携带完整当前载荷，仅终结块；END使用零block/ordinal/cursor/revision及空载荷，只有明确END且所有块finalized才Completed，不从EOF或当前块全结束猜测消息结束。
- 只保存最近事实并按完整身份/操作/载荷逐字节比较；同cursor精确重复不增revision，不同合法事实ProtocolError，旧cursor InvalidArgument、gap ProtocolError。新内容不能覆盖终态或finalized块；新块/新消息关联错误明确拒绝。消息终态同END幂等、不同终态AlreadyExists。
- ToolResult必须引用同Session/runtime generation内已finalized的ToolCall及调用代次，缺失/未完成/错范围InvalidArgument；重复调用ID/第二结果AlreadyExists。终态消息可显式release；仍被其他保留结果引用的调用消息返回AlreadyExists，先release结果再release调用，不留下查询悬空关系。
- 配额遵循ADR-0006：最多16消息、256块、4MiB/消息、32MiB全局及256KiB工具参数，可降低配置。计费包括元数据、内容、label和last-fact复制；替换临时新块与旧块同时计占用。所有验证/分配先于提交，OOM或额度不足保留旧内容/revision；未知ID NotFound、非法索引/状态/关联/版本InvalidArgument，额度OutOfMemory；失败输出清空。单块快照由调用者独立预算，禁止UI线程搬运大型数据。

## 20. 权限领域与批准执行门 V2（APP-05-A/B）

`include/aether/core/permission.h` V2替换Workspace-only开发接口，现有调用均迁移，不保留remember布尔参数等过宽别名。规则求值、等待记录及执行消费均由同一状态拥有者串行调用；Core只借用Workspace/Session/Runtime注册表，不访问OS、Store或UI，不阻拦未经Aether执行门的Runtime内部动作。

- 实际输入绑定Workspace/Session/Runtime/Provider、主体、执行者、工具/子工具、当前Runtime generation、请求、实际参数及0至8个目标；所有有效身份由可信入口提供，Trace不授权。存在且Open/Idle或Active/Ready的关系匹配后才采用可信默认yolo，缺少上下文拒绝。参数最多2048字节，工具名各127字节、目标各511字节且为执行边界规范化绝对slash身份；Core仅验证表示形态，不把字符串校验称OS隔离。
- 规则各可选维度作为匹配过滤器而非虚构继承链；Inherit只沿显式存在父规则且父匹配时继续，写入拒绝环/缺父；Deny>Ask>Allow，安全逐次Ask不被缓存覆盖，再按Session/Workspace/默认模式求值。evaluate只返回决策不分配等待，begin仅对Ask建立记录，单调过期为5分钟与业务deadline较早者，满载拒绝不批准。
- resolve冻结实际参数与请求/代次/policy revision比对，批准只生成资格；consume在实际执行前再次求值并原子消费单次资格，调用者必须紧接着执行同一受保护效果，不在中间等待或变更参数。已消费/拒绝/撤销/过期/关闭记录不能再次用，不能因为模式后来放宽忽略有身份的无效批准。执行失败不自动重试。
- 仅模式Ask提供记忆；无目标可选择会话工具精确等级，有目标只允许单次/目标/同目录记忆，不能扩大到工具级。目标批准高等级覆盖低等级，目录仅自身/直接条目，多目标全部覆盖；记忆仍绑定Session、Runtime代次、Provider、主体/执行者和工具/子工具，不因新request/参数或普通policy revision变化全部清空。新Deny/显式Ask始终优先。
- 会话记忆最多256条且64KiB，等待64条/每项8KiB载荷，规则128、mode64；均可降低构造参数。批准一次成功但记忆分配失败以独立memory_status提示，不假称已记住。revoke同时清其来源记忆，clear_memory/close_session显式清理；终结等待显式release，不自动驱逐/保存nonce。持久选项不在V2暴露，由APP-05-C/Store及真实Process联验后发布；APP-05父任务不能因此完成。

## 21. Backend领域查询 V1（APP-06-A）

Frontend可见合同为`include/aether/backend/query.h`，仅包含Foundation身份/字符串/追踪值和独立DTO；组合根专用`query_service.h`借用Core公开服务，不供UI包含。它是实际只读领域查询端口，不是APP-07异步Command或APP-08事件实现；API版本独立为1。

- 独立RequestContext携带版本、trace和可信inproc入口已验证的caller/Workspace读取范围，不混入业务DTO、不接纳外部自报权限。trace/span非零并且有效，缺失上下文拒绝；不根据trace授予权限。状态拥有者串行调用及映射，查询无IO/回调/取消/deadline，大型块查询不得在UI线程执行。
- Workspace范围查询仅导出所授权Workspace及Session/Message后代；Runtime必须经可读且关联的Session查询，禁止仅猜全局Runtime ID越范围。Workspace DTO不返回root路径/环境，错误只返回稳定code/安全提示/对应Trace ID，不透传Core私有对象或原始错误。调用者销毁/修改Core后已有DTO仍有效。
- Workspace/Runtime/Session/Message元数据按独立值返回；Block DTO一次分配拥有data/label且NUL终止，专用destroy释放，失败清NULL。单块响应连结构体≤1MiB且≤调用max_bytes，无静默截断；Core大块需后续正式受控分页/引用合同，不偷放宽UI响应预算。
- 未知版本NotSupported/version；缺caller/trace/ID/非法尺寸InvalidArgument/input，越范围PermissionDenied/forbidden，不存在NotFound/missing，分配/响应额度OutOfMemory/capacity。所有失败DTO清空，error必须提供；有合法trace时错误复制diagnostic ID，trace不合法明确不可用。查询没有隐式重试、自动保存或成功事件。

## 22. 有界异步Query Client V1（APP-06-Q）

公开入口`include/aether/backend/async_query.h`只包含Backend DTO；组合根创建Client后将其Query服务与借用Core注册表独占交由单一worker作状态拥有者，不与直接Core调用并发。UI侧仅submit/cancel/take/close查询；这不是Core普遍线程安全的承诺，不把工作线程用于任意修改领域对象。

- submit复制固定请求及Context，分配唯一query_id，回带receiver_generation；同步只做有界校验/预留并入队，不执行查询或搬运正文。API/version/授权失败可作为异步结果返回；受理只代表查询排队，不是业务请求成功。
- 最多32保留项，最大4MiB结果预算；每项预留固定结果加请求声明的块响应上限，结果未领取前不释放额度，无自动驱逐/合并/重试。单worker映射独立DTO，take转移拥有权，result_dispose回收；Client/Core销毁不影响已领取DTO。UI必须按receiver_generation丢弃过时结果并释放。
- 取消等待以ID/代次定位，排队取消不执行查询，运行中取消在映射前后观测；正常完成可能胜过迟到意图。取消不影响任何业务Command。close停止接收并请求取消，is_closed仅实际worker退出后为真；此前destroy返回AlreadyExists且不释放上下文，之后非UI线程join/释放未领取结果。无回调、无隐式超时/副作用重发。
- 线程安全门保护Client槽/预算，Executor锁提供完成发布同步；worker不持Client锁作查询，take只在实际完成后回收槽。Backend新依赖为私有`aether_executor`，OS机制仍隔离在Platform，不暴露控件/原生句柄。独立Query Client不完成APP-06-B/APP-07业务请求及APP-08订阅验收。

## 23. 本地状态命令 V1（APP-06-S）

`include/aether/backend/state_command.h`提供真实Workspace rename及Session request-close，直接入口仅状态拥有者调用，异步入口与Query共用同一后台串行执行域。独立StateCommandContext含可信入口已验证写权限，读取范围不自动授予写权限；业务输入与Context分离，expected_revision必填，所有输出沿用独立Backend DTO/安全错误。

- Rename复制非空UTF-8名称（<128字节、无内嵌NUL），执行前验证当前Workspace与revision；错误版本InvalidArgument/revision_conflict，源数据保持不变。Session请求关闭检查revision后只进入Closing，重复对相同当前Closing/Closed快照可幂等返回，绝不据此宣称Runtime已退出或所有资源close-complete。
- 异步命令通过专用submit接受，通用Query submit拒绝伪装command kind。槽位中拥有名称/Context，不保存UI指针；唯一工作ID不是APP-07业务request_id，不承诺持久保存、业务deadline、历史查询或事件发布。Command取消只在未dispatch时阻止，已dispatch真实成功/失败保留，不因迟到取消回写成Cancelled；无自动重试或回滚已提交事实。
- 这些端口落实类型化变更和Query同域交接，不代替APP-09用例协调、DATA保存、APP-10权限等待或APP-12关闭清理。Windows UI接入必须使用此Backend合同并准确显示local/Closing限制，不能访问Core或把内存改名说成已持久化。

## 24. 本地状态OpenView与有界交付 V1（APP-08-L）

`view.h`定义Workspace/Session单对象快照、Schema1事件、健康及接收器；async_query.h提供专用打开/health/退订/release。父APP-08全部作用域/分页/权限等待及业务事件仍按§4.4完整验收，本节不扩大为已完成的Message/Runtime流。

- 打开作为单个状态任务完成授权快照及登记，返回subscription_id/base_revision/sequence0，下一状态任务才能修改；由本Client专用状态命令产生的实际变化发布独立值事件。事件含schema/event/subscription/object/Workspace/适用Session/Runtime ID、订阅sequence、对象revision、Unix展示时间和操作trace，不用日志猜状态。
- 回调可能早于打开结果，被调用者必须预先保活接收器、有界暂存、安装快照后按订阅身份消费；UI投递按自己receiver_generation再次匹配。每订阅≤256条且≤2MiB，最多16条订阅；队列满/事件创建失败独立健康置ResyncRequired、丢弃积压，状态命令的提交结果仍可查询，不回滚真实变更。
- 交付由独立非UI线程串行处理且不持状态/Client锁回调；载荷只借用到返回。unsubscribe在线性化关闭门后不进入新event回调，当前回调可返回，closed回调一次且返回后才health Closed/release槽位；回调内允许非阻塞提交或退订，不得等待自己/销毁Client。Client关闭同时收束状态worker及交付域，活跃接收context不因超时释放。
- 支持通过query/command同一独占状态域观察局部生命周期；外部直接Core变更不与Client并发，不隐含能监听所有第三方源。重开只取新快照/新身份，不重放副作用。执行取消、订阅关闭、Client关闭和Session/Runtime真实资源关闭仍是不同事实。

## 25. 本地状态命令请求生命周期 V1（APP-07-L）

`command_request.h`将现有类型化本地改名/Session请求关闭受理为权威记录，不替代APP-07 Runtime/权限/保存业务聚合。进程内请求身份与后台工作关联，独立RequestContext及写范围必须有效；不复用Session/trace身份。输入复制/Ingress槽/终态记录在同Client门内原子发布，worker进入前等待受理关联可见。

- 固定≤128记录/<1MiB元数据，Ingress仍按原32项/双限；默认deadline从成功受理单调时刻起10min，专用类型化submit可降低timeout；无deadline不允许。state为Accepted/Running/Succeeded/Failed/Cancelled/TimedOut，terminal_revision第一次变1后不再变，默认终态保留10min。
- dispatch/finish/QueryRequest检查now>=deadline优先TimedOut；已运行或已提交效果仍可能发生，单独保存execution_started/complete/status与实际DTO。deadline终态先于晚成功时不回写Succeeded，不二发完成事实；取消只记录意图，未dispatch阻止修改，运行中真实结果可胜出，不强杀资源或假回滚。
- take_command_completed返回每个终态一次独立快照，QueryRequest仍可查；普通工作结果take不删除历史。release/过期自动回收必须terminal、实际执行完成、工作关联解除且完成事实已领取；不因保留截止提前free。控制读取/释放要求原caller与可读Workspace范围，取消另需可信写权/独立Context，不以trace授权。
- 时钟失败明确返回安全错误，不发幽灵受理；运行/收尾无法读时钟时以失败诊断并保留活跃资源，不编造墙钟截止。局部完成事实是有界可领取权威通道，不声称已实现APP-08全部事件或跨崩溃可靠通知。存储、Runtime、权限等待和业务关闭在后续完整用例联验。
