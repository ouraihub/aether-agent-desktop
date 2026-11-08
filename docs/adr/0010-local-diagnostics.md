# ADR 0010：本地结构化诊断与显式追踪身份

- 状态：已接受（DEC-10设计决策；不是日志功能已实现或三平台验证声明）
- 日期：2026-11-08
- 依据：用户要求直接参考本机Pi/OMP/Autorail已清楚的实现收敛；接口契约§13、工程规范§4。
- 补充ADR 0003；将其刻意未定的身份、编码、存储、过滤和查询边界收敛，不改变业务事件/日志分离。不引入OpenTelemetry、OTLP或Rust/JS tracing运行时。

## 1. 参考事实与取舍

| 实际源码 | 观察与采用/不采用 |
| --- | --- |
| `D:/workspace-ai/pi/packages/telemetry/src/memory.ts:93-95,313-335` | UUID recorder scope加局部span序号，显式carrier副本；证明跨scope身份必须有命名空间，不能把进程计数器称为全局trace。Aether采用固定宽度值，不复制JS对象/框架。 |
| `D:/workspace-ai/pi/packages/chord/src/telemetry-sqlite/index.ts:95-138` | settled数据flush与close、写入失败状态分离；Aether也明确flush/可查询窗口，但不另建遥测SQLite库。 |
| `D:/workspace-ai/oh-my-pi/packages/utils/src/logger.ts:370-415,418-449,459-479,520-534` | 上下文、JSON记录、PID文件隔离、1秒/64KiB批写、10MiB×5轮转与过滤；采用隔离及批写思路，不复制AsyncLocalStorage、默认全部等级和静默输出失败。 |
| `D:/workspace-ai/oh-my-pi/packages/utils/src/logger/rotating-file.ts:132-175,178-223` | 部分写入循环、轮转先关闭旧句柄、有限文件清理；Aether还必须暴露失败及严格总量，不把best-effort删除称为硬上限。 |
| `D:/workspace-ai/autorail/crates/observability/src/lib.rs:21-38,62-106` | 14个每日文件、环境覆盖pin及运行时reload、writer guard；采用覆盖来源与末尾flush，不采用仅Info/Debug或没有字节总限的按天保留。 |
| `D:/workspace-ai/autorail/crates/observability/src/op.rs:13-30` | 原子进程内op序号和单调耗时，不是128bit跨进程trace；不复制为Aether全局身份。 |
| `D:/workspace-ai/autorail/crates/observability/src/tail.rs:10-11,51-70` | 256KiB/2000行有界快照读取及忽略未完整尾行；采用有界扫描，不把最新文件tail冒充完整trace检索。 |
| `src/foundation/id.c:6-31`、`include/aether/foundation/id.h:11-16` | 当前AetherId为从零启动的64bit原子计数器；保留其现有进程实例范围，禁止转换/补零后当作128bit trace。 |

## 2. 身份和失败

- `trace_id`为16字节非零随机值，输出严格32位小写十六进制；`span_id`/`parent_span_id`为8字节非零随机值，输出严格16位小写十六进制。根父级输出JSON `null`。不加UUID连字符或使用主机字节序整数转储。TraceContext按值复制这些字节，不拥有业务对象；无上下文是显式状态，不以零ID伪造可查询记录。
- Platform提供受检的系统随机字节：Windows `BCryptGenRandom(NULL, ..., BCRYPT_USE_SYSTEM_PREFERRED_RNG)`并链接系统Bcrypt；Linux `getrandom`循环处理EINTR/短读；macOS `SecRandomCopyBytes(kSecRandomDefault, ...)`检查结果。不用时间、PID、rand()或Foundation计数器降级。非零约束至多重取一次，仍为零或OS失败即失败；这不是无限重试。
- root生成trace和root span，child保留trace并独立生成span；所有者先取得身份再发布上下文。随机身份在实例间/重启后具有概率唯一性，不是数学全局唯一或授权token：128bit trace的生日碰撞概率约为n²/2¹²⁹；64bit span风险按同一trace内span数计。当前不接纳第三方父级；受信的未来跨进程传播须另外批准协议与span碰撞处置，不能仅因宽度类似就宣称W3C Trace Context兼容。
- 身份生成失败不改变业务结果：该root标为diagnostic unavailable，禁止借用别的请求trace；child失败保留可关联的父上下文，但不伪造子span/终态。健康状态累加`id_generation_failed`，错误DTO返回明确诊断不可用原因；系统记录允许无追踪身份。下次独立操作可再次尝试随机源，不在当前操作中反复阻塞UI。
- 非法内部上下文拒绝记录为操作边界并计数；查询输入格式错误返回InvalidArgument，不自动修复大小写/长度。外来ID仅在已批准字段中存为远端关联，不覆盖本地身份、不授予访问权。PID只用于诊断，另生成128bit随机`process_instance_id`用于文件隔离；其失败时禁用文件输出并暴露健康故障，不以PID重用制造文件碰撞。

