# 05 数据持久化与配置详细设计

## 1. 设计状态与边界

本文在 [基础设施概要](../05-infrastructure-platform.md) 上给出 DEC-08 已收敛的 D-DATA 实现语义。SQLite 3.54.1 是本地 Store 的选定基线，yyjson 0.13.0（MIT）是 JSONL/受约束 JSON payload 的统一编解码选型；两项依赖尚未引入，不宣称已有 C 集成或三平台验证。逻辑记录与算法仍不是已批准 SQL Schema，不建立通用 ORM、事件溯源或跨 Runtime 分布式事务。遵守 [共享基线](00-contract-baseline.md)、[Core](03-core.md)、[Backend](02-backend.md) 及 [接口契约](../../INTERFACE_CONTRACTS.md)。
数据库权威分清：Core 拥有内存领域事实，Backend 协调保存，Store 管存储事实；三者通过 revision/保存状态交接而非互相直接改私有对象。SQLite C API 只在 Infrastructure 适配层出现，Core/Backend/UI 不接触 sqlite3 句柄、SQL 或底层错误原文。
## 2. 逻辑记录与键

| 逻辑集合 | 推荐字段/键 | 约束与保存范围 |
| --- | --- | --- |
| workspace | workspace_id、路径引用、显示名、config_revision、policy_revision、记录revision、时间 | 不连带删除用户工作目录；路径是受控数据，不出日志 |
| runtime_config | provider_id、配置ID、启动/连接参数引用、secret_ref、版本信息 | 不把活进程句柄、临时能力报告当持久实例 |
| session | session_id、workspace_id、provider_id、runtime_id关联、外部会话引用、模型/配置revision、状态、record_revision | 重启后原runtime_id不代表连接仍可用；外部引用重新核实 |
| message | message_id、session_id、role、status、content_revision、时间、保存状态 | Message终态不等于请求成功；部分内容明确标记 |
| content_block | message_id、block_id、ordinal、kind、受限payload/attachment_ref、revision | 顺序唯一、类型校验、Tool Call/Result关联完整，不存任意裸指针 |
| attachment | attachment_ref、受控位置、media_type、size、归属与生命周期 | 文件访问独立授权，不自动复制或删除工作区原文件 |
| execution_marker | request关联、session_id、启动事实、最终已知结果、execution_unknown标志 | 仅恢复诊断/一致性所需事实，不持久化可执行回调或自动重放 |
| config_entry | scope_kind、scope_id、key、typed_value、revision、secret_ref | 类型与适用作用域由配置Schema验证，敏感明文不进入普通值 |
| tool/mcp/skill_config | 来源ID、scope、version、启用意图、有效revision、credential_ref | 期望值与Runtime实际生效值分开 |
| permission_policy | scope、规则/版本、更新标记 | 临时一次性授权不持久化，详见Core D-PERMISSION |
| schema_meta | schema_version、迁移状态及必要兼容信息 | 不以软件版本字符串替代存储版本 |

这些是逻辑分区，可由数据库表或其他已选存储表达，不要求一行对应一张表。索引至少评估 session按workspace、message按session及稳定排序键；无负载证据不建全文搜索/向量索引。

## 3. Store 端口与资源

| 操作意图 | 输入/输出 | 失败行为 |
| --- | --- | --- |
| open/inspect_version | 受控路径及模式 → Store句柄/版本 | 不兼容不清空数据；失败无半公开句柄 |
| load_workspace/session | ID → 独立快照及revision | NotFound 与 IO/损坏区分；失败输出为空 |
| commit_changes | 类型化变更集、expected_revision → 新revision | revision冲突拒绝覆盖，失败保持原数据 |
| list_messages | session、稳定排序游标、页上限 → 有界页/继续标识 | 游标作用域/快照不匹配明确失败，不串session |
| save_config | 校验后值、scope、expected_revision → saved_revision | 保存失败不提升已保存值，错误保留原因 |
| close | Store实例 | 已接收写入按合同完成/拒绝，不释放仍被任务使用的连接 |

单一写入拥有者、短事务是硬约束；查询并发只使用 SQLite 已验证能力。端口返回值为调用方独立数据，底层查询缓冲只在内部借用。SQL 使用绑定参数，语句/事务配对释放，不暴露数据库错误原文到 UI。
Store 采用 SQLite 的类型化列、外键、唯一约束和绑定参数；可扩展 payload/显式 JSONL 交换格式统一使用 yyjson 0.13.0，核心字段不得退化为任意 JSON 字典。SQL 表名、排序键和迁移步骤来自编译期白名单。Store 路径只能由 Infrastructure/Platform 打开，WAL 及 `-wal`/`-shm` sidecar 纳入文件身份、备份和损坏保全；不把网络/同步目录或 WAL 当成跨进程无限并发保证。

## 4. 保存算法与副作用边界

### 4.1 创建工作区

校验路径权限和字段 → Core 构造合法候选 → Store 原子保存记录 → 状态拥有者安装快照/revision → 发布 WorkspaceCreated。保存失败释放候选，不发布成功。安装失败是可恢复一致性错误，须按已提交记录重新加载而非再生成第二条记录；关键内存先预留，缩小提交后失败窗口。

### 4.2 创建 Session

推荐先保存本地 Initializing 意图，再调用 Provider create_session，成功后保存外部引用及可用状态，最后发布可用事实。Provider失败保存 Failed并清理已取得外部资源。外部创建成功但第二次保存失败时，不宣称回滚外部行为；报告保存失败、追踪外部资源并尝试合同允许的关闭。重启后根据初始化标记显示恢复待核实，不默认再创建同一外部会话。

此流程的初始化意图是否对UI可见与错误返回的详细映射在 D-DATA/D-DOMAIN 共同评审；不锁住事务等待网络。

