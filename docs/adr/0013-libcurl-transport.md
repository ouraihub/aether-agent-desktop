# ADR 0013：libcurl HTTP 传输替换 WinHTTP

- 状态：已接受（用户明确选择；工程验证另计）
- 日期：2026-11-08
- 依据：本会话用户选择“引入 libcurl C 库”，并要求继续；工程规范 HTTP 候选与 ENG-07。
- 关系：替换本轮尚未发布的 WinHTTP GET 实验，不改变 ADR-0012 凭据后端及不回显要求。

## 问题与证据

Windows WinHTTP GET 实验在功能测试后出现每请求新增 Event，保留资源断言持续失败。
本会话独立裸 WinHTTP 程序、不链接 Aether 库也复现；纯 Winsock/fixture 对照不增长。
临时事件创建栈定位到 webio.dll→winhttp.dll，系统文件版本及实际结果见执行记录。
该证据不能推断所有 Windows 版本都有相同问题，也不能据此直接关闭系统内部句柄。

不删除断言、不升级或替换系统 DLL，也不使用 curl.exe 子进程代替库调用。
用户已明确批准引入成熟 libcurl C 库，采用一次替换而非保留并行生产路径。

## 决定

1. 固定官方 curl/libcurl **8.23.0** 源码；官方发行页列日期 2026-10-14。
   来源、archive 校验、许可证及编译选项维护在 third_party/README.md；不依赖浮动版本
   或系统 curl 可执行文件。官方来源：<https://curl.se/download.html>。
2. 采用上游 CMake 静态 libcurl，隔离第三方警告；自研 Infrastructure 包装维持 ISO C17
   严格警告与独立公共头，Core/Backend DTO/Frontend 不含 CURL 结构或句柄。
   本轮 Windows TLS 后端选 **Schannel**，不另引入 OpenSSL，不关闭证书/主机名验证。
3. 本次依然只发布受控 GET 技术切片：HTTP(S)、URL 复制及限制、有限响应、总截止、
   在途取消、owned response 与真实 HTTP 状态。禁止自动跳转、环境代理、netrc、cookie
   与宿主身份自动认证，不自动重放副作用。POST、模型/SSE、认证头与业务授权由后续任务实现。
4. 用 libcurl multi 控制请求；handle 由 worker 独占，取消通过同步意图及有界唤醒检查
   交接，不能在外部线程销毁在用 handle。总截止从 start 计算，不能每次读体重新计时。
5. libcurl 全局初始化和清理覆盖所有活跃请求及仍被调用者保留的资源；不得一个请求
   cleanup 破坏其他请求。共享状态仅为第三方基础生命周期引用计数，不持有隐式业务事实。
6. 同步迁移全部当前消费者、构建与文档，并删除失效 WinHTTP 源码、私有头、链接依赖。
   不保留兼容转发、双后端开关或系统 DLL 回退。

## 取舍与验证门槛

收益是复用成熟 C HTTP/TLS 实现及现有安全选项；代价是增加第三方源与升级维护责任。
HTTP 限额、取消、TLS 与资源回收仍需真实验证，库名不是安全或无泄漏证明。

保留已有自包含 loopback fixture、精确目标/二进制响应/状态码、超限、总截止慢滴流、
收到请求后取消、重复 wait/提前 destroy、资源不累计、自签 TLS 拒绝及正常非零失败诊断。
不能将排除资源套件后的其余回归写成全量通过。仅 Windows 验证不代表 Linux/macOS 可用；
非 Windows 的 TLS 依赖和运行证据单独落实，不用假成功后端填充平台矩阵。

升级时核对 curl 官方发布说明与安全公告、固定 archive/hash、许可证及构建选项，运行
同一资源和 TLS/取消回归；不静默改为系统 libcurl。未运行的检查不得记录为已通过。
