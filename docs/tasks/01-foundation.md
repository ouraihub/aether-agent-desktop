# 01 工程与跨平台基础任务

执行规则及跨文件ID索引见 [总看板](README.md)。状态只在本表维护。

设计入口：[平台执行详细设计](../design/detail/06-platform-execution.md)、[诊断详细设计](../design/detail/07-observability.md)、[工程规范](../CODING_STANDARDS.md)。用例EXEC/OBS编号均指对应详细文档，不是已运行测试。

| ID | 任务 | 状态 | 硬依赖 | 交付物与验收条件 |
| --- | --- | --- | --- | --- |
| ENG-01 | 建立真实C17/CMake工程 | 已完成 | DEC-02 | 已建立CMakeLists.txt、CMakePresets.json、foundation/core独立target、桌面入口和模块测试；Windows Clang/Ninja实际配置、编译、CTest及入口运行通过，macOS/Linux未验证 |
| ENG-02 | 代码规模与质量检查入口 | 已完成 | ENG-01 | `tools/check_code.py` 检查物理行、300行审查提示、500行硬上限、C行宽、Tab和行尾空白；已纳入CTest，合规检查实际通过 |
| ENG-11 | 最小Backend/Core生命周期边界 | 已完成 | ENG-01, DEC-03 | 已建立`aether_backend` target、Backend借用Core的公开生命周期接口和Backend smoke；Windows CMake/CTest 4/4及质量检查通过；不包含异步请求、DTO、事件、持久化或Runtime接入 |
| ENG-03 | 基础值、错误与资源合同实现 | 未开始 | ENG-01, DEC-03 | ID/字符串/类型化错误及释放接口；分配失败和部分初始化不泄漏，失败输出明确；不用巨型类型/容器框架 |
| ENG-04 | Platform时钟/路径/基础资源 | 未开始 | ENG-03, DEC-04 | 单调/墙钟、系统目录、编码及不透明资源接口；验证Unicode路径、非法路径/平台错误映射与释放；各目标OS证据交REL任务汇总 |
| ENG-05 | 有界执行器与计时器 | 未开始 | ENG-04, DEC-11 | 工作投递、取消观测、代次防晚到、关闭；通过EXEC-06/08及队列满、部分初始化；UI路径不得同步执行阻塞工作 |
| ENG-06 | Process Service | 未开始 | ENG-05, DEC-07 | 参数数组/环境白名单、stdout/stderr、写入完成、退出和受管关闭；实际可控进程验证EXEC-01至05，借用进程不误杀 |
| ENG-07 | 网络/HTTP与安全存储 | 未开始 | ENG-05, DEC-07, DEC-12 | 按已确认需求提供实际网络/HTTP和凭据端口；本地服务验证超时/TLS/取消/大小限制，安全存储不可用不明文回退；EXEC-09 |
| ENG-08 | TraceContext与Operation | 未开始 | ENG-03, ENG-04, DEC-10 | root/child、显式传播、一次终结、unfinished；验证OBS-02/03/04/05，跨栈异步无悬空，禁止全局当前trace |
| ENG-09 | 日志输出/轮转/健康状态 | 未开始 | ENG-05, ENG-08, DEC-11 | 有界记录、脱敏、六级过滤、实际文件轮转/读取基础、非递归故障；OBS-06/07及真实不可写/队列满，不修改业务错误 |
| ENG-10 | 可控测试设施与持续检查 | 未开始 | ENG-02, ENG-03, ENG-05 | 隔离时钟、临时数据/故障注入、可控子进程/Provider测试源及CI检查；真实运行确定性场景，测试替身明确标识，不作为正式Runtime |

实现时将公开符号与所有调用方同步，所有权和线程合同写入公开说明；当前无源代码，不能因为这里列出“CI”就声称已有CI。每项完成记录实际平台，三平台最终可运行性仍必须分别验收。
