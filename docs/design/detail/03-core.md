# 03 Core：领域对象、状态与安全规则详细设计

## 1. 范围与约束

本文细化概要 [03-core.md](../03-core.md) 中的领域建议，供评审，不是已接受 ADR、C17 枚举或 ABI。跨层语义以 [00-contract-baseline.md](00-contract-baseline.md) 为准；现行硬规范以 [接口契约](../../INTERFACE_CONTRACTS.md)、[架构](../../ARCHITECTURE.md) 和 [工程规范](../../CODING_STANDARDS.md) 为准。

Core 只拥有 Workspace、Session、Message、Runtime 注册关系和 Permission 领域事实。Backend 拥有应用请求受理、编排及独立请求终态；Adapter 拥有协议连接和外部缓冲区；Frontend 不写领域状态。Core 不解析协议、不调用 OS、不把日志或 TraceContext 放进持久业务对象。

推荐以一个明确的领域执行域串行提交可变状态。执行域可以由 Backend/运行时调度器承载，不等于固定线程数；IO 回调只能投递带对象 ID、代次和事实类型的消息。任何跨边界输入先校验大小、编码、关联 ID 和能力，再复制到 Core 所需寿命。

## 2. 对象字段与关系

下表是内部字段建议。字段类型、编码、容量和持久化列名须由 D-ID、D-LIMIT、D-DATA 评审收敛；ID 按类型隔离，不能用显示名或页面位置代替。

| 对象 | 必要内部字段（建议） | 关系/变更规则 |
| --- | --- | --- |
| AgentConfig | `agent_config_id`、`revision`、display_name、provider_id、runtime_profile、model、system_instructions、tool/skill allowlist、workspace policy ref、created/updated | 可被多个 Session 引用；Session 建立时固定配置快照 revision。配置修改不改写既有消息；是否允许 Session 显式切换须另行授权。 |
| AgentRun（仅在有独立事实时） | `agent_run_id`、agent_config_id、session_id、runtime_id、connection_generation、state、started/ended、failure | 不是 OS 进程，也不是 request_id。若实现不需要独立查询/关闭责任，则不设此实体，直接以 Session 与 Runtime 关联。 |
| RuntimeDescription | `provider_id`、版本/展示信息、能力集合、配置 schema ref、availability hint | 描述可存在而没有实例；能力声明不是当前可用性或权限批准。 |
| RuntimeInstance | `runtime_id`、provider_id、connection_generation、endpoint kind、capabilities snapshot、state、last fact、created/closed | `runtime_id` 标识运行/连接实例；每次重新连接递增 generation。不得把 provider 名称当实例 ID。 |
| RegistryEntry | provider/runtime key、registration token、owner、registered_at、unregistering、ref summary | Registry 是受控查找和关系协调入口，不是所有对象的 Manager 或隐藏全局单例。 |
| Workspace | `workspace_id`、canonical root ref、display_name、config revision、environment ref、tool/skill refs、permission policy ref、state、created/updated | Workspace 可关联多个 Session；关闭/删除前必须处理 Session、授权和资源引用。路径引用不自动授予读取权限。 |
| Session | `session_id`、workspace_id、agent_config_id/revision、runtime_id/generation、optional agent_run_id、state、created/updated/closed、next message order、context ref | 必须绑定存在的 Workspace 和明确 Runtime 关联；可有多个 Message。关闭只由 Session 拥有者提交。 |
| Message | `message_id`、session_id、role、content blocks、state、sequence/order、created/updated/completed、metadata、runtime turn ref | 必须属于指定 Session；`message_id`、块 ID 和增量代次用于归并，不能按到达时间附加到最近消息。 |
| ContentBlock | `block_id`、kind、ordinal、revision、status、payload/ref、tool_call_id/tool_result_for、delta cursor | kind 至少覆盖 Text、Image、File、ToolCall、ToolResult、StructuredData；未知 kind 不得静默转成文本。 |

Session 与 Runtime 是当前绑定，不代表 Runtime 由 Session 独占；共享 Runtime 关闭不得由单个 Session 直接杀掉。Workspace 配置的读取应固定到 Session 创建时的 revision，除非显式的 reconfigure 操作创建新 revision 并重验能力、权限和 Runtime；配置继承不等于权限继承。

## 3. Core 公开操作建议

