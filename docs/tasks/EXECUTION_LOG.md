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

### 2026-11-08 · DEC-11 · 容量基线调查/待确认

- 记录类型：认领/调查/待确认。
- 设计范围：输入/结构块、RequestRecord/终态保留、事实/订阅/UI队列、快照/分页、日志记录/文件/查询、Process管道、关闭截止；来源为DEC-05/07/08/10、详细设计及ENG-04实际Platform能力。
- 现有Aether实测/合同基线：错误消息160字节；Backend同步请求记录8槽；Platform当前目录缓冲4096字节；其余业务输入、事件、快照、日志和关闭容量尚未实现，不能把这些值泛化为产品容量。
- Pi/OMP证据：OMP v2单帧1 MiB、重组帧64 MiB、历史页256、Autorail读取尾部/协议帧64 MiB、日志日轮转14文件且读取最多300行/256 KiB；这些是参考实现的实测/协议约束，不是Aether批准值。OMP严格校验分片顺序/长度，坏帧关闭；Pi/OMP将输出截断并保留可追踪提示，成熟实现均区分回合产出与会话settled。
- Autorail/其他证据：Autorail Terminal队列256、stderr tail 64 KiB/summary 2 KiB，退出/回调使用有限等待；其具体数值不直接迁移。Aether必须按业务数据、平台、内存和真实负载重新测量。
- Aether候选基线：所有值用条目+字节双限；关键完成/权限/控制状态独立可查询；普通Delta可聚合/采样但必须标识不完整；输入超限同步拒绝，队列满显式拒绝或ResyncRequired，日志满增drop/health，不阻塞UI；关闭截止后保留资源责任，不释放活跃回调。容量测试使用确定性时钟、受控Provider/本地进程/临时Store/日志目录，禁止sleep碰运气。
- 待确认：项目负责人需确认初始数值及内存预算、RequestRecord终态保留期、消息/附件/快照/分页上限、订阅/UI/日志队列策略、单条/文件/总日志保留、Process管道与关闭截止。DEC-11不标记完成；ENG-05/ENG-09必须依此确认后实现。

### 2026-11-08 · ENG-04 · 回归认领

- 发现与复现：真实临时中文目录下旧平台程序输出`cwd_status=1`；旧Windows墙钟输出13438572307288，而同进程Unix毫秒为1794098707000，存在1601/1970纪元偏差。
- 范围：修复Windows宽字符路径查询和Unix纪元、路径嵌入NUL/长度检查、失败输出；补齐临时目录及用户数据目录查询和不透明资源释放合同。按common/windows/posix拆分，不添加业务文件授权或沙箱。
- 旧完成记录不足：Unicode字节复制不等于真实Unicode目录查询；未覆盖系统目录和分配失败；原报告的文件行数与重复释放描述不作为本轮证据。
- DEC-11：前置DEC-05/08/10仍待确认，容量只能提前调查；新ADR-0006纠正为提议，不启动ENG-05，不将用户指定参考实现写成数值批准。

### 2026-11-08 · ENG-04 · 回归修复完成

- 变更：删除`src/platform/platform.c`聚合源，拆成common路径值、windows时钟/路径、posix时钟/路径及internal私有头；公开头明确Unix毫秒、单调时间、错误输出、借用/释放、线程兼容及阻塞性；新增TEMP/USER_DATA OS目录，不创建产品数据目录。
- Windows：GetCurrentDirectoryW/GetTempPathW/SHGetKnownFolderPath转严格UTF-8；墙钟从FILETIME扣除Unix纪元；QPC按整秒+有限余数换算，检查范围。路径输入拒绝空值、非法编码、嵌入NUL及size_t溢出；一次分配内同时持有对象和内容，回滚不留半对象。
- 真场景失败前/成功后：旧版真实中文目录查询返回InvalidArgument；修复版独立Clang程序在中文+非BMP临时目录输出完整UTF-8 cwd，墙钟1794099355355与Unix秒换算1794099355000同属1970纪元，退出0。MSVC真实目录回归创建/切换/恢复/删除中文+非BMP目录，并与OS TEMP/LocalAppData结果比较，通过。
- Windows验证：`scripts\build-msvc.bat`配置/严格编译、CTest 7/7、32个源码文件质量检查、桌面入口通过；独立Zig Clang严格编译及Windows目录回归通过。测试专用allocator替换仅编入独立测试target，覆盖每个路径分配点OOM与清零输出、分配/释放净计数为0；生产无注入开关。
- Linux验证：WSL Ubuntu `cc`严格C17编译并运行Platform smoke（实际cwd、Unix毫秒、单调差值）；POSIX目录回归验证XDG/HOME/TMPDIR选择、真实中文目录、删除cwd后NotFound；ASan/UBSan分配失败回滚退出0；`cc -fanalyzer -fsyntax-only`无诊断。这只验证Platform，不是Linux完整项目/桌面验收。macOS未运行。
- 工具缺口：Zig Clang Static Analyzer尝试报告FileNotFound，未当作分析成功；Windows MSVC Debug EXE在未注入CRT路径的独立环境退出53，现有setup脚本环境下CTest通过，独立场景使用Clang产物；不宣称Debug产物可直接分发。
- 文件规模：CMakeLists 350行，职责是当前targets及测试装配，已审查无业务逻辑；platform.h 70、common/path.c 71、internal/path_internal.h 17、windows/clock.c 48、windows/path.c 126、posix/clock.c 32、posix/path.c 135；platform_smoke.c 82、windows_path_test.c 93、posix_path_test.c 77、allocation_test.c 78。均小于500行，无C行宽例外。
- 文档：README测试数量/Platform验证范围、接口规范§15、代码落位和ADR索引同步；受影响Markdown本地链接检查无缺失。
- 后续门槛：ADR-0006仅提议，DEC-11依赖DEC-05/08/10尚未齐；ENG-05保持未开始，不把参考授权转换为容量/安全策略批准，不提前实现缺前置的执行器。

### 2026-11-08 · DEC-05 · 按Pi/OMP实际实现完成语义决策

- 用户要求：在请求竞争选择问题上指定参考Pi/OMP，不重复询问抽象选项。直接核对本机Pi RpcClient/Agent生命周期，OMP RpcPromptResults/session-settle和Rust/Go客户端；未安装参考项目依赖、未修改参考仓库。
- 纠正旧证据：此前记录把Autorail的Pi适配器代码写成Pi源码依据，不能据此断言Pi也有OMP的prompt_result；本轮明确Pi当前等待agent_settled，OMP按本请求prompt_result并独立报告session_settled。Pi/OMP不是同一协议或相同超时实现。
- 实际场景：Bun导入Pi真实RpcClient，测试专用可控timer触发waitForIdle超时，随后接收agent_settled使新等待成功；不启动模型/CLI，不发送abort。临时脚本使用类型化测试边界，完成后删除。
- OMP验证：`uv run --no-project python -c ... unittest ...`仅运行官方SDK五个用例（忽略旧回合直到自身结果、本地prompt、事件过滤不阻止完成、后台工作使settled晚于yield、受理后失败），5/5通过（0.654秒）。其fake server仅验证真实SDK客户端，不是Aether Adapter或真实Runtime验收。
- Aether决策：客户端等待超时只撤销等待、不自动改业务终态；显式单调业务deadline由Backend拥有者先检查，处理时已到期则TimedOut，迟到成功留在execution事实。此优先规则来自Aether既有设计，不虚称Pi/OMP原样规则。取消、一次终态、早到响应、OpenView原子握手、单订阅sequence/健康、非UI串行回调及close-complete等统一正式维护于接口契约§4.4/§10.4，并给出九项可判定顺序示例。
- 保留与数值：由limits定义终态保留期限，回收仍需引用全解除；未批准具体时长/队列数值。DEC-05为语义决策完成；DEC-11仍依赖DEC-08/10，ENG-05未开始；ENG-12 V1未改行为，不以已有同步记录冒充异步合同实现。
- 文档：ADR-0005置已接受，Backend/Frontend/shared基线链接迁往正式合同，移除共享设计重复状态表，任务/ADR索引同步；无C/API符号变更，无新增永久测试。
- 完成检查：受影响Markdown本地链接和标题锚点自动检查无缺失；本次无C源码、CMake或平台行为变化，不重复运行无关骨架构建；Pi真实客户端可控场景及OMP五个官方SDK行为测试作为本次调查运行证据，不把它们当Aether异步实现证明。

