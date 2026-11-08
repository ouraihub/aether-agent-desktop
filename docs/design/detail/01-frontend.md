# 01 Frontend 详细设计

## 1. 状态、边界与阅读顺序

本文把[Frontend概要](../01-frontend.md)展开为实施方案；公共语义以正式合同为准，未发布函数/局部字段仍非ABI。用户要求工作台和模型服务严格参考Autorail，视觉与共享主题token入口见[UI设计](../ui/README.md)；不再沿用已撤回的通用SVG线框。GTK目标/平台验收仍见ADR-0009，容量已由DEC-11给出，不因预览存在标记UI功能完成。

前置依据：[共享基线](00-contract-baseline.md)、[架构](../../ARCHITECTURE.md) §2–6、[接口契约](../../INTERFACE_CONTRACTS.md) §3–11、§13、[工程规范](../../CODING_STANDARDS.md) §3–4、§6–7。Frontend 只依赖 Backend 公共契约及自己的 UI Adapter，不读数据库、业务文件或日志文件，不启动 Runtime，不直接调用 MCP。

请求终态、OpenView、revision/sequence和退订close-complete以[接口契约§4.4、§10.4](../../INTERFACE_CONTRACTS.md)为唯一正式语义；共享基线保留交接理由。Session/Message及权限规则见Core设计，配置保存见数据设计，诊断见可观测性设计。本文只定义显示算法，不增加业务状态机。

## 2. 文件职责与执行域

以下是拟议 Frontend 内部文件职责，不要求建立同名目录或为每个对象增加接口层；各实现只在责任实际出现时落位。

| 建议文件组 | 单一职责与主要数据 | 不得混入 |
| --- | --- | --- |
| `app_shell.c/.h` | 导航、窗口、当前 Workspace/页面、关闭交互；借用 Client | 业务停止算法、Runtime 进程所有权 |
| `backend_client.c/.h` | 类型化 API 调用、受理关联、订阅接收上下文、DTO 交接 | 自动重放 Command、第二个任务注册表、UI 控件 |
| `view_receiver.c/.h` | receiver 注册、代次、快照安装屏障、UI Queue 投递与失效 | 领域规则、通用持久事件总线 |
| `workspace_vm.c`、`runtime_vm.c`、`session_vm.c` | 各页面 DTO 投影、选择项、操作可用性与安全提示 | 跨页面共享的可变“当前请求” |
| `message_vm.c`、`message_blocks.c` | 编辑草稿、消息页归并、块投影；块绘制职责独立 | 原始 Runtime 协议解析、执行消息中的内容 |
| `task_vm.c`、`permission_vm.c` | 请求/清理事实展示、授权卡片交互 | 裁决成功或授权、UI 内恢复执行 |
| `tools_vm.c`、`skills_vm.c` | MCP/工具与技能的独立页面投影、表单及调用关联 | 自建执行器、统一安装协议 |
| `settings_vm.c`、`diagnostics_vm.c` | 草稿/有效配置比较；有界检索/诊断跳转 | 配置优先级裁决、日志业务状态推断 |
| `ui_adapter.c/.h` 及实际绑定文件 | 调度、控件、输入法、焦点、剪贴板、绘制资源 | UI 库类型穿入公共 Backend 头文件 |

所有 VM、控件与选择状态只在 UI 线程修改。Client 回调在契约声明的后台执行域复制必要载荷并入队；大型查询、附件访问与解析由 Backend 承担。Client 的接收记录不是 VM，寿命可超过页面。每个手写代码文件最多 500 行、超过 300 行审查职责，C 代码按 80 列目标/100 列常规上限，不按页面规模豁免。

## 3. 全桌面页面操作矩阵

所有页面统一区分 Loading、Ready、Empty、Error、Forbidden、ResyncRequired；Empty 仅来自成功查询的空结果。可保留过期内容供阅读，但明显标记，不用于授权或提交前置事实。列表和详情分别持有自己的 receiver，不按当前选中行解释后台事件。

