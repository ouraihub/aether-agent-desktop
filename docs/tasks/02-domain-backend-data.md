# 02 领域、Backend与数据任务

执行规则见 [总看板](README.md)。设计入口：[跨层基线](../design/detail/00-contract-baseline.md)、[Backend](../design/detail/02-backend.md)、[Core](../design/detail/03-core.md)、[数据配置](../design/detail/05-data-config.md)。CORE/BE/DATA用例引用各自文件。

| ID | 任务 | 状态 | 硬依赖 | 交付物与验收条件 |
| --- | --- | --- | --- | --- |
| APP-01 | Core类型化端口与Runtime Registry | 已完成 | ENG-03, DEC-06 | 不透明Registry/RuntimeRef、类型化Provider/Runtime身份、能力快照、generation防旧事实、共享引用和两阶段注销；MSVC9/9、Windows/Linux无UI真实链路、ASan/UBSan及分配失败回滚通过；无Runtime IO或空Adapter |
| APP-02 | Workspace与Agent配置领域 | 已完成 | APP-01, DEC-07 | 新增独立内存Workspace/AgentConfig注册表、revision/CAS更新、关闭状态、独立快照和容量边界；不接Store/Runtime/Session/权限执行，Windows行为测试通过 |
| APP-03 | Session状态拥有者 | 已完成 | APP-02, DEC-05 | 新增独立Session内存状态拥有者，绑定Workspace/AgentConfig/Runtime generation，Created/Idle/Active/Interrupted/RuntimeUnavailable/Closing/Closed转换，单回合限制和关闭清理；Windows行为测试通过，不含Message/Store/Backend请求状态 |
| APP-04 | Message与结构化增量 | 已完成 | APP-03, DEC-11 | V2六类结构块、Session/Runtime代次及块身份、精确cursor重放/gap、CAS replace、块finalize与真实message_end分离、Tool调用/结果关联、独立快照/引用释放及双限事务复制；Windows20/20、相关ASan/UBSan3/3与实际归并通过；无持久化/真实Runtime Adapter |
| APP-05 | Permission策略与授权范围 | 进行中 | APP-02, DEC-07 | 已有Workspace模式与有限批准记录；仍须正式合同的规则优先级、会话/执行者/多目标/目录范围、单次执行消费、撤销/关闭/到期复核及Store/执行端联验；当前实现不得作为生产执行授权门 |
| APP-05-A | 权限模式与规则求值 | 已完成 | APP-02, DEC-07 | V2可信关系、四种模式、Deny/Ask/Allow/Inherit显式父范围无环、字段复制与额度；Windows23/23及权限ASan/UBSan4/4通过 |
| APP-05-B | 批准记忆与执行前消费 | 已完成 | APP-02, DEC-07 | 有界可查询等待、单次参数/请求/代次/policy绑定、单调过期/撤销/关闭、会话工具/目标/一层目录及多目标全覆盖；实际文件写入只执行一次，重放/新Deny无副作用；父APP-05仍待C |
| APP-05-C | 持久批准与真实执行端联验 | 未开始 | APP-05-A, APP-05-B, DATA-01, ENG-06 | 将明确记住的持久目标批准存入受控Store，重启恢复/清除及真实Process拒绝后无副作用；父APP-05须本项完成才完成 |
| APP-06 | DTO与inproc类型化客户端合同 | 进行中 | APP-01, ENG-08, DEC-05 | 已有同步V1请求记录客户端与行为测试；完整Command/Query/Event、独立RequestContext及跨栈数据生命周期验收仍未完成 |
| APP-06-A | 独立上下文与类型化领域查询 | 已完成 | APP-01, ENG-08, DEC-05 | Backend实际只读Workspace/Session/Runtime/Message/Block查询，独立Context/版本/Workspace授权/安全错误与owned DTO；Windows25/25、相关消毒器及实际销毁后DTO有效验证通过；异步Command/Event仍归B与父任务 |
| APP-06-B | Command/Event与异步Client完整合同 | 未开始 | APP-06-A, ENG-05 | 类型化Command受理/QueryRequest/事件Schema及Client交接契约，连通实际后端处理入口；APP-07/08负责请求/订阅具体状态机，父APP-06保留聚合验收 |
| APP-06-Q | 有界异步Query Client | 已完成 | APP-06-A, ENG-05 | 单状态worker执行实际查询、Context复制、计数/响应额度预留、ID/receiver_generation、取消/关闭及owned结果转移；Windows28/28、三组异步消毒器及实际后台查询通过；不替代业务Command/事件订阅 |
| APP-06-S | 类型化本地状态命令交接 | 已完成 | APP-06-A, APP-06-Q, APP-02, APP-03 | Workspace rename/Session request-close，同后台状态域写授权/范围/CAS、owned输入、冲突/取消顺序；Windows30/30、相关消毒器5/5及实际改名/Closing通过；无持久保存/Runtime关闭或完整业务Command承诺 |
| FIX-PERM-01 | Permission输入与关闭边界修复 | 已完成 | APP-02, DEC-07 | StringView按长度精确匹配并复制持有；非法枚举/Ask答复拒绝，关闭Workspace及旧policy答复拒绝；失败输出Deny，容量乘法校验及满载回归通过；Windows15/15、WSL ASan/UBSan及真实复现通过；APP-05完整执行授权仍未完成 |
| APP-07 | RequestRecord受理/调度/终态 | 未开始 | APP-06, ENG-05, DEC-11, ENG-12 | 本地记录基础见ENG-12；本项仍须业务输入副本/额度后受理、执行前重查、deadline/取消竞争、QueryRequest；BE-01至04，单终态及迟到清理明确，不以同步记录表完成代替 |
| APP-07-L | 本地状态请求生命周期 | 已完成 | APP-06-A, APP-06-Q, APP-06-S, ENG-05, DEC-11, ENG-12 | 原子受理/副本/128保留记录、Accepted/Running/终态、单调deadline与取消先后、一次完成事实/回收保护；Windows37/37、相关消毒器2/2及真实请求调用链通过；父APP-07仍保留Runtime/权限/保存全部业务联验 |
| APP-08 | OpenView与事件订阅收尾 | 未开始 | APP-06, ENG-05, DEC-11 | 原子快照/增量基线、revision/sequence、健康失效、异步退订；BE-06/07、INT-05/06无UI可控竞态，满队列不静默假完整 |
| APP-08-L | 本地状态OpenView与事件闭环 | 已完成 | APP-06-A, APP-06-Q, APP-06-S, ENG-05, DEC-11 | Workspace/Session原子快照订阅、真实状态事件、有界非UI回调/ResyncRequired及退订close-complete；Windows33/33、相关消毒器4/4与实际命令事件链通过；父APP-08保留全部业务作用域/分页/权限事实整体验收 |
| DATA-01 | 存储实现与版本迁移 | 未开始 | ENG-04, ENG-05, DEC-08, DEC-11 | 真实选定Store、短事务/版本检查/迁移、独立快照与释放；详细数据用例DATA-01/05和旧版本样本迁移，不静默清空 |
| DATA-02 | 工作区/会话持久化 | 未开始 | DATA-01, APP-03 | 分阶段创建和外部关联、保存revision、关闭/恢复事实；详细DATA-02，外部成功而保存失败不假回滚/盲重建 |
| DATA-03 | 消息历史/附件/分页 | 未开始 | DATA-01, APP-04 | 内容块、执行意图、批量保存及保存状态、受控附件/历史页；详细DATA-03/06，重启只恢复已保存事实，不自动重发 |
| DATA-04 | 配置/凭据引用与生效 | 未开始 | DATA-01, ENG-07-C, DEC-08 | 期望/保存/有效值、覆盖来源、revision冲突、secret_ref及失败清理；ADR-0012普通SQLite明文JSON引用，可在整合时共库/共事务；详细DATA-04/07/08，凭据不回显、不拿掩码/旧缓存/ENV替代失败读取 |
| APP-09 | 业务应用服务完整接线 | 未开始 | APP-04, APP-05, APP-07, APP-08, DATA-02, DATA-03, DATA-04 | Workspace/Session/消息/Runtime/配置操作串起领域、Store和Provider端口；BE-08/12、INT-03/09，执行与保存分开；测试替身不冒充实际接入 |
| APP-10 | 权限交互与恢复步骤 | 未开始 | APP-05, APP-07, APP-08 | PermissionRequested/响应关联、迟到失效、有限resume_step与执行复核；BE-05、INT-07，页面关闭不批准、不串会话 |
| APP-11 | 日志配置与诊断查询Backend | 未开始 | ENG-09, APP-06, DATA-04, DEC-10 | 六级运行时设置/持久化/覆盖、有界检索/游标/访问控制；OBS-08/09、BE-09，查询不递归产生日志，错误保留诊断ID |
| APP-12 | Backend关闭与业务收尾 | 未开始 | APP-09, APP-10, APP-11 | 分阶段停止接收、在途请求/权限/订阅/Store和Provider收尾；BE-10、INT-12可控场景，无close-complete前释放/误报底层停止 |

DATA-*任务编号与详细设计内DATA-*验收用例属于不同命名空间；本表“硬依赖”始终指任务ID，“详细DATA-*”始终指设计用例。任务完成仍须在对应执行记录中写明实际调用链和结果，不能仅因公开头文件存在而完成。
