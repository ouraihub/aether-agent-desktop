# 07 日志与诊断共享能力详细设计

## 1. 范围与设计状态

日志是整个Aether的一项横切能力。本文件把现有 [追踪契约](../../INTERFACE_CONTRACTS.md) §13 和 [日志规范](../../CODING_STANDARDS.md) §4转成实施算法。[ADR 0010](../../adr/0010-local-diagnostics.md)已收敛DEC-10：系统随机身份、UTF-8 JSON Lines/yyjson、私有目录、固定容量、过滤与查询授权；这些是设计决定，不是已有logger或三平台行为验证。

与 [跨层基线](00-contract-baseline.md)、[Backend](02-backend.md)、[平台执行](06-platform-execution.md) 协作；记录仅观察业务结果，不裁决请求状态，不从日志重建业务消息。

## 2. 对象与内存模型

| 对象 | 推荐字段/职责 | 生命周期 |
| --- | --- | --- |
| TraceContext | trace_id、span_id及父关联；只读值 | root生成，child继承trace，跨队列复制 |
| Operation | Context、静态operation/layer、单调开始、ended标记 | 同步作用域或任务拥有；不能按值复制活动操作状态 |
| LogHandle | Logger端口、有效等级和必要实现引用 | 组合根持有，晚于全部生产者销毁 |
| FieldView | 字段类型、只读数据/长度、敏感策略 | 只在提交调用内借用 |
| OwnedRecord | 元数据与允许字段的有界副本 | 入队成功归writer，失败仍由提交路径清理 |
| WriterState | 有界队列、当前文件、轮转状态、关闭状态 | 单写入拥有者，锁/执行器由选型决定 |
| LogHealth | writer状态、丢弃计数、输出失败类别、采集状态 | 非递归可查询快照，不保存无界错误列表 |

trace_id为16字节、span_id/parent_span_id为8字节非零随机值，仅输出时转换32/16位小写hex，根父级为null。Platform使用Windows BCryptGenRandom、Linux getrandom、macOS SecRandomCopyBytes的受检结果；不使用Foundation进程内AetherId替代。生成失败暴露diagnostic unavailable，禁止借用其他请求身份；Context只关联身份，不能包含Session指针或全局current trace。记录含动态字段时须持有有界副本，不保存栈字符串。详细失败/碰撞边界见ADR 0010 §2。

## 3. operation算法

begin取得/派生Context、记录单调开始、标记active，并按操作等级尝试entry。filter只影响输出，不使Operation本身缺失；否则运行时切级别后会失去关联。

finish由业务状态拥有者提供终态，先防重复终结，再计算耗时并尝试对应事件；不因为日志失败回滚业务状态。同步用明确清理出口，异步等实际任务完成，不在submit返回时finish整个任务。

dispose只检测遗漏：active时产生一次unfinished诊断并关闭本地操作记录，不虚构业务result；已结束时不再记录。C没有Drop，必须在所有已取得资源的退出路径显式调用；崩溃无法保证执行。

取消动作与目标请求可以是不同trace；目标任务保持原trace，相关ID表达关联。请求已TimedOut但清理尚未结束时保留独立cleanup子操作，不二次修改父业务终态。

## 4. 记录管线与性能

1. 读取一致的有效级别快照，不满足阈值时立即跳过昂贵字段构造。
2. 提取允许字段，验证类型/长度，排除秘密、正文、原始URL和未评估路径/哈希。
3. 最多32个顶层字段，固定arena构造V1记录，UTF-8 JSON Lines（无BOM，LF）；统一yyjson 0.13.0严格编码/解析，不手写JSON框架。module/component/layer/operation/function最多128字节、message 1024、其他可选文本512。仅可选文本可在UTF-8边界截断并标记truncated；必填标识非法、重复键、非有限数、非法UTF-8或整条含LF超过16KiB则拒绝并计数。
4. 将编码后字节复制入单FIFO有界队列：1024条且4MiB，另计≤64KiB队列元数据；其中128条/512KiB预留WARN以上及所有终结/unfinished，普通记录最多896条/3.5MiB。满或try-submit争用失败丢新记录并按等级/优先级计数；释放本次资源，不无限重试/自旋/扩容，不回退为UI同步写盘，重要记录也不承诺零丢失。
5. writer只追加已编码字节、批写和轮转，不重复编码/解析；完成后释放队列槽位，文件错误更新LogHealth，不递归调用自身logger。

