# Aether 代码落位与模块分包规划

## 1. 文档职责

本文件是 Aether **代码应该放在哪里**的唯一规划入口，解决功能增加后的文件乱放、目录重复、模块边界漂移和 CMake 依赖失控问题。

它不替代：

- [ARCHITECTURE.md](../ARCHITECTURE.md)：分层职责、依赖方向和安全边界。
- [INTERFACE_CONTRACTS.md](../INTERFACE_CONTRACTS.md)：公共接口、所有权、生命周期和事件语义。
- [CODING_STANDARDS.md](../CODING_STANDARDS.md)：C17 编码、文件规模、行宽、测试和质量规则。
- [分层概要设计](README.md)：系统组成和模块协作。
- [分层详细设计](detail/README.md)：对象、状态、算法和验收。
- [项目任务看板](../tasks/README.md)：当前实现顺序和任务状态。

本文件解决空间问题；专题文档解决行为问题。新功能先在本文件确定落位，再按详细设计实现。规划目录不是现有代码，未开始的任务不创建空目录。

## 2. 工程模块边界

Aether 的模块边界由四项共同决定：

1. 目录边界：公共头、实现、私有头和测试按模块归档。
2. 命名边界：公共函数使用模块前缀，避免符号冲突。
3. 头文件边界：模块只能包含允许的公共头；私有头不能被其他模块引用。
4. CMake target 边界：链接依赖由 target 明确表达，不能靠相对路径或全局 source 列表绕过。

目录不是装饰。每个目录必须对应真实职责、依赖方向、资源所有者、测试范围和任务责任。任何模块都不得通过 `#include "../../other/src/internal.h"` 穿透边界。

### 文档落位

| 位置 | 固定内容 |
| --- | --- |
| 根目录 | `README.md`、`AGENTS.md`，以及必要构建/版本控制配置；不继续堆放规范文档 |
| `docs/` | `设计2.0.md`、`ARCHITECTURE.md`、`INTERFACE_CONTRACTS.md`、`CODING_STANDARDS.md` |
| `docs/design/`、`docs/design/detail/` | 概要、代码落位和分层详细设计 |
| `docs/tasks/`、`docs/adr/` | 任务/执行记录及架构决策 |

四份规范不得移回根目录或复制两份。目录调整后，修改引用方到文件实际位置的相对路径；不能为兼容旧链接保留根目录副本。文档新增遵循同一规则，根目录 Markdown 入口以此表为准。


## 3. 目标目录总图

这是项目完成后的**规划形态**，不是要求现在一次性创建所有目录：

```text
aether/
├── apps/
│   └── desktop/                    # 桌面组合根和进程入口
│       └── main.c
│
├── include/aether/                 # 公开 C 头文件
│   ├── foundation/                 # 无业务基础能力
│   │   ├── status.h
│   │   ├── id.h
│   │   ├── error.h
│   │   └── string_view.h
│   ├── contracts/                  # 跨层公共 DTO/事件（实际需要时建立）
│   ├── core/                       # 领域公开端口和不透明对象
│   │   ├── core.h
│   │   ├── workspace.h
│   │   ├── session.h
│   │   └── message.h
│   ├── backend/                    # Backend 对外 Command/Query/Event
│   ├── adapters/                   # Adapter 公开 Provider/协议端口
│   ├── infrastructure/             # 技术能力公开端口
│   └── platform/                   # 平台抽象公开端口
│
├── src/                            # 实现代码，按边界分包
│   ├── foundation/
│   │   ├── id.c
│   │   ├── error.c
│   │   └── string_view.c
│   ├── core/
│   │   ├── core.c
│   │   ├── workspace.c
│   │   ├── session.c
│   │   ├── message.c
│   │   └── internal/               # 仅 Core 内部可见
│   ├── backend/
│   │   ├── api/
│   │   ├── requests/
│   │   ├── services/
│   │   ├── events/
│   │   └── mapping/
│   ├── adapters/
│   │   ├── runtime/<name>/
│   │   ├── protocol/<name>/
│   │   └── tools/
│   ├── infrastructure/
│   │   ├── process/
│   │   ├── filesystem/
│   │   ├── persistence/
│   │   ├── configuration/
│   │   ├── logging/
│   │   ├── async/
│   │   ├── network/
│   │   └── security/
│   └── platform/
│       ├── common/
│       ├── windows/
│       ├── macos/
│       └── linux/
│
├── tests/
│   ├── foundation/
│   ├── core/
│   ├── backend/
│   ├── adapters/
│   ├── infrastructure/
│   ├── contract/
│   ├── integration/
│   └── e2e/
│
├── third_party/                    # 受控第三方源码，不放自研业务逻辑
├── tools/                          # 开发检查/生成工具，不是运行时业务
└── docs/                           # 设计、任务、ADR和验证记录
```