### 2026-11-08 · APP-01 · 认领与合同

- 依赖：ENG-03/DEC-06已完成。Core Registry维护Provider描述、Runtime实例代次和借用关系，不执行进程/协议/业务；capacity由构造者显式传入，不批准全局默认。
- 设计：不透明Registry/RuntimeRef；Provider身份由Registry生成并复制UTF-8名称/版本；Runtime绑定Provider、generation和已知能力快照，登记不等于Ready。新事实只接受当前generation；重连须递增generation并使旧ref失效；持有者仍可安全释放旧ref。
- 生命周期：begin_unregister阻止新借用，保留Closing快照；finish_unregister仅refs=0可移除。Provider在仍有Runtime时不可移除；Registry在有借用时不可销毁。查询为独立快照/复制输出，Ref通过配套release清空指针；无回调/线程/IO，调用者串行化同一Registry，销毁不杀外部Runtime。
- 文件：`include/aether/core/runtime_registry.h`，`src/core/runtime/`及私有头，`tests/core/`；aether_core继续仅链接Foundation。Provider执行函数表待真实RUN-01需求发布，不预建假操作。

### 2026-11-08 · APP-01 · 完成

### 2026-11-08 · APP-06 · inproc类型化Backend Client完成

- 新增`include/aether/backend/client.h`、`src/backend/client.c`和`tests/backend/client_test.c`。Client借用Backend，调用方负责串行化并保证Backend寿命；Client不持有输入payload、不创建UI/Runtime/IO线程。
- DTO为独立`AetherBackendClientRequest`，不暴露Backend内部记录；状态和结果显式映射，非法终态组合在Client边界拒绝，查询输出失败清零，释放通过Client转发到Backend。
- 范围严格限定当前同步V1本地请求记录；没有冒充异步Command/Query/Event、RequestContext、OpenView、deadline、取消底层工作或Runtime执行。
- Windows验证：`call scripts\\msvc-env.bat`；CMake Debug配置；`cmake --build build\\msvc --target aether_backend_client_test -j 6`；`ctest --test-dir build\\msvc -R aether_backend_client_test --output-on-failure`通过。
- 内容：Registry显式配置容量、Provider名称/版本复制与类型ID、Runtime实例代次与known/supported能力、状态快照、重连防旧事实/引用、共享引用及两阶段注销。begin_unregister关闭借用入口；finish只在refs=0移除；Provider不能在Runtime存在时移除；Registry不能在Ref存在时释放。未接通进程、协议、Adapter、Session、UI或持久化，不把登记Ready当外部握手。
- 文件/依赖：公开`include/aether/core/runtime_registry.h`；私有`src/core/internal/runtime_registry_internal.h`；`src/core/runtime/{registry,runtime,reference}.c`；两个`tests/core/runtime_registry_*test.c`。aether_core仍仅链接Foundation，私有include不传给消费者。README、接口规范§16、Core详细设计及落位规划同步；既有调用方不变，无兼容别名。
- Windows：`scripts\build-msvc.bat`严格MSVC编译、CTest 9/9、39源码文件质量检查及桌面入口通过。Zig Clang严格C17编译并运行`--smoke`：两个Runtime共享Provider，一实例双Ref阻止注销/销毁，重连拒绝旧generation/旧Ref，逐个释放后注销，复用槽位不复用ID，另一Runtime仍Ready，最后Provider/Registry释放完成，退出0。
- Linux：WSL Ubuntu严格cc编译实际Registry场景及test-only allocator替换版本，ASan/UBSan两程序都运行通过；覆盖Registry对象/Provider数组/Runtime数组/Ref每个分配点失败，无半公开输出、净持有分配数0；`cc -fanalyzer -fsyntax-only`无诊断。临时脚本/二进制清理，不修改用户Pi/OMP/Autorail仓库。
- 回归：注册不等于可借用、空/非法UTF-8/NUL/容量溢出、同名Provider与跨Registry隔离、能力不能supported超known、重复事实revision不增长、Unavailable不能新借用、Closing不能重连/更新或新借用、旧代次不覆盖、旧Ref可安全release、重复release清NULL、未知ID输出清零。两个引用模拟不同消费者，无悬空引用/共享实例误杀。
- 文件规模：CMakeLists 387行，仅target/测试装配且小于500，已审查；runtime_registry.h 110，internal头37，registry.c 165，runtime.c 159，reference.c 67，行为测试179、分配测试104；新增手写C均小于300。存在81–84列目标提示但无超过100列或特殊例外。
- 未验证：macOS；Linux完整CMake/桌面；真正Provider执行、Session关系回调或Runtime IO。Foundation ID耗尽/持久唯一性仍沿D-ID后续边界；没有声明全局持久ID。APP-01范围内无未完成项，APP-02仍需DEC-07。


### 2026-11-08 · APP-02 · Workspace与Agent配置领域完成

- 新增`include/aether/core/workspace.h`、`src/core/workspace.c`和`tests/core/workspace_test.c`。Core拥有独立Workspace/AgentConfig内存事实；Workspace保存有界UTF-8显示名/root_path、revision和Open/Closing/Closed状态，AgentConfig保存显示名/provider_id/model与revision。
- Create复制输入，失败输出清零；Query返回值快照；Update使用expected_revision，冲突拒绝且不覆盖；Workspace关闭分为begin/finish；Agent配置模型更新递增revision，删除明确释放登记。容量显式传入，无全局表、IO、Store、Runtime、Session或UI依赖。
- 安全边界：root_path只是领域引用，不授予访问权限；Agent配置不是Runtime进程别名；没有把配置变化静默应用到既有Session，因为Session尚未实现。
- Windows验证：`call scripts\\msvc-env.bat`；`cmake --build build\\msvc --target aether_workspace_test -j 6`；单测通过；完整CTest 11/11通过。
- 文件规模：`workspace.h` 107行、`workspace.c` 341行、`workspace_test.c` 78行；workspace.c超过300行但职责单一为Workspace/Agent内存领域注册表，低于500行，无行宽例外。

### 2026-11-08 · APP-02 · 回归修复与完整CTest

- 修复Workspace实现的容量乘法溢出校验、公共头`stddef.h`依赖和编辑残留；Workspace/Agent测试重新编译并通过。
- Windows完整CTest重新运行，11/11通过，包含Trace、Registry、Platform、Backend、Backend Client、Core和Workspace测试。

