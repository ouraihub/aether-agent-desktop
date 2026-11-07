# ADR 0005：请求受理、事件视图与退订交接

- 状态：提议（DEC-05调查已完成候选收敛，容量、保留期和正式API仍待确认）
- 记录日期：2026-11-08
- 决策依据：DEC-05；Pi/OMP RPC实现的request ID与prompt_result、session_settled、abort、分片校验和有界关闭语义；`docs/design/detail/00-contract-baseline.md` §3–4；`docs/design/detail/02-backend.md` §2–8；`docs/design/detail/01-frontend.md` §5、§8–9。
- 替代关系：补充ADR 0004的请求/事件交接；不替代容量数值、存储、权限或具体Runtime协议决策。

## 背景

ENG-12已有同步、有界、八槽本地记录，但没有异步输入副本、deadline、取消竞争、事件订阅、OpenView或close-complete。Pi与OMP的成熟RPC客户端区分命令受理与prompt_result，按请求ID收集事件；OMP还区分一次yield与session_settled，并在坏分片时关闭连接。Aether需要吸收这些可观察语义，但不能把第三方RPC字段暴露到Frontend，也不能复制其无界通知缓存或语言运行时。

## 提议决策

1. **受理**：入口完成有界参数校验、能力快照读取、输入复制、额度取得和RequestRecord登记后返回本实例代次内唯一的request_id。没有request_id的同步拒绝不发布RequestCompleted；受理不代表执行、保存或业务成功。
2. **终态**：Backend是RequestRecord唯一终态裁决者。每个受理请求最多发布一次逻辑RequestCompleted；完成、失败、取消、deadline和迟到事实回到同一状态拥有者。裁决前先检查单调deadline，再按有效事实顺序处理；迟到事实只补充execution/cleanup/persistence，不改已提交终态。
3. **取消与稳定**：CancelRequest是独立控制请求。它表示取消意图，不等于底层已停止；未执行请求可直接Cancelled，已执行请求需等待真实取消事实，Runtime不支持取消时返回NotSupported且不杀共享Runtime。受理回合结束不代表会话无后续活动；若Runtime提供类似settled事实，Adapter可映射为能力范围内的稳定状态，但Aether不把第三方事件名当公共Schema。
4. **OpenView**：OpenView在同一状态操作中原子登记订阅、复制过滤条件、创建不可变快照并返回subscription_id、base_revision和该订阅sequence基线。快照/队列/映射任一步失败整体撤销。revision是领域版本，sequence只在单订阅内递增，不承诺全局顺序、持久重放或跨订阅相同序号。
5. **背压与重同步**：订阅队列和事实队列有界；满载、sequence缺口、快照失效或未知不可解释事件使健康进入ResyncRequired，停止继续交付增量并保留可查询控制状态。不得静默丢完成/权限事实、无限增长或向已满队列追加错误通知。重新OpenView只重建读模型，不重放发送、授权、工具或保存副作用。
6. **退订**：Unsubscribe关闭入口后不开始新用户回调；已进入回调可以完成，排队载荷释放。回调可发起非阻塞退订或Command，不同步等待自身。active_callback归零后发出一次close-complete，之后才能释放callback context；重复退订关联同一关闭过程。
7. **协议隔离**：Pi/OMP的prompt_result、session_settled、rpc_chunk和abort仅作为Adapter调查输入。Aether事件保留版本、事件ID、对象关联、sequence、时间和明确所有权，Frontend不依赖第三方原始事件。坏帧/旧代次/未知内容按统一协议或重同步错误处理。

## 待确认项

- RequestRecord终态保留期及回调引用解除条件。
- 输入/事实/快照/订阅队列的条目和字节容量，以及关键事实的保留策略。
- 正式事件Schema版本、可空关联字段、时间基准和sequence分配。
- 回调执行域、线程安全/重入规则和close-complete通知形式。
- 每类Command的deadline默认值、无deadline是否允许及控制请求自身完成语义。

这些项目由DEC-11及APP-06/APP-07/APP-08接口实现前确认；本ADR在确认前保持“提议”，不授权生产异步实现。