**只建立当前任务需要的最小子树。** 例如开始 Foundation 任务时创建 `include/aether/foundation/`、`src/foundation/`、`tests/foundation/`；不因为规划中存在 `src/backend/` 就提前创建空 Backend。

## 4. 模块落位矩阵

| 功能 | 公共头 | 实现目录 | 测试目录 | 允许依赖 | 禁止放置 |
| --- | --- | --- | --- | --- | --- |
| ID、状态、错误、字符串视图 | `include/aether/foundation/` | `src/foundation/` | `tests/foundation/` | ISO C 标准库 | Core 状态、UI、数据库、Runtime |
| Workspace、Session、Message、Permission | `include/aether/core/` | `src/core/` | `tests/core/` | Foundation、Core 自己定义的抽象端口 | Backend 编排、协议解析、OS API、UI |
| Backend Command/Query/Event | `include/aether/backend/` 或 `contracts/` | `src/backend/api/`、`requests/`、`services/`、`events/`、`mapping/` | `tests/backend/` | Core 公共接口、契约、注入的技术端口 | UI 控件、具体 Runtime、SQL/OS 句柄 |
| Runtime Provider 与 Adapter | `include/aether/adapters/` | `src/adapters/runtime/<name>/` | `tests/adapters/<name>/` | Core Provider、Infrastructure 端口、选定协议 | Core 领域状态、Backend 私有结构、UI |
| 通用协议解析 | `include/aether/adapters/` | `src/adapters/protocol/<name>/` | `tests/adapters/protocol/<name>/` | 标准 C、协议测试数据 | 某个 Runtime 的领域策略；未证明复用前不抽通用层 |
| MCP、Skills、Tool 映射 | Core 领域端口在 `include/aether/core/`；协议端口在 `include/aether/adapters/` | `src/adapters/tools/` 或实际执行边界 | `tests/adapters/`、`tests/core/` | Core 权限、Infrastructure IO | 把 Runtime 内部能力伪装成 Aether 执行 |
| 进程、文件、网络、存储、配置 | `include/aether/infrastructure/` | `src/infrastructure/<capability>/` | `tests/infrastructure/<capability>/` | Platform、已批准第三方库 | Core 业务规则、Frontend |
| OS 差异 | `include/aether/platform/` | `src/platform/<os>/` | `tests/infrastructure/platform/` | OS C API | Workspace/Session/Runtime 业务逻辑 |
| 桌面入口 | 通常不提供公共头 | `apps/desktop/` | `tests/e2e/` 或应用烟测 | 已组装 Backend/Core/Adapter/Platform | 领域状态机、数据库实现 |
| 代码检查和生成 | 不进入运行时公共头 | `tools/` | 工具自身的最小测试或临时验证 | Python/标准工具 | 运行时业务、业务状态、生产凭据 |

## 5. 每个模块的固定组成

一个真实模块按需要包含以下内容：

```text
<module>/
├── include/aether/<module>/       # 仅公开稳定接口
├── src/<module>/                  # 实现
│   └── internal/                  # 私有头，仅本模块构建目标可见
├── tests/<module>/                # 行为测试
└── CMakeLists.txt                 # 只有模块复杂到需要时才建立
```

不要为一个只有一个实现、没有替换需求的内部函数创建工厂、Provider 或额外接口。模块拆分依据真实职责：状态所有者、协议解析、数据映射、IO资源、应用编排分别处理；不能按固定行数机械切片。

### 公共头文件规则

- 路径必须使用 `include/aether/<module>/`，不把公共头散落在 `src/`。
- 只暴露调用方需要的类型和函数；对象优先使用不透明结构体。
- 只能包含标准头和允许的公共头，不能包含 `.c`、`internal/` 或第三方实现头。
- 字段、所有权、线程安全和生命周期必须与 [INTERFACE_CONTRACTS.md](../INTERFACE_CONTRACTS.md) 一致。
- 修改公共头必须搜索全部引用、同步调用方和测试，并记录接口影响。

### 私有头文件规则

- 放在 `src/<module>/internal/` 或同模块实现目录。
- 私有函数优先 `static`；确需跨多个本模块 `.c` 文件共享时才使用私有头。
- CMake 用 `PRIVATE` include 路径暴露给本模块，不把私有路径传给消费者。
- 禁止其他模块通过相对路径引用私有头；需要跨边界时设计公开端口，而不是偷偷共享实现结构。

## 6. CMake target 规划

目标依赖应形成单向图：

```text
foundation
    ↑
core + core_ports
    ↑
backend_contracts / infrastructure_ports / adapter_ports
    ↑
backend / adapters / infrastructure / platform
    ↑
desktop app
```

实际链接方向需按消费者定义端口调整，但必须满足架构规范：