### 4.3 消息与流式内容

1. 在发送外部副作用前保存必要用户消息及执行意图，或明确该用例不承诺断电恢复；推荐前者用于持久会话。
2. Provider 调用后接受归一化增量，Core 更新内容revision，Backend 标记保存 pending。
3. 推荐按有界批次/时间窗提交最新块变更，不每token建事务；阈值在 D-LIMIT 定义。内存通知携带保存状态，不能称已持久化。
4. 完成/失败/取消后提交已确认内容及执行结果；若落盘失败，分别保留执行结果与 save_failed，不把成功外部执行改写成“未执行”。
5. 内容缓冲达到预算时背压或拒绝继续接收的行为须先定义；禁止无限缓存，也不能悄悄丢文本继续标记完整。

外部发送已发生而结果未知，保持 execution_unknown，不自动重放。日志不是恢复消息正文的备份。

## 5. 版本、并发和查询

采用乐观 revision 检查避免配置编辑或并发写丢更新；冲突返回现有状态类别与独立业务代码，UI取得最新值后由用户决定，不自动覆盖。revision 是保存/CAS版本，不自动代表完整历史。

消息分页使用稳定 `(创建顺序键, message_id)` 或评审等价键，游标绑定session与查询条件；活跃消息内容更新使用对象revision，不用列表游标替代流式更新。历史查询与 OpenView 基线协调遵循 [00](00-contract-baseline.md)。

删除语义分三步：标明目标为元数据/历史/受管附件中的哪一类；校验在途引用和权限；事务删除所属记录后处理受管附件。外部用户文件不在删除范围，附件失败可见且可再次清理，不用事务假装回滚文件系统副作用。

## 6. 启动恢复与迁移

启动读取 schema_version → 识别兼容范围 → 获得迁移互斥/实例锁 → 通过 SQLite 在线 backup API，或在关闭 Store 后对已 checkpoint 且一致的快照保全数据库及 `-wal`/`-shm`/`-journal` sidecar（禁止对活跃数据库及 sidecar 分别复制伪称一致快照）→ 执行逐版本步骤 → 验证外键、唯一键、Session/Message/ContentBlock关联、配置作用域与 secret_ref → 对外开放Store。更高未知版本拒绝写入和破坏性打开；失败回滚事务并保留原数据，不自动删库重建。损坏时关闭失败句柄、按文件身份保全原件，Store安全拒绝；不得跳过损坏行继续宣称完整。跨进程/崩溃遗留执行记录恢复为“非活动、结果需核实”，不能重建指针、取消令牌或授权；不自动重放外部副作用。真实旧格式样本必须覆盖消息、配置、凭据引用和中途失败。

## 7. 配置解析与生效

每项配置Schema定义类型、允许作用域、默认值、敏感性、生效时点及是否允许覆盖。普通可继承配置有效值顺序固定为 Session 显式覆盖 → Workspace → 应用持久化偏好 → 内置默认；未声明可继承的键不合并，权限策略不因配置继承而继承。CLI/环境变量仅在该键显式允许时作为临时启动覆盖，优先级为 CLI → ENV → 上述持久化值；日志等级保持其专门硬约定。

保存步骤：输入校验与授权 → 检查expected_revision → 安全字段写入 Secret Store 或普通持久化 → 持久化成功 → 重算 effective value 及来源 → 按 Schema 生效规则通知消费者。返回 saved_revision、effective_value/source、apply_state（applied/pending/failed）；应用失败但保存成功时返回 saved + apply_failed/pending，不显示“已生效”。

敏感配置普通表只存不透明 `secret_ref`、安全存储类型及必要版本/状态，不存明文、掩码伪值或可逆临时备份。`secret_ref` 不可读取、被删除、版本不兼容或安全存储不可用时，返回稳定的 `AETHER_CONFIG_SECRET_UNAVAILABLE` 业务错误及真实状态类别；不得以空字符串、默认值、旧掩码或未声明环境变量静默替代。需要秘密的新建/重连/外部调用被阻止并显示已配置但不可用；跨安全存储与配置库更新不是原子事务，写成功一侧保留可清理引用，禁止删除仍在使用的凭据。

## 8. 文件分工与实施检查

建议拆为 `workspace_store.c`、`session_store.c`、`message_store.c`、`config_store.c`、`store_migration.c` 等真实职责；共享连接/事务辅助小型化。公开端口属于其消费者允许的依赖边界，具体后端留Infrastructure。每文件500行上限不靠复制SQL/状态机规避。

| 用例 | Given / When | Then |
| --- | --- | --- |
| DATA-01 | 创建工作区，写入失败 | 不出现成功事件，原数据不变，候选资源释放 |
| DATA-02 | Session外部创建成功，保存外部关联失败 | 显示执行/保存差异，外部资源清理或明确残留，不再次盲创建 |
| DATA-03 | 流式接收后中断/重启 | 只展示已保存内容及不完整标记，不伪称全部完成或自动重发 |
| DATA-04 | 两编辑者同revision保存 | 第一成功，第二明确冲突，不能丢更新 |
| DATA-05 | 旧数据迁移中途失败 | 不清空原数据，不发布成功版本，恢复步骤可说明 |
| DATA-06 | 删除历史含用户文件引用 | 仅删除授权元数据/受管资源，用户原文件不受影响 |
| DATA-07 | 凭据保存失败或配置生效失败 | 不泄漏秘密，不伪报已保存/已生效，旧可用值可定位 |
| DATA-08 | 日志CLI覆盖同时保存INFO偏好 | 当前有效覆盖保持，重启无覆盖时读取已保存偏好 |

通过 [实施验证](08-implementation-validation.md) 的数据集与故障方案运行；上述是设计用例，不是本轮已运行代码结果。
