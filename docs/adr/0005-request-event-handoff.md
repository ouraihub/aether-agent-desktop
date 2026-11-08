# ADR 0005：请求受理、事件视图与退订交接

- 状态：已接受（DEC-05语义决策；用户指定按Pi/OMP实际做法收敛，不代表异步API已实现或容量已批准）
- 记录日期：2026-11-08
- 决策依据：DEC-05；Pi/OMP RPC实现的request ID与prompt_result、session_settled、abort、分片校验和有界关闭语义；`docs/design/detail/00-contract-baseline.md` §3–4；`docs/design/detail/02-backend.md` §2–8；`docs/design/detail/01-frontend.md` §5、§8–9。
- 替代关系：补充ADR 0004的请求/事件交接；不替代容量数值、存储、权限或具体Runtime协议决策。

## 背景

ENG-12仅有同步八槽记录。直接读取本机参考源码后确认：Pi当前RpcClient的等待超时只撤销监听/响应等待，abort是独立命令；Agent.waitForIdle等待agent_end监听器收尾。OMP的RpcPromptResults按ticket一次报告prompt_result，结果取最终assistant stopReason，sessionSettled另行计算；SDK注册收集器早于写命令，兼容结果早于ack。二者不能混写成同一协议，也没有给Aether提供跨线程完成时间总序。

## 决策及正式维护处

1. **受理**：入口完成有界参数校验、能力快照读取、输入复制、额度取得和RequestRecord登记后返回本实例代次内唯一的request_id。没有request_id的同步拒绝不发布RequestCompleted；受理不代表执行、保存或业务成功。
2. **终态**：Backend是唯一终态裁决者；当前采用既有Aether基线的单调deadline优先检查，再按拥有者处理顺序提交一次逻辑终态。客户端等待超时不修改业务状态、不自动abort。Pi/OMP的等待机制支持后者，但不是Aether显式业务deadline优先规则的来源；Aether不依赖第三方墙钟完成时间。正式状态表和顺序示例见[接口契约§10.4](../INTERFACE_CONTRACTS.md#104-请求终态与等待交接dec-05)。
3. **取消与稳定**：CancelRequest是独立控制请求。它表示取消意图，不等于底层已停止；未执行请求可直接Cancelled，已执行请求需等待真实取消事实，Runtime不支持取消时返回NotSupported且不杀共享Runtime。受理回合结束不代表会话无后续活动；若Runtime提供类似settled事实，Adapter可映射为能力范围内的稳定状态，但Aether不把第三方事件名当公共Schema。
4. **OpenView**：采用Aether现有设计的原子快照/订阅交接，单订阅sequence，不承诺跨订阅全序；不是声称Pi/OMP具有同名握手。正式信封、序号、失效、线程与退订规则见[接口契约§4.4](../INTERFACE_CONTRACTS.md#44-视图握手与回调交接dec-05)。
5. **背压与重同步**：订阅队列和事实队列有界；满载、sequence缺口、快照失效或未知不可解释事件使健康进入ResyncRequired，停止继续交付增量并保留可查询控制状态。不得静默丢完成/权限事实、无限增长或向已满队列追加错误通知。重新OpenView只重建读模型，不重放发送、授权、工具或保存副作用。
6. **退订**：Unsubscribe关闭入口后不开始新用户回调；已进入回调可以完成，排队载荷释放。回调可发起非阻塞退订或Command，不同步等待自身。active_callback归零后发出一次close-complete，之后才能释放callback context；重复退订关联同一关闭过程。
7. **协议隔离**：Pi/OMP的prompt_result、session_settled、rpc_chunk和abort仅作为Adapter调查输入。Aether事件保留版本、事件ID、对象关联、sequence、时间和明确所有权，Frontend不依赖第三方原始事件。坏帧/旧代次/未知内容按统一协议或重同步错误处理。

## 容量与实现交接

- 终态记录保留时间由不可变limits配置指定，最早回收点是终态保留截止且全部回调/映射解除；达到记录上限拒绝新受理，不驱逐活跃记录。数值统一由DEC-11验证，不要求DEC-05先重复批准。
- APP-06/07/08必须在签名发布前确定类型化DTO/释放函数和Schema编码；语义版本从1起，不支持的主版本明确拒绝，不把原始Runtime载荷转发UI。
- 回调在非UI交付域运行，同订阅串行，不持状态锁；可提交非阻塞Command或发起退订，不得同步重入状态处理或等待自身。具体线程数不是DEC-05要求。
- 有限业务deadline由受理时单调时间起算，排队和权限等待计时；调用方等待预算独立。每类Command数值由对应API/DEC-11发布，不能把SDK的30/60秒等待默认值当作Aether业务截止。

## 取证与验证

- Pi：`packages/coding-agent/src/modes/rpc/rpc-client.ts`的waitForIdle/collectEvents/send/abort；`packages/agent/src/agent.ts`的abort/waitForIdle/finishRun。运行真实RpcClient的可控计时器路径：先等待超时，再接收agent_settled，后一次等待成功，未发送abort。
- OMP：`packages/coding-agent/src/modes/rpc/rpc-prompt-results.ts`及session-settle.ts；SDK的Go prompt.go和Rust client.rs。SDK Python官方5项测试通过：旧回合隔离、本地处理、过滤不影响完成、yield与settled分离、受理后失败。测试使用参考项目明确标记的fake server，不是Aether Adapter或真实模型验收。
- 严格分片校验仅是Runtime Adapter协议证据；OpenView和C回调释放是Aether自己的边界要求。共享设计和ADR只解释实现顺序，正式行为以接口契约为唯一维护处。
