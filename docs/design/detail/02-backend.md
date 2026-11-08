# 02 Backend：请求协调与应用用例详细设计

## 1. 适用范围与设计地位

本文展开Backend实现流程。DEC-05请求/事件正式语义唯一维护于[接口契约§4.4及§10.4](../../INTERFACE_CONTRACTS.md)，ADR-0005记录参考依据；共享基线只解释实现交接。其他未决DTO、存储和权限内容仍为建议，不是已发布C API或已有实现。

硬规范见 [架构](../../ARCHITECTURE.md) §2–6、[接口契约](../../INTERFACE_CONTRACTS.md) §2–4、§7–13、[工程规范](../../CODING_STANDARDS.md)。Backend 仅依赖 Core 公开接口和技术能力的公开抽象；没有 UI、具体 Adapter、数据库驱动或操作系统句柄。Agent 描述与 Runtime 实例分别查询，不按 Agent 名称选择协议，也不将 Native 实现替换成 Mock。

## 2. 内部记录、拥有者与资源

推荐应用/Core 状态在一个串行执行域处理；入口只做有界校验、复制和受理，IO/计时器仅投递事实。执行域不是固定线程数。用户回调在独立交付执行域串行运行，不持状态锁、不直接修改 Core。

| 记录 / 字段 | 职责、所有权与约束 |
| --- | --- |
| `Backend.lifecycle / instance_generation` | 生命周期与本实例代次；组合根持有 Backend，代次参与请求定位，不复用旧实例句柄 |
| `Backend.requests / subscriptions / ingress` | 有界请求索引、订阅索引及入口队列；登记/发布受入口门保护，发布后可变业务字段只由状态拥有者修改 |
| `ServiceDeps.core / scheduler / clock` | 借用 Core 公开服务、调度能力及单调时钟；寿命覆盖 Backend，禁止服务自行创建另一套注册表 |
| `ServiceDeps.storage / configuration` | 借用保存/配置公开端口；返回提交事实，不交出事务句柄、SQL 或配置文件路径 |
| `ServiceDeps.logger / diagnostics / limits` | 借用统一日志与受控读取端口；限制配置为已验证不可变值，不是请求 TraceContext |
| `RequestRecord.request_id / kind / input` | 一次受理身份、类型标签与类型化输入副本；变长内容由记录拥有；不使用通用 `void*` 结果 |
| `RequestRecord.context / operation_span` | 只读 RequestContext 小值副本；可变 span 终结标记独立保存，由请求协调者一次终结 |
| `RequestRecord.workspace_id / session_id / message_id` | 可空且类型隔离的领域关联；未创建消息时不伪造 message_id，不保留领域裸指针 |
| `RequestRecord.runtime_id / connection_generation / operation_token` | 绑定 Core 返回的执行关联；外部协议 ID 留在 Adapter；token 失效后不得重新路由到别的请求 |
| `RequestRecord.state / cancel_requested / deadline` | 基线请求状态、取消意图和单调截止；不存第二份 Session/Message 状态机 |
| `RequestRecord.resume_step / permission_id / permission_revision` | 等待权限时的有限用例步骤与关联；不是任意函数闭包，不携带调用栈地址 |
| `RequestRecord.execution_status / cleanup_status / persistence_status` | 分别表达底层执行、资源清理、保存事实；请求终态不代表三者都已完成 |
| `RequestRecord.result / error / terminal_revision` | 类型化结果、安全错误及终态提交版本；保存执行已发生的事实，失败不能将其清空 |
| `RequestRecord.pending_callbacks / retain_until` | 未解除任务关联数与终态可查询保留界限；只有保留期结束且关联全部解除才可回收 |
| `Subscription.id / scope / filter / view_revision` | Backend 拥有的订阅身份、已复制过滤条件及视图版本；过滤条件改变重新 OpenView |
| `Subscription.next_sequence / queue / queued_bytes` | 仅该订阅的交付序号与有界载荷；同一业务事实可在不同订阅取得不同 sequence |
| `Subscription.health / closing / active_callback` | 可独立查询的健康状态、关闭门、进行中回调计数；健康状态不依赖已满的数据队列 |
| `Subscription.callback / callback_context / close_completion` | Client 提供的接收关系；Backend 借用 context 到 close-complete，不能保存 ViewModel 指针 |

