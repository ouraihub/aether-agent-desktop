# 00 范围、环境与决策前置任务

执行规则见 [总看板](README.md)。本表是任务状态的唯一维护处；初始全部未开始，不因设计文档已写就标记决策完成。依赖为任务ID，跨文件位置查总看板。

设计入口：[概要路线](../design/06-delivery.md)、[详细基线 D-*](../design/detail/00-contract-baseline.md)、[ADR规则](../adr/README.md)。每项交付均包含明确结论、依据、适用范围、受影响契约及确认记录；需要项目负责人决定的重大取舍不得自行写成已接受。普通调查可以先完成，但缺少批准时整项进入待确认，不算已完成。

| ID | 任务 | 状态 | 硬依赖 | 交付物与验收条件 |
| --- | --- | --- | --- | --- |
| DEC-01 | 核实仓库与交付范围 | 已完成 | 无 | 已核实当前仓库原为规范文档阶段；本轮先落地P1最小C17/CMake骨架，不提前生成业务空模块；P2–P6及P7仍按看板跟踪 |
| DEC-02 | 工具链与工程验证方案 | 已完成 | DEC-01 | 已核实本机CMake 4.2.3、Clang 22.1.4（Windows GNU目标）和Ninja 1.13.2；采用CMake Preset/Ninja/CTest作为当前Windows入口，macOS/Linux尚未验证，后续按实际环境补充 |
| DEC-03 | D-ID身份与基础错误合同 | 已完成 | DEC-01 | 已实现进程实例范围的 `AetherId`（原子递增、有效性、比较）和固定容量 `AetherError`（清空、边界复制、截断拒绝）；本合同不宣称跨进程/持久化全局ID，后续公共契约可扩展 |
| DEC-04 | D-EXEC执行域与关闭方案 | 已完成 | DEC-02 | ADR-0004收敛单一逻辑状态拥有者、有界事实交接、取消/停止/关闭分离、有限优雅关闭后强制终止；Windows可控线程试验通过；线程数、具体库和容量数值由后续任务实测，不宣称已实现执行器 |
| DEC-05 | D-REQUEST及D-EVENT交接 | 已完成 | DEC-03, DEC-04 | 参考Pi/OMP实际代码与5项客户端回归，正式合同§4.4/§10.4确定受理/一次终态/等待超时与业务deadline分离、顺序示例、OpenView/sequence/重同步/close-complete；容量/保留时长数值归DEC-11，异步API仍未实现 |
| DEC-06 | D-DOMAIN领域状态 | 已完成 | DEC-03 | ADR-0004收敛Runtime描述/实例、runtime_id+generation、无独立AgentRun、默认单回合、能力条件并发、真实locator恢复和未知内容块拒绝边界；具体Core实现仍由APP任务交付 |
| DEC-07 | D-PERMISSION安全策略 | 已完成 | DEC-06 | 用户要求参考Pi/OMP已收敛为正式合同§11.1及ADR-0007：默认yolo、显式拒绝/Ask、单次/会话/目标记忆、范围与失效、真实Runtime控制边界；参考客户端审批协议3/3通过；不宣称执行门或沙箱已实现 |
| DEC-08 | D-DATA存储与配置 | 已完成 | DEC-06 | ADR-0008选SQLite3.54.1/Public Domain、统一yyjson0.13.0/MIT，短事务/CAS、迁移一致备份/损坏保全、保存与执行分离、配置生效及secret_ref合同；本会话用户选择由ADR-0012部分替代系统安全后端为普通SQLite明文JSON，ENG-07-C进行中；既有Python事务烟测不是C集成或DATA验收 |
| DEC-09 | D-UI框架与循环桥接 | NAppGUI作为当前Frontend实现，正式接线仍进行中 | DEC-02, DEC-04 | 用户确认先用NAppGUI落地，保持Backend唯一入口和可替换UI Adapter边界；Windows原生窗口/构建已实测，Linux GTK3后端已实测；macOS、中文IME/焦点/DPI/AT、完整Backend关闭链和真实业务接线仍未验证，不把选型demo当产品完成 |
| DEC-10 | D-DIAGNOSTICS日志方案 | 已完成 | DEC-03, DEC-04 | ADR-0010和正式§13收敛16/8字节随机trace/span、JSONL/yyjson、私有目录/多实例配额、过滤/健康/查询授权；OMP真实日志context及WARN过滤通过，Aether logger仍未实现 |
| DEC-11 | D-LIMIT基础容量基线 | 已完成 | DEC-04, DEC-05, DEC-06, DEC-08, DEC-10 | ADR-0006落定任务/事实/请求/消息/附件/Store/Process/日志双限、保留/关闭和272MiB受管载荷预算；求和核实、OMP实测帧限；后续模块须边界与负载验证，不宣称RSS/SLA |
| DEC-12 | D-PROTOCOL调查目录与首接入方案 | 已完成 | DEC-01, DEC-06, DEC-07 | ADR-0011建立全部Runtime/MCP/Skills目录、首选OMP18.18.0 stdio RPCv2再Pi，真实隔离握手/查询/错误/EOF退出通过；Native范围明确，其他目标仍待各RUN核实，不冒充Adapter完成 |

DEC-12完成不要求提前核实所有厂商的完整协议。每个RUN/TOOL任务必须在自身实现前核实本目标真实版本、资料、协议和权限边界，重大选择得到确认后才实现；因此某个未就绪厂商不阻塞其他目标。首目标也只交选型/资料证据，正式功能由对应RUN任务实际运行验收。

决策任务涉及的小型试验是选型证据，不等于对应工程功能已实现。临时试验完成后清理，保留可复核结果；生产实现由后续任务交付。
