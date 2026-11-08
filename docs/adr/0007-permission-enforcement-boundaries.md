# ADR 0007：权限求值、单次批准与执行边界

- 状态：提议；DEC-07仍待安全策略确认，不授权危险操作或沙箱承诺。
- 日期：2026-11-08
- 依据：用户指定参考本机Pi/OMP；Aether架构§6、Core详细设计§7、接口契约§11。
- 关系：补充ADR-0004/0005，不替代Runtime协议、进程隔离或OS路径机制决策。

## 实际参考与差异

Pi `packages/coding-agent/src/core/agent-session.ts` 的_beforeToolCall仅在存在扩展处理器时执行；runner.ts中block结果阻止调用。project-trust.ts在无UI且没有信任决定时返回false，但项目资源信任不等于逐工具权限或OS隔离。Pi README明确默认以启动用户权限运行。

OMP `packages/coding-agent/src/tools/approval.ts`支持allow/deny/prompt，工具或用户deny优先；缺失执行上下文时always-ask，但配置默认可为yolo，不能泛称OMP默认最小权限。restrictedFolders把非deny结果改为每次prompt，不是操作系统沙箱。`extensibility/extensions/wrapper.ts`在参数被扩展修改后重新求值并针对实际参数审批；无UI不能满足待批准检查，非显式允许答复按deny。OMP还支持会话、目录、目标及跨会话授权；Aether本提案不默认引入这些扩大授权。

## 保守提案

1. Workspace/Session/Runtime/Tool是可组合匹配维度。操作与目标必须完整，能力未知或缺失不能通过Ask补成支持。Inherit只继续查声明的父作用域；无匹配允许为Deny。
2. 任一适用Deny优先，其次Ask，其次Allow。窄范围Allow不覆盖宽范围Deny，单次批准只满足该次Ask，不改持久策略。该组合是Aether自己的安全规则，不宣称Pi/OMP具有相同继承图。
3. 批准绑定可信主体、request_id、operation、不可变目标、Workspace/Session/Runtime generation、policy_revision和单次nonce；拒绝、关闭、取消、撤销、代次/策略变化使旧批准失效。参数或目标变化须重新求值，不能沿用旧审批。
4. Ask使用有限单调有效期，不超过目标业务deadline；具体默认时长归DEC-11/API批准。无人答复、通道失效或超时不执行；UI不自动续期或批准。一次授权只由实际执行拥有者在副作用前原子消费，失败不自动退还，重试重新批准；不持久化一次性nonce。
5. 可信入口重新校验调用者及参数；执行端在最后可检查点重查策略/代次/有效期/目标，并消费授权。读/写/删除/启动进程/网络是不同operation；UI隐藏按钮、日志和登记能力不能成为授权。
6. 当前Platform路径对象仅是UTF-8值，不作路径授权。文件范围实施必须核实链接/重解析点、目录句柄/最终资源和检查到使用竞态；不能只靠字符串前缀。具体Windows/POSIX技术方案须在实际Filesystem/Process任务中审查，不在本文伪造已实现能力。
7. Runtime原生审批只映射其真实协议支持的范围；Aether Host工具走Aether执行门。第三方Runtime绕过Host的内部工具不受此门保证。借用连接关闭不授予终止外部进程权限；受管进程环境只传白名单，凭据不进普通日志/DTO。
8. 建议默认拒绝无法兑现所需控制的工作流，不静默降级。若产品要支持显式未隔离模式，必须另行批准：由用户按Runtime实例明确接受控制缺口，不显示“已受控”，不能用一次Ask伪装强制隔离。这是需负责人决定的产品安全取舍。

## 威胁/验收矩阵

| 场景 | 提案要求 |
| --- | --- |
| Workspace Deny与Session Allow同时匹配 | Deny，副作用计数为0 |
| Ask答复到另一request/目标/实例代次 | 拒绝，不恢复执行 |
| 批准后参数/路径/策略改变 | 旧批准失效，针对实际目标重查 |
| 两个执行者同时提交同一批准 | 仅一个成功消费，无双重副作用 |
| 无人回答、无UI、deadline到达 | 不执行，不将Ask降为Allow |
| 符号链接/重解析点切换目标 | 复核真实资源；无法满足限制时拒绝 |
| 控制一个Session取消共享Runtime | 不杀其他Session或借用进程 |
| Runtime内部Shell未暴露审批协议 | 报告实际缺口，不声称Aether已拦截 |
| 事件丢失/页面关闭后迟到批准 | 权威等待项重建，失效项不执行 |

## 完成条件与当前缺口

确认策略组合、单次授权和未隔离模式后，将批准语义迁入正式契约，设计保留算法理由。实际授权、路径/进程强制及拒绝后无副作用由APP-05/APP-10/ENG-06等任务运行验证；本文不是实现完成，也不批准Windows/POSIX沙箱选型。APP-02仍受DEC-07硬依赖，不用参考项目存在该能力当作Aether已验证。
