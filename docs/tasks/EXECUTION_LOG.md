# Aether 任务执行与交接记录

任务状态唯一来源为 [各阶段任务表](README.md)，本文件记录负责人、过程、证据和阻塞；不维护另一份状态总表。

## 记录方式

每次认领、重大调整、阻塞、验证和交接追加一条记录；同一天同任务可用序号区分，不覆盖旧失败证据。记录只写实际发生的事实，不能提前填“通过”。负责人可以是明确的人或AI会话标识，不能默认所有任务都由当前AI永久负责。

每条记录使用如下字段说明组织正文，无需复制空字段到全部任务：

| 字段 | 必须填写的事实 |
| --- | --- |
| 任务ID / 记录日期 / 负责人 | 对应阶段任务行、实际日期和承担者 |
| 记录类型 | 认领、进展、待确认、阻塞、验证、完成或回归；不是第二份当前状态 |
| 依赖与基线 | 已完成前置的记录位置、已确认契约/ADR、实际仓库状态 |
| 实施范围 | 目标文件/模块、接口与调用方、明确非目标 |
| 设计与场景 | 对应设计章节和具体FE/BE/CORE/RT/DATA/EXEC/OBS/INT用例 |
| 实际变更 | 做了什么、行为差异、资源/数据/权限影响；可附提交标识 |
| 验证证据 | 平台/版本、实际命令或UI步骤、输入、预期/实际结果、持久证据文件位置 |
| 文件规模/例外 | 修改代码物理行数、超过300行的职责判断、行宽等已记录例外 |
| 决策或阻塞 | 缺失条件、已尝试方法、影响范围、需要谁提供什么、解除条件 |
| 交接与下一步 | 可复用产物、剩余工作、下一就绪任务；不要把阶段完成写成项目完成 |
| 完成审查 | 是否满足所有交付/验收、未验证范围为何不影响本任务或为何仍阻塞 |

验证输出过长时，在实际执行后按需保存到 `docs/tasks/evidence/` 或仓库已有持久CI产物并链接；不预建空证据文件，不只引用会过期的工具输出URI。证据脱敏，不存真实凭据、完整用户正文或敏感路径。

## 执行记录

### 2026-11-07 · DEC-01 · 完成

- 记录类型：基线核实/完成。
- 负责人：当前AI会话。
- 依赖与基线：仓库在本轮开始时只有规范文档；已读取任务看板、概要、详细设计和工程入口。
- 实施范围：确认先交付P1最小C17/CMake骨架；未创建Frontend、Backend、Runtime、MCP或存储空模块；P2–P7仍由独立任务跟踪。
- 验收：仓库范围和当前阶段边界已写入 `docs/tasks/00-decisions.md` 的DEC-01；没有缩减后续项目方向。
- 交接：后续以DEC-02环境证据和ENG-01工程骨架为基础。

### 2026-11-07 · DEC-02 · 完成

- 记录类型：工具链核实/完成。
- 负责人：当前AI会话。
- 实际环境：CMake 4.2.3；Clang 22.1.4，目标为x86_64-w64-windows-gnu；Ninja 1.13.2；Windows本机验证。
- 决定范围：当前采用CMake Preset `default`、Ninja、Debug、Clang和CTest作为Windows开发入口；未宣称macOS/Linux验证，也未选定第三方业务依赖。
- 验收：`cmake --preset default` 实际配置成功。后续ENG-01补充编译、测试与入口运行证据。
- 交接：三平台工具链和实际CI仍由后续任务/发布验收补充。

### 2026-11-07 · ENG-01 · 完成

- 记录类型：认领/实现/验证/完成。
- 负责人：当前AI会话。
- 设计范围：P1最小C17工程；不实现业务领域、Frontend、Backend、Runtime或第三方依赖。
- 实际变更：
  - `CMakeLists.txt`：C17、严格警告、`aether_core`静态库、桌面入口、CTest smoke目标。
  - `CMakePresets.json`：Windows当前Clang/Ninja Debug入口。
  - `include/aether/core.h`：最小不透明Core对象和状态接口。
  - `src/core.c`：创建、状态查询、销毁；显式失败和NULL安全。
  - `apps/desktop/main.c`：实际桌面骨架入口。
  - `tests/core_smoke.c`：创建、查询、无效参数和重复销毁烟测。
  - `.gitignore`：忽略生成的`build/`目录。
- 验证命令与实际结果：
  - `cmake --preset default`：成功，生成 `build/default`。
  - `cmake --build --preset default`：首次因测试缺少 `<stddef.h>` 失败；补充标准头后再次执行成功，构建3个目标。
  - `ctest --preset default --output-on-failure`：1/1测试通过，0失败。
  - `./build/default/aether_desktop.exe`：输出 `Aether desktop skeleton ready.`，退出成功。
  - `wc -l CMakeLists.txt CMakePresets.json include/aether/core.h src/core.c apps/desktop/main.c tests/core_smoke.c`：最大文件86行，低于500行；当前仅有CMake文件和最小C代码，无行宽例外。
- 未验证项：macOS/Linux构建和运行；代码规模/行宽自动检查工具（ENG-02）；真实业务功能。
- 交接：下一可实施任务为ENG-02或ENG-03；ENG-03仍需DEC-03，不能因骨架存在提前实现完整错误/ID合同。
- 完成审查：ENG-01交付物和Windows验收满足；未把跨平台或业务阶段标为完成。

