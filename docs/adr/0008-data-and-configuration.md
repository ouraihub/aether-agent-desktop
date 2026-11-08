# ADR 0008：SQLite 事务存储、版本化会话与配置生效边界

- 状态：已接受（技术选型与数据语义；§5 凭据后端及跨库要求部分由 ADR-0012 替代，实现验收另计）
- 记录日期：2026-11-08
- 决策编号：DEC-08
- 依据：`docs/design/detail/05-data-config.md`、`docs/design/05-infrastructure-platform.md`、`docs/INTERFACE_CONTRACTS.md`、ADR-0001/0004/0005/0007；本机 Pi/OMP 源码调查；SQLite 官方资料（版本历史、编译说明、事务/WAL、版权声明）。
- 关系：补充 ADR-0004 的 Session/执行边界及 ADR-0007 的授权持久化范围；不替代 DEC-05 的请求终态、DEC-07 的权限求值、DEC-11 的容量数值，也不把本记录变成已引入依赖或已完成实现。
- 变更记录（2026-11-08）：原决定要求系统 Secret Store、禁止明文后端并假定配置与秘密分属安全边界；用户本会话明确选择“照 OMP 存普通 SQLite”，由 [ADR-0012](0012-sqlite-credentials.md) 替代这些要求。以下 §5 为现行语义，保留普通配置引用、授权、不回显及不可用拒绝；不是把原决定追认为曾批准明文。

## 背景与问题

Aether 的 Workspace、Session、Message、ContentBlock、Attachment、ExecutionMarker、Configuration 和 `secret_ref` 需要在同一个本地 Store 中表达。创建 Session、保存消息与执行标记、更新配置及删除历史会跨多个实体；仅以文件追加或逐个 JSON 写入，无法让这些实体共享一次提交、进行 expected-revision 比较，也难以把迁移失败和部分写入报告为一个可恢复结果。与此同时，Provider/Runtime 的网络或进程副作用不属于本地数据库事务，不能把数据库锁持有到外部调用结束。

Pi 的 `SessionManager` 明确把会话存成带 `id`/`parentId` 树的 append-only JSONL（本机 `pi/packages/coding-agent/src/core/session-manager.ts:977-987`），版本迁移从 v1→v2→v3（同文件 `:286-347`），解析时跳过畸形行（`:354-367`）。这适合单会话日志和分支，但不能作为 Aether 多实体事务的直接存储合同。OMP 的 SQLite 工具对忙、WAL 恢复、文件身份和损坏保全有实际处理（`oh-my-pi/packages/utils/src/sqlite.ts:112-147,174-200,279-340`），可作为故障边界参考；它不是 Aether 已接入的代码。

配置参考也必须保留边界：Pi 设置管理器以全局/项目两层文件、锁、读-改-写保存（`pi/packages/coding-agent/src/core/settings-manager.ts:160-169,186-251`），并以项目覆盖递归合并全局值（`:142-147`）；OMP 的配置目录按用户级再项目级返回优先顺序（`oh-my-pi/packages/coding-agent/src/config.ts:77-155`）。这些是参考行为，不是 Aether 可直接复制的 API。

## 决策

### 1. 存储引擎与 C17 适配