### 2026-11-08 · APP-05 · Permission策略与授权范围完成

- 新增`include/aether/core/permission.h`、`src/core/permission.c`和`tests/core/permission_test.c`。PermissionRegistry按Workspace隔离策略，支持`yolo`、`write`、`always-ask`、`always-ask-cached`；默认未配置策略不自动批准。
- 规则：yolo允许全部等级，write允许read/write，always-ask-cached对read无目标记忆时允许、write/exec建立Ask；Ask答复只能消费一次，remember只缓存Allow，policy revision变化使旧缓存不命中，revoke显式清除记录。
- 这是Core领域求值和有限批准记录，不是OS沙箱、路径安全、Runtime原生权限桥接或持久化；没有UI对话框，不把Ask伪造为Allow，不保存秘密。
- Windows验证：`call scripts\\msvc-env.bat`；`cmake --build build\\msvc --target aether_permission_test -j 6`；Permission测试通过；完整CTest 14/14通过。

### 2026-11-08 · APP-03 · Session状态拥有者完成

- 新增`include/aether/core/session.h`、`src/core/session.c`和`tests/core/session_test.c`。SessionRegistry借用Workspace/Runtime registry，不拥有其资源；创建时校验Workspace Open、AgentConfig存在、Runtime generation匹配且Runtime Ready，复制关联和Agent revision。
- 实现Created→Idle→Active→Idle、RuntimeUnavailable、Interrupted、Closing→Closed；Active只允许一个在途turn，Interrupted清理active turn但禁止重新StartTurn，关闭不复活Session。所有查询返回值快照，失败清零，未知ID明确NotFound。
- 不实现Message、Store、Backend RequestRecord、Runtime IO、持久化、恢复协议或权限执行；Session状态不复制Backend请求终态。
- Windows验证：`call scripts\\msvc-env.bat`；`cmake --build build\\msvc --target aether_session_test -j 6`；Session测试通过；完整CTest 12/12通过。
- 文件规模：`session.h` 111行、`session.c` 319行、`session_test.c` 76行；session.c超过300行但职责单一为Session状态注册表，低于500行，无行宽例外。

### 2026-11-08 · APP-04 · Message结构化增量完成

- 新增`include/aether/core/message.h`、`src/core/message.c`和`tests/core/message_test.c`。MessageRegistry借用SessionRegistry；创建仅接受存在且Idle的Session，快照复制Message ID/Session/role/state/revision/cursor/text。
- Text append要求连续cursor；重复同cursor幂等，跳跃cursor返回PROTOCOL_ERROR，过期cursor拒绝；replace要求expected_revision，所有内容变更先检查容量；Completed/Interrupted/Failed后拒绝晚到增量且重复finish不改终态。
- 当前实现明确只覆盖受限Text块，未伪装Image/File/ToolCall/ToolResult/StructuredData、持久化或Runtime协议归并；Session状态仍由APP-03拥有，Backend请求终态独立。
- Windows验证：`call scripts\\msvc-env.bat`；`cmake --build build\\msvc --target aether_message_test -j 6`；`ctest --test-dir build\\msvc -R aether_message_test --output-on-failure`通过。Message边界修复后测试通过。
- 文件规模：`message.h` 80行、`message.c` 205行、`message_test.c` 78行；均低于300/500行，无行宽例外。

### 2026-11-08 · DEC-07 · 真实审批门取证与安全提案

- 直接取证：Pi core/agent-session.ts工具调用钩子、extensions/runner.ts阻断、project-trust.ts无UI返回false；项目资源信任不是沙箱。OMP tools/approval.ts工具/用户deny优先、缺上下文always-ask但配置默认可为yolo；restrictedFolders每次prompt不是OS隔离。wrapper.ts对扩展修改后的参数重新求值，按实际参数审批，无UI无法满足审批则拒绝，非显式允许答复deny。
- 没有照搬：OMP允许会话/目录/跨会话缓存授权，Aether提案只单次批准；Pi默认用户权限不满足Aether最小权限保证。引用实现只用于证据，不自动批准权限扩大或未隔离模式。
- 新增ADR-0007（提议）：组合范围Deny>Ask>Allow/Inherit不授权、request/目标/代次/policy绑定nonce、有限有效期、最后检查点重查并原子消费、无UI/超时不执行、路径真实资源与TOCTOU审查、借用进程不可误杀、Runtime未暴露操作不得虚称受控，列九项威胁验收。
- 实际验证：uv运行OMP官方Python SDK三个审批交互用例（headless取消Ask使prompt结束、headless取消交互、确认/取消往返），3/3通过（0.087秒）；fake server只验证成熟客户端协议交互，不是Aether权限执行证明。源码/脚本/依赖未改，不重复构建无关骨架。
- DEC-07保持待确认：必须由负责人确认所需控制不可兑现时“默认拒绝”还是“逐实例显式未隔离运行”；不伪造批准。APP-02未开始；该安全策略确认后再迁正式合同并实现。ADR索引、任务状态同步，无新增手写代码文件。
- 文档检查：ADR-0007、ADR索引、决策任务表及执行记录共4份文档的16个本地Markdown链接目标存在，缺失0；本轮未改C源码、公共API或target依赖，无新增代码行宽/规模例外。

### 2026-11-08 · DEC-07 · 用户纠正授权体验方向

- 用户反馈：“为什么不能参考pi 和omp的做法要反复的需要我确认”“我现在再用的就是omp的感觉就特别好用”“我现在用omp 开发就没有授权”。此前把参考实现收窄为只能单次授权、再要求用户选择新增限制，是实施方向偏离，不是用户提供的信息不足。
- 修正ADR-0007：参考OMP模式化执行，普通调用不人为叠加审批；确需确认时支持按真实能力提供单次、会话、目标/目录记忆。撤回此前产品二选一，不再把它列作等待用户答复的阻塞。DEC-07恢复进行中，不伪造完整合同或功能完成。
- 源码复核：approval.ts模式等级、sessionApprovalScope、目录仅一层和多目标全覆盖；approval-cache.ts会话内存/跨会话存储及清除。OMP配置默认可为yolo；未读取当前用户实例配置，不断言具体生效项。本轮只修正文档，没有改产品代码、API或依赖，也没有新运行时验证。
- 同步Core详细设计§7、APP-05验收及任务入口，避免旧“只能一次授权”候选继续指导实现。ADR仍使用“提议”标准状态，不将方向纠正冒充完整安全合同已接受。
- 来源关系核实：OMP README明确声明fork of Pi/pi-mono；OMP coding-agent依赖自身@oh-my-pi/pi-agent-core与pi-ai，session/agent-session.ts实际导入；本机Pi对应使用@earendil-works包。共同来源不等于两个当前分支行为相同，后续以OMP实际行为为主要体验参考，Pi用于理解共同基础和差异。

### 2026-11-08 · 00决策组 · 优先收敛与DEC-07完成