采用简单有界队列及单writer，避免手写无锁环形缓冲。producer复用16KiB编码区+32KiB JSON arena，filter前不构造字段，不回退无限堆分配；writer持64KiB批缓冲，1秒或64KiB先到即flush，重要记录唤醒writer尽快flush。JSON保留键由封装写入，不接受调用方覆盖身份/时间/等级，UTC RFC3339毫秒时间用于展示，单调毫秒用于duration；schema/version和字段语义同步正式契约§13.2。

高频Delta不逐token记录，汇总字节/次数/状态；采用采样/限频时暴露不完整。多实例文件以随机process_instance_id隔离；不跨实例抢同一活动文件，不将PID重用视为唯一身份。不引入通用tracing库或远端采集后端。

## 5. 六级和配置

六级阈值及默认INFO沿用工程规范；确定`--log-level` > `AETHER_LOG_LEVEL` > persisted > INFO，接受不区分ASCII大小写的完整等级词，不支持任意filter表达式。非法CLI拒绝启动参数，非法ENV显式忽略该覆盖并暴露健康警告，非法持久值按INFO运行且提示配置损坏，不自动覆盖磁盘；设置API非法输入拒绝。

SetLogPreference：验证等级 → 保存偏好 → 重算覆盖后的有效等级 → 通知logger → 返回saved/effective/source。保存失败保留旧偏好及有效值；临时覆盖不持久化，覆盖存在时保存成功但effective不变须明确显示。不另加高于CLI的隐式运行时覆盖；Backend配置服务拥有偏好/来源，LogHandle只接收有效值，不内置产品数据库。

每条记录按生成时有效阈值决定是否采集；查看器level筛选是独立Query条件。INFO时被丢弃的DEBUG不因展开或重启恢复，切级别中途造成entry缺失应标明采集条件。

## 6. 输出文件与关闭

路径为Platform每用户本地目录：Windows `%LOCALAPPDATA%/Aether/logs`、Linux `${XDG_STATE_HOME:-$HOME/.local/state}/aether/logs`、macOS `~/Library/Logs/Aether`。POSIX目录0700/文件0600；Windows受保护DACL仅当前用户及SYSTEM。验证所有者/权限，通过受控目录句柄拒绝符号链接/重解析点；不能保证时禁用文件输出，不回落共享目录或stderr。不是防同用户恶意进程的安全审计设施。

实例目录使用128bit随机process_instance_id并持OS实例锁；文件为`aether.<UTC日期>.<segment>.jsonl`。单文件8MiB（写前预测轮转）、每实例8文件且64MiB、每用户256MiB且64实例目录。根配额锁内预留完整8MiB segment额度；清理其他实例必须先非阻塞取得其锁，只删除已登记且验证为普通文件的旧segment。不能清理/删除失败不再创建文件、不超过配额，健康状态报告retention_failed；不能凭PID或mtime猜测其他实例已退出。

最长保留14×24小时，自文件创建UTC计；容量压力可提前删最旧关闭文件。启动/轮转/每小时清理，活动文件到期先轮转，不删别人的活动文件。墙钟回退仍受字节及文件数限制。轮转先停止旧文件追加并关闭句柄，再受控建新文件；失败进入degraded，不假称仍能持久化。

关闭停止所有生产者后，writer排空/flush/close截止1秒；超时返回未排空及close pending，不释放仍运行writer的资源，不让UI同步等待。flush仅到OS缓存，无逐条fsync/FlushFileBuffers保证。FATAL不自动abort，不承诺异常退出或断电最后记录落盘。