每个受理请求预留最小终态记录空间，正常结果载荷分配失败仍能记录 `OUT_OF_MEMORY`。队列容量、总字节数、终态保留期与在途上限由 D-LIMIT 批准；到限拒绝新受理，不驱逐仍有回调的记录。内部错误保留受控原因，外部只返回稳定类别/业务码和安全文案；错误复制/释放沿统一错误契约。

## 3. 用例输入、前置、输出、错误与事件矩阵

下列名字是语义操作。所有 Command 成功入口返回自己的 `request_id` 与诊断关联；表中“结果”指异步完成结果。入口拒绝没有 request_id、没有随后完成事件。Query 返回独立 DTO；需要 IO 或较大快照时异步返回，不阻塞 UI。业务事件名除根规范已列名称外均为待评审 Schema。

| 用例 / 输入 | 状态域重查的前置 | 结果 | 主要失败 / 事实事件 |
| --- | --- | --- | --- |
| `OpenWorkspace(path_or_id, options)` | 路径/范围有效，访问获准，数据版本可读 | workspace_id、revision、恢复摘要 | 权限/IO/迁移不可用；WorkspaceChanged |
| `CreateSession(workspace_id, agent_id, runtime_id, options)` | 关联正确、能力支持、工作区可写；Agent 配置与实例选择不混用 | session_id、revision、保存状态 | 不存在/能力不支持/存储失败；SessionCreated、RequestCompleted |
| `CloseSession(session_id, expected_revision)` | 版本匹配；Core 决定领域关闭条件 | 最新快照、未结束工作及保存状态 | stale revision/底层清理失败；SessionUpdated、RequestCompleted |
| `StartRuntime(provider_id, configuration_ref, workspace_id)` | Provider 存在、配置有效、启动权限满足 | runtime_id、connection_generation、能力快照 | 不支持/权限/启动错误；RuntimeStarted 或 RuntimeError |
| `StopRuntime(runtime_id, expected_generation)` | 实例代次匹配；不是某个发送请求的取消别名 | 停止结果、受影响请求摘要 | 不支持/退出失败；RuntimeStopped/RuntimeExited、相关请求完成 |
| `SendMessage(session_id, blocks, expected_revision)` | Session 可发送、结构块受支持、并发规则与权限成立 | message_id、执行结果、保存状态 | 参数/状态冲突/Runtime/IO；MessageStarted、MessageDelta、MessageCompleted、RequestCompleted |
| `CancelRequest(target_request_id)` | 目标属于当前 Backend 可访问范围 | 目标当前状态、cancel_requested、是否已发取消 | NotFound/NotSupported；目标可能稍后 RequestCompleted |
| `RespondPermission(permission_id, target_request_id, decision, expected_revision)` | 等待项仍有效、范围与版本匹配、当前调用者可答复 | 答复已应用或拒绝原因，不表示工具完成 | PermissionDenied/过期/已消费；PermissionResolved，目标后续事实 |
| `InvokeTool(session_id, tool_id, arguments, expected_revision)` | 工具与输入 Schema 有效、可由 Aether 执行、授权满足 | 结构化输出、执行/保存状态 | 不支持/权限/工具错误；ToolCallStarted、ToolCallCompleted |
| `ChangeIntegration(scope, entry_id, configuration, expected_revision)` | MCP/Skill 条目和配置有效，目标能力存在 | 保存版本、激活结果、需重连/重启说明 | 配置/权限/IO/协议错误；配置或集成状态变化 |
| `SetConfiguration(scope, patch, expected_revision, persist)` | 字段可改、配置版本匹配、敏感值走受控存储 | saved/effective/source、覆盖原因、应用结果 | 参数/冲突/IO；ConfigurationChanged、RequestCompleted |
| `QueryRequest(request_id)` | 记录尚在声明保留期且范围可访问 | 请求状态、执行/清理/保存状态、结果或安全错误 | NotFound；不生成业务事件 |
| `OpenView(scope, filter, page_size)` | 范围可读、过滤/分页限制满足 | 快照、subscription_id、base_revision、sequence 基线 | 参数/不存在/内存/容量；后续增量或 ResyncRequired 健康状态 |
| `QueryPage(scope, cursor, expected_revision)` | 游标范围/条件/版本仍有效 | 有界 DTO 页、下一游标、revision | stale cursor/revision；不返回伪一致的拼接页 |
| `QueryDiagnostics(trace_id, filters, cursor, limit)` | 诊断访问范围合法，过滤和数量有界 | 记录页、时间/耗时、截断/轮转/丢失状态 | 参数/权限/IO；不生成业务事件 |
| `Unsubscribe(subscription_id)` / `CloseBackend()` | 句柄属于当前实例；允许重复发起关闭 | 非阻塞发起结果及最终 close-complete | 无效句柄/清理失败；不将退订映射成业务取消 |