### 当前模块化重组与GitHub首提交 · 完成

- 任务范围：按 `docs/design/CODE_LAYOUT.md` 将 Foundation/Core 公共头、实现和测试归档；建立 `aether_foundation` / `aether_core` CMake target边界；创建 `README.md` 与 GitHub 项目描述文档。
- 实际落位：`include/aether/foundation/`、`src/foundation/`、`tests/foundation/`、`include/aether/core/`、`src/core/`、`tests/core/`、`apps/desktop/`。
- Git：初始化 `main` 分支，远程为 `https://github.com/ouraihub/aether-agent-desktop.git`，首提交 `0f24551`，已推送并通过远程 `main` 分支核对。
- 验证：模块化重组后 Windows CMake 配置、编译、CTest 3/3、桌面入口和 `python tools/check_code.py` 实际通过；最大手写文件低于500行，无行宽例外。
- 未验证：macOS/Linux、业务Backend/Frontend/Runtime/持久化；本次GitHub推送只交付当前工程骨架和文档，不代表产品完成。
- 下一步：DEC-04/DEC-06，之后按任务依赖实现最小真实 Backend；不能先创建其他空模块。

### 2026-11-07 · DEC-03 / ENG-02 · 完成

- 记录类型：基础合同/质量检查/验证/完成。
- 实际变更：Foundation 模块实现 `AetherId`、`AetherStatus` 和固定容量 `AetherError`；`tools/check_code.py` 检查源码物理行、C 行宽、Tab、行尾空白及500行上限，并作为CTest `aether_code_quality`。
- 模块落位：`include/aether/foundation/`、`src/foundation/`、`tests/foundation/`；Core 位于 `include/aether/core/`、`src/core/`、`tests/core/`。CMake targets 为 `aether_foundation` 与 `aether_core`，依赖单向。
- 验证：Windows Clang/CMake/Ninja 下 `cmake --build --preset default`、`ctest --preset default --output-on-failure`（3/3通过）、`python tools/check_code.py`（12个源码文件通过）。
- 合同边界：ID当前为进程实例范围，不宣称跨进程/持久化全局唯一；错误为固定容量复制，超长拒绝；完整跨层ID/错误合同仍由后续DEC/接口评审扩展。
- 未验证：macOS/Linux；Platform、执行域、Core Session/Message状态、Backend和真实业务。
- 交接：DEC-04、DEC-06仍是下一阶段；不要在其完成前创建依赖执行器/领域状态的Backend空模块。

### 最小Backend/Core边界 · ENG-11 · 完成

- 实际落位：`include/aether/backend/backend.h`、`src/backend/backend.c`、`tests/backend/backend_smoke.c`；CMake target为`aether_backend`，只链接公开`aether_core`，Backend借用Core，不拥有或释放Core。
- 行为：Backend create/initialize/start/stop/close调用真实Core生命周期；无效参数和重复阶段由状态合同拒绝；未实现异步请求、DTO、Event、持久化、Runtime或UI，不创建空接口。
- 验证：`cmake --preset default`、`cmake --build --preset default`、`ctest --preset default --output-on-failure`（4/4通过）、`python tools/check_code.py`（15个源码文件通过）、`./build/default/aether_desktop.exe`运行成功。
- 未验证：macOS/Linux；DEC-04完整执行域/队列/关闭方案；DEC-06完整Workspace/Session/Message领域状态。
- 交接：下一步先完成DEC-04/DEC-06，再扩展Backend请求/DTO/事件；ENG-11不代表Backend产品能力完成。

### 文档目录纠正

- 按用户明确要求，四份规范固定保存在 `docs/`：`设计2.0.md`、`ARCHITECTURE.md`、`INTERFACE_CONTRACTS.md`、`CODING_STANDARDS.md`。
- 之前为修复引用将文件移回根目录的处理错误，现保留用户的目录安排，并修正入口、ADR、概要/详细设计和任务中的相对链接。
- 根目录只保留README和AGENTS两个Markdown入口；约束已写入AGENTS和代码落位规划。旧分包草稿改为导航，避免重复或冲突规则。

### ENG-12 · 本地有界请求记录 · 完成

- 范围：串行调用的同步8槽请求记录，不执行Runtime业务。submit清空失败输出、分配进程内ID；query复制快照；finish检查终态与结果配对且只能提交一次；release仅回收终态槽位；stop将未完成本地记录转Cancelled。完整APP-07仍未开始。
- 落位：公开合同 `include/aether/backend/request.h`；私有结构 `src/backend/internal/backend_internal.h`；请求逻辑 `src/backend/requests/request.c`；生命周期 `src/backend/backend.c`；回归场景 `tests/backend/request_test.c`。`aether_backend`的私有include路径不传给消费者，依赖仍为Backend → Core → Foundation。
- 实际验证：Windows Clang 22.1.4；`cmake --preset default`、`cmake --build --preset default`、`ctest --preset default --output-on-failure`成功（5/5）；`python tools/check_code.py`检查19个源码文件通过，无行宽例外。
- Clang Static Analyzer：`clang --analyze -std=c17 -Iinclude -Isrc/backend/internal -Xanalyzer -analyzer-output=text src/backend/backend.c src/backend/requests/request.c`退出0，无诊断；这不是动态内存检查。
- 独立行为烟测：临时无UI程序链接实际三个静态库，输出 `accepted id=1 state=0`、`completed state=1 result=0`、`released record; lookup=NOT_FOUND; backend closed`，退出0；临时源码和可执行文件已清理。
- 回归覆盖：容量满、终态不自动驱逐、显式释放再受理、旧ID不命中新记录、跨Backend隔离、快照副本、失败输出清空、非法终态配对、重复finish保持原结果、stop不覆盖已有终态。
- 手写文件行数：CMakeLists.txt 224；request.h 65；status.h 16；backend.c 118；backend_internal.h 22；request.c 124；backend_smoke.c 52；request_test.c 132。均为单一职责且低于300行，无500行/行宽例外。
- 未完成/未验证：完整异步任务、业务DTO/输入副本、deadline/事件/TraceContext、持久化及真实Runtime；macOS/Linux和动态内存工具未验证。现有进程内ID耗尽/重启语义仍由D-ID后续收敛，不宣称全局持久唯一。
- Git交付：文档目录修正已独立提交 `b327236`；本实现与本记录一起提交并推送main。不得把同步登记/手动终结报告为用户业务执行完成。