所有操作是类型化输入/输出的领域端口建议，不接受无类型 `void*`。成功只表示 Core 已提交领域事实，不表示请求完成、落盘成功或外部进程退出；异步请求的 request_id 和终态由 Backend 独立管理。

| 操作 | 输入 | 成功输出 | 主要失败/不变量 |
| --- | --- | --- | --- |
| RegisterRuntime | description、provider token、能力事实、connection_generation | runtime_id、registry revision、Runtime 快照 | provider 不匹配、重复代次、能力格式非法；不把登记当可用。 |
| UpdateRuntimeFact | runtime_id、generation、归一化事实 | 新 runtime revision/event | 未知或迟代次为 stale/ignored；不得覆盖新代次状态。 |
| UnregisterRuntime | runtime_id、generation、原因、关闭模式 | unregistering/closed 结果及受影响关系摘要 | 先阻止新绑定，标记不可用，再通知 Session/请求；不留悬空引用。共享实例仅释放本登记关系。 |
| CreateWorkspace | workspace 输入、配置 revision | workspace_id、Workspace 快照 | 路径/配置/权限校验失败不创建半对象。 |
| CreateSession | workspace_id、agent_config_id/revision、runtime_id/generation、初始元数据 | session_id、Created/Initializing快照；初始化确认后Idle | 前置失效不创建；外部及持久化交接由Backend推进，不能只验证本地字段就称初始化成功。 |
| StartTurn | session_id、用户Message输入、能力上下文 | message_id/turn ref、领域接受事实 | 默认Idle可发送；Active仅在Provider明确支持并发turn且会话合同允许时接收，否则状态冲突；复制失败不产生幽灵Message。 |
| ApplyMessageDelta | session_id、message_id、block_id、generation/cursor、operation、payload | 新 message/block revision、状态事实 | 关联错误拒绝；重复幂等，过期/越序需重同步或失败，不猜配对。 |
| FinishMessage | message_id、终止事实、generation | Message 完成/失败/中断快照 | 终止只提交一次；不推断 Backend 请求成功。 |
| CloseSession | session_id、原因、模式 | Closing/Closed 快照或异步 close fact | 先停止新 turn，再等待领域内在途引用收束；重复关闭幂等。 |
| QuerySnapshot | typed scope、revision/cursor | 独立所有权快照 | 不返回内部可变指针；stale scope 明确失败。 |
| EvaluatePermission | subject、operation、resource、scope、policy revision、request context | Deny/Ask/Allow决策与匹配摘要 | 不产生批准；未知能力或无可验证访问范围不能通过Ask变成支持。 |
| ApplyApproval | approval token、原请求目标摘要、expires、single-use | 绑定当前policy revision的一次性授权记录 | 不修改持久策略revision；目标/范围/版本不匹配拒绝，执行前再次检查。 |

返回快照和错误中的 TraceContext 由 Backend/RequestContext 独立携带，不写入对象字段。Core 可返回业务 code、对象 revision 和冲突原因；新增错误名须映射现有 AetherStatus 类别并经契约评审。

## 4. Session 状态与转换

Active推荐表示至少一个未终止turn，初始化成功且无在途工作为Idle。Interrupted仅表示会话上下文被明确中断、需显式恢复，不等同于普通请求取消；连接失效用RuntimeUnavailable。状态由Core串行提交，不复制Backend请求终态。