| 页面 | 查询/展示的最小事实 | 用户操作 → 输入与完成输出 | 特有失败或禁用理由 |
| --- | --- | --- | --- |
| Workspace | ID、名称、目录摘要、可访问性、配置 revision、关联 Session 数 | 创建/打开：用户选择的目录及公开配置 → Workspace ID 与保存结果；选择：仅改变导航；查看配置：按 ID 跳转 | 路径失效/越界、权限拒绝、落盘失败；选目录对话框不代表验证通过 |
| Runtime | provider_id、runtime_id、连接代次、版本、状态、受管/借用、能力及控制边界 | 启动/连接：Provider 与受支持连接配置 → 实例事实；停止/断开：实例及代次 → 实际状态；刷新：新事实 | 未安装、协议不符、未知能力；借用实例显示“断开”，不暗示已杀进程 |
| Session | Workspace/Runtime 关联、状态、模型、Agent 配置引用（若提供）、历史可用性 | 创建：Workspace、Runtime、支持的会话配置 → Session ID；打开历史：只读；恢复/关闭：明确 Session → 实际结果 | Runtime 不可用、外部恢复不支持/过期、关闭中；切页不关闭会话 |
| Message | 有序消息及块、发送/保存状态、草稿与未读变化 | 发送：Session 与有界结构化内容 → request_id，再关联权威 Message；附件预览：授权引用 → 有界内容 | 不支持的块、输入超限、附件失效、受理后失败；不以气泡出现表示已发送成功 |
| Tasks | Backend 请求状态、所属对象、取消能力、执行/清理事实、结果及诊断关联 | 按对象/状态筛选；取消：精确请求身份 → 取消受理或现状；跳转对象/诊断 | NotFound/保留期结束、取消不支持；TimedOut 与底层仍运行并列显示 |
| Permissions | permission_id、受保护 request_id、目标摘要、作用域、期限、允许选项、控制边界 | 确认/拒绝：权限身份、所见版本与契约允许的决定 → Backend 确认 | 过期、已响应、策略改变、Session 关闭；不自行增加“永久允许” |
| MCP | 服务 ID、来源/范围、连接代次、状态、发现结果与错误 | 保存受支持服务配置；连接/断开、刷新发现 → 实际连接/工具事实 | 认证失败、协议不符、断连；配置保存不等于连接成功，不提供伪连接按钮 |
| Tools | 工具 ID、执行者、来源、输入要求、能力与权限边界 | 详情；仅可直接调用工具提供表单：工具身份/版本、作用域、输入 → 请求与结果；Runtime 自有工具显示只读映射 | 工具消失/版本改变、参数失败、权限拒绝；登记可见不代表 Aether 可执行 |
| Skills | 技能 ID、来源、适用范围、有效性、启用/实际应用状态 | 发现/刷新、查看元数据；能力允许时启停/配置 → 保存及实际应用结果 | 无效技能、作用域不适用、Runtime 无启停能力；不预设统一下载、安装或执行命令 |
| Settings | 各层配置、已存值、有效值/来源、可编辑性、需重启/新会话提示 | 编辑/撤销草稿；保存：scope、expected_revision、差异 → 保存与生效事实；凭据：保留/替换/清除意图 | 冲突、字段校验、安全存储失败、保存失败；临时覆盖不被静默写回 |
| Diagnostics | Trace ID、过滤条件、记录、时间/耗时、缺失/轮转/输出健康 | 检索/翻页、复制诊断 ID、查看关联对象；采集级别设置经 Settings | 无权查询、游标失效、记录未采集/已轮转、诊断 ID 不可用；零行不等于无错误 |

Agent 是可选的领域配置/角色引用，Runtime 是实际执行/连接实例；界面不把 Agent 名称当成 runtime_id。Aether Native 与其他真实 Provider 按同一能力入口展示；测试替身必须显式标识，不能充当正式 Native。任务页面不扩展为 P7 编排、长期记忆或远端管理。

## 4. VM 最小字段与容量边界

所有列表项只存公开 DTO 投影或其独立所有权，不持有 Core 指针。显示名称不是键。业务对象 ID、Backend 实例作用域内 request_id、connection_generation、view_generation、分页 query_generation 彼此独立。