### 2026-11-08 · ENG-03 · 认领

- 记录类型：认领/实施计划。
- 负责人：当前AI会话。
- 依赖与基线：ENG-01、DEC-03及ENG-02已完成；当前工程为Foundation/Core/Backend C17静态目标，工作树认领前干净。
- 实施范围：在Foundation新增有界UTF-8 `AetherStringView`及复制/比较接口，补齐统一状态类别，增强固定容量 `AetherError`复制接口，并同步Foundation测试与CMake；不实现Platform、异步执行器、领域对象或动态容器。
- 设计与场景：`docs/design/detail/00-contract-baseline.md` §2.2；`docs/INTERFACE_CONTRACTS.md` §8–9；IM-01；覆盖空/非空视图、非法UTF-8、容量不足、错误复制和无动态资源释放路径。
- 资源边界：StringView只借用调用方UTF-8字节，调用方负责寿命；复制接口只写调用方提供的NUL终止缓冲区；AetherError固定容量、不持有动态内存，复制为值复制。
- 交接：实现后运行Windows配置、编译、CTest、质量检查和Foundation独立烟测；成功后提交并推送GitHub，再转下一个依赖满足任务。

### 2026-11-08 · ENG-03 · 完成

- 记录类型：实现/验证/完成。
- 实际变更：新增 `include/aether/foundation/string_view.h` 与 `src/foundation/string_view.c`，提供借用型有界UTF-8视图、严格编码验证、NUL终止复制和字节比较；扩展 `AetherStatus` 至接口规范要求的基础错误类别；增加固定容量 `aether_error_copy`；MSVC下将ID递增改为 `_InterlockedIncrement64`，保留非MSVC C17原子实现，解决该便携MSVC工具链未启用C原子支持的问题。
- 调用方与构建：`CMakeLists.txt`将字符串视图纳入 `aether_foundation`；Foundation smoke覆盖ASCII/UTF-8/非法编码/空指针/容量不足/错误复制，既有Core和Backend调用方只消费兼容的状态枚举。
- 资源与线程：StringView只借用输入字节且不保存；复制由调用方提供缓冲区；AetherError为固定值对象；ID生成在MSVC使用进程内原子递增，非MSVC使用C17原子；未新增动态资源或后台线程。
- 实际验证（Windows）：`D:\Applications\msvc-portable\msvc\setup_x64.bat`初始化MSVC 19.44.35228；CMake 4.4.5 + Ninja 1.13.2配置/构建成功；`ctest --test-dir build\\msvc-cmake --output-on-failure` 5/5通过；`build\\msvc-cmake\\aether_desktop.exe`输出 `Aether desktop skeleton ready.`；MSVC直接Foundation、Core、Backend请求烟测均退出0；Zig Clang C17 Foundation smoke退出0；`uv run --no-project python tools/check_code.py`等价质量检查通过（21个源码文件）；`git diff --check`通过。
- 文件规模/例外：受影响CMakeLists 225行；string_view.h 33行；string_view.c 104行；error.h 34行；error.c 53行；id.c 37行；status.h 21行；foundation_smoke.c 54行；均低于300行职责审查阈值和500行上限，无行宽例外。质量检查实际报告21个源码文件通过。
- 工具环境纠正：此前误执行过 `scoop install python` 和其随带的 `dark`；已执行 `scoop uninstall python dark` 清理。项目验证使用现有 `uv`（版本0.10.4，运行Python 3.13.7），未将工具安装写入仓库。
- 未验证：macOS/Linux；未运行动态内存分析器；CMake检测到Python后已通过CTest质量测试。ENG-03不包含Platform、执行器、异步、TraceContext或业务领域。
- 完成审查：ENG-03交付物和Windows/Clang行为证据满足本任务；下一可执行任务为DEC-04或DEC-06，ENG-04仍需DEC-04，未将基础值合同等同于完整Backend业务能力。

- 提交后复核：提交 `237f9bb` 已推送 `origin/main`。提交后首次直接运行旧CMake缓存时，CTest的缓存质量检查仍指向已卸载的临时Scoop Python，导致该测试未运行；随后重新配置时显式清除不可用解释器，MSVC构建、CTest 4/4和桌面入口均再次通过。独立 `uv run --no-project python tools/check_code.py`已通过，因此该缓存问题不属于源码失败。

### 2026-11-08 · Windows build reproducibility · 完成

