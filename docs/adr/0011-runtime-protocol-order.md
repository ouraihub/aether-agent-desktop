# ADR 0011：首Runtime与协议调查目录（DEC-12）

- 状态：已接受（接入顺序/资料决策，不是Adapter或模型调用已实现）。
- 日期：2026-11-08。
- 依据：用户要求清楚决策直接参考Pi/OMP；DEC-06领域和DEC-07实际权限边界。
- 关系：补充ADR-0004/0005/0007；不批准所有厂商采用同一wire协议。

## 1. 选择与理由

首个真实适配目标选**OMP 18.18.0，stdio自定义JSONL RPC v2**，由RUN-06交付；其次Pi由RUN-04独立核实当前版本协议。已安装OMP与本机源码版本均18.18.0，源码与语言中立wire schema可直接核对，C17可通过受管进程/字节流接入，无需把Node/Bun嵌入Core。不同于MCP的JSON-RPC 2.0，不用名称猜ACP。OMP自有ACP入口存在，但本次不选，避免为第一条链路增加未经实测协议。

RUN-01宿主/APP业务/Process前置完成后优先RUN-06；RUN-04随后；Codex/Claude/OpenCode可在各自前置就绪后独立实施，不强制串行等待；DSH等待产品身份与官方接口核实但不阻塞其他Runtime；Native按RUN-08/09真实模型/工具闭环实施。没有删除任何目标或用首个目标代替全项目。

## 2. 调查目录与门槛