`CancelRequest`、`RespondPermission` 作为 Command 有各自请求记录和独立操作 trace；`target_request_id` 是关联，不挪用目标身份。取消命令完成只表示取消意图已处理，权限答复命令完成只表示答复被应用；二者都不能代替目标请求的最终结果。目标请求完成和控制命令完成通过各自 request_id 区分。

推荐错误映射：stale revision、队列满、请求额度满分别使用 `INVALID_ARGUMENT` 加 `revision_conflict`、`capacity_exceeded`、`request_limit`；关闭入口使用 `NOT_INITIALIZED` 加 `backend_closing`。这些业务码及映射需 D-REQUEST/D-LIMIT 评审，不增加 AetherStatus 枚举。内存不足、权限拒绝、IO、协议和 Runtime 失败分别保留现有类别，不统称 INTERNAL_ERROR。

### DTO 映射与线程合同

入口、查询投递及关闭发起接口推荐 Thread Safe，保护范围是句柄生命周期门和受理队列；调用方仍须保证传入缓冲在同步返回前有效，不能与提交同时释放。Core/用例推进仅在状态执行域使用，DTO 释放不访问 Core。Query completion 与事件回调都不在 UI 线程，不允许调用者同步等待状态域；释放 Backend 必须等 close-complete。

| 映射边界 | 确切处理 |
| --- | --- |
| SendMessage DTO → Core 输入 | 逐块保留类型、顺序、文本长度、附件引用和必要 MIME/元数据；先验证总尺寸及已声明能力，再由 Core 检查消息规则；文件引用不等于允许读文件 |
| Core Session/Message 快照 → DTO | 只导出 ID、revision、公开领域状态、结构块及允许展示的错误/保存状态；请求状态通过关联字段另查，不用请求 Succeeded 覆盖 Message 状态 |
| Provider/Core 事实 → Event DTO | 保留七个现行元数据字段，额外关联放共享基线的信封；无 Session/Runtime 时显式可空，使用领域事实时间语义而非伪造全局顺序 |
| 用例结果 → 完成/查询 DTO | 按 request kind 选择具体结果类型；复制安全错误及 execution/persistence/cleanup 事实，不暴露 Provider token、SQL、原始协议或第三方句柄 |
| 配置/诊断 → DTO | 普通值按 Schema 导出，秘密只回 configured/受控引用；诊断记录经过权限/脱敏/数量过滤，再返回独立有界页 |

映射失败销毁已构造的输出子项，返回空输出和明确错误；不能返回半个内容块数组。查询结果由对应释放函数回收，事件载荷仍只借用到回调返回；不会为同进程边界增加序列化往返或强制所有内部对象深复制。

## 4. submit：有界受理算法