- 记录类型：环境脚本/文档/验证。
- 实际变更：新增 `scripts/setup-msvc.bat`、`scripts/msvc-env.bat`、
  `scripts/build-msvc.bat` 和对应 `scripts/msvc-LICENSE.txt`；新增 `msvc`
  CMake configure/build/test presets；README补充新机器安装、便携MSVC下载、
  `uv`质量检查、干净构建和CMake缓存失效处理。
- 环境边界：脚本默认使用 `D:\Applications\msvc-portable\msvc`，支持
  `PORTABLE_MSVC_ROOT`；缺少工具链时使用Windows `curl`和`uv`下载MSVC
  14.44/Windows SDK 26100 x64。工具链不复制进仓库，遵守微软组件许可。
- 实际验证：`scripts\\build-msvc.bat --clean`完成MSVC配置、19个构建步骤、
  CTest 4/4、`uv run --no-project python tools/check_code.py`（21个源码文件）
  和桌面入口运行；`cmake --list-presets=all`列出default/msvc三类预设。
- 未验证：新机器实际下载流程未重复下载（当前机器已有工具链）；macOS/Linux
  未验证。

### 2026-11-08 · DEC-04 · 认领与调查

- 记录类型：认领/调查。
- 依赖与基线：DEC-02已完成；当前Aether已有同步Backend请求记录，但没有异步线程、队列、Timer或Process Service实现。
- 调查范围：Windows x64执行域、线程/条件变量、停止/Join、任务所有权和回调关闭边界；不引入libuv、线程池或生产异步API，不把临时试验当产品实现。
- 设计依据：`docs/design/detail/06-platform-execution.md` §2、§5、§9；`docs/ARCHITECTURE.md` §5；`docs/INTERFACE_CONTRACTS.md` §10。
- 当前结论：业务状态应由单一拥有者串行处理；IO/工作线程只投递有界事实；队列满必须显式拒绝；取消请求、底层停止、资源关闭和close-complete必须分开表达；UI不得等待Join。线程数、队列容量、跨平台抽象和强制终止策略仍未批准。
- 可调查候选：当前便携MSVC提供Windows线程API、`_beginthreadex`、SRWLOCK和Condition Variable；跨平台生产代码仍需Platform封装，不能把Windows头带入Core或Backend。
- 交接：运行一次Windows可控线程/条件变量试验；若证据满足，则将DEC-04转待确认并保留需项目负责人确认的选项，同时继续推进不依赖DEC-04的DEC-06。

### 2026-11-08 · DEC-04 · 验证/待确认

- 记录类型：验证/待确认。
- 实际试验：临时 `build/dec04_executor_probe.c` 使用便携MSVC 19.44的Windows `_beginthreadex`、SRWLOCK和Condition Variable；工作线程等待有界共享状态，主线程投递值21，工作线程产生42并通知，主线程随后设置停止事实、唤醒并等待线程退出。进程退出码0；临时源文件、对象和可执行文件已清理。
- 试验覆盖：后台工作不在提交线程执行；条件变量等待/唤醒；停止请求后不再接受工作；显式Wait/Join后释放线程句柄；没有在回调中释放仍被工作线程使用的状态。
- 结论边界：Windows原生机制可支撑候选执行域，但本试验没有批准固定线程数、无界队列、业务取消终态、Timer代次、Process Service、UI等待策略或跨平台实现。生产代码仍必须通过Platform边界隔离Windows API；Core/Backend不得包含Windows头。
- 未验证：macOS/Linux线程语义；队列满、取消竞争、deadline generation、关闭中回调、进程管道和资源压力；DEC-04因此进入待确认，不标记已完成。
- 待确认事项：项目负责人需确认（1）状态执行域采用单线程事件循环还是其他明确拓扑，（2）队列/任务容量和满载策略，（3）取消请求与实际停止/清理的可观察分离，（4）关闭等待截止及遗留资源责任，（5）Windows/POSIX Platform封装范围。确认后同步正式契约，再实施ENG-04/ENG-05。

### 2026-11-08 · DEC-06 · 认领与调查

- 记录类型：认领/调查/待确认。
- 依赖与基线：DEC-03已完成；DEC-06不依赖DEC-04。现有Core仅提供组件生命周期骨架，没有Workspace、Runtime Registry、Session或Message领域对象。
- 调查范围：`docs/design/detail/03-core.md` 全部领域关系、Runtime描述/实例区分、Workspace/Session状态表、Core验收边界及接口契约的Session/Message要求；不创建未确认的公共ABI，不把同步Core生命周期误报为领域状态实现。
- 候选收敛：第一阶段不单独创建AgentRun，除非后续出现独立查询/关闭责任；RuntimeDescription与RuntimeInstance分离，实例以`runtime_id`和`connection_generation`识别；Session必须绑定Workspace和Runtime代次；默认只允许Idle创建turn，Active并发需能力和契约明确；未知内容块拒绝，不静默转Text；普通turn结束回Idle，连接失效为RuntimeUnavailable，会话上下文中断为Interrupted，显式关闭进入Closing后才Closed。
- 必须确认：上述候选涉及公共状态和消费者行为，项目负责人需确认（1）AgentRun是否保留独立实体，（2）Active是否允许并发turn及能力条件，（3）Session初始化/恢复/关闭的外部成功边界，（4）未知ContentBlock拒绝还是受限保留，（5）Runtime重连时旧Session绑定失效或允许恢复的规则。未确认前不实现APP-01/APP-02/APP-03/APP-04。
- 未实现/未验证：未新增Core代码或空模块；未进行Runtime、持久化、权限和消息流实现。DEC-06进入待确认，不标记完成。
- 下一步：等待领域公共语义确认；在DEC-06确认前，DEC-07/DEC-08/APP-01及其后续领域任务按硬依赖保持未开始。