| 前态 | 触发/条件 | 后态 | 必须副作用与失败路径 |
| --- | --- | --- | --- |
| Created | 开始验证 Workspace、AgentConfig、Runtime | Initializing | 固定配置和 runtime generation；禁止新的 turn。 |
| Created | 输入/关联/容量验证失败 | Failed | 记录失败原因；不发布可用 Session，不遗留部分绑定。 |
| Initializing | Provider绑定确认且Backend确认该用例所需保存/准备成功 | Idle | 发布可查询快照；不把本地参数验证当外部创建完成。 |
| Initializing | Runtime 不存在/不可用且不可恢复 | RuntimeUnavailable | 保留可诊断关联，不伪造 Active；是否可重试由显式操作决定。 |
| Initializing | 内部/持久化提交失败 | Failed | 释放临时关系；Backend 自己的请求终态另行裁决。 |
| Idle | StartTurn成功且创建在途turn | Active | 增加turn引用；用户内容可已完整，助手响应按Receiving消息处理。 |
| Idle | Runtime 事实变为不可用 | RuntimeUnavailable | 阻止新 turn；既有事实按消息/请求规则收尾。 |
| Active | 仍有在途 turn | Active | 增量只更新指定 Message；不得因单个 chunk 改 Session 时间状态。 |
| Active | 最后一个turn正常完成、独立失败或普通取消，Session仍有效 | Idle | 消息、领域与请求结果分别交付；普通取消不强制要求Session恢复。 |
| Active | Runtime断连且尚有工作 | RuntimeUnavailable | 优先处理连接失效，保留已有内容，不再按普通turn结束回Idle。 |
| Active/Idle | Provider明确报告会话上下文中断、需恢复，连接仍可诊断 | Interrupted | 与普通turn取消区分，不接受新turn，等待恢复或关闭。 |
| Created/Initializing/Idle/Interrupted/RuntimeUnavailable/Failed | 显式关闭 | Closing | 阻止新工作，作废授权，清理已取得关系；Failed残留资源仍须清理。 |
| Active | 显式关闭 | Closing | 先标记禁止新工作；等待/取消领域引用，不能回滚已发生副作用。 |
| Closing | 所有领域引用、订阅交接、Runtime 关系已解除 | Closed | 只允许查询保留快照/诊断；释放内部资源。 |
| Closing | 关闭清理不可完成 | Failed | 保留清理失败事实和关联，仍不得恢复接受新 turn。 |
| Created/Initializing/Idle/Active/Interrupted/RuntimeUnavailable | 不可恢复领域异常 | Failed | 保留已提交事实，不伪装Closed。 |
| Interrupted/RuntimeUnavailable | 显式恢复获准、旧工作已收尾且Runtime支持恢复 | Initializing | 实际恢复成功仍经初始化转Idle，不原地改Active。 |
| Closing | 重复关闭 | Closing | 复用原关闭过程，不重复释放。 |
| Closed | 重复关闭/迟到事实 | Closed | 返回已关闭事实，释放载荷，不复活。 |

分支优先级：连接失效走RuntimeUnavailable；会话上下文受损走Interrupted；普通turn终结且会话仍有效才回Idle。Failed不自动恢复，可关闭后另建Session；未列状态/操作组合明确拒绝，同一事件不能匹配两个转换分支。

Session Close 的顺序建议为：置 `Closing`、阻止新操作、撤销等待授权、通知 Adapter 停止产生新事实、等待/标记领域引用结束、解绑 Registry 关系、提交 Closed。Backend 仍必须按自己的 RequestRecord 规则裁决取消/失败/超时；关闭不是请求终态。

## 5. Message 与 delta 归并

Message推荐状态为Receiving、Completed、Interrupted、Failed，后三者为终态。Frontend草稿不进入Core；完整用户内容可以直接形成Completed消息，但只表示内容完整，不表示发送/请求执行成功。助手流式消息从Receiving开始；关闭不删除已提交块。

流式事实携带session/message/block身份、connection_generation、块内delta_cursor、operation（create/append/replace/finalize/message_end）、payload及工具关联。Adapter按已验证协议顺序生成单调cursor，只有协议重复证据才能复用cursor，不能按相同文本去重。message_end是消息级事实，不要求伪造block_id。

推荐归并算法：

1. 在状态拥有者执行域验证Session/Message及Runtime代次；块级操作继续验证block，message_end走消息级完整性分支不查伪block。关联不符拒绝，旧代次stale释放。
2. 检查 Message 是否已终态。终态后的重复相同事实可幂等确认；不同内容、迟到 finalize 或新块一律不改写终态，记录冲突/迟到事实。
3. 新block仅接受create、起始cursor及完整kind/ordinal，ordinal冲突拒绝。未知kind按评审合同拒绝或受限保留，不自动转Text。
4. 本块保存last_cursor及最近事实的受控内容/操作校验信息。同cursor相同事实为重复不更新，不同事实为冲突；小于last为stale；恰等于last+1才接收；大于last+1为gap，进入不完整/故障处理。起始cursor统一约定，不保留无界历史去重表。
5. append先检查容量，replace检查expected_block_revision；先构造有效新内容再提交，分配失败保持原内容和revision。实际变更递增Message/Block revision，重复不递增。
6. finalize只关闭指定块，不据“当前块都关闭”推断再无新块。只有Adapter确认真实协议的message_end且所有必需块完整才Completed；EOF不能代替message_end，中断为Interrupted、明确错误为Failed。
7. 将“消息已完成”反馈给 Backend；Backend 再依据独立 RequestRecord、持久化和外部执行事实裁决请求终态。

