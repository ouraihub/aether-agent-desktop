# 02 领域、Backend与数据任务

执行规则见 [总看板](README.md)。设计入口：[跨层基线](../design/detail/00-contract-baseline.md)、[Backend](../design/detail/02-backend.md)、[Core](../design/detail/03-core.md)、[数据配置](../design/detail/05-data-config.md)。CORE/BE/DATA用例引用各自文件。

| ID | 任务 | 状态 | 硬依赖 | 交付物与验收条件 |
| --- | --- | --- | --- | --- |
| APP-01 | Core类型化端口与Runtime Registry | 已完成 | ENG-03, DEC-06 | 不透明Registry/RuntimeRef、类型化Provider/Runtime身份、能力快照、generation防旧事实、共享引用和两阶段注销；MSVC9/9、Windows/Linux无UI真实链路、ASan/UBSan及分配失败回滚通过；无Runtime IO或空Adapter |
| APP-02 | Workspace与Agent配置领域 | 未开始 | APP-01, DEC-07 | 工作区/Agent配置关系、revision、范围校验与关闭引用；未知ID/配置变化/共享Runtime关系可解释，Agent不是进程别名 |
| APP-03 | Session状态拥有者 | 未开始 | APP-02, DEC-05 | 已确认完整状态表、创建/初始化/turn/关闭/恢复；CORE-SES用例，普通取消与Interrupted区别明确，不复制Backend请求状态 |
| APP-04 | Message与结构化增量 | 未开始 | APP-03, DEC-11 | 类型化块、关联、cursor去重/gap、replace/finalize/message_end；CORE-MSG用例，容量/分配失败不改原内容，块结束不冒充消息结束 |
| APP-05 | Permission策略和临时授权 | 未开始 | APP-02, DEC-07 | 求值、Ask等待数据、单次授权/版本/期限/消费；CORE-PERM全部场景及宽Deny/窄Allow冲突，执行前复核不双消费 |
| APP-06 | DTO与inproc类型化客户端合同 | 未开始 | APP-01, ENG-08, DEC-05 | Command/Query/Event、RequestContext独立、错误映射和释放；真实无UI客户端调用，跨栈数据有效，不引入无类型通用结果 |
| APP-07 | RequestRecord受理/调度/终态 | 未开始 | APP-06, ENG-05, DEC-11, ENG-12 | 本地记录基础见ENG-12；本项仍须业务输入副本/额度后受理、执行前重查、deadline/取消竞争、QueryRequest；BE-01至04，单终态及迟到清理明确，不以同步记录表完成代替 |
| APP-08 | OpenView与事件订阅收尾 | 未开始 | APP-06, ENG-05, DEC-11 | 原子快照/增量基线、revision/sequence、健康失效、异步退订；BE-06/07、INT-05/06无UI可控竞态，满队列不静默假完整 |
| DATA-01 | 存储实现与版本迁移 | 未开始 | ENG-04, ENG-05, DEC-08, DEC-11 | 真实选定Store、短事务/版本检查/迁移、独立快照与释放；详细数据用例DATA-01/05和旧版本样本迁移，不静默清空 |
| DATA-02 | 工作区/会话持久化 | 未开始 | DATA-01, APP-03 | 分阶段创建和外部关联、保存revision、关闭/恢复事实；详细DATA-02，外部成功而保存失败不假回滚/盲重建 |
| DATA-03 | 消息历史/附件/分页 | 未开始 | DATA-01, APP-04 | 内容块、执行意图、批量保存及保存状态、受控附件/历史页；详细DATA-03/06，重启只恢复已保存事实，不自动重发 |
| DATA-04 | 配置/凭据引用与生效 | 未开始 | DATA-01, ENG-07, DEC-08 | 期望/保存/有效值、覆盖来源、revision冲突、secret_ref及失败清理；详细DATA-04/07/08，凭据不回显、不拿掩码当值 |
| APP-09 | 业务应用服务完整接线 | 未开始 | APP-04, APP-05, APP-07, APP-08, DATA-02, DATA-03, DATA-04 | Workspace/Session/消息/Runtime/配置操作串起领域、Store和Provider端口；BE-08/12、INT-03/09，执行与保存分开；测试替身不冒充实际接入 |
| APP-10 | 权限交互与恢复步骤 | 未开始 | APP-05, APP-07, APP-08 | PermissionRequested/响应关联、迟到失效、有限resume_step与执行复核；BE-05、INT-07，页面关闭不批准、不串会话 |
| APP-11 | 日志配置与诊断查询Backend | 未开始 | ENG-09, APP-06, DATA-04, DEC-10 | 六级运行时设置/持久化/覆盖、有界检索/游标/访问控制；OBS-08/09、BE-09，查询不递归产生日志，错误保留诊断ID |
| APP-12 | Backend关闭与业务收尾 | 未开始 | APP-09, APP-10, APP-11 | 分阶段停止接收、在途请求/权限/订阅/Store和Provider收尾；BE-10、INT-12可控场景，无close-complete前释放/误报底层停止 |

DATA-*任务编号与详细设计内DATA-*验收用例属于不同命名空间；本表“硬依赖”始终指任务ID，“详细DATA-*”始终指设计用例。任务完成仍须在对应执行记录中写明实际调用链和结果，不能仅因公开头文件存在而完成。