- 用户要求继续任务且指出00仍未完成；本轮优先推进DEC-07至12，不跳到APP-02。APP-02委派在研究阶段暂停，确认无代码/接口改动。
- DEC-07：直接核对OMP18.18.0 approval.ts模式/显式拒绝/等级、wrapper.ts实际参数与会话记忆、approval-cache.ts目录一层及多目标匹配；采纳用户已明确的免反复授权体验，正式接口§11.1定义默认yolo、组合范围映射、单次/会话/目标批准与撤销、无UI/迟到答复、原生Runtime实际控制边界。架构§6、Core设计和验收同步；仅语义决策完成，APP-05/10和ENG-06仍须实现。
- 参考验证：uv运行OMP SDK的headless取消Ask、headless取消交互、确认/取消往返，3/3通过（0.090s）。它们启动本地fake server，仅证明参考客户端协议交互，不冒充Aether权限执行。实际安装`omp --version`输出18.18.0，与源码包版本一致。

### 2026-11-08 · DEC-08/09/10/11/12 · 决策收敛与实际试验

- 负责人：当前AI会话；00优先，不实施APP-02或新增产品空模块。参考Pi/OMP实际源码，不再把明确工程取舍挂成等用户确认。新ADR-0008/0010/0011与既有0006/0007收敛数据、日志、容量、首Runtime；ADR-0009保留真实UI门槛阻塞。
- DEC-08：SQLite3.54.1（官方chronology核对2026-10-16、Public Domain）及yyjson0.13.0/MIT只选型、未引入。CAS/短事务/一致备份、迁移损坏保全、保存与执行分离、配置saved/effective/source与secret_ref不可用合同已定。`uv run --no-project python build/decision_data_smoke.py`观察CAS拒绝旧revision、外键失败回滚多项变更及高版本拒绝后原记录保留，退出0；Python自带SQLite仅语义试验，不是3.54.1 C集成或DATA模块验收。
- DEC-10：`uv run --no-project python build/decision_log_smoke.py`启动OMP实际logger-contract-probe.ts，隔离临时目录验证交错context、显式覆盖、共享上下文清除，以及WARN保留error/warn过滤info/debug，退出0；不改外部源码/用户日志/环境。Aether16/8字节系统随机身份、严格JSONL/yyjson、私有目录/多实例锁配额、过滤/健康/查询范围正式同步§13及工程§4；不是Aether logger已实现。
- DEC-12：`omp --version`18.18.0；`uv run --no-project python build/decision_rpc_smoke.py`以环境白名单、空临时cwd/PI_CODING_AGENT_DIR禁用工具/扩展/持久session启动真实OMP，不发送prompt/凭据；观察ready versions=[1,2] frame=1048576 reassembled=67108864、v2协商/get_state成功、未知命令关联失败、stdin EOF后stdout排空exit0。首次未指定模型真实退出1“No default model selected”，加显式模型选择才通过，不回退读取用户配置。选首OMP RPCv2再Pi；全部Runtime/MCP/Skills目录与Native实际模型/工具范围落ADR-0011，其他版本/真实模型仍归RUN验收。
- DEC-11：既有8槽API不变，落定新任务/事实/请求/权限/消息/附件/Store/Process/日志双限与保留/截止；显式载荷求和175.703125MiB，加64MiB解码/16MiB快照为255.703125MiB，初始受管池272MiB。数值是有限工程起点不是RSS/SLA，后续ENG/APP/DATA必须低容量确定性边界与真实负载验证。
- DEC-09：原生Windows `where pkg-config`不可用，但WSL Ubuntu实查GTK4.14.5和WSLg可用。临时C17探针严格构建后运行：mapped=1、scale=1、accessible-role=68、worker回调断言UI线程并应用中文文本，退出0；首次GTK第三方头pedantic错误通过-isystem隔离，未关闭自研警告。EGL/MESA有驱动告警，未宣称后端无问题。程序化UTF-8不是IME，角色不是AT连接，scale1不是DPI切换；目标4.24.1/原生Windows/macOS及真实IME/焦点/DPI/AT、完整Backend关闭链仍缺运行证据，DEC-09阻塞而非等待用户确认。
- 影响：只改文档/任务/正式语义，无产品C API/source/CMake/target依赖变更，无永久手写代码新增或行宽例外；临时脚本和二进制清理。解除UI阻塞需要相应平台工具链与真实输入/AT环境，不降低验收；其他前置已就绪任务可继续。
- 文档验收：19份受影响规范/ADR/设计/任务文档共111个本地Markdown路径与章节链接有效；00共12项，11项已完成、仅DEC-09阻塞，已完成项依赖全部满足，未通过调整依赖或删除任务冒充完成。AGENTS阅读索引补§17；无产品代码变更故不重建无关骨架。

### 2026-11-08 · UI设计基线 · Autorail参考

- 用户指出当前没有UI设计图，要求参考`D:\\workspace-ai\\autorail`。已读取Autorail实际工作台、设置、Projects、Chat源码及设计文档/截图：Balanced布局由顶部上下文栏、40px活动栏、约280px项目面板、中间Chat、右侧详情和可折叠底部Terminal组成；设置是左导航+右内容对话框；后台事件按面板拥有者投递。
- Aether不复制Autorail Rust/GPUI实现或Git/Terminal业务，只采用其布局密度、面板组合、主题token集中管理和“组合根装配、面板不知容器位置”的原则。Aether主工作台改为Workspace/Session/Runtime/Task/Permission/Tools/Diagnostics优先；文件差异/终端只在真实能力接通后出现。
- 新增`docs/design/ui/workbench.svg`和`docs/design/ui/README.md`作为静态设计基线；设置/诊断图由并行设计切片补充。图中均为示例数据，非UI实现、非ABI、非真实状态。
- 设计边界：UI只经Backend Contract，VM只持DTO投影；每页Loading/Ready/Empty/Error/Forbidden/ResyncRequired明确；receiver/view_generation和close-complete语义沿接口§4.4；默认采用已收敛OMP式免重复授权体验。GTK4仍受DEC-09实窗/IME/DPI/AT/三平台门槛约束，不把设计图标为UI任务完成。

### 2026-11-08 · UI设计纠偏与Token验证