1. 初始化失败输出为空，验证 API/DTO 版本、必填与空指针、UTF-8、块数量及字节总量；使用检查过的长度加法，未知块/能力不能静默丢弃。
2. 可信入口验证调用元数据；合法上下文继续传播，否则创建独立操作上下文。TraceContext 不参与权限；ID 生成失败显式返回且不借用别的请求身份。
3. 读取可安全访问的能力快照，明确不支持就拒绝；这不替代 dispatch 时的可变条件重查。复制异步需要的业务数据及只读上下文。
4. 在入口门内重查 Backend 接收状态，取得请求/队列/最小终态额度、分配 request_id，将初始化记录登记并原子发布到有界入口队列。登记与入队要么都成功，要么撤销登记、额度和输入副本。
5. 释放入口门后返回类型化受理响应。状态拥有者开始修改记录前接管其所有权；记录查询也经过状态域，不与登记竞争裸读。
6. 不承诺受理响应先于事件送达，Client 必须按 request_id 关联；不在入口锁内调用用户回调，不隐式重试同一 DTO。deadline 推荐从受理时单调时间起算，排队与 Ask 均计时；是否允许无 deadline 由具体用例发布。

## 5. dispatch：推进用例而非复制领域状态

1. 取出记录，检查生命周期、取消标记和 deadline；到期/未执行已取消走统一 finish，不先执行“最后一次尝试”。
2. 按 `kind` 调用明确用例函数；重新解析领域 ID，校验 expected_revision、Session/Workspace/Runtime 关联及能力代次。旧版本不自动覆盖新内容。
3. 调 Core 公开规则判断可执行性及权限。Allow 进入下一步；Deny 产生权限失败；Ask 保存有限 `resume_step` 与 Core 权限等待关联，再发布 PermissionRequested。
4. 涉及保存的用例向 [数据与配置](05-data-config.md) 交接准备/提交操作；仅收到符合该用例的提交事实后越过相应边界。不持数据库事务等待 UI 或外部 Runtime。
5. 由 Core 选择已注入 Provider 并登记 operation_token/实例代次，确保关联先于潜在同步回调可见；Provider 事实一律回投状态域，禁止重入更新记录。
6. 调用返回“已发起”只更新执行事实，不 finish 成功；立即拒绝则撤销未启动的关联并 finish。后续进度等待事实推进。

同一 Session 的并发发送、Message 状态、Session 关闭条件唯一由 [Core](03-core.md) 决定。Backend 可以有多个 RequestRecord，但不能通过自己的 `busy` 布尔值另定领域并发策略；领域拒绝映射为相应异步错误。请求取消/超时后 Session 不必关闭，Message 也不被强制映射成同名状态。

## 6. Provider 事实接收与 finish

### 6.1 事实接收顺序

1. Adapter 经 Core 的事实入口提供归一化内容、operation_token、runtime_id、connection_generation 及显式追踪关联；Backend 不解析协议、猜测“最近请求”或按外部 ID 单独查找。
2. 状态拥有者核对关联/代次与载荷约束。未知、重复、旧代次事实按 Core/Adapter 契约拒绝或忽略并释放；独立 Runtime 退出属于实例事实，不伪造成某次请求响应。
3. 有效事实先交 Core 应用领域规则；只对真正改变的对象推进 revision，并得到领域事实/只读快照。重复 Delta 的识别由归一化协议关联保证，不能凭文本相等丢弃合法重复内容。
4. Backend 映射统一事件 DTO、分配各订阅 sequence。消息完成事件表达领域事实，RequestCompleted 表达应用结果；存在最终保存时，两者可以先后到达且含义不同。
5. 需要持久化的事实进入 05 的保存路径；用例等待规定的保存确认后调用 finish。请求已超时则只更新真实执行/清理/保存事实，不再次裁决。
6. 输入事实队列满不能静默丢弃后继续声称流完整：停止接收该工作流的新执行，按 Runtime 层的受控背压/故障路径交接；故障控制状态必须有独立有界通知通道或可查询状态。不能依赖再次塞入已满队列。

### 6.2 finish 的单入口算法