### 2026-11-08 · DEC-04/DEC-06 · Pi与Autorail参考调查

- 记录类型：外部实现调查/决策输入，不是实现完成。
- Autorail证据：`D:\workspace-ai\autorail\docs\design\native-multi-agent\03-architecture.md` §4采用“每个聊天一个引擎子进程”；退出顺序为停止新请求、取消待答交互、关闭stdin并等待有限宽限期，超时才终止进程树；`crates/agents/src/session.rs`使用原子`disposing/closed`、有界`mpsc::sync_channel(1)`回复等待、显式`begin_dispose`、有限等待后kill；`crates/agents/src/process.rs`将stdin、stderr、wait拆成独立线程并维护退出/管道关闭事实。该实现支持Aether候选的显式状态、关联ID、晚到响应丢弃和有限关闭，但它是Rust/Autorail实现，不能直接作为Aether C17依赖或已批准库。
- Pi证据：`D:\workspace-ai\pi\README.md`说明RPC模式是正式控制入口；Pi源码中`prompt`先检查当前回合空闲，发送`prompt`后由`agent_start/agent_settled`报告运行边界；`abort`是显式停止命令；会话通过`sessionFile`和历史恢复；Pi没有内建权限沙箱，不能把其默认权限模型当作Aether权限合同。当前Pi仓库的完整队列/关闭细节由RPC会话和引擎负责，Aether只能复用外部事实，不假设能控制Pi内部所有工具执行。
- 形成候选：DEC-04采用“单一领域状态拥有者 + 有界事实/回复通道 + 显式取消 + 有限优雅关闭后再强制终止 + close-complete后释放回调上下文”；不锁定线程库、固定线程数或Pi内部实现。DEC-06暂不建立独立AgentRun；Session默认单回合，只有Runtime能力明确支持并发才允许；普通回合结束回Idle；连接/进程退出进入RuntimeUnavailable或Closed的明确分支；旧会话通过Runtime真实session locator恢复，不把本地历史等同外部恢复；未知内容块拒绝或显式Unsupported，不转Text。
- 未解决：以上是参考证据和候选，不是Aether重大决策批准。DEC-04仍待确认；DEC-06仍待确认；没有新增Core公共ABI、Runtime适配器或Pi依赖。

### 2026-11-08 · DEC-04/DEC-06 · 正式收敛与交接

- 记录类型：用户确认后的决策收敛/完成审查/交接。
- 用户指定参考：继续参考 `D:\workspace-ai\autorail` 与 `D:\workspace-ai\pi`。调查结果已转入 ADR 0004 和正式架构/接口契约；没有复制Rust实现、没有引入Pi依赖、没有宣称控制Pi内部权限。
- DEC-04完成边界：单一逻辑状态拥有者；IO/工作线程通过有界事实交接；取消受理、底层停止、资源关闭和close-complete分离；先有限优雅关闭，随后才可按已批准策略强制终止；UI不等待Runtime或Join。具体物理线程、平台封装、队列数值仍由ENG/DEC-11实测。
- DEC-06完成边界：Runtime描述/实例分离；`runtime_id + connection_generation`；不建立独立AgentRun；Session默认单回合，能力明确时才允许并发；普通回合结束回Idle；真实session locator才代表外部恢复；未知ContentBlock不转Text。具体领域代码仍由APP-01至APP-04实现。
- 正式文件：`docs/adr/0004-execution-and-session-boundaries.md`、`docs/ARCHITECTURE.md` §5、`docs/INTERFACE_CONTRACTS.md` §10。
- 下一任务：DEC-05已认领进行中。范围限定为Backend RequestRecord与Event/OpenView交接语义；不实现Runtime、UI或Core领域对象，不冻结具体异步库。

### 2026-11-08 · DEC-05 · 认领与范围

- 记录类型：认领/实施范围。
- 依赖与基线：DEC-03、DEC-04已完成；ADR-0004和接口契约§10已生效。现有ENG-12仅为同步8槽本地记录，不能代替DEC-05的异步请求、事件订阅和退订合同。
- 调查范围：`docs/design/detail/00-contract-baseline.md` §3–4、`docs/design/detail/02-backend.md`、`docs/design/detail/01-frontend.md`及现有Backend请求代码；只收敛请求/事件公共语义，不实现Runtime、UI或Core领域对象。
- 候选规则：Backend是请求终态唯一裁决者；受理成功与业务完成分离；取消请求只设置取消事实，实际Cancelled需底层确认；deadline先由同一拥有者检查并裁决；迟到结果只更新执行/清理事实，不改已提交终态；每次OpenView原子登记订阅并返回快照、revision、subscription_id和sequence基线；队列满/gap使视图ResyncRequired；Unsubscribe后不启动新回调，close-complete后释放回调上下文。
- 待确认验收：需列出受理/完成/取消/deadline竞争的确定顺序、RequestRecord查询保留期、事件sequence作用域、快照与增量交接、队列满策略、回调线程/重入规则和退订close-complete保证。完成前不扩展APP-06/APP-07/APP-08公共ABI。

### 2026-11-08 · DEC-05 · Pi/OMP请求事件参考