- 用户不接受第一版UI并强调严格参考`D:\workspace-ai\autorail`；旧三张SVG线框撤回，不再作为设计基线。先前`.omp/smoke.png`实际是浏览器/终端截图，并非Autorail工作台；该误判在本记录纠正，不据其宣称视觉一致。当前源码期间有更新，重读workbench、chat/view、projects、files/changes和theme/roles/metrics，未修改参考工程。
- 新预览按实际40px顶栏/活动栏、280px项目栏、460px会话列、28px页签、260pxChanges和默认收起终端重做；保留原色板/语义角色，去除Core指针/generation等实现文案与重复导航。文件/差异/终端内容均明显标设计示例，未接业务，未宣称新增工程代码实际存在。
- 主题使用theme-tokens.json唯一数据源，基础palette→语义→组件引用，Web变量/GTK命名颜色由theme.mjs同源导出；明暗模式和Autorail原色/蓝/紫独立切换。浏览器实际验证六组合正文/背景生效、项目/终端/任务折叠开关及无根页面溢出，导出明暗PNG供查看，不依赖聊天SVG变量注入。此前黑/白预览不可用不计验证通过，当前是HTML视觉预览而非GTK应用。
- 模型服务配置按用户新增要求严格参考Autorail ProvidersPage，不自创通用保存表单；保留服务商栏/标题启用/密钥检测管理/轮流使用/API线路/模型分组顺序。显示/复制秘密与Aether不回显合同冲突时明确禁用，不擅自放宽。最终交互证据补充在本轮交付记录，UI任务状态保持未开始。
- 用户补充LumaKit后核对：Autorail锁定model-picker/usage-status到7c46b2b9；正确本机组件在D:/workspace-ai/LumaKit（HEAD b5c020...），desk-dev/LumaKit为不同旧目录。读取分组picker/effort rail、usage状态及合同，区分模型服务页与模型选择器，未复制Rust依赖或声称两个版本相同。
- GTK Token试验：同一theme-tokens.json导出六组GTK命名颜色，WSL GTK4.14.5实际窗口依次替换CssProvider；mapped=1、parsing-error=0且前景色核对通过，退出0。首次弃用style-context API被严格警告拒绝，改gtk_widget_get_color后通过，未关闭警告；仍有EGL/MESA环境告警，不是完整GTK UI/平台验收。
- 用户提供真实模型服务截图后改以该图及源码为基准，默认七牛云关闭/空密钥/未选择模型；不再将无关终端截图当视觉证据。
- 模型服务预览实测：`providers.html` 浏览器初态三个服务商均存在且停用、七牛云/硅基流动/Fenno均空密钥/空模型/单OpenAI Chat线路；逐项选择后地址分别为`https://api.qnaigc.com`、`https://api.siliconflow.cn/v1`、`https://api.fenno.ai/v1`，GET预览均为`/v1/models`，未发网络请求。搜索Fenno只留一行；预设更多菜单删除项disabled；线路设置新增请求头行；密钥管理/新建服务商/模型设置对话框打开，提交只显示未接Backend不保存。
- `populated`仅为显式内存交互示例：线路切换、模型搜索/能力过滤/分组折叠、模型设置字段均可见；原密钥显示/复制按钮disabled。浏览器验证暗/浅×Autorail/蓝/紫六组主题背景和正文生效，导出`providers-dark.png`/`providers-light.png`；不读取真实配置、不发外网、不声称UI功能完成。
- 用户提供的真实模型服务截图作为当前视觉基准；模型预设源码语义来自Autorail `core/preset.rs`/`settings-store/presets.rs`，三预设首次均disabled且不可删除，种子幂等且不覆盖用户编辑。LumaKit仅用于聊天模型选择器/用量状态，版本锁与本机HEAD差异已记录，未混入模型服务页。
- C UI替代评估已写`docs/design/ui/UI_OPTIONS.md`并链接ADR-0009：核对官方NAppGUI/libui-ng/IUP/LVGL/Nuklear/WebView/Sciter资料，保留GTK4原生C控件与C宿主WebView两条值得同页实测的路线，NAppGUI为非GTK原生候选；没有新增依赖、UI语言授权或假三平台验证。用户仅要求评估，本轮不擅自切换实现。
- 最终设计工具规模：index.html59、workbench.css117、workbench.mjs74、theme.mjs65、providers.html59、providers.css133、providers.mjs239行；均单职责且<300/500，无手写C永久文件、无target依赖变化。7份相关文档35个本地链接目标有效；浏览器无页面JS错误。临时GTK源码/脚本/六组CSS与遗留两组CSS共10文件及二进制已清理；浏览器验证tab关闭。

### 2026-11-08 · DEC-09 · NAppGUI/EFL原生对照演示认领

- 用户明确不采用WebView，要求先看NAppGUI和EFL效果demo后自行做最终选择。本轮只交隔离的真实控件选型程序，不改正式产品target或宣称选型已定；同一模型服务页、三预设空态和主题/交互进行比较。
- 环境准备：WSL Ubuntu原缺GTK3/EFL开发包；首次apt下载因旧索引404失败，刷新索引后安装开发/截图包成功，连带更新发行版依赖。NAppGUI锁v1.6.3官方release（MIT），zip SHA256=52fcfe34bf766b35ed044e16c32f32a5fff0878efe5964a33509ef60f10f0aa8；vendor仅存build实验目录，禁用NAPPGUI_WEB，不引入产品构建。

### 2026-11-08 · DEC-09 · GTK/NAppGUI/EFL三套原生对照演示

- 历史记录：本轮曾在`tools/ui-demos/{gtk,nappgui,efl}/`制作三套隔离原生对照实验；GTK4使用WSL `4.14.5`，EFL使用Elementary `1.27.0`，NAppGUI使用`1.6.3.7077`。该实验代码、截图和GTK便携包已在用户确认NAppGUI后清理；以下构建结果作为历史选型证据保留，不表示当前仍有GTK/EFL实现。
- 历史验证：NAppGUI Linux GTK3后端和Windows原生后端均曾构建运行；GTK/EFL仅曾在WSL验证，所有三平台、IME/焦点/DPI/AT、Backend关闭链和真实业务接线均未完成。
- 当前结论：NAppGUI是用户确认的当前Frontend入口，未来替换只允许在`ui_adapter`边界进行；不把历史实验或当前原型标记为完整产品。


### 2026-11-08 · UI-01前置 · NAppGUI Frontend入口构建验证

- `tools/ui-demos/nappgui/CMakeLists.txt` target已从选型demo命名为`aether_frontend_nappgui`；NAppGUI类型仍限制在该Frontend实现内，未进入Backend/Core公共头。入口当前仍是独立原型，不接Backend。
- Windows MSVC x64实际命令：`cmake -S tools/ui-demos/nappgui -B build/ui-native/frontend-nappgui-msvc -G Ninja -DNAPPGUI_ROOT=build/ui-native/nappgui-sdk-msvc-env -DCMAKE_BUILD_TYPE=Release`；随后`cmake --build build/ui-native/frontend-nappgui-msvc -j 6`退出0，产物复制到`build/ui-native/windows-demo/aether_frontend_nappgui.exe`。
- 实际启动：`start "Aether Frontend NAppGUI" /wait build\\ui-native\\windows-demo\\aether_frontend_nappgui.exe`；窗口保持运行导致命令超时，符合GUI进程行为；没有报告构建或链接错误。
- 已知未完成：NAppGUI仍使用原生Check Button而非Autorail Switch；Backend Client、UI Queue、DTO/VM、中文IME/焦点/DPI/AT、macOS和关闭链仍未实现/验证。

### 2026-11-08 · UI-01 · NAppGUI应用壳认领

- 依赖核对：ENG-01、ENG-04已完成；DEC-09已确认NAppGUI为当前Frontend入口。范围只覆盖`apps/desktop/`中的NAppGUI应用壳/UI Adapter候选、导航和模型服务配置视图；不实现APP-06 Backend Client、APP-08 OpenView事件订阅、业务DTO、持久化、Runtime或权限执行。
- 设计依据：Frontend详细设计§2/§5，ADR-0009中NAppGUI替换边界和UI线程约束，Autorail模型服务页面参考。UI回调只改变本地演示视图状态并显示未接Backend，不伪造保存/检测/同步成功。
- 验收场景：Windows真实窗口启动、导航/服务商选择/主题/检测和同步按钮可见并可操作；窗口关闭无进程残留。中文IME、焦点/DPI/AT、Backend事件交接和close-complete仍是后续验证。

### 2026-11-08 · UI-01 · NAppGUI应用壳实现进展

- 已将NAppGUI原型target稳定命名为`aether_frontend_nappgui`，保留在独立Frontend路径；没有修改Core/Backend公共API，也没有创建并行GTK/EFL实现。
- 当前交付内容：1040×700窗口、三栏导航/服务商/详情布局、服务商选择、主题切换、密钥密码输入、检测/同步未接Backend提示、关闭回调和明确的NAppGUI资源销毁路径。
- 本阶段不扩展业务：回调只改变UI本地状态；不读写配置/数据库、不启动Runtime、不调用MCP、不保存密钥、不联网。