| Target | 公开内容 | 私有实现 | 允许链接 |
| --- | --- | --- | --- |
| `aether_foundation` | `include/aether/foundation` | `src/foundation` | 标准 C |
| `aether_core` | `include/aether/core` | `src/core` | `aether_foundation`、Core端口依赖 |
| `aether_contracts`（需要时） | DTO/Event公共头 | 契约映射实现 | Foundation |
| `aether_backend` | Backend API | requests/services/events/mapping | Core、Contracts、注入端口 |
| `aether_adapter_<name>` | Adapter接口/测试所需端口 | 某Runtime实现 | Core、Infrastructure端口、协议实现 |
| `aether_infrastructure_<name>` | 技术能力端口 | 具体库/IO | Platform、必要第三方库 |
| `aether_platform_<os>` | Platform端口 | OS实现 | OS SDK/标准库 |
| `aether_desktop` | 无公共库，应用入口 | `apps/desktop` | 已组装目标 |

当前骨架可以暂时由 `aether_core` 包含 Foundation/核心的最小实现；当 Foundation 真实合同落地时，必须拆成独立 `aether_foundation` target，而不是继续把所有基础代码堆进 `aether_core`。

每个独立可测试模块有自己的测试 target。测试 target 只能链接被测 target 和明确测试依赖；不通过测试 target 访问另一个模块的私有实现。

## 7. 新功能落位决策树

新增功能前按以下顺序回答：

1. **它维护谁的权威状态？**
   - Workspace/Session/Message/Permission/Tool 规则 → `core`。
   - 应用请求、Command/Query、DTO映射、订阅 → `backend`。
   - 外部协议、Runtime连接、外部ID → `adapters`。
   - 文件/进程/网络/数据库/日志实际资源 → `infrastructure`。
   - Windows/macOS/Linux差异 → `platform`。
   - 页面、ViewModel、控件、用户交互 → `frontend`（当前按任务实际建立）。
2. **它是否只是跨层数据合同？** 放 `contracts/` 或对应公共头，不放某个具体实现目录。
3. **它是否是同一模块内部辅助？** 放该模块 `src/<module>/internal/`，不要扩大公共 API。
4. **它是否真的需要替换/多实现？** 只有真实边界或测试替换需求才定义接口；否则使用普通 C 函数。
5. **它是否会调用 OS 或第三方库？** 放 Infrastructure/Platform/Adapter 边界，不让依赖渗入 Core。
6. **它是否是未来能力但当前没有调用方？** 不创建空文件、空 target 或 TODO；等对应任务开始。

如果一个功能同时命中多个层，拆成多个有真实职责的模块，通过已评审接口交接；不能把所有代码放到“功能名称”目录下的单个 Manager。

## 8. 典型功能的固定落位

### 新增一个 Runtime

```text
include/aether/adapters/runtime.h        # Core定义的Provider端口（如已有则复用）
src/adapters/runtime/codex/
├── lifecycle.c                           # 实例/连接/关闭
├── mapping.c                             # 外部ID与代次
├── normalize.c                           # 外部事实→统一事实
├── codec.c                               # 真实协议帧处理
└── internal/                             # 仅该Adapter私有
 tests/adapters/codex/
```

不要把 Codex 代码放到 `src/core/`、`src/backend/` 或 `src/runtime.c` 巨型文件中。第二个 Runtime 创建第二个 Adapter，不修改 Core 主体。

### 新增一个 Backend 用例

```text
include/aether/backend/<command>.h       # 仅当是公共合同
src/backend/api/<command>_entry.c        # 参数验证与受理
src/backend/services/<name>_service.c    # 用例编排
src/backend/mapping/<name>_dto.c         # Core结果→DTO
src/backend/events/<name>_events.c       # 事实映射/订阅（如需要）
tests/backend/<name>_test.c
```

请求状态放 Backend；Session/Message 规则仍放 Core；保存由 Infrastructure 端口执行。禁止在 UI 回调里直接实现用例。

### 新增持久化能力

```text
include/aether/infrastructure/persistence.h  # 窄端口，由消费者定义/复用
src/infrastructure/persistence/<backend>/    # 具体数据库/文件实现
src/infrastructure/persistence/migrations/   # 真实迁移（如选定）
tests/infrastructure/persistence/
```

表结构、事务、迁移和保存成功语义由数据任务确定；不能把 SQL 直接写入 Core 或 Backend service。

### 新增页面

```text
src/frontend/<page>_vm.c                  # 页面投影和交互状态
src/frontend/<page>_view.c                # 组件/布局（框架确定后）
src/frontend/<page>_client.c              # 只有确有页面专用映射时
 tests/frontend/<page>/
```

页面只通过 Backend Client 使用 DTO/Event；不能读取 Store、日志文件或 Runtime。