1. Aether 的本地持久化 Store **选用 SQLite 3.54.1 作为当前基线版本**（SQLite 官方 release history 在 2026-10-16 列出 3.54.1）。工程必须锁定下载/源码校验值和构建选项；选型本身不代表 C 集成或平台验收，依赖落地以实际工程记录为准。
2. 选用 SQLite 的理由是单进程桌面应用所需的成熟事务、外键、约束、索引、CAS 查询和单文件部署，能在一次短事务内提交多个 Aether 实体。SQLite 官方说明其为 ANSI-C 实现，并给出 Unix/macOS 的 `configure && make`、Windows 的 `nmake /f Makefile.msc` 及 MSVC 编译路径；官方版权页声明 SQLite 核心代码和文档为 Public Domain。来源：<https://sqlite.org/chronology.html>、<https://www.sqlite.org/howtocompile.html>、<https://www.sqlite.org/copyright.html>。
3. 采用 SQLite C API 的窄 Infrastructure 适配层。第三方 `sqlite3.c/.h` 属外部代码，按供应链许可清单和独立告警规则编译；Aether 自研包装、Store 端口、错误映射和资源清理采用 ISO C17（ADR-0001）。Core、Backend API、Frontend 不包含 SQLite 头文件，不接收 `sqlite3*`、SQL 字符串或底层错误原文。
4. Store 只接受类型化变更集和绑定参数；表名、排序列和迁移步骤来自编译期白名单，禁止将用户输入拼接为 SQL。数据库路径由 Infrastructure/Platform 解析并做权限检查，不允许 UI 或 Runtime 直接打开 Store 文件。普通可扩展载荷和显式 JSONL 交换格式统一采用 DEC-10 选定的 yyjson 0.13.0（MIT）进行 UTF-8 JSON 编解码；核心字段仍使用 SQLite typed columns，不把 JSON 当作绕过 Schema 的任意字典。yyjson 的 allocator/write buffer 与非标准扩展开关须按 DEC-10 的 C17/MSVC 边界配置。
5. 连接所有权固定为 Store 实例内部：一个逻辑写入拥有者串行化短事务；读操作取得独立快照/DTO，不能把 SQLite 查询缓冲或连接借给调用方。实现可使用 SQLite WAL 以允许有限读写并发，但数据库仅放在 Aether 管理的本地应用数据目录，不放在不具备可靠锁语义的网络/同步目录；WAL、`-wal`、`-shm` sidecar 必须纳入备份、身份检查、迁移和损坏保全。不得把 WAL 当成跨进程无限并发承诺。

### 2. 版本化 Session、CAS 与保存/执行分离

1. `schema_meta` 保存独立的整数 `schema_version` 和迁移状态；每个 Workspace、Session、Message、ContentBlock、ConfigEntry 等可变聚合保存单调 `record_revision`。Store 的 `commit_changes(changes, expected_revision)` 只在期望 revision 仍匹配时提交并返回新 revision；不匹配返回稳定冲突业务码，不自动覆盖或重试。revision 是并发/保存版本，不宣称自动保留每次内容的完整历史；若产品需要分支历史，必须以显式 Session/Message 记录实现，而不能借用 SQLite WAL。
2. Session 持久化保存 Workspace 关联、Runtime 的诊断性旧关联/connection generation、外部 locator、模型/配置 revision、状态和执行关联；这些 Runtime 关联不代表可恢复的活实例，Runtime ID、进程句柄、取消令牌、能力报告和临时密钥永不写入可恢复实例状态。重启只恢复已保存事实，外部 locator 必须由 Adapter 再验证。未验证的初始化或在途执行恢复为非活动/需核实，不自动重新创建或重放副作用。
3. 需要可靠恢复的发送用例先在一个短事务中保存用户消息、执行意图和必要 revision，再释放数据库锁并调用 Provider。外部调用完成后另起短事务保存归一化结果、`execution_result` 和持久化状态。数据库事务不能包住网络/进程调用，也不能回滚已发生的外部动作；外部成功而结果保存失败时同时报告 execution-success/`save_failed`（或 `execution_unknown`），禁止显示为已回滚，禁止盲目重发。
4. 保存失败不发布“已保存”事件；已发生的外部事实与保存故障必须可独立查询。内存领域状态、Store 事实和 Backend 事件的先后按各用例合同交接，不能由 SQLite 提交自动替代请求终态。

### 3. 迁移、损坏与回滚