### 2026-11-08 · UI-01 · NAppGUI正式桌面target落位

- Frontend源码已从实验路径迁入`apps/desktop/nappgui/frontend.c`，构建配置迁入`apps/desktop/nappgui/CMakeLists.txt`；根CMake新增`AETHER_BUILD_NAPPGUI_FRONTEND`选项，默认OFF，避免未配置NAppGUI SDK时影响基础工程。
- target仍只依赖NAppGUI和Windows系统库，不链接Core/Backend；NAppGUI控件类型只存在Frontend实现。未来替换方案仍限于Frontend/UI Adapter边界。
- Windows实际配置/构建：`cmake -S . -B build/ui-native/desktop-nappgui-msvc -G Ninja -DAETHER_BUILD_NAPPGUI_FRONTEND=ON -DNAPPGUI_ROOT=build/ui-native/nappgui-sdk-msvc-env -DCMAKE_BUILD_TYPE=Release`；`cmake --build build/ui-native/desktop-nappgui-msvc --target aether_frontend_nappgui -j 6`退出0，产物复制至`build/ui-native/windows-demo/aether_frontend_nappgui.exe`。
- 实际启动命令保持GUI窗口运行，命令超时是窗口生命周期而非构建失败。未完成：Backend Client/DTO/UI Queue、中文IME/焦点/DPI/AT、macOS、关闭链和正式业务接线。

### 2026-11-08 · REL-04前置 · GitHub macOS NAppGUI构建工作流

- 新增`.github/workflows/macos-nappgui.yml`，固定`macos-14` runner、Xcode `15.4`、NAppGUI源码版本`1.6.3`和macOS deployment target `13.0`；workflow先从仓库源码构建NAppGUI SDK（`NAPPGUI_DEMO=OFF`、`NAPPGUI_WEB=OFF`），再配置根工程的`aether_frontend_nappgui` target。
- 工作流上传未签名的`aether_frontend_nappgui-macos.tar.gz`。没有Apple Developer secret时不执行签名/notarization，不把构建产物称为发布安装包。
- 本机Windows只做workflow静态结构检查和现有NAppGUI Windows target回归构建；macOS配置、编译、窗口、AppKit/IME/Retina/VoiceOver和关闭链必须等待GitHub macOS runner或真实Mac，未在本地宣称通过。

### 2026-11-08 · UI-01 · Win11可复现Frontend入口

- 新增`scripts/build-nappgui-frontend.bat`：调用portable MSVC、默认读取`build\\ui-native\\nappgui-sdk-msvc-env`、配置根CMake的`AETHER_BUILD_NAPPGUI_FRONTEND=ON`、构建`aether_frontend_nappgui`并复制到`build\\ui-native\\windows-demo\\aether_frontend_nappgui.exe`。可通过`NAPPGUI_ROOT`覆盖SDK位置。
- README补充Win11执行入口。实际运行`scripts\\build-nappgui-frontend.bat`成功，Ninja报告无待构建并确认产物复制；Windows配置日志仅显示Python质量检查解释器不可用，不影响Frontend target构建。
- 尝试启动Windows GUI：`start`在当前cmd包装环境受引号/权限限制未得到稳定进程观察；此前同一exe的实际GUI启动已记录，当前未把该次命令包装失败写成程序失败。应用仍可从资源管理器或README命令启动。

### 2026-11-08 · UI-01 · Windows回归验证

- `scripts\\build-nappgui-frontend.bat`重新配置并构建`aether_frontend_nappgui`成功，产物为`build\\ui-native\\windows-demo\\aether_frontend_nappgui.exe`。
- 首次直接调用`ctest --test-dir build\\msvc`未注入MSVC环境，9/9测试以`0xc0000135`失败；随后通过`call scripts\\msvc-env.bat`重新运行，同一CTest目录9/9全部通过。前者是运行环境缺失，不是源码回归。
- 当前机器未发现`uv.exe`，未运行Python质量检查；不声称质量检查通过。Frontend源码仍低于500行，未接Backend业务，不新增伪造业务测试。
- NAppGUI Frontend仍可通过资源管理器运行；Backend Client/DTO/UI Queue、中文IME/焦点/DPI/AT、macOS和关闭链保持未完成。

### 2026-11-08 · ENG-08 · TraceContext与Operation完成

- 新增`include/aether/foundation/trace.h`、`src/foundation/trace.c`和`tests/foundation/trace_test.c`；Foundation公开值类型包含16字节Trace ID、8字节Span ID、父关联、有效性、Operation状态、结果、耗时和clock regression标记。
- root/child通过调用方提供的受检随机源生成身份，随机失败或全零失败且清空输出；child复制trace并将父span保存为parent_span，不使用AetherId或全局current trace。Operation不分配、不持有业务/UI指针；重复finish返回AlreadyExists，dispose将active标为Unfinished，倒退时钟耗时为0。
- 不在本任务实现JSONL/yyjson、随机源Platform封装、logger/writer、轮转、健康或诊断查询；这些仍由ENG-09/APP-11及对应Platform任务负责。
- Windows验证：`call scripts\\msvc-env.bat`；CMake Debug配置；`cmake --build build\\msvc --target aether_trace_test -j 6`；`ctest --test-dir build\\msvc -R aether_trace_test --output-on-failure`通过。完整CTest 9/9通过。
- 文件规模：`trace.h` 98行、`trace.c` 141行、`trace_test.c` 76行；均低于300/500行，无行宽例外。


### 2026-11-08 · GitHub进展提交 · 验证与完成状态纠正

- 用户要求及时提交GitHub；本次保存当前开发进展，不宣称整个阶段完成。APP-04/05/06恢复进行中，保留完整验收范围；上文将Text子集、批准答复去重、同步客户端称为整项完成的记录不成立。resolve去重不是副作用执行前的单次原子消费。
- APP-05尚无完整规则优先级、Session/执行者/Runtime代次绑定、多目标/目录批准、期限及执行复核；现有匹配使用C字符串而非完整StringView长度语义，未覆盖非终止输入安全性。不得将该注册表用于生产执行授权；本次不把现有测试通过解释成安全合同通过。
- 恢复根CMake中此前被Trace测试替换的`aether_runtime_registry_test`入口，保留原行为覆盖；修正Frontend提示文本的108列行宽和CODE_LAYOUT中过时实验路径，删除README重复命令段。
- 本次实际运行：MSVC Debug CMake配置及全量构建成功；`ctest --test-dir build\\msvc --output-on-failure`为15/15通过；`build\\msvc\\aether_desktop.exe`输出`Aether desktop skeleton ready.`退出0。`scripts\\build-nappgui-frontend.bat`重新编译、链接、复制Windows Frontend成功；本次未重验窗口交互。
- `uv run --no-project python tools/check_code.py`首次发现Frontend超宽行，换行后检查58个源码文件通过；仍有81–100列目标警告，无100列硬限制或500行例外。CMake自身未找到Python解释器，质量工具通过uv单独运行，不计入本次CTest数量。动态内存工具、macOS/Linux及真实Runtime/权限副作用均未在本次验证。
- 当前修改代码行数：根CMake 461；Frontend CMake 67、frontend.c 287、构建bat 31；client.h/c/test 70/199/46；workspace.h/c/test 114/360/65；session.h/c/test 100/320/74；message.h/c/test 80/205/78；permission.h/c/test 85/254/63；trace.h/c/test 98/141/76。超过300行的根CMake负责target装配，workspace.c负责Workspace/AgentConfig注册表，session.c负责Session状态注册表；均未超过500行。
- 落位与依赖：Core领域代码位于`include/aether/core`和`src/core`，仍只依赖Foundation；Backend Client位于Backend边界；Trace位于Foundation；可选NAppGUI target位于`apps/desktop/nappgui`，未链接Core/Backend，未扩大UI业务访问。
- `.github/workflows/macos-nappgui.yml`暂不纳入本次提交：依赖本地build目录中的第三方源码，且Frontend CMake的非Windows分支要求GTK3，尚不能支持宣称的macOS路径。文件保留本地，不触发误导性的远端CI；此前工作流记录只是创建进展，不是可用CI证明。

