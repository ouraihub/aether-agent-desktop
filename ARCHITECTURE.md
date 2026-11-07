# Aether 总体架构

## 1. 范围、状态与决策依据

本文件维护模块职责、源码依赖、运行调用、部署与线程边界，以及权限执行边界。迁移来源为 [总规范](设计2.0.md) 原 §2–3、§5–6、§11–13、§15；这些章节现保留导航，不再维护第二份细则。

Aether 管理 Runtime，而不是强行改造 Runtime。自研业务代码采用 C17，目标平台为 Windows / macOS / Linux。当前只有规范文档，没有已实现的模块、通信协议、线程池或安全沙箱；下述目录表示规划边界，不表示现有代码。

- 已确定：前后端分离、Core 面向接口、独立 Runtime Adapter、显式资源所有权、异步事件驱动、初期进程内 Backend 通信。
- 未确定：第三方库及版本、UI 与后台事件循环集成方式、线程数量、具体沙箱、首个接入 Runtime 与其协议版本。
- 契约字段、生命周期和并发语义见 [INTERFACE_CONTRACTS.md](INTERFACE_CONTRACTS.md)；C 实现、日志、依赖与质量门槛见 [CODING_STANDARDS.md](CODING_STANDARDS.md)。历史决策与依据见 [ADR 索引](docs/adr/README.md)。

“必须/禁止”是硬约束；“建议/候选”不是已批准选型。补充边界说明不代表第三方 Runtime 已提供对应控制能力。

整个项目的实施蓝图见 [项目概要设计](docs/design/README.md)：分别描述 Frontend、Backend、Core、Runtime/工具、Infrastructure/Platform，并由独立文档组织跨层流程及验收。本文件继续作为架构边界的维护处，概要不替代这里的依赖与安全约束。

## 2. 模块职责与源码依赖

### 2.1 分层职责

| 边界 | 应负责 | 不得承担 |
| --- | --- | --- |
| Frontend | 页面、组件、ViewModel、UI State、用户交互、消息及 Runtime/Session/Workspace/工具/配置展示 | 领域规则、Runtime 协议、进程管理、数据库与业务文件访问、直接调用 MCP、耗时业务操作 |
| Backend | API 入口、参数验证、Command/Query 分发、应用服务、业务流程与事务协调、请求上下文、异步任务、事件转发和 API 生命周期 | UI 控件、窗口对象、ViewModel 指针或 UI 框架依赖 |
| Core | Agent、Runtime、Session、Workspace、Message、Permission、Tool、MCP、Skills、Memory、Event、Configuration 的领域模型、规则与所需抽象接口 | UI、操作系统专用实现、具体 Runtime 和协议实现 |
| Adapters | Runtime 启动/连接/停止协调、协议及消息转换、事件与错误映射、能力发现、状态同步 | 通用领域规则、直接操纵 UI、自行绕过 Process Service 启动进程 |
| Infrastructure | Process、文件系统、数据库、网络/HTTP/IPC、日志、异步任务、计时器、加密、配置存储、缓存等通用技术实现 | 核心业务规则及 UI 状态 |
| Platform | 进程和句柄、路径/文件锁、动态库、系统目录和环境变量、Named Pipe/Unix Domain Socket、系统通知及系统级权限差异 | Session 等领域规则及特定 Runtime 协议 |

每个模块只负责一类核心能力，拥有必要的公开接口、独立测试和明确依赖。禁止循环依赖、访问他模块私有结构、巨型 Manager，以及为目录整齐而机械拆出无职责的模块。

### 2.2 依赖规则

源码依赖是公开头文件/构建目标的引用关系，不是运行时执行顺序。Core 通过函数表调用 Adapter 的实现，不等于 Core 可以包含 Adapter 的头文件。