| 目标 | 已核实资料入口 | 当前证据/接入决定 | 实现任务 |
| --- | --- | --- | --- |
| OMP | 本机`oh-my-pi/docs/rpc.md`、`packages/coding-agent/src/modes/rpc/{rpc-mode,rpc-types,rpc-frame}.ts`、`wire/rpc-wire.schema.json`；[官方仓库](https://github.com/can1357/oh-my-pi) | 18.18.0真实ready/v2协商/get_state/错误关联/EOF退出通过；首接入stdio v2，未发送模型prompt | RUN-06 |
| Pi | 本机`pi/packages/coding-agent/docs/{rpc,rpc-commands,rpc-extension-ui}.md`、`src/core/agent-session.ts`；[官方仓库](https://github.com/earendil-works/pi) | 本机package版本1.5.0；JSONL RPC、受理与agent_settled分离。不能直接套OMP prompt_result/v2分片；尚未运行本版本CLI | RUN-04 |
| Codex | [官方App Server](https://developers.openai.com/codex/app-server) | 官方富客户端入口；当前未锁CLI精确版本/生成schema，不预写方法。RUN实施时先真实initialize与schema核实 | RUN-02 |
| Claude | [官方Agent SDK](https://code.claude.com/docs/en/agent-sdk/overview.md)、[Headless CLI](https://code.claude.com/docs/en/headless.md) | 官方SDK为Python/TS；其他语言可CLI子进程。Bridge/CLI精确版本、商业条款/登录限制和权限回送需RUN任务核实，不默认依赖私有SDK wire | RUN-03 |
| OpenCode | [官方Server](https://opencode.ai/docs/server/)、[SDK](https://opencode.ai/docs/sdk/) | 官方headless HTTP/OpenAPI/SSE入口；实现前核实版本和/auth，受管server仅loopback并有凭据，不直接公开无鉴权端口 | RUN-05 |
| DeepSeek Harness/DSH | 项目要求中的稳定目标名，尚无可核实的唯一产品仓库/版本 | 未建立官方身份，不凭DeepSeek模型API推断存在Agent Harness协议；RUN-07先查明发行物与接口，其他Runtime可继续 | RUN-07 |
| Native | [模型API官方资料入口](https://platform.openai.com/docs/api-reference/responses)及Core详细设计§9 | 选首个模型接入为可配置OpenAI Responses API直连，准确模型ID/端点/认证由合法配置提供；实际schema、SSE事件与结束/工具语义在RUN-08锁定核实，不把其他“兼容”端点当已支持 | RUN-08/09 |
| MCP | [规范2025-11-25](https://modelcontextprotocol.io/specification/2025-11-25)、[schema](https://github.com/modelcontextprotocol/specification/blob/main/schema/2025-11-25/schema.ts) | 初始目标版本2025-11-25，按initialize协商，不强制未支持服务接受；Tool/Resource/Prompt分别发现与授权，初期Client，不预建Server | TOOL-02/03 |
| Skills | 本机Pi/OMP Skills加载/配置源码及Runtime详细设计§8 | 内容/元数据不是执行协议；按来源版本、范围、指纹/启用与effective证据管理，不因读到SKILL文件自动执行，逐Runtime映射另验 | TOOL-04/05、RUN-10 |

外部资料核实日期为本ADR日期，链接是调查入口，不是这些Runtime所有版本能力声明。RUN实施必须记录精确版本/平台/许可/官方资料快照、能力和真实执行结果；未调查、未实现与明确不支持分开。

## 3. OMP映射与安全边界

- `omp --mode rpc`启动受管子进程；Process Service提供参数数组、显式cwd和环境白名单。凭据只在有授权且确需模型调用时注入，不透传宿主全部环境。连接一实例当前一个活动Session；多Session UI通过多个实例或明确的受控切换实现，不能并发重绑已有Session。
- 初始ready使用v1，验证versions/限制后协商v2并等待关联成功响应，再发布Ready。物理帧1MiB（含LF）、逻辑重组64MiB；输入不接受rpc_chunk。分片验证ID/index/count/byteLength、严格base64/UTF-8，禁止交织或中断拼接；超限/坏帧关闭故障连接，不静默截断关键内容。
- 外部关联包含runtime_id/generation/channel/id，响应按id而非最近请求。prompt受理不等完成；只用同id的prompt_result或明确agentInvoked=false本地完成裁决，session_settled是独立静止事实。message_end仅是消息结束，agent_end不能替代完整历史或请求终态。
- abort是该实例当前执行取消，不能伪装为共享实例任意单请求隔离；未验证并发turn能力时限定单turn。stdout持续消费，stderr单独有界排空且不回显敏感正文。stdin EOF后继续读stdout，排空至退出；超时由受管停止策略收尾，借用连接不杀外部进程。
- 按DEC-07映射原生模式，不人为添加逐工具审批；扩展UI/Host Tool只有实际协议支持时映射。Runtime自有工具不等于Aether执行门已拦截其全部行为，不承诺OS沙箱。恢复必须使用真实session locator，不能用Aether显示历史冒充外部resume。
- OMP stdout慢消费者可溢写临时盘并增长（官方rpc.md说明），不是Aether有界内存/磁盘保证。Adapter必须持续读、有限下游积压并在持续无法交接时关闭连接，暴露不完整与执行不确定性，不能用无界本地spool补偿。

## 4. Native范围

首个Native闭环只做真实文本/流式模型回合、明确结束/错误/取消，随后按RUN-09接真实已注册工具并回送结果；工具按声明顺序串行、额度有界，拒绝不执行副作用，不自动重试副作用。不是固定回复、模拟器或重写第三方Runtime。暂不承诺图像输入、并发工具、自动上下文压缩、长记忆或其他模型兼容；这些需能力/任务证据而不是假成功。真实模型调用需要合法端点/凭据，不在本决策试验读取用户凭据或产生计费请求。

## 5. 实际验证与未验证

运行已安装`omp --version`为18.18.0；临时程序在空临时cwd/独立PI_CODING_AGENT_DIR、环境白名单、禁用tools/extensions/skills/rules/context/lsp/UI/持久session后启动真实OMP，显式选择模型但不发送prompt或API key。观察ready `[1,2]`、1MiB/64MiB，协商v2成功，get_state成功，未知命令返回匹配id的失败，关闭stdin后stdout排空、exit=0。

第一次隔离启动未指定模型，真实进程返回“No default model selected”退出1，未发布Ready；补充显式模型选择后通过。该失败是启动前置证据，不靠用户真实配置回退。临时文件清理，未改Pi/OMP仓库或用户配置。

未验证真实模型回复/取消/多会话/恢复、Aether C Adapter、其余Runtime、跨平台；这些由RUN任务验收。DEC-12完成条件是首目标真实资料/接入顺序/Native范围与调查目录，不要求把所有厂商实现提前完成。