1. 校验事实关联与终结资格，检查单调deadline；裁决遵循[接口契约§10.4](../../INTERFACE_CONTRACTS.md#104-请求终态与等待交接dec-05)，不另设线程墙钟优先级。
2. 已终态只处理执行、清理及保存补充事实并释放载荷；未终态一次性写入 state、类型化 result/error、terminal_revision 与保留时点。
3. 作废未消费的权限等待与恢复步骤，解除不再需要的计时任务；已提交外部工作继续保留关联直到底层停止或回调已可靠断开。
4. 提交可查询的权威结果后发布一次逻辑 RequestCompleted，并终结请求拥有的操作 span。事件队列失败标记订阅 ResyncRequired，不撤销权威终态或追加矛盾完成。
5. 后续清理由独立清理操作/span 承担，关联原 trace/request；更新 QueryRequest 的 cleanup_status，不再次结束原操作。终态通知不承诺崩溃后可靠重放。

## 7. cancel 与权限恢复

`cancel(target)` 在状态域按如下顺序执行：查目标和访问范围；终态返回原结果且不发外部取消；未执行时标记取消并 finish；已执行先查真实取消能力，不支持返回 NotSupported 且不杀共享进程；支持则标记意图并至多发送一次取消。重复取消返回现状，等待明确取消完成事实；此间成功仍可胜出。取消传输失败保留“已请求、未确认”事实和错误，不谎报目标停止，按现有 deadline/底层故障事实收尾。

`resume(permission_response)` 必须完成以下步骤，策略优先级与临时授权语义引用 [Core 权限设计](03-core.md)，Backend 不维护副本：

1. 按 permission_id 找到仍在等待的目标记录，核对答复关联、Workspace/Session/Runtime/Tool 范围、permission_revision 和调用者权限。
2. 检查目标未终态、未取消、未到期，Session 未关闭、Runtime 代次未变化；失效时拒绝答复并清理等待，不恢复执行。
3. 请求Core应用该答复并建立对应一次性授权，重复答复不重复创建资格。拒绝结束目标；批准只使有限resume_step可恢复，不在Backend提前消费执行token。
4. 重新校验当前策略与目标，按resume_step恢复一次；策略已改变遵从Core重决策，不用旧批准强行Allow。
5. 实际执行拥有者在副作用前复核权限/路径并原子消费一次授权，不能Backend和执行端各消费一次。Runtime原生权限仅在Adapter确认实际协议响应可执行时报告已应用；无法控制的内部行为明确能力缺口。

等待权限必须可通过权威 Query/OpenView 重建，不能只存在弹窗事件中。关闭/取消时先使等待项无效，再清理显示事件；页面消失本身不是拒绝、授权或业务取消。

## 8. OpenView、交付与退订

1. 校验 scope/filter/页面容量，复制条件并预留有界队列和快照预算。由状态拥有者在一次串行操作内登记订阅、取得不可变领域快照、base_revision 和 sequence 基线。
2. 建立边界后产生的领域变化进入订阅队列；快照映射若需在状态域外继续，仅使用已取得的不可变投影，禁止稍后重新读取可变 Core 对象拼接。
3. 快照/队列/响应映射任一步失败均撤销订阅并释放载荷，不返回半成功。成功响应交给 Client，Client 先保留接收关系，再将快照安装到 UI。
4. 交付域按订阅序列单次进入回调，载荷只借用至回调返回；Client 复制需排队的数据。快照先后与回调调度无全局保证，客户端以握手基线协调。
5. 队列满、缺口或快照失效将订阅健康置 ResyncRequired，停止增量交付、释放不再有用的积压；保留可查询控制状态直到重新打开/退订，不悄悄压掉权限或终态事件。
6. Unsubscribe 设置关闭门，此后不再开始用户回调，丢弃未执行队列；已进入回调可以退出。active_callback 归零后发送一次 close-complete，再允许释放 context。
7. 有效句柄上的重复退订关联同一次关闭完成；句柄释放后再调用属于无效使用，不承诺永久保存 tombstone。回调中可提交非阻塞 Command 或发起退订，不得同步等待本回调退出或同步重入状态处理。

revision 属于状态版本，sequence 属于订阅交付，不用两者互相补洞。单订阅在过滤完成后分配 sequence；同一对象版本不变不产生虚假的状态推进。分页绑定视图条件和 revision，旧游标返回明确 stale 错误；不引入持久事件重放协议。

## 9. 事务、配置与诊断交接

具体记录、原子写入、恢复和迁移只有 [05 数据与配置](05-data-config.md) 维护。Backend 提供类型化业务变更、expected_revision、request/context 和用例提交阶段，接收保存成功/失败事实，不向 Provider 传数据库事务。

- CreateSession：按 05 先保存本地 Initializing 意图，再由 Core 调 Provider 创建；成功后保存外部关联与可用状态，最后发布可用会话事实和 SessionCreated。Provider 失败保存失败事实并清理；外部成功但关联保存失败报告执行/保存差异，追踪残留资源，不盲目再创建。初始化中间态是否展示随 D-DATA/D-DOMAIN 确认，不把它当可用成功。
- SendMessage/工具：受理不执行 Store IO；异步 dispatch 按 05 保存该用例必要的消息/执行意图后才越过外部副作用边界，准备保存失败属于受理后 Failed。最终执行与保存分别交接；对明确要求保存才成功且尚未终态的用例，外部成功但保存失败裁决 Failed，结果仍带 execution_result=success 与 persistence_status=failed。不要求持久化的操作不能一概转失败，已 TimedOut 等终态也不被晚到保存错误改写。
- 超时/取消：外部已经发生的事实仍按 05 处理，不能以请求终态为由丢掉已知结果；不自动重发、不伪造事务回滚。跨重启在途操作只报告恢复出的已知/不确定事实，不自动恢复执行。
- SetConfiguration：输出 saved_value、effective_value、source、apply_status；保存失败与临时覆盖导致有效值未变分开。日志级别遵守命令行 > 环境变量 > 持久用户设置 > 默认 INFO，不能由 API 请求越过更高优先级。
- 日志与诊断交接 [07 可观测性](07-observability.md)：Backend 创建/验证上下文、传递只读值、映射安全响应；Logger 输出失败不改变业务结果。诊断查询有界、脱敏、防递归，不直接返回文件路径或秘密内容。

## 10. Backend close 与部分初始化

| 生命周期阶段 | 入口与处理 | 允许释放的资源 |
| --- | --- | --- |
| Constructing | 顺序记录成功取得的依赖/句柄；未就绪入口返回 NotInitialized | 启动失败按成功取得顺序逆序清理，不调用未初始化成员 |
| Running | 接收 Command/Query/OpenView | 仅已满足各自回收条件的记录 |
| Quiescing | 原子关闭新工作入口；仅保留受控取消、只读收尾查询、退订；作废权限恢复 | 不释放状态拥有者和回调依赖 |
| Draining | 未执行请求取消；在途请求按能力停止；等待提交/底层事实，应用原 deadline 或已声明关闭截止 | 已完成并解除引用的单项资源 |
| ClosingCallbacks | 停止事件生产，全部订阅走退订；等待 Provider/IO 入口解除及回调退出 | 完成各自 close-complete 后释放接收上下文/任务映射 |
| Closed | 对外 close-complete，销毁请求/订阅索引和 Backend 自有资源 | 组合根随后关闭借用依赖；日志最后收尾 |

关闭截止到达可将未终态请求按基线裁决超时，但不能把底层未知标成 stopped。不能及时安全断开的工作须由执行层保留独立清理责任和有效上下文；Backend close-complete 仅在不再可能回调 Backend 后发生。清理失败通过关闭结果及诊断报告，不靠提前 free“完成关闭”。重复 CloseBackend 共享同一个关闭过程；UI 不同步等待。

## 11. 建议文件划分与迁移清单

| 建议文件组 | 单一职责 |
| --- | --- |
| `contracts/api/backend_*.h`、`contracts/dto/*` | 类型化 Command/Query 与公开 DTO、配套释放和版本契约 |
| `backend/api/command_entry.c`、`query_entry.c` | 有界入口校验、上下文及受理/查询交接 |
| `backend/requests/request_registry.c`、`request_completion.c` | 索引/受理资源与终态裁决；不生成第二套领域模型 |
| `backend/services/session_service.c`、`message_service.c` | 对应用例步骤和 05 提交交接；其他服务仅随真实用例加入 |
| `backend/services/permission_service.c`、`configuration_service.c` | 答复关联/恢复与配置应用协调 |
| `backend/events/view_subscription.c`、`event_delivery.c` | 原子视图边界、健康状态、串行交付与关闭 |
| `backend/mapping/*_dto.c`、`backend/backend_lifecycle.c` | 明确类型的投影/错误映射与启动关闭，不建立通用反射框架 |

上述是实施落位建议，不创建空目录/空 Manager。手写代码最多 500 行，超过 300 行审查职责；80 列目标、100 列硬上限，不能机械切片规避。

调用方迁移清单：① Frontend Client 全部 Command 区分受理/完成，控制命令使用 target_request_id；② 页面以 OpenView 替代无关联 Query+Subscribe，处理 ResyncRequired 与 close-complete；③ Core 只回传领域事实和执行关联，不裁决应用请求终态；④ Adapter 外部映射加入实例/连接代次，不反向依赖 Backend；⑤ 存储/配置端口返回阶段性保存事实，调用方显示执行与保存差异；⑥ 诊断调用方统一传递只读 RequestContext、使用独立 DTO/错误释放接口；⑦ 契约测试、无 UI 客户端、测试替身同步全部行为。当前没有既有源码迁移；正式发布前一并收敛版本，不保留旧别名或双轨完成路径。

## 12. BE 验收与评审门槛

以下为待实施的 Given/When/Then，不表示已运行测试；由 [实施与验证](08-implementation-validation.md) 纳入集成验收。

| 编号 | Given | When | Then |
| --- | --- | --- | --- |
| BE-01 | 输入来自临时缓冲且事件可能先回调 | submit 成功后调用方立即释放输入 | 内容/上下文仍有效，受理与完成按 request_id 正确关联 |
| BE-02 | 任一复制/登记/入队步骤注入失败 | 提交 Command | 无受理 ID、无完成事件，已取得资源全部撤回 |
| BE-03 | 完成、取消确认和 deadline 交错 | 状态拥有者按不同顺序处理 | 仅一个符合基线的终态，清理事实可继续变化，无重复 span 终结 |
| BE-04 | 目标已终态、未知或不支持取消 | CancelRequest 重复调用 | 分别返回原状态、NotFound、NotSupported，不误杀共享 Runtime |
| BE-05 | Ask 后取消、关闭、代次或策略变化 | 迟到/重复批准到达 | 不恢复失效步骤，不重复消费授权，执行边界仍检查权限 |
| BE-06 | OpenView 建立期间持续更新 | 返回并安装快照，再接收增量 | 不丢交接窗口；快照失败撤销订阅，溢出进入可查询 ResyncRequired |
| BE-07 | 回调已进入且另有排队事件 | 回调内退订或页面关闭 | 不启动新回调，排队载荷释放，close-complete 后才能释放 context |
| BE-08 | 要求保存才成功的请求尚未终态，外部执行成功而最终保存失败 | finish 接收保存错误 | 请求失败且明确执行已成功/保存失败；若原已 TimedOut 则只更新保存事实，不改终态、不回滚或自动重发 |
| BE-09 | 日志不可写且存在临时级别覆盖 | 业务完成、保存日志偏好并查询 | 业务结果不被日志污染，saved/effective/source 与故障状态可区分 |
| BE-10 | 部分初始化或在途工作无法立即停止 | CloseBackend 重复发起 | 单一关闭过程，无悬空回调，未知执行与清理失败不假报成功 |
| BE-11 | 两实例复用同一个外部请求 ID | 旧连接事实到达新连接 | 不跨实例/代次关联，不更新无关 Session 或请求 |
| BE-12 | 无 UI 客户端持有旧 revision/游标 | 并发修改后提交或分页 | 返回明确冲突，不旁路 Core、不返回混合快照、不泄漏领域指针 |

DEC-05已收敛D-REQUEST/D-EVENT语义，APP-06/07/08按接口契约§4.4/§10.4实现与验证；签名/类型化DTO仍需发布检查。剩余前置包括D-DATA用例提交点、D-PERMISSION执行授权、D-LIMIT输入/事实/订阅/保留数值、D-ID/D-DIAGNOSTICS代次与诊断返回；不重复批准已收敛语义，也不声称线程库或容量已批准。
