# 09 C 模块分包与目录约定

## 1. 目标

C 语言没有 Java `package` 语法；Aether 通过目录、公开头文件、私有头文件和 CMake target 建立等价的工程模块边界。目录不是装饰：它必须和依赖方向、所有权、测试及任务责任一致。

## 2. 标准模块结构

```text
include/aether/<module>/       # 模块公共头文件
src/<module>/                  # 模块实现
src/<module>/internal/         # 模块私有头文件（需要时）
tests/<module>/                # 模块单元测试
```

当前模块：

```text
include/aether/foundation/     # ID、错误、状态、字符串等无业务基础能力
src/foundation/
tests/foundation/

include/aether/core/           # 领域对象和状态
src/core/
tests/core/

apps/desktop/                  # 应用组合根，不是业务模块
```

后续真实任务开始时再建立 `backend`、`adapters`、`infrastructure`、`platform` 等目录；不创建空目录或空模块。

## 3. 依赖规则

- 公共头文件只能包含允许的公共头文件和标准 C 头文件；禁止包含 `.c` 文件或私有头文件。
- `foundation` 不依赖 Core、Backend、Adapter、Infrastructure 或 Platform。
- `core` 可依赖 foundation 和自己定义的抽象端口；不得包含具体 Adapter、数据库、日志后端或 UI 头文件。
- 应用入口可以依赖多个已组装 target；它是组合根，不承担领域规则。
- 模块实现文件可包含本模块私有头文件；跨模块内部协作必须先评估是否应成为公开契约，禁止通过相对路径偷偷穿透边界。
- CMake target 表达可链接依赖。能独立测试的模块应有独立 target；测试 target 只链接被测模块及其明确依赖。

## 4. 命名和规模

公共头文件使用 `aether_<module>/name.h`；公共函数使用 `aether_` 前缀；模块私有函数保持 `static`。一个文件一个主要职责；500 个物理行是硬上限，超过300行先审查。禁止用 `part1.c`、`part2.c`、巨型宏或包含实现片段隐藏规模。

模块拆分按真实边界进行：状态所有者、协议解析、数据映射、IO 资源、应用编排分别拆分；不能复制状态机、DTO、错误合同或资源所有者。

## 5. CMake 模块模板

每个真实模块至少需要：

1. 一个明确的 library target 或应用 target。
2. 只暴露必要的 `PUBLIC` include 路径。
3. 私有实现依赖使用 `PRIVATE`。
4. 目标级 C17 与严格警告。
5. 对应的行为测试 target。
6. 任务清单中的交付和验收记录。

不为只有一个内部函数的模块机械创建接口、工厂或 target。目录和 target 随真实职责增长，不能预建整个规划树。

## 6. AI 实施要求

修改模块前读取本文件、模块详细设计和任务表。移动文件必须同步所有 include、CMake source、测试和文档链接；不保留兼容别名掩盖旧路径。完成报告列出模块依赖、文件职责、物理行数和实际验证。
