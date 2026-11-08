# 01 工程与跨平台基础任务

执行规则及跨文件ID索引见 [总看板](README.md)。状态只在本表维护。

设计入口：[平台执行详细设计](../design/detail/06-platform-execution.md)、[诊断详细设计](../design/detail/07-observability.md)、[工程规范](../CODING_STANDARDS.md)。用例EXEC/OBS编号均指对应详细文档，不是已运行测试。

| ID | 任务 | 状态 | 硬依赖 | 交付物与验收条件 |
| --- | --- | --- | --- | --- |
| ENG-01 | 建立真实C17/CMake工程 | 已完成 | DEC-02 | 已建立CMakeLists.txt、CMakePresets.json、foundation/core独立target、桌面入口和模块测试；Windows Clang/Ninja实际配置、编译、CTest及入口运行通过，macOS/Linux未验证 |
| ENG-02 | 代码规模与质量检查入口 | 已完成 | ENG-01 | `tools/check_code.py` 检查物理行、300行审查提示、500行硬上限、C行宽、Tab和行尾空白；已纳入CTest，合规检查实际通过 |
| ENG-11 | 最小Backend/Core生命周期边界 | 已完成 | ENG-01, DEC-03 | 已建立`aether_backend` target、Backend借用Core的公开生命周期接口和Backend smoke；Windows CMake/CTest 4/4及质量检查通过；不包含异步请求、DTO、事件、持久化或Runtime接入 |
| ENG-12 | 本地有界请求记录与结果保留 | 已完成 | ENG-11 | 同步8槽记录登记、快照、终态一致性与一次提交、显式释放/停止收尾；无UI独立程序实际运行，5/5 CTest与Clang分析通过；仅本地记录，不替代APP-07业务/异步/追踪验收 |
| ENG-03 | 基础值、错误与资源合同实现 | 已完成 | ENG-01, DEC-03 | Foundation提供进程安全ID、UTF-8字符串视图校验/复制/比较、完整基础状态类别和固定容量错误复制；失败输出明确，无动态资源泄漏路径；不使用容器框架 |
| FIX-ID-01 | 共享ID耗尽永久拒绝 | 已完成 | ENG-03, ENG-05 | Foundation统一CAS饱和，UINT64_MAX只发一次后永久返回0不重用；并发近耗尽/INT64边界、Windows35/35、相关消毒器2/2与实际失败前后复现通过 |
| ENG-04 | Platform时钟/路径/基础资源 | 已完成 | ENG-03, DEC-04 | Unix纪元墙钟/单调时钟、拥有型UTF-8路径、OS当前/TEMP/USER_DATA目录；Windows中文/非BMP目录、系统目录、分配失败回滚及Linux平台实际烟测通过；macOS和完整跨平台桌面未验证 |
| ENG-05 | 有界执行器与计时器 | 已完成 | ENG-04, DEC-11 | 原生worker/有界双限投递、复制输入/Trace、取消观测、结果保留、代次计时事实及close-complete；Windows18/18、Linux19/19、相关ASan/UBSan及真实后台计算通过，部分初始化回滚验证；macOS/TSan未验证，不代表Process/异步Backend已接通 |
| ENG-06 | Process Service | 进行中 | ENG-05, DEC-07 | 参数数组/环境白名单、stdout/stderr、写入完成、退出和受管关闭；实际可控进程验证EXEC-01至05，借用进程不误杀；Windows优先，非Windows仍保留未验证 |
| ENG-06-W | Windows受控进程与有界管道 | 已完成 | ENG-05, DEC-07 | 实际argv/环境/双流/写入/Job树/借用PID关闭、Windows40/40/分配回滚通过；连续环境/资源存储及统一释放后七源码MSVC/WX/analyze无诊断，首次失败句柄增量裸Win32对照不持续增长；不代替父任务授权/其他平台验收 |
| ENG-06-P | 执行授权与平台聚合联验 | 进行中 | ENG-06-W, APP-05-A, APP-05-B | Windows实际资源/参数固定和批准消费已由ENG-06-PW验证；非Windows、Runtime调用与共享取消边界仍保留聚合验收，父ENG-06不提前完成 |
| ENG-06-PW | Windows实际资源批准与spawn | 已完成 | ENG-06-W, APP-05-A, APP-05-B | 实际exe/cwd/Workspace及祖先句柄固定、owned argv/env精确绑定、单次消费立即spawn；真实Unicode标记/拒绝无文件/过期撤销/参数环境变化/重放/失败回滚通过，Windows42/42及九源码MSVC分析无诊断；非OS沙箱 |
| ENG-07 | 网络/HTTP与安全存储 | 未开始 | ENG-05, DEC-07, DEC-12 | 按已确认需求提供实际网络/HTTP和凭据端口；本地服务验证超时/TLS/取消/大小限制，安全存储不可用不明文回退；EXEC-09 |
| ENG-08 | TraceContext与Operation | 已完成 | ENG-03, ENG-04, DEC-10 | Foundation值型16/8字节TraceContext、root/child传播、Operation一次finish/unfinished和单调耗时保护；Windows CTest实际通过，未实现JSONL logger/writer/诊断查询 |
| ENG-09 | 日志输出/轮转/健康状态 | 未开始 | ENG-05, ENG-08, DEC-11 | 有界记录、脱敏、六级过滤、实际文件轮转/读取基础、非递归故障；OBS-06/07及真实不可写/队列满，不修改业务错误 |
| ENG-10 | 可控测试设施与持续检查 | 未开始 | ENG-02, ENG-03, ENG-05 | 隔离时钟、临时数据/故障注入、可控子进程/Provider测试源及CI检查；真实运行确定性场景，测试替身明确标识，不作为正式Runtime |

实现时将公开符号与所有调用方同步，所有权和线程合同写入公开说明；当前无源代码，不能因为这里列出“CI”就声称已有CI。每项完成记录实际平台，三平台最终可运行性仍必须分别验收。