1. 打开流程先读取 `schema_version`，再取得实例迁移锁。高于当前支持版本的 Store **拒绝写入和破坏性打开**，返回版本不兼容错误；不删除、不清空、不降级写回。低版本逐版本迁移，每一步只做该版本所需变更，并在同一事务中更新版本元数据。
2. 迁移前必须通过 SQLite 在线 backup API，或在关闭 Store 后对已 checkpoint 且一致的快照，保留数据库及所有相关 sidecar 的可恢复备份；禁止对仍有活跃写入的数据库文件及 sidecar 分别复制并宣称一致快照。迁移事务提交前验证关键外键、唯一键、Session→Message/ContentBlock 关系、配置作用域与 `secret_ref` 引用。成功提交并完成校验后才向 Backend 发布 Store 可用。迁移任一步骤、校验、锁或备份失败，回滚未提交事务并保持原数据；无法证明原文件未变时从保全副本恢复，最终以安全错误拒绝开放，绝不自动新建空库覆盖用户数据。
3. 启动或读写发现 SQLite corruption/损坏时，先关闭失败句柄，按文件身份和实例锁保全原数据库及 `-wal`/`-shm`/`-journal`，记录不含路径/秘密的稳定诊断信息，Store 进入不可用状态。自动删除、静默重建和“跳过损坏行继续宣称完整”均禁止。只有用户明确执行恢复/新建操作并已保留原件，才可创建空 Store；恢复过程不得将秘密明文迁移到普通配置。
4. 迁移样本必须包含真实旧格式的 Session、消息/内容块、配置和凭据引用，并覆盖中途失败、重启、未知更高版本和 sidecar 保全。`quick_check` 或等价完整性检查只能证明数据库结构检查结果，不能替代业务关系验证。

### 4. 删除与附件生命周期

1. 删除命令必须声明目标是元数据、历史记录还是 Aether 受管附件，并在 Backend/Core 先检查权限、在途引用和保留规则。事务删除元数据和附件引用；外部用户工作区文件、用户提供的路径和 Runtime 资源不在自动删除范围。
2. 受管附件文件在数据库提交成功后再删除；文件删除不是数据库事务的一部分。删除失败把附件标成 `cleanup_pending`，保留安全诊断和可再次清理的引用；不得因文件系统失败假装数据库回滚，也不得因数据库回滚提前删除仍被引用的文件。清理操作幂等并受同一授权边界约束。
3. 查询附件只返回受控 `attachment_ref`/元数据，实际打开必须经过 Backend 授权和路径检查。不得把绝对路径、文件内容或凭据放进普通事件、日志或 UI DTO。

### 5. 配置优先级、生效和 `secret_ref`

1. 每个配置 Schema REQUIRED 定义类型、作用域、默认值、敏感性、允许覆盖源和生效时点。普通可继承配置的持久化有效值顺序固定为：**Session 显式覆盖 → Workspace → 应用持久化偏好 → 内置默认**；未声明可继承的键不参与合并。权限策略不因配置继承而继承。
2. 启动/调用临时覆盖不改写持久化值。对 Schema 明确允许的进程启动覆盖，顺序为 CLI → 允许的环境变量 → 上述持久化有效值；未声明的环境变量一律不参与。日志等级保留既有专门规则 CLI → ENV → persisted → INFO，不泛化为所有配置。
3. 保存流程为：校验与授权 → expected_revision CAS → 写普通值或 credentials 数据 → 提交持久化值 → 重算 effective value/source → 按 Schema 生效时点通知消费者。响应必须分别表达 `saved_revision`、`effective_source`、`apply_state`（`applied`、`pending` 或 `failed`）；保存成功但重连/运行时应用失败时，返回 saved + apply_failed/pending，旧实例的已生效状态仍可查询，不显示新配置“已生效”。
4. 敏感配置普通表只存不透明 `secret_ref` 和必要版本/状态，不存秘密明文、掩码伪值或可逆“临时备份”；引用的 credentials 数据按 ADR-0012 采用普通 SQLite 非加密 JSON。日志、错误、Query/UI DTO 和事件不回显秘密。`secret_ref` 不可读取、被删除、版本不兼容或凭据存储不可用时，必须报告 `AETHER_CONFIG_SECRET_UNAVAILABLE`（映射既有 `AETHER_STATUS_PERMISSION_DENIED`、`AETHER_STATUS_IO_ERROR` 或 `AETHER_STATUS_NOT_SUPPORTED` 的真实原因），**不得**用空字符串、默认值、旧掩码、旧缓存或环境变量静默替代。需要该秘密的新建/重连/外部调用被阻止并显示“已配置但不可用”；已运行实例不因无法再次读取而伪报新配置已生效，后续行为按 Runtime/Session 合同收尾。
5. `secret_ref` 是逻辑引用而非 OS 安全边界。credentials 与普通配置可在整合时定义为同一数据库/事务，不强制跨库；只有实际共用事务并完成集成才承诺原子提交。若实际分库，凭据写成功而配置提交失败时保留可清理引用，不删除仍被引用的记录；配置已提交而凭据不可用时记录不可用状态并阻止依赖操作。临时密钥不持久化，不自动读取外部凭据；明文数据库、WAL/journal 与备份风险、非取证删除边界见 ADR-0012。