| 使用方 | 允许依赖的边界 | 关键限制 |
| --- | --- | --- |
| Frontend | Backend 公共 API/客户端契约、DTO/事件契约、自己的 UI Adapter | 不引用 Core、Runtime Adapter、Infrastructure；业务请求不能旁路 Backend |
| Backend 应用服务 | 前后端契约、Core 公开接口、所需技术能力的公开抽象 | 不按 Runtime 名称绑定具体实现；不把 UI 对象送入下层 |
| Core | 自身及其他 Core 模块的公开接口、无业务状态的通用基础类型 | 不依赖具体 Adapter、Infrastructure 实现、Platform 或 UI；需要的外部能力通过显式接口注入 |
| Runtime/协议 Adapter | Core 定义的 Provider/事件等接口、所需 Infrastructure 公开服务 | 不反向依赖 Backend 或 Frontend；不将第三方协议对象暴露成领域对象 |
| Infrastructure 实现 | 它实现的抽象接口、Platform 及必要第三方库 | 对领域接口的依赖仅为实现契约，不得借此承载业务决策 |
| Platform 实现 | 无业务基础类型、操作系统 API | 不向上依赖 Core、Backend 或 Frontend |
| 公共契约与基础类型 | 必要标准 C 类型和受控公共定义 | 不包含 UI、第三方库、具体实现头文件；不能把 `shared/` 变成绕过分层的入口 |

技术能力的接口应放在消费者可依赖的独立公开边界；接口文件与实现源码不能因同一目录就强制一起链接。通用技术接口不必全部塞进 Core，领域所需端口由 Core 定义。构建目标粒度由实际模块确定，不预设插件 ABI 或通用依赖注入框架。

### 2.3 装配入口与资源归属

桌面应用启动入口承担组合根职责：选择具体实现、创建实例、通过构造参数或初始化配置注入依赖，组织启动和关闭。这个装配位置可以知道具体实现，但不能成为第二个业务入口。

Session 等模块不得自行创建 Logger、Event Bus 或 Runtime Registry。构造者必须声明依赖是借用还是转移；共享依赖的寿命须覆盖使用者。初始化、启动失败与退出路径遵循接口生命周期契约，不假设所有组件已成功启动。

## 3. 运行调用关系

### 3.1 发送消息场景

1. Frontend 从用户操作生成请求 DTO，经 Backend API 提交，不持有 Core 内部对象。
2. Backend 验证输入、请求上下文和适用权限，通过应用服务调用 Core 公开接口。进入异步执行前必须完成数据复制或明确的所有权转移。
3. Core 按 Session/Runtime 关联及能力模型选择已注入的 Runtime Provider 接口；Adapter 将请求映射为所选 Runtime 的协议。
4. 需要创建外部进程时，Adapter 只能请求统一 Process Service；操作系统差异由 Platform 处理。现有连接或远端连接不应被强行建模为本地子进程。
5. 外部结果经 Adapter 校验、错误转换和事件映射进入统一事件模型，驱动 Core 状态及 Backend 事件分发。
6. Frontend 通过订阅收到前端事件 DTO，经 UI Queue 在 UI 线程更新显示。原始 Runtime 事件可保留作脱敏诊断，但不成为 UI 契约。

请求被受理不是任务已完成；完成、取消、超时和错误按接口契约报告。Query 同样经过 Backend，不因只读而获得直接读数据库或领域内存的许可。日志用于诊断，不充当业务事件通道。

### 3.2 权限请求与故障返回

- Aether 控制的操作在执行副作用之前经过权限决策；需要 Ask 时由 Backend 发布权限请求，Frontend 展示并将决定经 API 送回，UI 不直接恢复底层执行。
- Runtime 原生权限请求由 Adapter 关联到相应会话和请求，再映射到同一交互入口；无法关联或无法实际执行的决定不能报告为成功授权。
- Runtime 异常退出、断连、协议错误及拒绝操作必须转换为可定位的状态和错误，不能吞掉异常或让请求永久表现为运行中。是否重连、重试或恢复会话不在此默认批准。

### 3.3 可追踪日志的架构归属

一次独立用户操作、命令或后台任务建立 root trace，各真实边界建立 child span。Frontend 操作上下文经 Backend 请求入口进入应用服务、Core 接口、Adapter/Infrastructure，再随异步结果返回；调用方未提供合法上下文时，由相应受控入口建立。不能每层重新生成 root，也不能从 UI 对象或全局“当前 Session”猜测归属。契约详见 [INTERFACE_CONTRACTS.md](INTERFACE_CONTRACTS.md) §13。