## 3. 编码与依赖

选择UTF-8、无BOM、每记录一行JSON加LF，`schema_version=1`；公共字段语义仍唯一维护在接口契约§13.2。时间为UTC RFC3339固定毫秒`...SS.sssZ`，duration为非负有限毫秒数（单调时钟）；等级为大写六级字符串，ID/所有64bit不透明业务身份用字符串，计数为无符号整数，布尔/null按JSON类型。保留字段只能由统一封装设置，调用方不能覆盖等级/身份/时间；未知schema版本显示unsupported，不默默套用V1。

编码/解析统一选用[yyjson 0.13.0](https://github.com/ibireme/yyjson/releases/tag/0.13.0)，固定commit `6447536015f3d600f3d65323b10976103b337ca7`（官方annotated tag所指），[MIT许可证](https://github.com/ibireme/yyjson/blob/0.13.0/LICENSE)须随分发保留；这是依赖选择，不是已引入源码。其C89兼容、MSVC条件支持、UTF-8严格校验、自定义allocator及无分配write_buf适合C17边界。实现引入时记录源码校验摘要，不跟随master；不得再为日志引第二个JSON库。关闭非标准JSON、文件便捷API及无关JSON Patch/Pointer，保持UTF-8验证，reader/writer depth limit=4。producer在filter之后用固定arena和写出缓冲区编码一次，队列持有编码后字节，writer只追加/批写，不重新解析编码。arena耗尽显式丢弃，不回退默认堆分配。不得手写通用JSON序列化器。

最多32个顶层字段；稳定标识词（module/component/layer/operation/function）最多128 UTF-8字节，message最多1024字节、其他可选文本最多512字节（均不含终止符）。超长可选文本仅在UTF-8边界截断并置`truncated=true`；必填身份/标识词超限、重复键、非法UTF-8、NaN/Inf或编码后超过16KiB整条拒绝并计数，不能截断JSON字节。原始控制字符全部JSON转义，正文、凭据、原始路径/URL按工程规范禁止，不因TRACE或“仅本地”例外。未知业务字段不自行展开任意对象/堆栈。

## 4. 目录、安全与多实例

Platform选择每用户本地状态目录：Windows `%LOCALAPPDATA%/Aether/logs`；Linux `${XDG_STATE_HOME:-$HOME/.local/state}/aether/logs`（XDG变量须绝对路径）；macOS `~/Library/Logs/Aether`。不写工作区、共享临时目录或安装目录。目录POSIX 0700、文件0600；Windows受保护DACL仅当前用户及SYSTEM（不继承宽泛Users/Everyone读取权）。已有目录必须验证所有者、权限及非符号链接/重解析点；通过受控目录句柄创建文件、拒绝链接跳转，无法保证时禁用文件输出并返回故障，不靠字符串前缀承诺安全。拥有该用户权限的恶意程序仍可读/改日志，不是防同用户攻击的审计设施。

每实例`<process_instance_id>/aether.<UTC日期>.<segment>.jsonl`，排他创建，不共享活动文件。实例持有OS锁，清理只删除管理器登记且已验证的普通日志文件；清理其他实例前须非阻塞取得该实例锁，不能按PID存活或mtime独断已死。根目录配额锁串行分配文件容量，锁竞争不等待UI。权限故障禁止回落到公开目录/控制台；受控启动错误提示可用固定安全文案，不输出敏感路径。

## 5. 初始容量与解释（同步DEC-11）

所有MiB/KiB均为2的幂；下列为批准的工程起点，不是实测吞吐/SLA。

| 资源 | 初值/严格行为 | 预算理由 |
| --- | --- | --- |
| 编码后单条（含LF） | 16KiB | 沿用ADR0006；不存正文，足够边界元数据。 |
| 每进程记录队列 | 1024条且4MiB编码字节；另计最多64KiB槽位/分配元数据，其中128条/512KiB只给WARN以上或任意终结/unfinished | 普通记录最多896条/3.5MiB；重要记录可用剩余全部容量。单FIFO保序，禁止锁竞争时无限自旋；try-submit争用失败也计drop。满则丢新记录并按level/priority计数；关键记录也非零丢失保证。 |
| 每producer构造空间 | 16KiB输出+32KiB JSON arena，线程复用 | 先filter再触碰；不复制/清零整块未用空间。N个记录线程预算48KiB×N。 |
| writer batch | 64KiB或1秒先到；重要记录唤醒writer尽快flush | 复用OMP经验；不在生产者同步写盘。 |
| 活动文件/实例总量 | 单文件8MiB，最多8个文件（含活动）且64MiB | 写入前预测轮转，不像参考实现允许多一条；UTC日期也轮转，小文件数也有界。 |
| 每用户所有实例总量 | 256MiB，包含尚未删除/待删除文件；最多64个实例目录 | 独立实例不能乘出无限磁盘量；manager预留完整8MiB segment额度，锁内核算。只清理未锁定旧文件；无空间/无法清理则停文件输出，不越过配额。 |
| 保留 | 最长14×24小时，自文件创建UTC时间计；容量压力可提前删除最旧关闭文件 | 取Autorail两周诊断窗口，但不承诺14天必存；启动/轮转/每小时清理，活动文件不删。到期活动文件先轮转；墙钟回退时字节/文件双限仍生效。 |
| Query页 | 默认100条，最大500条且JSON DTO编码≤1MiB | 先到即分页；每记录仍≤16KiB，不因500条暗许8MiB响应。 |
| Query工作量/并发 | 每调用扫描≤8MiB或100ms先到，最多2个在途扫描/进程 | 非UI线程；命中少也返回continuation+scan_incomplete，不谎称不存在。超限拒绝Busy，不建无限等待队列。 |
| Query状态 | 每调用192KiB解析/读取scratch，响应≤1MiB；最多16个游标，每个≤1KiB，5分钟失效 | 查询两路≤2.375MiB，cursor≤16KiB；与记录队列4MiB、writer64KiB及线程scratch分别核算，不遗漏DTO。 |
| 关闭 | 最后生产者停止后writer drain/flush截止1秒 | 超时报告未排空/close pending；OS写入无法强制取消时资源仍归owner，不能释放活跃writer或无限延长UI关闭。 |

根配额锁内不读整个日志正文，仅有限目录元数据；新实例超目录数时先清理未锁定且已空目录，不能清则文件输出不可用。删除失败计入实际占用，停止轮转写入并暴露retention_failed，不能继续无限创建文件。文件本身是唯一持久日志副本，不额外压缩/建索引/缓存全历史。内存预算不含OS page cache和平台线程栈；这些由DEC-11分列，不伪称全进程RSS上限。

## 6. 过滤、故障与关闭

六级阈值、默认INFO；命令行`--log-level` > 环境`AETHER_LOG_LEVEL` > 已保存用户偏好 > INFO。仅接受不区分ASCII大小写的六个完整等级词，不接受模块过滤表达式。非法CLI使启动参数校验失败；非法环境明确忽略该覆盖并在健康状态提示、继续低优先级；非法持久值以INFO运行并报告配置损坏，不自动覆盖磁盘。设置API非法值拒绝。运行时设置保存后重算有效值，CLI/ENV覆盖不落盘；保存失败保留旧偏好/有效值，成功但被覆盖须返回saved/effective/source。不另设偷偷高于CLI的会话覆盖。第三方输出不能通过开TRACE转储正文。

健康状态为固定快照：running/degraded/closing/closed、采集level/source和generation、按等级/优先级drop计数、filtered计数、编码/随机源/写盘/轮转/清理错误计数、最后安全错误类别和失败时间、最后成功flush及待写条数；计数饱和而不回绕，持久日志不可用时仍可查询。健康信息不是另一条递归日志，UI不能用全局drop精确断言某trace丢几条。

写入处理短写，持久错误后关闭/封存可能半条的segment、标记degraded并丢弃后续文件记录计数；仅显式用户重试或下一次启动重新打开，不后台无限重试。重试先验证权限/配额，再新建segment，不接在损坏半行后面。队列溢出只丢本次，不改业务结果；不对stderr同步回落。flush指排出用户态缓冲到OS，不逐条fsync/FlushFileBuffers，也不承诺断电/kill/崩溃前最后记录；正常关闭也不是数据库事务。

## 7. 查询授权与缺失语义

仅本机桌面受信Backend Client可请求诊断；Runtime/MCP/插件没有该能力。应用级记录须应用诊断权限；限定Workspace调用只能看到授权workspace_id，缺workspace_id记录及其他workspace不可见。初期桌面本地用户有应用诊断权限，但不能因此开放远端无鉴权接口。授权在每页重查，不以trace/游标为凭证，不返回文件路径或原始畸形记录。

受控文件目录扫描；默认当前实例，可显式请求同用户历史实例，仍检查范围。按trace精确及operation/layer/component/level过滤；不接任意路径、SQL、正则或shell。按实例/segment/字节偏移给出稳定物理顺序，显示墙钟但不虚构跨进程时序。游标是Backend持有的随机不可猜句柄，绑定授权主体、范围、过滤器、文件代次与快照长度；过期、轮转删除或授权变化明确失效，不重定向到别的文件。每次扫描到预算即返回游标，直至有限集合耗尽，因而不是只搜最新8MiB。

活动文件未完成尾行忽略并提示pending；封存文件损坏行或未知schema跳过、统计并提示incomplete，不把畸形原文返回UI。返回采集级别/generation、保留窗口、扫描是否完成、已知损坏/丢弃/输出故障；过滤历史不追补，不把空页或全局健康正常推断为某trace完整。常规查询/健康读取不逐次写日志；故障仅更新非递归健康计数。query/page本身受取消与Backend关闭合同约束。

## 8. 实施门槛与可复核路径

本轮已运行OMP真实logger探针的context与WARN过滤场景，结果见[执行记录](../tasks/EXECUTION_LOG.md)：交错上下文隔离、显式覆盖/shared清除和WARN过滤通过。尚无Aether logger；Windows/macOS/Linux随机源、权限、轮转、锁配额和故障注入仍须ENG实现验证，现有clock/path不等于随机/ACL API。

以下为可复核的OMP现成fixture入口（必须临时目录，不在用户真实日志目录运行）：

```powershell
$root = Join-Path ([IO.Path]::GetTempPath()) ('aether-omp-log-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root, "$root/primary", "$root/secondary" | Out-Null
$env:OMP_LOG_LEVEL = 'debug'
bun D:/workspace-ai/oh-my-pi/packages/utils/test/fixtures/logger-contract-probe.ts context "$root/primary" "$root/secondary" "$root/result.json"
Get-Content "$root/primary/*.log"
Get-Content "$root/result.json"
```

执行者须保存/恢复原`OMP_LOG_LEVEL`并清理临时目录。该fixture直接调用OMP logger，在交错await/context/withoutContext下输出记录；只证明OMP行为，不证明Aether span、三平台权限或容量。父可另用同fixture的`matrix`场景及`OMP_LOG_LEVEL=warn`观察级别过滤；不要把该四级logger称为Aether六级验收。Aether实现验收沿详细设计OBS-01–09及工程规范§4.8，另加随机源失败/ID零值、JSON arena耗尽/控制字符、双实例配额锁、ACL拒绝、删除失败、稀疏trace跨扫描窗口与游标授权撤销。

## 9. 正式契约同步点

接口契约§13、工程规范§4和DEC-11已同步身份、编码、过滤/查询授权及容量入口。详细设计保留算法，公共字段语义唯一在接口规范；ADR0003保留历史未定事实，由本ADR补充，不重写历史。后续依赖引入与真实Aether链路仍须独立验收。