### 2026-11-08 · FIX-PERM-01 · 认领

- 负责人：当前AI会话；依赖APP-02/DEC-07已满足，关联APP-05已登记安全缺口。本轮只修复现有Permission API的输入/内存/关闭边界，不新增线程、库、UI或执行服务，不将父任务缩范围或标完成。
- 范围：`src/core/permission.c`、公开权限头的生命周期说明和现有Permission行为测试；使用现有StringView长度语义，记录持有副本；显式校验枚举集合、Open Workspace和答复对应的policy revision；容量乘法在分配前检查。API签名不变。
- 验收：实际无UI程序观察缓存对完整/截短视图及关闭状态的结果；回归测试覆盖非终止视图、原输入修改、错误枚举、失效答复和满容量，失败不改变既有批准。Windows配置/全量CTest、源码检查及可用的内存检查；无真实执行端时不宣称副作用授权已验证。
- 语言服务查询返回无C语言服务器，改用目标搜索定位全部调用方；现有调用在Core测试，尚无产品执行消费者。

### 2026-11-08 · FIX-PERM-01 · 完成与真实复现

- 根因与变更：PermissionRecord原先保存借用输入且用strcmp忽略视图长度，现改存Workspace/等级及长度、复制字节并用长度+memcmp精确匹配，无调用方指针跨返回持有；复用原固定槽位，无新增分配或依赖。明确枚举集合与Allow/Deny答复，Ask不能作为答复；set_mode/evaluate/resolve重查Workspace Open，答复检查policy revision；create分配前校验乘法、失败清空输出，evaluate失败输出统一Deny/无效ID/零revision/非记忆。
- 真实复现：临时无UI C程序链接实际Core/Foundation，修复前截短target输出`decision=0 from_memory=1`、Closing Workspace输出`evaluate_status=0`，退出1；修复后同一程序输出`decision=2 from_memory=0`、`evaluate_status=2`，退出0。2分别对应Ask/NotInitialized；不执行外部命令或真实工具。首次手工链接运行库不一致有LNK4098，修复后使用/MDd匹配Debug，链接无该告警。
- Windows实际命令：`call scripts\\msvc-env.bat`；`cmake -S . -B build\\msvc -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=cl`；全量build及`ctest --test-dir build\\msvc --output-on-failure`15/15通过；桌面骨架实际输出ready退出0。新增回归覆盖精确边界无NUL输入、同缓冲不同长度、输入修改后的持有副本、非法枚举/答复、容量溢出/满载、撤销重用ID、policy变更及Closing/Closed下缓存和答复不授权。
- WSL实际使用cc严格C17+`-fsanitize=address,undefined -fno-omit-frame-pointer`编译Permission行为测试与独立烟测，`ASAN_OPTIONS=detect_leaks=1`运行均退出0、无消毒器诊断；`cc -std=c17 -Wall -Wextra -Wpedantic -fanalyzer -fsyntax-only -Iinclude src/core/permission.c`退出0无诊断。仅覆盖相关Core路径，不是Linux完整桌面/macOS/真实Runtime验收。
- `uv run --no-project python tools/check_code.py`检查58文件通过；CMake自身仍未检测到Python，检查独立执行。修改手写文件：permission.h 106行、permission.c 301行、permission_test.c 246行；permission.c超过300行但单一职责为同步权限记录生命周期/输入求值，无OS/UI/Store职责，低于500行；无100列或500行例外。target依赖不变，仍Core→Foundation。
- 公共签名不变，头文件补充当前开发API的线程/借用/输入长度/输出/错误和非生产执行门限制；APP-05仍进行中，未将本修复冒充规则优先级、Session/执行者范围、持久记忆、deadline或副作用前原子消费验收。临时烟测源码/产物清理；本次不修改本地未提交macOS工作流。

### 2026-11-08 · ENG-05 · 有界执行器与计时器认领

- 依赖ENG-04/DEC-11已满足；负责人当前AI会话。按ADR-0004/0006实现Infrastructure执行器与Platform原生线程边界，不引入libuv/线程池依赖或业务状态；初期默认1 worker，可配置1–4，1MiB线程栈。Windows原生与POSIX实现各自隔离。
- 接口先行：`include/aether/infrastructure/executor.h`声明双限队列、复制输入和TraceContext、结果保留槽、取消观测、ID/generation及显式关闭；计时器由状态拥有者传单调now领取，不创建独立计时线程。关闭/取消走独立控制门，无UI等待或worker直接写Core/Backend。
- 范围与分工：父会话拥有executor、timer、根CMake/文档整合；独立实现切片拥有两个Platform线程实现，独立测试切片拥有执行器行为测试。共享公共头由父会话维护；不在切片中运行构建，合并后统一验证。
- 计划验证：真实后台计算/Trace交接、队列条数/字节边界、关闭中满载提交、运行中取消及完成事实、旧代次/计时事实拒绝由消费者复核；Windows/Linux构建、确定性测试和消毒器、部分初始化故障清理；macOS平台仍需真实环境，不将POSIX实现等同Mac验证。

### 2026-11-08 · ENG-05 · 完成