## 替代方案

- **继续使用 JSONL/每实体 JSON 文件（Pi 风格）**：保留人类可读、追加和单会话分支的优势；但跨 Workspace/Session/Message/Config/Attachment 的原子提交、外键、CAS 和迁移回滚需自行构造，畸形行跳过会产生“可读但不完整”的风险。拒绝作为 Aether 主 Store；可作为显式导入/导出格式，导入须全量校验后一次提交。
- **文件 + 自研事务日志/事件溯源**：可定制恢复，但要维护日志截断、快照、索引、并发、崩溃恢复和安全删除，超出当前桌面产品需求。拒绝，且不把日志当业务恢复源。
- **客户端/服务器数据库（PostgreSQL 等）**：多客户端并发能力强，但引入服务部署、认证、网络断连和升级运维；当前 Aether 是本地桌面应用。拒绝作为首个本地 Store，未来若需求改变须新 ADR。
- **其他嵌入式数据库**：可重新评估，但在本轮没有比 SQLite 更强的事务、C17/三平台、供应链和部署证据。

## 影响与风险

**收益：** 多实体短事务、外键/唯一约束、revision-CAS、稳定分页和单文件本地部署可由成熟引擎承载；Core/Backend 只依赖窄 Store 端口，今后替换实现不改领域合同。配置期望值、保存事实、有效值和应用状态可独立呈现；附件删除和外部执行不再伪装为可回滚事务。

**风险与控制：** SQLite 是单写入者模型，写入拥有者、短事务、busy/锁错误、有界查询和容量数值必须在 DEC-11/实现中落实；WAL sidecar、网络目录和文件锁语义必须在 Windows/macOS/Linux 分别验证。损坏保全需要足够磁盘和可靠文件原子操作，恢复失败只能安全拒绝。第三方升级须保留 SQLite 3.54.1、yyjson 0.13.0 的来源、SHA-256、许可证及迁移样本。凭据不可用与配置已保存/未生效必须独立呈现，不能回退旧缓存。ADR-0012 已明确凭据为非加密 JSON，数据库、WAL/journal 和备份可能含秘密；OS 权限不提供同用户隔离，删除不保证取证擦除。备份保留量、普通配置整合事务和完整 SQL schema 仍须按实际实现定义，不再等待系统安全后端选型。

## 版本与升级策略