- 记录类型：外部实现调查/决策输入。
- OMP证据：`D:\workspace-ai\oh-my-pi\sdk\python\omp-rpc\README.md` 将 `prompt` 的响应确认与后续 `prompt_result` 终态分开；`session_settled` 只在后台任务和队列都排空后表示会话真正稳定；`prompt_and_wait` 按自己的request ID等待，不能被其他回合的迟到`agent_end`提前结束。`D:\workspace-ai\oh-my-pi\sdk\go\omp-rpc\client.go` 以request ID关联响应，关闭取消在途Host调用、关闭stdin并有界等待/终止进程；`Frames`保留未消费通知，连接关闭后再排空。`frame.go`严格校验分片序列，坏序列关闭连接。`wire.go`定义`prompt`受理、`prompt_result`完成、`abort`、`session_settled`、steering/follow-up队列和`isSettled`。
- Pi对照：Pi/Autorail的成熟共同点是“受理确认不等于完成”“按请求ID关联”“回合结束与会话真正settled分离”“取消可观察但不伪称底层立即停止”“关闭需先停止输入再有限收尾”。Aether采用这些语义，但不复制其语言、协议字段或无界Frames实现。
- DEC-05候选收敛：请求ID在Backend实例代次内唯一；受理返回request_id但不发布完成结论；每个受理请求最多一个逻辑RequestCompleted；完成/失败/取消/deadline竞争回同一状态拥有者，先检查deadline，再按有效事实顺序裁决；迟到事实不改终态，只更新执行/清理状态。事件sequence仅在单订阅内递增，revision表达领域对象版本；OpenView原子返回快照、base_revision、subscription_id、sequence基线；队列满或sequence缺口进入ResyncRequired；Unsubscribe关闭入口后不启动新回调，已有回调退出后发送一次close-complete。
- 未解决：RequestRecord终态保留期、队列/快照容量、事件Schema版本、回调线程/重入和具体deadline数值仍需DEC-11/正式API任务确认；不把Pi/OMP协议直接暴露给Aether Frontend。

### 2026-11-08 · DEC-05 · 待确认交接

- 记录类型：候选决策完成/待确认。
- 正式文件：新增 `docs/adr/0005-request-event-handoff.md`；同步 `docs/INTERFACE_CONTRACTS.md` §4.3；保留详细设计中的实现算法，不将Pi/OMP字段复制为Aether ABI。
- 候选规则：受理与完成分离；Backend单一终态裁决；按Backend实例代次内request_id关联；deadline先检查；迟到事实只补充执行/清理/保存；OpenView原子快照握手；单订阅sequence；满载/gap进入ResyncRequired；Unsubscribe关闭入口，close-complete后释放回调上下文。
- Pi/OMP参考：OMP的`prompt_result`、`session_settled`、request ID等待、严格chunk校验和有界关闭；Pi的prompt/abort与单回合边界。参考只用于语义证据，Aether不引入其协议、语言运行时或无界Frames缓存。
- 待确认：RequestRecord保留期、输入/事实/快照/订阅容量、事件Schema版本/时间基准、回调线程与重入、deadline默认值和控制请求完成语义。DEC-05转待确认，APP-06/07/08公共实现保持未开始。

### 2026-11-08 · DEC-07 · 权限调查/待确认

- 记录类型：认领/调查/待确认。
- 设计基线：`docs/design/detail/03-core.md` §7、`docs/design/detail/06-platform-execution.md` §7、`docs/ARCHITECTURE.md` §6；权限策略与实际执行检查必须分离，Aether不能把UI隐藏、Runtime名称或子进程边界当授权/沙箱。
- Pi证据：Pi官方README明确默认继承启动进程权限，没有内建文件、进程、网络或凭据限制；其Containerization文档区分整进程隔离和仅工具隔离，并警告挂载目录、环境变量、凭据目录及网络暴露决定真实边界。该证据支持Aether必须显示Runtime控制缺口，不得宣称Pi内部行为受Aether权限拦截。
- OMP证据：OMP文档/代码存在工具确认、Host Tool取消和运行环境能力，但成熟RPC客户端的确认只覆盖暴露给Host/Adapter的调用；不能据此推断任意Runtime内部Shell、文件或网络操作均受Aether控制。未知、未暴露或无法复核目标必须拒绝或明确Unsupported。
- Aether候选：Workspace/Session/Runtime/Tool组合范围；Deny > Ask > Allow，Inherit仅继续查找；Ask绑定不可变目标摘要、policy_revision、Runtime generation、期限和一次性nonce；执行前重查，执行拥有者原子消费一次；新Deny、代次变化、过期、撤销和关闭均使旧批准失效；读与写/删除分别授权；路径不能只用字符串前缀防越界，需评估符号链接/重解析点和TOCTOU。
- 待确认：项目负责人需确认策略继承/冲突、一次授权期限与无人响应、可控Runtime权限请求映射、路径强制机制、借用/受管进程环境边界和无法控制Runtime时的默认产品行为。DEC-07不实现权限代码，不宣称Pi/OMP提供沙箱。

### 2026-11-08 · DEC-08 · 数据与配置调查/待确认