文本增量只适用于同一 Text block；ToolCall/ToolResult 必须以 `tool_call_id`、调用代次及结构校验关联，缺关联不配对。文件、图像、结构化数据优先使用受控引用或已校验载荷；ContentBlock 的引用权限仍由 Workspace/Permission 决定。

## 6. Runtime 与 Registry

RuntimeDescription 表示“是什么/支持什么”，RuntimeInstance 表示“哪一次运行或连接”，Runtime 可用性表示“此刻能否接受事实”，Permission 表示“本次主体是否获准”。四者不互相替代。Runtime 的 `Stopped`、`Unavailable`、`Degraded` 等外部事实不得直接改写 Session；Core 按 generation 和绑定规则转换为 Session 的 RuntimeUnavailable、Failed 或正常收尾。

新实例登记验证Provider与描述，分配runtime_id并原子插入。同一实例重连保留runtime_id并更新connection_generation，旧Session绑定失效或经真实恢复重新绑定；新实例另发runtime_id。查找返回受控快照/句柄，不暴露可变内部指针。

注销步骤为：标记 `unregistering` 并阻止新绑定、冻结能力快照、列出 Session/AgentRun/请求关联、通知各自拥有者、等待引用解除或记录明确的 unavailable/cleanup pending、再移除条目并发布 close-complete。注销不杀共享进程，不取消 Backend 请求的终态裁决，也不释放仍被回调使用的 context。若清理失败，条目可保留为不可用 tombstone，直到引用安全解除；不得留下悬空 runtime_id 关系。