- **引擎版本：** 工程落地时固定 SQLite 3.54.1 的来源与校验值；后续仅在依赖评审后升级，记录 SQLite 引擎版本、编译选项和许可证。引擎升级不等于 Aether `schema_version` 迁移。
- **数据版本：** `schema_version` 从 1 起单调递增；每个迁移步骤有编号、前置/后置不变量、备份/回滚策略和旧样本。支持范围外的高版本在打开检查阶段拒绝写入和破坏性打开，不能降级覆盖。
- **应用/接口版本：** Store 端口返回独立的数据/对象 revision；事件和 DTO 版本遵循 INTERFACE_CONTRACTS，不把 SQLite 文件格式暴露给 Frontend。任何破坏字段语义、删除数据或改变 `secret_ref` 行为的升级必须新 ADR 并提供迁移。
- **导入导出：** JSONL 仅是显式受校验的交换格式，不是内部权威格式；导入失败全量回滚，导出不携带秘密明文。

## 实施与验证交接

### Python标准库语义试验

以下脚本已由父任务以`uv run --no-project python build/decision_data_smoke.py`实际运行通过，退出0；临时文件随后清理。它只证明所用Python sqlite3的revision-CAS和短事务回滚，高版本拒绝是该脚本的应用层逻辑，不是SQLite自动替Aether实现迁移检查。未验证选定3.54.1 C编译、三平台文件锁、旧格式迁移或安全存储，不把试验当DATA实现。

```python
import sqlite3

db = sqlite3.connect(":memory:")
db.execute("pragma foreign_keys = on")
db.execute("create table schema_meta (schema_version integer not null)")
db.execute("insert into schema_meta values (1)")
db.execute("create table session (id text primary key, revision integer not null, state text not null)")
db.execute("insert into session values ('s1', 1, 'idle')")

# CAS: stale expected revision must update zero rows.
changed = db.execute(
    "update session set state = ?, revision = revision + 1 where id = ? and revision = ?",
    ("active", "s1", 1),
).rowcount
assert changed == 1
stale = db.execute(
    "update session set state = ?, revision = revision + 1 where id = ? and revision = ?",
    ("closed", "s1", 1),
).rowcount
assert stale == 0

# A multi-entity write rolls back as one unit.
db.commit()  # publish CAS assertions before starting the explicit transaction
db.execute("begin")
db.execute("update session set state = ? where id = ?", ("closing", "s1"))
db.execute("create table message (id text primary key, session_id text not null references session(id))")
try:
    db.execute("insert into message values ('m1', 'missing-session')")
    db.commit()
    raise AssertionError("foreign-key failure was expected")
except sqlite3.IntegrityError:
    db.rollback()
assert db.execute("select state from session where id='s1'").fetchone()[0] == "active"

# A higher unknown schema version is rejected without destructive rewrite.
db.execute("update schema_meta set schema_version = 99")
version = db.execute("select schema_version from schema_meta").fetchone()[0]
assert version > 1
try:
    if version > 1:
        raise RuntimeError("unsupported schema version")
except RuntimeError:
    assert db.execute("select count(*) from session").fetchone()[0] == 1

print("DEC-08 SQLite semantic smoke: PASS (Python sqlite3 only; not C integration)")
```

### 实现前必须满足的门槛

1. 引入依赖前提交 CMake 目标、固定 SQLite 来源/哈希/许可证清单，以及 Windows/MSVC、macOS/Clang、Linux/GCC 的真实编译和运行证据；本 ADR 的官方命令只是可行性资料，不是三平台验证。
2. 交付 Store 端口和独立 DTO/错误映射，覆盖 DATA-01 至 DATA-08、INT-09、旧版本真实样本、损坏保全、WAL sidecar、权限/路径和附件清理；不得以 Python smoke 或 Pi/OMP 行为冒充 C 集成。
3. 写入/查询/增量/备份容量已由DEC-11/ADR-0006给出；凭据后端按ADR-0012由ENG-07-C交付普通SQLite JSON及不可用映射，不要求系统Secret Store。DEC-08是设计决策完成，DATA-01至04仍须真实实现；凭据切片不替代ENG-07网络/HTTP或三平台验证。