- 记录类型：认领/调查/待确认。
- 设计基线：`docs/design/detail/05-data-config.md`、`docs/design/05-infrastructure-platform.md`、`docs/INTERFACE_CONTRACTS.md` §8、§12；当前Aether没有Store、配置、迁移、Secret Store或数据目录实现，未创建空模块。
- Pi证据：Pi把会话作为按工作目录分桶的版本化JSONL文件，自动保存、可继续/恢复/分支；文档明确历史会话与Prompt History分离，`--no-session`是显式易失模式。Session格式带版本、稳定ID、parent/leaf关系和迁移；持久化设计强调原子发布/损坏或旧版本处理，真实会话恢复依赖外部session文件而不是UI历史。Pi凭据与设置是独立配置边界，秘密不应混入普通会话记录。
- OMP证据：OMP Session文档采用版本化JSONL、cwd编码目录、blob store与独立history索引；Open记录保留未知字段以支持兼容，session文件、索引和运行时状态分离。其实现还展示原子生成数据替换、失败恢复、有限输出和显式保留/淘汰策略。Aether只吸收“版本/迁移/原子发布/独立秘密引用/执行与保存分离”的语义，不复制Node/Bun/Rust实现或默认选择JSONL/SQLite。
- Aether候选：Core拥有内存领域事实，Backend协调Store提交；Store只报告保存事实。记录分离Workspace、Runtime配置引用、Session外部关联、结构化Message/Block、execution_marker、config_entry、工具配置和schema_meta；临时授权不落盘，秘密只保存secret_ref。保存采用expected_revision乐观冲突，失败不提升saved_revision；外部副作用成功而保存失败分别报告，不盲目重放。迁移读取schema_version、加互斥、逐版本验证，失败不清空/重建；崩溃在途只恢复Unknown/需核实，不自动重发，外部Resume必须经真实Adapter验证。
- 待确认：负责人需确认本地Store选型与许可证/三平台能力、写入拥有者与事务边界、Session/Message保存时点、迁移备份/回滚、配置作用域/优先级/生效时点、系统Secret Store不可用时的行为、附件与用户原文件删除边界。DEC-08不标记完成，不实现DATA-01至DATA-04。

### 2026-11-08 · DEC-10 · 日志与诊断调查/待确认

- 记录类型：认领/调查/待确认。
- 设计基线：`docs/design/detail/07-observability.md`、`docs/INTERFACE_CONTRACTS.md` §13、`docs/CODING_STANDARDS.md` §4；当前Aether无TraceContext、Logger、日志文件、轮转或诊断Query实现，未创建空模块。
- 参考边界：Pi和OMP将会话、运行状态、错误与输出诊断分开，支持有界/截断提示、请求或会话关联；Autorail明确使用分层tracing、子进程退出和错误摘要脱敏。它们的库、格式和内部字段不直接成为Aether契约；Pi/OMP不证明Aether可见第三方Runtime内部trace。
- Aether候选：显式值型TraceContext，独立root/child span，跨队列/回调按值复制；结构化记录字段含module/component/layer/operation/event/trace/span/result/duration/error_code及适用业务ID；业务事件与日志分离；有效等级CLI > ENV > persisted > INFO；先判断阈值再构造昂贵字段；单writer、有界记录/队列/文件/总量/保留期，失败递增可查询健康/丢弃事实，不递归、不改变业务结果；Backend按trace和operation/layer/component/level有界查询，Frontend不读文件。
- 脱敏边界：不写API Key、Token、密码、Authorization/Cookie、完整Prompt/模型输出、工具参数正文、完整敏感路径或未经评估哈希；控制字符转义；Runtime不支持传播时只记录Aether本地调用和实例/连接作用域映射。
- 待确认：负责人需确认Trace ID编码/唯一性及生成失败、记录编码、输出目录权限、单条/队列/文件/总量容量、轮转与保留、级别覆盖参数、查询授权和关闭截止。DEC-10不标记完成，不实现ENG-08/ENG-09/APP-11。

### 2026-11-08 · DEC-10 · Pi/OMP/Autorail观测参考

- 记录类型：外部实现调查/决策输入。
- Autorail证据：`D:\workspace-ai\autorail\docs\design\native-multi-agent\06-logging.md`采用独立observability模块、按日文件轮转保留14个、固定时区时间戳、运行时级别切换、受控tail读取和`op{id,at}`操作span；`crates/observability/src/op.rs`用进程内原子编号和单调耗时，完成行区分成功/失败。其实现是Rust/tracing，不直接迁移到Aether C17。
- Pi/OMP证据：Pi提供可选TelemetrySpan、父span显式传递、请求/轮询/工具/运行汇总等无内容或有限属性，取消保持明确状态；OMP使用结构化logger、有限诊断/输出、tool和agent span以及可选Telemetry，强调状态/事件不依赖解析日志。两者都不证明第三方Runtime内部trace可见。
- 形成候选：Aether独立TraceContext/Operation值对象，root由入口创建，child由真实边界派生；跨任务/回调复制，不用全局trace或线程ID；记录结构化且有界，优先关联ID、错误码、状态、大小、耗时，默认不记录正文/凭据；阈值过滤前跳过昂贵字段；单writer和有限队列/文件/总量，输出故障通过health/drop计数暴露，不递归、不影响业务结果；Backend提供按trace和operation/layer/component/level的有界查询，Frontend不读日志文件。
- 限制与风险：Autorail的14天、256KiB tail、300行等属于其产品实测，不直接成为Aether容量；Pi/OMP的Telemtry/Logger库不引入Aether依赖。Aether必须独立确认ID编码、文件/队列容量、轮转、权限、脱敏和关闭截止。
- 未解决：DEC-10仍待确认；不实现ENG-08/ENG-09或诊断UI，不宣称本仓库已有追踪链路。