磁盘满、权限错误、部分写失败等更新固定非递归LogHealth：状态、有效level/source/generation、按等级/优先级drop及filtered计数、随机/编码/写入/轮转/清理故障计数、最后安全错误类别/时间、最后成功flush和待写数。计数饱和不回绕。写入短写循环处理，持久失败封存可能半行segment并停止文件输出；仅显式重试或下次启动重新验证后建新segment，不后台无限重试。不覆盖业务错误，不递归调用logger。

## 7. Backend查询与UI交接

QueryTrace输入为严格32位小写hex trace及可选operation/layer/component/level、实例/Workspace范围、页上限与不透明游标。默认当前实例，可显式选同用户历史实例。Backend每页先授权：本机受信桌面Client可有应用诊断权限，限定Workspace客户端只读其授权workspace_id记录，不能看缺workspace_id或其他Workspace；Runtime/MCP/插件无此能力。trace和游标不是权限，不提供远端无鉴权入口。

采用非UI线程有界扫描，不引索引数据库：默认100条、最多500条且响应JSON≤1MiB；每次最多8MiB或100ms先到即返回continuation/scan_incomplete，可继续直至有限文件快照耗尽，不只搜最新文件。最多2个在途扫描，超限Busy、不建无限等待；每调用192KiB scratch及≤1MiB响应，16个游标每个≤1KiB且5分钟过期。响应记录不足但扫描预算到达不能返回“未找到”。

游标为Backend持有的随机句柄，绑定主体/权限范围、过滤条件、实例/文件代次、快照长度及偏移；逐页重授权，过期/删除/授权变化明确失效。按实例/segment/偏移排序，墙钟展示不承诺跨进程总序。扫描只在受控文件集合中进行，不接受任意路径/SQL/Shell/正则。活动文件未写完尾行略过并提示pending；封存损坏行/未知schema跳过并提示incomplete，禁止原样回显畸形记录。

DiagnosticPage DTO包含记录、游标、扫描是否完成、采集条件及保留/健康信息；不能从全局drop推断特定trace损失条数，不造父span，不把空页认作业务成功。常规查询/健康读取不写递归诊断；异常只更新有界健康计数。Frontend只展示并复制ID，不读文件，页面关闭按Backend取消/close-complete清理。数值和内存预算解释见ADR 0010 §5，DEC-11同步同一基线。

## 8. 文件职责和验收

建议实际实现按trace_context、operation、log_record、log_writer、log_file、diagnostic_query职责拆分，不提前创建空文件。公共基础接口不依赖产品UI/业务实体，具体文件输出只在Infrastructure。

| 用例 | Given / When | Then |
| --- | --- | --- |
| OBS-01 | DEBUG下真实请求跨服务和Adapter | 返回trace可检索父子边界、入口/出口/耗时 |
| OBS-02 | 并发任务交错且提交栈结束 | trace不串，字段没有悬空引用 |
| OBS-03 | finish失败后重复finish成功 | 不出现矛盾终态，不改原业务结果 |
| OBS-04 | 任务dispose未finish且重复dispose | 仅一次unfinished，无伪result |
| OBS-05 | 关闭DEBUG，字段构造有可控计数 | 构造未执行、无相应可避免分配，不只断言没输出 |
| OBS-06 | 合成秘密/正文/换行走完整日志和查询 | 文件与UI均无泄漏、无额外伪造记录 |
| OBS-07 | 队列满/文件不可写/关闭中提交 | 有界、健康状态可见、不递归、不阻塞UI |
| OBS-08 | 保存级别重启并施加不同覆盖 | 阈值、偏好、来源和实际输出一致 |
| OBS-09 | 分页中发生轮转/损坏/权限变化 | 明确失效/不完整/拒绝，不能越界或假完整 |

上述是Aether后续验收，不是本轮实现。实际OMP logger临时fixture的context与WARN过滤场景已运行通过，见执行记录；只证明OMP，不证明Aether span/轮转/权限/容量。Aether仍须验证随机源失败、arena耗尽、多实例配额/权限、删除失败、稀疏trace扫描及游标授权撤销。