- 上下文及追踪身份属于可独立依赖的公共基础契约；Core 仅接收上下文和注入的日志接口，不依赖日志文件后端、UI、产品配置或某个 tracing 库。
- Logger 的输出、轮转、有界读取与故障状态属于 Infrastructure，单调时钟和 OS 差异经 Platform 提供；组合根组织初始化和关闭，不在业务模块各自安装 logger。
- Backend/配置服务管理日志级别偏好、持久化、临时覆盖及诊断查询授权；Frontend 经 Backend 筛选记录、复制诊断 ID，不直接访问日志文件。诊断查询要防递归记录，不成为新业务事件通道。
- 任务队列、回调、计时器、进程流读取和事件信封携带显式上下文，业务对象关闭后不得再借用其内存。取消、超时和后续资源清理的关联不能靠线程 ID 或最近一次请求推断。
- Runtime 的追踪元数据只经受支持协议传播。不支持时，Aether 记录本地调用 span 及带实例/连接作用域的请求映射；不能修改第三方 prompt 或宣称掌握其内部链路。

本次采纳的是本地可检索追踪需求，不是选定日志库、线程模型、存储格式或 OpenTelemetry/OTLP 后端。工程要求、可见性边界与验收见 [CODING_STANDARDS.md](CODING_STANDARDS.md) §4。


## 4. 进程与部署边界

### 4.1 初期部署

Frontend、Backend、Core、所需 Adapters/Infrastructure 和 Platform 可在同一个 Aether 桌面进程中运行，初期 Backend 通过 In-Process Transport 提供服务。逻辑分离不依赖进程分离；Backend 必须支持没有 UI 的独立运行和测试。

第三方 Runtime 可作为受管外部进程，或通过其稳定接口连接。应用内组件、Runtime 子进程和操作系统是不同的故障边界；同一进程内的 Core 隔离只是模块边界，不是内存隔离或安全沙箱。

启动外部进程须明确可执行文件、参数、工作目录、环境变量及资源归属。关闭时须明确哪些进程/连接由 Aether 拥有：关闭借用连接不等于可以终止他人启动的进程。终止、取消、异常退出和管道/句柄回收均需验证。

### 4.2 后续演进

独立 Backend 进程、多前端与远程访问是路线，不是当前必须实现的设施。候选传输包括 Windows Named Pipe、macOS/Linux Unix Domain Socket、HTTP、WebSocket、SSE。

切换前须评估序列化/版本兼容、身份认证、授权、断连、事件顺序与资源生命周期，并记录真实决策。不得仅为未来网络化先定义复杂网络协议、空 IPC 目录或插件系统。进程内 DTO 也不得泄漏领域指针，使边界在实际需要时可演进。

## 5. 线程、队列与关闭边界

### 5.1 已确定的执行约束

| 执行域 | 责任 | 边界要求 |
| --- | --- | --- |
| UI 线程 | UI 事件、轻量请求提交、ViewModel/UI 状态更新 | 不同步等待 Runtime、不执行长时间 IO、不直接解析大型数据 |
| 应用/Core 状态执行域 | 业务状态和请求协调 | 明确状态归属；优先消息传递和单线程事件循环约束，减少共享可变状态 |
| IO 执行域 | 进程管道、网络、计时器等异步活动 | Adapter 处理外部数据，不直接访问 UI；不得隐式并发修改领域状态 |
| 阻塞/计算工作执行域（按需） | 无法非阻塞执行的操作 | 结果回到状态拥有者；是否需要线程池由实际负载决定 |
| UI Queue | 后台结果跨线程移交 | 携带具有有效生命周期的 DTO，接收方销毁后的处理必须定义 |

“执行域”不是“一域一线程”的要求，也不等同于单独进程。具体共享事件循环还是独立后台循环、线程数量和任务调度方式尚未决定。libuv 是候选之一，不是已批准实现。

### 5.2 必须在实现前落实的事项

- 标明每类可变状态的拥有者、可调用线程、回调线程以及是否允许重入；多线程访问时按接口声明使用锁、原子操作或队列。
- 区分 API 调用返回与后台完成；不能因同进程调用就借用调用方临时内存到后台。
- 明确队列容量、生产者过快与消费者过慢时的行为，特别是流式消息和关键完成/权限事件；未选择策略前不能暗示无丢失、无限缓存或全局有序。
- 取消、超时、退订和销毁的竞态由 [接口契约](INTERFACE_CONTRACTS.md) 定义并验证，不能靠延迟释放的偶然时序保证安全。