### 2026-11-08 · DEC-09 · UI框架与循环桥接调查/待确认

- 记录类型：认领/调查/待确认。
- 设计基线：`docs/design/detail/01-frontend.md`、`docs/design/06-delivery.md` P2、`docs/ARCHITECTURE.md` §2/§5；当前Aether只有C17桌面骨架，无UI库、窗口、Backend Client或UI Queue。
- LumaDesk证据：`D:\workspace-ai\desk-dev\LumaDesk`使用Rust 2024 + `gpui-kit = 0.9.0`及LumaKit路径依赖；UI实体订阅输入/状态变化并调用`cx.notify`，后台任务通过线程/通道或poll回到UI，关闭前检查ChatSession与picker，实际功能依赖其Rust/LumaKit边界。它不能直接成为Aether C17依赖；许可证/三平台C绑定需单独核实。
- Autorail证据：`D:\workspace-ai\autorail`使用`gpui-kit`组装根，Core/Services不依赖GPUI；`AppState::run`把阻塞服务放background executor，再通过`cx.spawn`回UI更新实体；Services向有界Terminal队列和会话通道投递事件，窗口关闭由组合根协调。该模式验证了“UI只投递意图、后台结果回UI、状态服务与UI分离”的边界，但不批准Aether采用GPUI或Rust。
- Aether候选：Frontend只依赖Backend DTO/Event；UI线程独占VM/控件；Backend Client持receiver_id/view_generation和独立callback context；后台结果复制后入有界UI Queue；OpenView快照与事件按base_revision/sequence安装；关闭先使receiver失效、退订、释放队列载荷，close-complete后才释放context；UI不等待Runtime/IO/Join。真实框架需提供C17绑定、后台投递、中文输入、焦点、高DPI、可访问性和销毁回调能力。
- 平台/许可缺口：当前未批准NAppGUI、GPUI、GTK、SDL或其他UI库；未验证Windows/macOS/Linux窗口与输入矩阵，也未选定最低OS/编译器、渲染后端、打包许可或C ABI策略。
- 验证矩阵：最小真实窗口提交一个Backend Command/Query/Event；后台操作不阻塞UI；中文输入/焦点/高DPI/键盘可访问性；快速切页/窗口销毁后的晚到事件；关闭截止、无悬空回调和借用Runtime不被误杀。DEC-09保持待确认，不实现UI代码。

### 2026-11-08 · ENG-04 · 认领与实施范围

- 记录类型：认领/实施计划。
- 依赖与基线：ENG-03、DEC-04已完成；当前Foundation/Core/Backend无Platform target，桌面入口仅验证Core生命周期。
- 实施范围：按代码落位规划新增 `include/aether/platform/platform.h`、`src/platform/platform.c`、`tests/platform/platform_smoke.c`，建立 `aether_platform` target。提供单调纳秒/墙钟毫秒快照、UTF-8借用输入复制为拥有型不透明路径、当前目录读取及配套释放；Windows使用Platform源内UTF-16/UTF-8边界转换，非Windows使用C/POSIX平台实现。
- 合同边界：路径对象只保证输入编码、长度、复制和生命周期，不宣称路径权限、符号链接隔离、TOCTOU防护或沙箱；当前目录是平台事实，不自动授予访问权限。时钟快照调用同步、输出由调用方提供或由Platform拥有并释放；不创建线程、不保存调用方临时指针。
- 设计与场景：`docs/design/detail/06-platform-execution.md` §5–7、EXEC-07相关基础；覆盖空/非法UTF-8、Unicode路径复制、当前目录、时钟读取、重复释放/NULL安全和MSVC/Clang编译。
- 下一步：实现后执行Windows MSVC构建、CTest、质量检查及独立平台烟测；macOS/Linux仍只做源码可移植性检查，不能宣称运行验证。

### 2026-11-08 · ENG-04 · 完成

- 记录类型：实现/验证/完成。
- 实际变更：`include/aether/platform/platform.h`、`src/platform/platform.c`、`tests/platform/platform_smoke.c`；CMake新增`aether_platform` target和`aether_platform_smoke`测试，Platform仅依赖Foundation。
- 行为：Windows使用QueryPerformanceCounter/Frequency安全分解换算单调纳秒，GetSystemTimeAsFileTime提供墙钟毫秒，GetCurrentDirectoryA读取当前目录；非Windows使用CLOCK_MONOTONIC/gettimeofday/getcwd。UTF-8路径输入先校验再复制为Platform拥有的NUL终止缓冲区，destroy支持NULL。
- 实际验证：`scripts\\build-msvc.bat --clean`配置/构建23步，CTest 5/5通过，质量检查24个源码文件通过，桌面入口输出`Aether desktop skeleton ready.`；MSVC与Zig Clang直接Platform smoke均退出0；首次Clang烟测发现性能计数器整数溢出，已改为整秒+余数换算后重跑通过；`git diff --check`通过。
- 文件规模/例外：platform.h 34行，platform.c 174行，platform_smoke.c 39行；均低于300行职责审查和500行上限。质量检查无超过100列错误，无行宽例外。
- 安全/未验证：路径对象不提供权限、符号链接/TOCTOU防护或沙箱；未验证macOS/Linux运行、动态内存工具和路径访问策略。ENG-04只交付基础平台值与资源边界。
- 交接：ENG-05仍需DEC-11；下一可推进决策为DEC-11或依赖满足的其他调查任务。