APP-01现有C17注册端口与引用注销行为见[接口契约§16](../../INTERFACE_CONTRACTS.md#16-core-runtime-registry-v1app-01)，代码位于`src/core/runtime/`。Provider执行和Session订阅仍由后续RUN/APP任务交付；本层不预建函数表或假Runtime。Registry只复制值快照、检查generation、关闭借用门并等待Ref解除，未接通OS或第三方协议。

## 7. Permission 作用域与安全算法

Permission只表达可执行边界内的授权，不拦截未知Runtime内部行为。用户明确要求参考OMP正常开发不反复授权的体验；采用模式化执行方向，不把逐工具弹窗当作必要前提。规则字段建议为subject、operation、resource、scope、effect、conditions、policy_revision、期限与来源；Workspace、Session、Runtime、Tool是组合维度，不强行排成继承链。详细求值与公开合同由DEC-07继续收敛，见[ADR-0007](../../adr/0007-permission-enforcement-boundaries.md)。

推荐求值步骤（修订候选，不宣称已实现）：

1. 规范化实际operation/resource、工具能力等级与上下文；读取有效模式、显式工具规则及已记忆授权。Inherit继续查声明的父范围，不等于Allow；缺失上下文不能凭空获得权限。
2. 保留OMP式直接执行、按等级确认及缓存确认路径，显式deny不能被模式或缓存覆盖。Aether组合范围的冲突映射须在正式合同中确定，不将旧Deny > Ask > Allow候选误称为OMP算法。
3. 普通调用已被模式/规则/有效授权覆盖时直接执行，不生成多余Ask；确需交互时才建立待确认项。明确不支持的能力返回NotSupported，不能用批准伪造能力。
4. 交互参考OMP提供单次、会话内工具批准、目标/目录的会话或跨会话记忆，仅列出当前调用支持的范围。会话工具批准绑定工具/子工具与等级，目录批准仅自身直接条目，多目标须全部覆盖；显式逐次确认不能被普通缓存跳过。
5. 单次批准绑定请求与不可变目标，只消费一次；会话和持久批准按选定范围复用，不绑定每个新request而强迫重新弹窗。会话结束清除会话批准，持久记忆来自明确选择并可清除；存储实现由DEC-08与DATA任务落实。
6. 执行前按实际参数、当前规则及授权范围重查；变更后的调用若仍在有效授权内无需重复批准，不在范围内则按当前模式处理。取消、失效或错误请求的迟到答复不能恢复执行。确需Ask但无交互通道时不伪造批准。
7. 记录决策来源和结果，不把秘密、正文或凭据写入普通日志。Runtime原生授权只映射真实协议；没有OS沙箱不自动阻止常规使用，也不额外逐工具套一层审批。工作流明确要求的强制限制无法满足时须准确报告，不得声称受控。

Core验证和应用有效策略/授权，Backend负责真实交互；无需确认的调用不创建人工批准步骤。既有Workspace范围、Process Service、环境白名单及路径安全要求不因免弹窗消失；审批与OS隔离是不同能力。

## 8. 待评审决策

以下为共享D-*的本层收敛点，不另建与验收编号重名的决策表；确认后同步接口和消费者：

- D-DOMAIN：AgentRun是否有独立职责；Active并发规则、初始化成功/关闭/恢复和Interrupted含义；未知内容块处理。
- D-ID/D-PROTOCOL：实例重连代次、Adapter归一化cursor、真实message_end证据及Tool关联，不能按相同文本去重。
- D-PERMISSION：作用域匹配、Deny > Ask > Allow组合、授权截止/失效和执行端单次原子消费。默认不持久化一次性授权。
- D-EXEC/D-LIMIT：Registry注销时引用等待、不可用关系保留及回调关闭界限，不释放仍在使用的对象。

## 9. CORE 验收边界

- **CORE-REL-01**：Given 未知 Workspace、Runtime generation 或跨 Session Message ID，When 创建/归并，Then 原子拒绝且不产生部分关系。
- **CORE-REL-02**：Given 同一 Runtime 被两个 Session 使用，When 一个 Session 关闭或注销关系，Then 只解除自己的关系，不杀共享实例、不留悬空引用。
- **CORE-SES-01**：Given Session 初始化成功且无在途 turn，When 查询状态，Then 为 Idle；Given 至少一个未终止 turn，Then 为 Active。
- **CORE-SES-02**：Given最后turn普通完成/失败/取消且Session有效，When提交事实，Then回Idle；Given上下文中断或连接失效，Then分别Interrupted或RuntimeUnavailable，不匹配普通结束分支。
- **CORE-SES-03**：Given Closing/Closed/Failed Session，When 提交新 turn或迟到增量，Then 不接受、不复活，已提交内容和清理事实可查询。
- **CORE-MSG-01**：Given 相同 cursor 的重复 delta，When 再次归并，Then 内容与 revision 不重复增长；Given 跨块/跨消息 delta，Then 拒绝。
- **CORE-MSG-02**：Given 迟到、跳 cursor、未知块或未关联 Tool Result，When 归并，Then 得到显式 stale/resync/unsupported/association error，不静默猜测。
- **CORE-MSG-03**：Given Runtime 中断，When 已有部分块，Then 保留已提交内容并将 Message 置 Interrupted/Failed，不能伪造 Completed。
- **CORE-RT-01**：Given 旧 generation 的迟到 Runtime 事实，When Registry 已有新 generation，Then 不覆盖新实例状态。
- **CORE-RT-02**：Given 注销请求，When 仍有 Session/回调引用，Then 先阻止新绑定并保留不可用关系，待 close-complete 后才释放。
- **CORE-PERM-01**：Given 无匹配允许规则或上下文缺失，When 评估高风险操作，Then Deny；Inherit 不能单独产生 Allow。
- **CORE-PERM-02**：Given Ask 获得一次性批准，When policy revision、目标或 Runtime generation 改变，Then 执行前复核拒绝且不产生副作用。
- **CORE-PERM-03**：Given Core 收到 Ask，When 没有 Backend/用户批准事实，Then Core 不擅自批准，执行端不得把 Ask 当 Allow。
- **CORE-PERM-04**：Given宽Deny与窄Allow或Ask与Allow并存，When求值，Then分别Deny或Ask；一次批准不能越过Deny或重复消费。
- **CORE-OWN-01**：Given Adapter、Backend、Frontend 直接改 DTO 或迟到回调，When Core 提交状态，Then 只有 Core 拥有者按规则变更领域状态；Backend 独立裁决 RequestRecord 终态。

这些验收覆盖领域边界，不等同于真实 Runtime、UI、持久化事务或跨崩溃恢复验收；相关行为分别由 Runtime、Frontend、Data 和 Backend 详细设计负责。