关闭须协调停止接收新工作、处理在途请求、停止事件生产与订阅、完成异步资源关闭、最后释放被引用对象。具体阻塞/异步关闭接口和等待上限由契约决定；硬约束是 UI 不阻塞、对象释放后无悬空回调、共享依赖不能先于使用者销毁。

## 6. 权限与安全边界

### 6.1 策略与执行分离

Core 的 Permission 领域负责策略语义，Backend 协调请求和用户交互；操作所在的 Process/Filesystem/MCP/Tool 服务或 Adapter 承担可实现的执行检查。UI 隐藏按钮不是授权，Frontend 传入的路径、范围与决定都必须由可信业务入口验证。

权限须能够按 Workspace、Session、Runtime、Tool 控制。策略包含 Allow、Deny、Ask、Inherit，默认遵循最小权限。建议覆盖读文件、写文件、执行命令、网络访问、MCP 工具、外部进程及 Workspace 访问。

继承优先级、冲突消解、授权有效期、撤销以及无人应答处理尚须通过契约明确；不能让 AI 自行把 Inherit 或 Ask 解释成 Allow。危险操作应支持用户确认，确认必须关联到正确操作而非泛化授权。

### 6.2 实际可执行的控制边界

| 操作来源 | Aether 可负责的控制点 | 不得宣称的保证 |
| --- | --- | --- |
| Aether 自己执行的文件、进程、工具操作 | 统一服务执行前授权、校验路径与范围、受控环境与参数、结果和审计 | 只在 UI 检查即可防止绕过 |
| Runtime 暴露的原生权限请求 | Adapter 映射请求、用户决定与实际协议响应，验证版本支持 | 任意 Runtime 都支持拦截所有危险操作 |
| Runtime 自行执行且未经过授权协议的操作 | 限制 Aether 提供的配置和环境，准确展示能力缺口 | 配置了 Workspace 路径便能阻止 Runtime 访问其他路径 |
| 操作系统强制隔离 | 仅在具体隔离方案实现且验证后报告其能力 | 子进程天然是沙箱，或同进程模块边界提供 OS 级隔离 |

如果某工作流依赖强制限制而当前 Runtime/OS 组合无法提供，必须明确说明无法满足，不能静默降级成“已受控”。是否允许受限能力以外的运行模式属于需确认的安全策略，本次不预先批准。

### 6.3 必须保留的安全要求

- 外部进程启动统一通过 Process Service；禁止任意拼接 Shell 命令，禁止默认透传全部环境变量。
- 不得绕过 Workspace 权限；文件操作必须校验路径和访问范围。路径隔离方案必须评估符号链接/重解析点及检查与使用之间的变化，不能仅凭字符串前缀宣称隔离成立。
- Aether 执行的 MCP Tool 必须有权限检查，权限请求有明确用户交互；取消、终止与清理覆盖异常场景。
- API Key、Token、密码不得写入日志；敏感配置应加密或使用系统安全存储，选型与版本另行评估。
- 统一权限模型只能承诺实际执行边界内的保障。安全限制、未覆盖能力和实际 Runtime 版本必须能够向用户说明。

## 7. Runtime 与工具生态边界

### 7.1 Runtime 接入

每种 Runtime 应有独立 Adapter。优先标准协议或官方稳定接口，能力发现结果驱动功能呈现，不按 Runtime 名称硬编码。接入建议保留如下，仅作为调查起点，不表示官方已支持或已经验证：

| Runtime | 原规范候选方向 | 原规范备用方向 |
| --- | --- | --- |
| OpenAI Codex | App Server | ACP / CLI |
| Claude Code | ACP / Agent SDK Bridge | CLI |
| Pi | Native RPC / ACP | CLI |
| OpenCode | ACP | CLI |
| oh-my-pi | ACP | Native RPC / CLI |
| DeepSeek Harness | ACP / SDK JSON-RPC | Headless |
| Aether Native | 内部 C API | 无预设备用方案 |

接入前必须核实 Runtime 实际版本、官方资料、所需 Bridge 的来源与维护责任。不要把终端文本解析作为长期首选协议；确需 CLI 时独立封装，处理 stdout、stderr、退出码、超时、异常退出、取消、终止，并避免依赖不稳定的终端格式。