## 9. 禁止落位

以下路径和做法禁止作为“临时方便”：

- 禁止把新业务代码继续堆到根目录 `src/`。
- 禁止创建 `src/manager.c`、`src/common.c`、`src/utils.c` 承担多个层的职责。
- 禁止把 Core、Backend、Adapter、Infrastructure 全部放进一个 target。
- 禁止把第三方库调用放进 Core 或 Frontend。
- 禁止把平台 `#ifdef` 散落在 Core/Backend。
- 禁止把 Runtime 原始协议对象、OS句柄、数据库连接暴露到公共 DTO。
- 禁止把测试、临时脚本、生成代码和第三方源码混入生产模块。
- 禁止用 `TODO`、空函数、Mock 目标或未接线目录表示模块已经建立。
- 禁止因文件接近500行而机械切片；先按职责拆分，保持唯一状态机和资源所有者。

## 10. 新文件提交前检查

- [ ] 已在本文件找到明确落位，且没有更近的现有模块可复用。
- [ ] 文件属于一个真实模块，一个主要职责，一句话可描述。
- [ ] 公共/私有头文件位置正确，include方向合法，无相对路径穿透。
- [ ] CMake target、PUBLIC/PRIVATE include和链接依赖正确。
- [ ] 没有新增第二套DTO、状态机、错误合同或资源所有者。
- [ ] 已同步任务表、设计文件、测试和全部调用方。
- [ ] 实现不超过500行；超过300行已完成职责审查；C行宽符合80/100规则。
- [ ] 有对应行为验收，不以编译通过、文件存在或函数被调用作为完成证明。
- [ ] 运行结果、平台、未验证项和后续交接写入执行记录。

## 11. 当前落位

当前已存在代码按本规划应整理为：

```text
include/aether/foundation/status.h
include/aether/foundation/id.h
include/aether/foundation/error.h
include/aether/core/core.h
src/foundation/id.c
src/foundation/error.c
src/core/core.c
tests/foundation/foundation_smoke.c
tests/core/core_smoke.c
apps/desktop/main.c
include/aether/backend/backend.h
include/aether/backend/request.h
src/backend/backend.c
src/backend/internal/backend_internal.h
src/backend/requests/request.c
tests/backend/backend_smoke.c
tests/backend/request_test.c
```

Backend生命周期与请求记录分别位于模块入口和requests目录。Platform公共头位于`include/aether/platform/`，路径值/内存归属位于`src/platform/common/`，OS目录和时钟查询位于`windows/`及`posix/`，私有结构位于`internal/`；target仅依赖Foundation和OS库。Frontend、Adapters、Infrastructure仍按真实任务建立，不提前创建空模块。

Core Registry公开头`include/aether/core/runtime_registry.h`；内部布局`src/core/internal/runtime_registry_internal.h`；`src/core/runtime/registry.c`负责存储/Provider，`runtime.c`负责实例代次状态，`reference.c`负责借用寿命。它们进入`aether_core`，依赖仍只有Foundation，私有头不传给消费者。

当前UI实现位于`apps/desktop/nappgui/`，构建为可选`aether_frontend_nappgui`；GTK/EFL原生选型实验已删除，不保留并行UI实现。该入口暂不链接Core/Backend，正式Backend Client和UI Adapter接入仍须按UI-01/UI-02验收，不得把原型窗口标作完整产品。

ENG-05新增`include/aether/infrastructure/executor.h`及`src/infrastructure/async/`：executor.c拥有构造/关闭资源，work.c拥有任务调度/取消/结果交接，timer.c拥有单调到期事实；私有共享状态只在executor_internal.h。`aether_executor`依赖`aether_platform`并经其依赖Foundation，不链接Core/Backend/UI；子目录CMake独立维护该模块target及行为/故障测试，根CMake只装配入口。

原生monitor/thread公开边界在`include/aether/platform/thread.h`；实现分别位于`src/platform/windows/thread.c`与`src/platform/posix/thread.c`，Win32使用SRW锁/条件变量及CRT安全线程，POSIX使用默认非robust mutex/condition与pthread；1MiB栈由创建参数设置。无OS类型泄漏到Infrastructure/Core。测试落在`tests/infrastructure/`，不建立未接通的Process/Store空模块。

APP-04消息V2保留`src/core/message.c`负责注册表、生命周期与独立快照；`src/core/messages/apply.c`是唯一归并/终态入口，`payload.c`负责类型化载荷校验和事务复制，`association.c`负责工具配对及引用释放约束。私有结构位于`src/core/internal/message_internal.h`，不扩大公共头内部结构；仍进入`aether_core`且只依赖Foundation。消息流/内容块/分配失败测试由`tests/core/CMakeLists.txt`装配，根CMake只保留测试子目录入口。