| VM/记录 | 必需字段 | 更新与失效责任 |
| --- | --- | --- |
| ViewState（各 VM 组合使用） | receiver_id、view_generation、scope/filter、phase、subscription_id、base_revision、last_sequence、snapshot_ready、error | UI 拥有；更换 scope/filter、重同步或关闭立即增加代次 |
| PageState | query_generation、snapshot_revision、cursor、pending_query、has_more、loaded_ids、anchor_id | 同一方向一次在途翻页；条件变化清空游标；Backend 拒绝过期 revision 时重建 |
| Workspace/Runtime/Session VM | 当前选择 ID、有界行集合、详情 DTO、capability_revision、操作禁用原因 | 选择不改变领域关联；已删除对象保留“不可用”提示并撤销动作 |
| Message VM | session_id、按 message_id 索引的可见页、按 block_id 的块、draft/draft_revision、submission、scroll_anchor、follow_tail、unread_count | 草稿仅 UI 所有；消息内容/状态由 Backend；附件仅持授权引用 |
| Task VM | 请求身份、对象关联、state、cancel_requested、execution_status、cleanup_status、结果/安全错误 | 状态仅投影；Client 只持交付关联，不复制所有历史任务 |
| Permission VM | permission_id、revision、请求/目标关联、allowed_decisions、expiry 提示、交互状态 | 过期与授权结论来自 Backend；本地计时器仅停止旧确认并触发查询 |
| Tool/Skill VM | 服务/工具/技能身份与版本、scope、executor、能力、配置草稿、关联 request_id | 发现刷新后旧工具表单标记 stale，不自动套用新 Schema |
| Settings VM | scope、loaded_revision、saved_values、effective_values/sources、draft_patch、dirty_fields、validation、saving_revision、save_result | 保存请求冻结一份差异；用户后续编辑不被旧响应覆盖 |
| Diagnostics VM | trace/filter、query_generation、cursor、rows、capture_level/source、availability、error | 时间排序仅用于显示；不推导跨进程因果或业务终态 |
| Client Receiver | receiver_id/generation、订阅句柄、安装前缓存、health、closing、callback context | Client 拥有并同步；close-complete 前不能销毁 |

页面数、缓存页数、单页条数、草稿/附件引用大小、预安装事件数/字节数、UI Queue 数/字节数、单批更新时间均有有限上限（D-LIMIT）。历史控件按可见范围建立；淘汰页保留滚动锚点，不驻留全部消息。超限拒绝新输入或进入 ResyncRequired，不无限扩容，也不静默截断用户待发送内容。

## 5. 快照、事件归并与分页算法

### 5.1 打开和重同步

