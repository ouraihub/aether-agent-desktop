# ADR 0004：执行域与Runtime Session边界

- 状态：已接受（用户要求参考Pi与Autorail实现后的Aether边界决策；不表示Aether异步执行器或Runtime Adapter已实现）
- 记录日期：2026-11-08
- 决策依据：Aether `DEC-04` / `DEC-06`；`docs/design/detail/06-platform-execution.md`、
  `docs/design/detail/03-core.md`、`docs/ARCHITECTURE.md` §5；Windows可控线程试验；
  `D:\workspace-ai\autorail` 的执行/关闭实现与 `D:\workspace-ai\pi` 的RPC Session语义。
- 替代关系：补充 ADR 0002 的执行与领域边界；不替代Runtime协议、权限或持久化决策。

## 背景

Aether需要把UI、Backend、Core、IO及受管Runtime的状态和资源交接说清楚。Autorail为每个聊天维护独立Runtime进程，使用独立读写/退出观察活动、显式取消和有限优雅关闭；Pi以RPC命令、运行/收敛事件、`abort`和真实session locator表达回合与恢复。两者的实现语言、进程模型和库不能直接复制到Aether C17 Core。

## 决策

1. **状态拥有者**：每类可变领域状态由单一逻辑拥有者按串行顺序提交。IO/工作线程只能投递带关联身份、代次和事实类型的有界消息；不得直接修改Core、Backend或UI状态。
2. **执行边界**：Aether不引入Pi/Autorail的Rust运行时，也不因参考实现引入libuv或通用线程池。平台线程、等待和进程机制隔离在Platform/Infrastructure；Core和Backend不包含OS头。具体线程数由实际负载和平台验证决定。
3. **队列**：异步输入、回调和回复通道必须有界。容量不足时显式拒绝或使视图进入ResyncRequired；不得阻塞UI、无限增长或静默丢失完成/权限事实。具体初值和压力证据由DEC-11收敛。
4. **取消与关闭**：取消受理、底层停止、资源关闭和`close-complete`是不同事实。关闭先停止新工作、撤销/取消可取消交互、请求优雅收尾；到达有限截止后才使用已批准的强制终止策略。仍有回调引用时不得释放上下文。UI不得等待Runtime或Join。
5. **Runtime Session**：Runtime描述与Runtime实例分离；实例使用Aether `runtime_id`及`connection_generation`。Session默认一次只接受一个进行中的turn；只有Adapter报告能力且Session合同允许时才接受并发turn。普通turn结束回Idle，连接/进程失效进入RuntimeUnavailable或Closed的明确分支；不能把Pi内部状态猜成Aether事实。
6. **AgentRun**：当前不建立独立AgentRun实体。只有出现独立查询、权限、关闭或持久化责任时，才另行提出实体和契约；Session与Runtime关系不能借用OS进程身份。
7. **恢复与内容**：历史记录不等同外部Runtime上下文恢复。只有Adapter返回并验证真实session locator且能力允许时才报告恢复成功。未知ContentBlock不转成Text，返回Unsupported或明确重同步结果。

## 理由

这些边界保留了Autorail的显式资源寿命和有限关闭，也保留了Pi的RPC回合、abort和真实会话定位语义，同时避免把第三方的单聊天进程、内部队列、权限缺口或Rust类型直接变成Aether公共ABI。单一状态拥有者和有界交接符合Aether现有架构硬约束，并为后续Backend事件、Runtime Adapter和UI Queue保留替换空间。

## 影响与风险

- DEC-04和DEC-06的决策语义已收敛；具体线程拓扑、队列数值、Timer和Process Service实现仍需各自任务及DEC-11验证。
- Aether不能宣称控制Pi内部工具或文件权限；Pi项目明确没有内建权限沙箱，Aether只有在自身执行边界或Runtime暴露协议范围内提供控制。
- 首个真实Runtime仍需按实际版本资料和协议单独验证；本ADR不批准Pi Adapter，也不批准任何自动下载安装或凭据传递方式。
- Session/Message/Core公共头文件实现必须同步这些规则，并覆盖迟到事实、关闭和未知内容错误路径。