- 实现：`aether_executor`在构造时预留任务槽/输入/计时器，投递时检查条数和字节双限并复制输入及Trace，不产生额外堆分配；默认1/最多4 worker，原生栈1MiB。运行回调在锁外，仅交回技术结果；Core/Backend/UI状态不由worker修改。计时器由状态拥有者以单调now领取，不建额外线程，携带target/generation并要求接收者复核，取消不能收回已交付事实。
- 取消/关闭：排队取消保留Cancelled且不调用工作；运行取消只置同步意图，不覆盖真实结果。结果保留到take；满载不阻塞/同步执行。关闭拒绝新工作、清排队并通知运行任务，实际worker退出后才close-complete，未完成destroy不释放；无强杀或截止后假释放。构造失败关闭并join已启动worker；故障测试覆盖4处分配、monitor创建、1至4个worker创建失败后的回滚。
- Windows实际命令：MSVC环境下CMake Debug配置/全量build及CTest18/18通过。首次测试对destroy(NULL)断言与既有安全销毁约定冲突，按实现契约改为OK后通过；未删除业务断言。真实临时程序链接实际库，输出`worker trace=17 span=23 generation=9 sum=15`、`owner result id=1 trace=17 sum=15`、`timer due delivered once; target generation preserved`、`close-complete observed; worker joined`退出0。
- Linux实际命令：`cmake -S . -B build/eng05-linux -DCMAKE_BUILD_TYPE=Debug`、build -j4及CTest19/19通过（GCC13.3，含质量检查）；首次全量build暴露旧client_test缺stddef.h，补标准头后通过。相同临时后台程序链接POSIX实际库，输出同样sum/trace/计时/关闭事实退出0。3组执行器测试`--repeat until-fail:50`全部通过；队列满、输入独立副本、运行/排队取消、旧代次、保留结果/槽复用、关闭资源未提前释放、四worker及timer截止/满载/取消/迟到身份均覆盖。
- 内存/静态：`build/eng05-asan`以`-fsanitize=address,undefined -fno-omit-frame-pointer`构建三个执行器测试，`ASAN_OPTIONS=detect_leaks=1 ctest ... -R aether_executor`3/3通过，无消毒器诊断。末次make有WSL文件时间轻微未来/clock-skew告警，目标源码实际重新编译链接后测试通过；不隐去环境告警。GCC `-fanalyzer -fsyntax-only`检查三个async源码和POSIX线程源码退出0无诊断。TSan普通与-no-pie两次均在启动时`unexpected memory mapping`退出66，环境阻断，不计数据竞争检查通过；macOS未验证。
- 架构/契约：新增公共executor/thread头及接口规范§18，Win32 SRW/condition/_beginthreadex、POSIX默认非robust mutex/condition/pthread隔离在Platform；无第三方新依赖。target为executor→Platform→Foundation；根CMake只装配子目录，模块CMake维护源码/测试。Backend client测试仅补标准头，无业务API变更；Process/Store/异步Backend尚未接通，不能将ENG-05当成它们完成。
- 文件规模：根CMake467；executor.h126、thread.h41；windows/thread.c130、posix/thread.c156；async/CMake39、executor_internal.h47、executor.c131、work.c168、timer.c85；executor_test.c413、timer_test125、failure_test107；client_test47。根CMake负责target装配、413行测试负责worker队列/取消/关闭的同一生命周期，计时器独立行为文件；均低于500行，无100列硬限制例外。质量工具检查69文件通过，81–100列目标警告保留。临时烟测及TSan产物清理，不改本地未提交macOS工作流。
- 完成审查：ENG-05交付项和EXEC-06/08基础设施场景通过；未来消费者必须验证generation再应用事实。已知工具缺口TSan/macOS如上，不声称三平台交付或产品联验。下一就绪任务：ENG-06 Process Service、ENG-09日志输出；APP-04/05/06继续按完整验收推进。

### 2026-11-08 · APP-04 · 按任务规划继续认领

- 用户要求macOS先放下、优先后端与Windows前端，随后明确继续按任务规划；不绕过APP-06/07/08等硬依赖去把演示控件临时接成产品。当前先完成已进行中的APP-04，依赖APP-03/DEC-11已满足；macOS不安排本轮实施或验证。
- 原Text-only接口无法表示块身份、真实message_end或工具关联，进行V2干净切换（`AETHER_MESSAGE_API_VERSION=2`），删除旧append_text/replace_text/finish；调用搜索只有现有消息测试，全部迁移。语言服务器不可用，已用目标搜索核对调用方。
- 模型：消息元数据+按ordinal稀疏块索引，六种类型载荷、资源使用不透明ID、工具调用有明确ID/代次；payload复制和精确last-fact去重独立于唯一apply状态机。每消息/全局字节含元数据与重放副本，构造配置只能降低ADR-0006上限；分配失败/超限不改已有内容，terminal释放支持引用保护。
- 分工：父会话拥有公开/私有合同、注册表/归并/关联、CMake和文档；payload实现与行为测试两个独立切片并行。统一合并后验证Windows全量、实际归并与可用消毒器，不将模拟消息流当真实Runtime接通。

### 2026-11-08 · APP-04 · 结构化消息V2完成

- 交付：六类结构块（Text/Image/File/ToolCall/ToolResult/Structured），Session/Message/block/Runtime generation精确关联，创建cursor1、连续后续cursor、所有变更CAS及最近事实逐字节去重；冲突/跳跃/过期明确失败。finalize只结束块，Completed必须显式END且所有块完成；Interrupted/Failed保留已提交部分。当前Runtime代次已失效、跨消息块或Closing Session新增量不改旧内容。
- 工具/资源：调用ID与调用generation配对，结果仅引用同Session/runtime generation已finalized调用，拒绝缺关联/错代次/重复调用及第二结果；保留结果阻止释放源调用，结果释放后才可释放调用。Image/File只保存受控不透明resource_id/MIME，不访问文件、不授予权限；归一化工具/structured UTF-8不隐式解析成JSON或执行。
- 资源：消息元数据快照按值，块快照一次分配独立拥有，销毁Core后仍有效。载荷单分配含内容/label/last-fact重放数据；构造limits最多16消息/256块/4MiB每消息/32MiB总量/256KiB工具参数，替换暂存新旧同时计费。额度不足或分配失败不改变原内容/块数/revision；显式release回收终态，无自动驱逐。旧Text接口和单字符串快照删除，V2主版本明示，全部调用方迁移无shim。
- 实际Windows：MSVC Debug全量build和`ctest --test-dir build\\msvc --output-on-failure`20/20通过。首轮非法UTF8测试char强转触发C4310，改字节转义字面量，未降低警告；首轮测试把关联错误/冲突终态状态码与契约不符、foreign Session辅助函数用错Session，修正测试输入/契约断言后通过，未删除行为覆盖。
- 真实归并：临时C程序链接实际Core/Foundation，输出`normalized stream: text=hello world cursor=2 revision=2`（重复增量不增长）、`block finalized: message_state=0 (Receiving=0)`、`explicit message_end: state=1 blocks=1`，退出0；这是归一化事实场景，不是第三方Runtime真实连接或Windows页面接线。
- 内存/静态：复用WSL消毒器环境，显式重新配置`-fsanitize=address,undefined -fno-omit-frame-pointer`后构建三个Message测试，`ASAN_OPTIONS=detect_leaks=1 ctest ... -R aether_message`3/3通过，无诊断；首次增量make尚未更新所有新target入口，重新配置后可构建，存在既有WSL clock-skew告警如实保留。GCC `-fanalyzer -fsyntax-only`检查4个消息源码退出0无诊断。消息分配注入覆盖构造两处、消息索引创建、创建/替换块及快照失败；观察旧内容和预算不变、销毁无剩余分配。
- 质量与规模：`uv run --no-project python tools/check_code.py`检查76文件通过。根CMake461，message.h171，message.c225，message_internal.h57，apply.c170，payload.c238，association.c93；tests/core/CMake30、fixture144、message_test335、blocks_test271、allocation_test143。335行测试单一职责为消息流的归并/绑定/终态/额度不变量，根CMake只装配target；所有文件<500，无100列例外。target仍Core→Foundation，不依赖UI/OS/Adapter。
- 完成审查：APP-04领域交付及CORE-MSG-01/02/03、适用CORE-REL/SES迟到消息场景覆盖；真实wire归一化、持久化、Backend事件和Windows呈现属于各自后续任务，不以此标记它们完成。README/正式接口§19/代码落位/任务表同步；临时烟测清理。本轮按用户指示不安排macOS。下一任务按规划继续APP-05/APP-06及其依赖，不提前把演示按钮标成功能。