UI侧`open_view(scope, filter)`算法采用[正式视图握手](../../INTERFACE_CONTRACTS.md#44-视图握手与回调交接dec-05)：

1. UI 创建 VM 和 receiver_id，增加 view_generation，设置 Loading；Client 先登记可存活的接收关系，再发起 Backend OpenView，分别传业务参数与只读 RequestContext。
2. OpenView 尚未返回时，Client 将该次打开关联的早到事件放入有界缓存；不要求 VM 已存在于回调线程，也不假设响应先于事件进入 UI Queue。
3. 返回失败则释放输出，撤销此次接收关系；Backend 不得返回半订阅。成功时 Client 接管 snapshot、subscription_id、base_revision、sequence 基线并排入安装任务。
4. UI 根据 receiver_id 与代次找 VM；不匹配时释放快照并退订这个过时的打开结果，不能安装到新页面。匹配时一次替换快照投影并设置 last_sequence 为该订阅基线。
5. 标记 snapshot_ready，按订阅顺序应用缓存和后续事件。复制、安装或缓存失败时将接收健康置为失效，停止增量，异步退订并显示 ResyncRequired；不能留下“已完整加载”。
6. 用户刷新或重同步时使旧代次失效，发起新 OpenView；旧接收上下文独立等待 close-complete。只重建读模型，不重放发送、授权、工具调用或保存。

### 5.2 单事件归约与批量绘制

1. 入队项仅含 receiver_id、view_generation、订阅身份和拥有的载荷；在 UI 线程先查接收注册表，失效则立即释放。
2. 验证 Schema、scope 和 subscription_id。sequence ≤ last_sequence 的重复交付不再次应用；sequence 不连续、健康失效或未知且无法安全解释的变更进入 ResyncRequired。
3. 对连续事件按领域 ID 定位对象，并比较对象 revision；旧投影不得覆盖更新的对象。revision 是对象版本，不能拿它代替 sequence 判断订阅丢失，也不要求全局 revision 连续。
4. 消息增量只更新明确的 Session/Message/block；工具结果按公开调用关联归位。找不到必要基础块、增量前置版本不满足或消息归属不一致时重同步，不追加到最后一条消息。
5. 内容操作按 [Core 消息契约](03-core.md) 的追加/替换及终结语义执行；完成事件只确认其声明的事实，不能补造缺失正文。请求终态不会因迟到消息变回 Running；可合法保留的迟到输出由 Backend 投影决定。
6. 归约成功后推进 last_sequence。UI 可在有限批次结束时一次绘制变更块；只合并绘制失效区域，不删改业务事件，不能跨完成、错误、权限边界合并不同内容操作。
7. Backend 队列或 UI Queue 满时通过接收健康/控制状态暴露失效；UI 调度周期检查它，不依赖向已满队列再投一个错误通知。重同步期间权限卡片禁止继续提交。

### 5.3 分页与滚动

`load_page` 携带 scope/filter、游标及要求的快照 revision；Client 附 receiver/view/query 三重关联。返回只有在三者及本次游标仍匹配时才可合并，否则释放，不覆盖新筛选结果。独立诊断检索同样使用 query_generation，但不伪造业务事件订阅。

页按稳定对象 ID 去重，排序依 Backend 提供的顺序键而非 UI 到达时刻。新增历史页与已加载对象重叠时保留较新 revision；分页期间删除或重排使页边界不再成立则废弃结果和旧游标、重新 OpenView，避免在 UI 维护无界删除墓碑。Backend 明确拒绝旧快照/游标时显示“历史已变化”，不拼接两个不一致快照。未加载区域的不可局部应用变更同样触发重建。

阅读历史时保存首个可见 message_id 与块内偏移，插入新页后恢复锚点；新回复仅增加未读提示。只有用户已在尾部或明确点击“跳到最新”才跟随滚动。切会话保留有界草稿/锚点，重新查询历史不代表恢复 Runtime 执行上下文。

## 6. 能力、消息与异步任务交互

`derive_action_state(action, object, capabilities, permission_hint, view_health)` 输出 enabled 与稳定 reason_code，不执行业务。依次检查页面有效性、能力报告可用性/代次、是否支持、对象当前可用状态、明确权限拒绝和同一表单是否提交中；未知/查询失败/不支持/暂不可用分别展示。Ask 不等于 Deny：若操作本身可用，允许提交，由 Backend 发起授权。最终能力与权限校验仍在 Backend。

能力变化使模型、图像、文件、Thinking、取消、恢复、工具、Skills 等控件重新计算；不兼容草稿标记具体块，保留内容供用户移除或改选 Runtime，不静默删附件或降级成文本。切 Runtime 不能偷偷改变现有 Session 的归属。

| 内容块 | 最小呈现 | 交互与失败 |
| --- | --- | --- |
| Text | 有界文本、流式未完成提示 | 作为数据呈现；富文本渲染不得执行嵌入内容 |
| Image / File | 类型、允许展示的名称/大小、引用可用性 | 预览/获取均经 Backend 授权；失效显示原因，不直接打开任意路径 |
| Tool Call / Result | 调用关联、工具来源/执行者、状态及安全结果 | 调用与结果可分开到达；不从显示文本执行工具 |
| Structured Data | 有界结构化查看或安全文本表示 | 大内容按需获取；解析失败保留受限提示，不阻塞 UI |
| 未知块 | 类型/版本、不可呈现说明和可用诊断关联 | 不丢弃其存在，不把原始载荷当可执行 HTML/脚本 |

| 本地提交阶段 | 触发及处理 | 用户可见含义 |
| --- | --- | --- |
| Editing → Submitting | 冻结 draft_revision 与提交内容；禁重复点击；建立本次只读追踪上下文 | 正在提交，尚无业务完成结论 |
| Submitting → Rejected | 同步拒绝，无 request_id；保留草稿与安全错误 | 未受理，可修改后另行提交 |
| Submitting → Accepted | 记录 Backend 请求身份；仅当当前草稿仍等于冻结版本才清空编辑区 | 已受理，不声称成功或已保存 |
| Accepted → 状态投影 | 按请求事件或 QueryRequest 更新 Accepted/Running/WaitingPermission/终态 | 业务终态沿用基线；失败时可显式把提交副本恢复为新草稿 |
| 任一在途 → 取消提交中 | 按请求身份发取消；重复点击受本地 pending 限制 | 尚未停下；NotSupported 则解释限制 |
| 等待超时/接收失效 | 展示等待/同步问题，查询权威请求；不可盲目再发送 | 不把等待超时写成业务 TimedOut，也不自动重放副作用 |

受理响应可能晚于对应事件到达 UI：Client 以本次提交关联暂存匹配请求事实，取得 request_id 后交接；无法确认受理时标记“状态待确认”，禁隐式重试。完成先于取消确认时按 Backend 的终态展示，不由按钮状态反推。任务状态、执行是否停止、清理是否完成、消息是否保存分开显示。

## 7. 权限、设置与诊断的专用算法

### 7.1 权限卡片

卡片状态为 Showing、Submitting、CheckingValidity、Resolved、Unavailable，仅描述交互。显示完整且经 Backend 脱敏的目标摘要、Workspace/Session/Runtime/Tool 关联、控制边界与允许选项；详情异步更新不能悄悄扩大用户刚确认的目标。

提交时冻结 permission_id、所见 revision 与决定，暂禁重复按钮；由 Backend 检查请求仍待决、范围/策略版本/期限及一次性约束。成功受理仍等确认；拒绝或执行失败不改成“允许成功”。收到失效、取消、关闭、Runtime 退出或策略变更事实即撤销旧卡片按钮，必要时重新查询。使用 Backend 提供的有效期提示安排本地倒计时；本地到点仅转 CheckingValidity 并禁止旧确认，不独立裁决领域过期。迟到确认返回 stale/expired 时展示最新事实，不自动对新权限请求补发同一决定。失联、重同步、详情缺失期间不允许确认；取消/关闭弹窗也不自动批准或替业务取消。

### 7.2 设置草稿与保存

1. 打开 scope 的配置快照，分别保存 loaded_revision、已存值与有效值/来源；秘密字段仅显示“已配置/未配置”，草稿用保留、替换、清除表达意图，不用掩码字符串替代真实值提交。
2. 编辑只修改 draft_patch/dirty_fields；本地按公开字段约束给提示，Backend 仍完整校验。更换 scope 或离页遇未保存草稿时提供继续编辑、明确丢弃或保存后离开，不默默丢弃。
3. 保存冻结 expected_revision 与差异；只锁该次保存，允许后续编辑形成更新草稿版本。受理不清 dirty，不显示“已保存”。
4. 完成后按 Backend 返回的持久化与实际应用事实更新 saved/effective；只清除仍等于本次已保存值的 dirty 字段，保存期间的新输入保留。若仅保存成功但应用失败/需重启，分别说明；失败不得假称重启可恢复。
5. revision 冲突时保留草稿，重新查询并对比已存值，要求用户明确重新应用差异；不自动覆盖他处修改。持久化事务与恢复规则不在 UI 重做。
6. 日志级别同时显示持久偏好和有效值，优先级遵循正式规范的命令行 > 环境变量 > 用户设置 > 默认 INFO。存在覆盖时保存偏好不改称有效值已经变化；清除临时覆盖不是 UI 擅改启动环境。

### 7.3 日志检索

从错误入口复制安全 trace_id 到 Diagnostics，带入公开对象范围但仍由 Backend 授权；每次独立搜索建立新的操作上下文，搜索目标 trace_id 是查询参数，不冒充本次请求的 TraceContext。trace 不可用时明确提示，可用其他受控筛选定位，不生成假的关联 ID。

检索参数包括 trace_id、operation/layer/component、level、时间范围与有界页大小；默认显示 INFO 及更严重记录。筛选变化递增 query_generation 并清游标，迟到页释放。显示 timestamp、duration_ms、operation、层/组件、结果与缺失说明；父子关联依据 span，不按时间邻近猜测。未采集、轮转、队列丢弃、输出故障和无权限分别展示；调整显示级别不能恢复过去未采集记录，查询结果不更新 Tasks/Session 状态。

## 8. 关键函数意图及失败出口

以下名称是内部职责草案，不是冻结符号；业务调用分别使用类型化 API，不引入通用 `void*` Command。UI 函数仅 UI 线程调用，Client 接收函数只触碰其拥有的同步记录；全部异步完成经 receiver 投递。

| 函数意图 | 输入 → 输出 | 失败/关闭出口 |
| --- | --- | --- |
| `open_view` / `install_snapshot` | scope/filter/context → pending 打开；快照信封 → 已安装 VM | 分配/API/安装失败释放已得资源；过时成功结果须退订 |
| `receive_event` / `reduce_event` | 借用事件 → 自有队列载荷；连续事件 → 投影变更集合 | 复制失败/队列满置健康失效；不留借用指针，不推进失败事件游标 |
| `load_page` / `merge_page` | PageState/context → 在途查询；页与代次 → 更新页/锚点 | stale 结果只释放；revision 失效重建，查询错误保留已有内容及提示 |
| `submit_message` | session、冻结草稿/context → 受理身份或拒绝 | 失败保留草稿；未知受理不自动重试；关闭页不取消后台发送 |
| `request_cancel` | 完整 Backend 请求身份/context → 取消响应/现状 | 终态不重发底层取消；NotFound、NotSupported 和等待超时分开 |
| `respond_permission` | 权限身份、所见版本、决定/context → 确认状态 | stale/expired 时更新卡片；不能给另一权限请求复用决定 |
| `save_settings` | scope、expected_revision、冻结差异/context → 保存/生效结果 | 冲突保留草稿；秘密失败输出不含原值 |
| `query_diagnostics` | 授权范围、过滤/游标/context → 有界页与可用性 | 无权限/游标失效/记录缺失分别反馈，不访问日志路径 |
| `close_view` / `close_receiver` | receiver/generation → 页面已失效、异步收尾令牌 | 可重复发起；只在 close-complete 且本地载荷收尾后释放 context |

## 9. UI 资源表与关闭顺序

| 资源 | 创建/持有者 | 借用与释放条件 |
| --- | --- | --- |
| 窗口、控件、绘制/输入法资源 | UI Adapter，页面持有 | 仅 UI 线程使用/释放；解绑回调后销毁，部分创建失败逆序释放 |
| VM、草稿、可见页 DTO | UI 页面 | 无后台裸指针；退出时按草稿选择处理，DTO 调创建方释放接口 |
| Query 快照/异步结果 | Backend 创建，Client 接管再移交 UI | 投递失败、过时、安装失败均释放；成功转移后 Client 不重复释放 |
| Event 载荷副本/UI Queue 项 | Client 创建，队列持有 | 入队成功移交；消费或丢弃均释放；只带接收标识，不带 VM 地址 |
| 订阅句柄/callback context | Client | 页面可先消失；Unsubscribe close-complete 前保持 context 存活 |
| UI 计时器/延迟绘制回调 | UI Adapter/页面 | 关闭先撤销；已排队回调按 receiver 失效丢弃，不能读已释放 VM |
| 附件预览引用与图像资源 | Backend 授权返回引用；UI Adapter 持显示资源 | 离页释放显示资源并解除引用；不获得底层文件/执行句柄所有权 |
| 提交/查询关联记录 | Client | 受理交接/结果交付/视图等待撤销后移除；业务请求寿命由 Backend 管理 |

页面关闭算法：禁交互 → 使 receiver/view_generation 失效并从可投递注册表移除 → 撤销 UI 计时器和视图等待 → 异步 Unsubscribe → 丢弃/释放对应 UI Queue 项 → 释放页面/VM；Client 在 close-complete 后且本地交付引用归零时释放 context。回调内可发起退订但不得同步等自身结束。OpenView 尚未完成就关闭时保留最小 Client 打开记录，迟到成功必须收回其订阅，不能因 VM 已不存在而泄漏。

应用关闭先处理未保存草稿，再停止新交互，由组合根协调业务停止、权限撤销与持久化；UI 通过独立的关闭进度通道显示结果，不阻塞等待。业务关闭与页面退订不是同一动作。最后停止前端投递、完成接收收尾、销毁 UI 资源；关闭截止到达只能显示未完成清理，不授权释放仍被回调引用的上下文或杀死借用进程。

## 10. FE 验收场景与评审门槛

以下为后续可执行验收，不表示本次文档任务已运行测试。竞态可用明确标识的契约测试源制造；真实窗口、Runtime、MCP/Skills 支持分别记录平台与版本，模拟结果不能替代真实能力。

| 编号 | Given / When / Then |
| --- | --- |
| FE-01 | Given Workspace 列表与合法/越界目录；When 创建或打开并切换；Then 成功显示权威 ID/保存结果，拒绝保留安全原因，选择不修改 Session 归属。 |
| FE-02 | Given 受管 Runtime 与借用连接；When 启动/连接/停止/断开且能力未知或变化；Then 文案区分进程与连接，按钮按能力而非名称，无法控制的范围可见。 |
| FE-03 | Given 已保存历史但 Runtime 无 Resume；When 打开 Session 并尝试恢复/关闭；Then 历史可读、恢复禁用有理由，只有独立关闭命令改变业务状态。 |
| FE-04 | Given OpenView 快照尚未安装；When 更新先到、重复到或队列满；Then 不漏初始化交接，重复不重复应用，满载进入 ResyncRequired 而非完整假象。 |
| FE-05 | Given 两 Runtime 多 Session 同时输出；When 消息块/工具结果交错并切页；Then 按实例/会话/消息/调用关联归并，不写当前最后一条消息或串 trace。 |
| FE-06 | Given 历史翻页在途；When 改筛选、切会话、删除/重排或旧页晚到；Then 代次与 revision 阻止旧页覆盖，失效游标重建，滚动锚点仍可解释。 |
| FE-07 | Given 可发送草稿含图像/文件；When 同步拒绝、受理后失败、能力撤销或双击发送；Then 保留可恢复草稿，不删块、不重复提交、不把受理显示成业务成功。 |
| FE-08 | Given 在途任务；When 取消与成功竞争或等待超时后仍执行；Then 只显示 Backend 终态，分别显示取消请求中、执行与清理，绝不盲重发。 |
| FE-09 | Given 待确认权限；When 本地倒计时到点、后台过期/策略变化或迟到确认；Then 旧卡片禁用并查询，失效决定不能作用于新请求，UI 不裁决授权。 |
| FE-10 | Given MCP 工具可调用、Runtime 自有工具只映射；When 连接/发现/调用及断连；Then 展示真实执行者，参数和权限失败可见，只读工具不可绕过 Backend 执行。 |
| FE-11 | Given 有效/无效及不同作用域 Skills；When 刷新、配置或启停；Then 展示校验和实际应用结果，不支持能力明确禁用，不伪造安装/启用成功。 |
| FE-12 | Given 设置有临时覆盖与未保存差异；When 保存期间再编辑、发生 revision 冲突或落盘失败；Then 新草稿不丢、失败不称已保存，持久偏好与有效来源分开。 |
| FE-13 | Given 凭据已配置；When 保留/替换/清除并触发失败；Then 不回显原值、不提交掩码、不在错误/日志/诊断复制中泄漏凭据。 |
| FE-14 | Given 一次真实失败带 Trace ID；When 检索、改筛选、翻页及复制；Then 可看允许记录的时间/耗时与关联，旧结果不覆盖新查询，缺失原因不伪装无错误。 |
| FE-15 | Given OpenView 在途、回调已进入、UI Queue 有载荷；When 快速关页/反复关页/退出；Then 无 VM 悬空访问，迟到订阅被收回，context 直到 close-complete 才释放。 |
| FE-16 | Given 部分 UI 初始化、复制/投递失败及未知块；When 分配失败或超限；Then 成对释放、有限内存、明确错误/受限呈现，不执行不可信内容。 |
| FE-17 | Given 每个矩阵页面；When 返回加载、空、错误、无权和需同步状态；Then 均有可区分文案和合法后续操作，键盘焦点/中文输入/高 DPI/可访问性在真实窗口验证。 |
| FE-18 | Given 未保存草稿和仍运行任务；When 离页或退出并遇清理截止；Then 草稿选择明确、离页不取消任务、退出不阻塞 UI，不虚报底层已停止或已回收。 |

实施前：D-UI方向与循环桥接见[ADR-0009](../../adr/0009-ui-and-loop-bridge.md)：GTK4 C API限制在ui_adapter，后台有界载荷通过显式UI main-context source投递并检查receiver/代次，投递不等于close-complete。本轮WSL GTK4.14.5真实窗口mapped、程序化中文UTF-8及worker回UI通过；目标4.24.1、原生Windows/macOS、真实中文IME/焦点/DPI切换/辅助功能和完整Backend关闭链仍缺证据，DEC-09不标完成、不再等用户选方案。临时实验已清理，不存在永久验证脚本。其余D-*按正式合同与ADR收敛；DEC-05握手/竞争/close-complete不重复设计，P7不在此冻结。