### 7.2 MCP、Skills、Tool Registry

- MCP 模块集中管理 Server/Client、连接生命周期、Tool/Resource/Prompt 发现、调用、错误和权限；Client 实现不得散落到业务模块。Core 管领域与抽象，协议和 IO 放在适当 Adapter/Infrastructure 边界。
- Skills 的存储和加载独立于 Runtime Adapter。建议支持发现、元数据、加载、校验、版本、启停、作用域及依赖；向不同 Runtime 的配置映射属于 Adapter。
- Tool Registry 统一管理 Aether 可管理工具的注册、查询、描述、权限、生命周期、调用、结果和审计；不同 Runtime 的工具能力通过 Adapter 映射，Registry 不依赖某个具体 Runtime。
- 对 Runtime 内部自有的工具/MCP/Skills，统一登记和展示不等于接管执行。必须标明实际执行者与权限控制点；Runtime 未暴露的调用不能伪装为经过 Aether 检查或完整审计。该区分落实“管理而非强行改造 Runtime”的既有边界。

## 8. 规划目录与模块落地

| 规划位置 | 用途与边界 |
| --- | --- |
| `apps/desktop/` | 应用入口与组合装配，不承载领域逻辑 |
| `frontend/presentation/`、`frontend/ui_adapter/` | 页面、组件、ViewModel、UI State/Events；具体 UI 绑定隔离 |
| `backend/api/`、`backend/services/`、`backend/transport/` | Command/Query/Event 入口、应用服务、实际需要的传输实现 |
| `contracts/api/`、`contracts/dto/`、`contracts/events/`、`contracts/schemas/` | 公开前后端契约；不含 Core 内部布局 |
| `core/` | Agent、Runtime、Session、Workspace、Message、Permission、Tool、MCP、Skills、Memory、Event、Configuration 等按需领域模块 |
| `adapters/runtime/`、`adapters/protocol/` | 各 Runtime 适配与复用协议实现 |
| `infrastructure/` | Process、Filesystem、Database、Network、Logging、Async、Crypto、Config 等技术实现 |
| `platform/windows/`、`platform/macos/`、`platform/linux/` | OS 专用实现 |
| `shared/` | 必要 Common、Memory、String、Collection、Error 等无业务基础能力，不重复造通用库 |
| `tests/` | Unit、Integration、Contract、E2E 及隔离的 fixtures；具体组织按已实现模块决定 |
| `docs/`、`third_party/` | 深入设计/API/开发资料与 ADR；受控第三方依赖 |

根目录预计由 CMakeLists、CMakePresets 和实际开发说明提供构建入口。只创建当前交付所需模块，不一次性建立所有空目录。目录可调整，分层、所有权和公开接口边界不能随之消失。

## 9. 架构验收与待决策门槛

改变模块职责或依赖时必须检查：Frontend 只通过 Backend，Backend 可无 UI，Core 不包含具体 Runtime，平台差异不泄漏，新增 Adapter 不要求修改 Core 主体，每个组件可独立测试且有清晰生命周期。

工程建立后应以无 UI Backend 场景、公开接口的依赖检查、Mock/Simulator 隔离测试和真实 Runtime 接入分别验证；模拟结果不证明真实协议兼容。异常退出、关闭期间回调、权限拒绝和消费者滞后属于相关边界的验收场景，而不是只看能否显示消息。

| 待决策事项 | 决策前需要的证据 | 必须落实的时点 |
| --- | --- | --- |
| UI 框架、异步库及循环集成 | 目标平台可用性、许可证、最小 UI/后台交互验证 | 引入相应依赖前 |
| 实际线程拓扑、队列容量与背压 | 生命周期方案、负载及退出行为 | 发布异步契约与实现前 |
| 首个 Runtime 与协议/Bridge | 官方版本资料、能力与权限限制、真实运行结果 | 宣称适配支持前 |
| 权限继承、强制隔离与无法约束时的策略 | 威胁模型、Runtime 控制能力、各平台可执行机制 | 发布相应安全承诺前 |
| Backend 独立进程或网络传输 | 实际需求、序列化/安全/断连成本 | 开始相应架构迁移前 |

待决策不等于留下代码占位；没有这些信息时可继续不受影响的任务，但不得发布缺乏必要语义的接口或宣称相应能力已经完成。
