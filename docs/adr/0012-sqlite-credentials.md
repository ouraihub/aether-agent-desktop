# ADR 0012：OMP 式普通 SQLite JSON 凭据存储

- 状态：已接受（本会话用户明确选择；工程验收另计）
- 记录日期：2026-11-08
- 对应任务：ENG-07-C；父任务 ENG-07 仍包含未实现的网络/HTTP
- 决策依据：用户明确选择“照 OMP 存普通 SQLite”，接受数据库、WAL/journal 与备份泄漏秘密的风险；本机 Pi 1.5.0、OMP 18.18.0 精确源码及 ADR-0008 的 SQLite/JSON 基线。
- 替代关系：部分替代 [ADR-0008](0008-data-and-configuration.md) §5 的系统 Secret Store/跨安全边界要求，以及 [ARCHITECTURE](../ARCHITECTURE.md) §6.3 的敏感配置必须加密或使用系统安全存储要求。ADR-0008 的非秘密配置、CAS、保存/生效、迁移和外部副作用边界保留。

## 背景与源码依据

本次是用户明确批准的存储取舍，不是系统安全存储故障后的降级，也不把“SQLite”或“JSON”称为加密。

| 参考版本 | 实际源码与观察 | 采用范围 |
| --- | --- | --- |
| Pi 1.5.0 | `pi/packages/coding-agent/package.json:2-3`；`src/core/auth-storage.ts:36-37,64-78,98-109,451-481`：`auth.json` 后端、文件创建 mode 0600、锁内读取及 `JSON.stringify`/UTF-8 写回 API key/OAuth 数据 | 凭据数据与普通设置逻辑分离；普通文件权限不是凭据加密 |
| OMP 18.18.0 | `oh-my-pi/packages/ai/package.json:2-3`；`src/auth/sqlite-credential-store.ts:140-157`：`serializeCredential` 对 `api_key` 的 key 及 `oauth` 数据调用 `JSON.stringify`；`:786-798`：`auth_credentials.data TEXT NOT NULL`；`:467-474`：绑定参数写入；`:637-646`：普通 SQLite/WAL | 直接采用普通 SQLite 持久化 JSON 凭据的取舍，不照搬完整账户/租约/缓存系统 |
| OMP SQLite 打开路径 | `oh-my-pi/packages/utils/src/sqlite.ts:174-187`：从 `bun:sqlite` 创建 `new Database(dbPath)`；凭据文件 `:7,448-450` 接收该 Database，无凭据加密变换 | Aether 改用 C API/yyjson，不引入 Bun、TypeScript 或加密扩展 |

源码位置均相对于本机参考仓库根；只阅读公开源码和版本元数据，不读取用户的 `auth.json`、数据库、环境秘密或其他真实凭据。上游仓库来源：[Pi](https://github.com/earendil-works/pi)、[OMP](https://github.com/can1357/oh-my-pi)。精确行号是上述本机版本调查位置，不承诺未来上游分支不变。

## 决策

1. **持久凭据采用非加密的普通 SQLite JSON 数据。** 固定 SQLite **3.54.1**（Public Domain）与 yyjson **0.13.0**（MIT）基线，沿用 ADR-0008 的依赖来源、校验值和私有适配边界。`api_key`/`oauth` JSON 必须受类型、UTF-8、长度和总量约束，不接受任意无界载荷；通过绑定参数及 revision-CAS CRUD 访问。不引入 SQLCipher、应用层加密、系统安全后端或加密失败回退。
2. 普通配置仍只保存不透明 `secret_ref` 和必要版本/可用状态，不在普通配置值、Session、历史消息中复制秘密。引用指向 credentials 数据，而不是必须指向 OS Secret Store。凭据与普通配置可在整合时定义为同一数据库/同一事务；本决定既不强制跨库，也不承诺尚未接通的跨模块原子提交。若实际分库，必须显式处理部分成功、引用清理及仍在使用的记录，不能假称跨库原子。
3. 凭据存储同步操作仅供 worker 执行域；调用者串行化同一实例，UI 不直接打开数据库或读取秘密。具体公共接口由正式接口契约及公开头文件定义，不由本 ADR 预造签名、容量值或错误枚举。
4. `secret_ref` 缺失、删除、版本不兼容、损坏、权限拒绝或 IO 失败时，报告稳定业务错误及真实状态类别，阻止依赖秘密的新建/重连/外部调用。**不得使用空值、默认值、掩码、旧缓存或环境变量回退**，不得静默清空/重建损坏库。旧实例的真实运行事实可以查询，但不能借旧值伪报新的读取或配置应用成功。
5. 临时 API key/调用覆盖和一次性授权仍不持久化；只有明确要求保存的凭据才进入 credentials 数据。普通配置优先级及 Schema 显式允许的 CLI/ENV 临时覆盖不变，ENV 不是凭据读取失败的替代源。不自动探测、读取、导入 Pi/OMP 或其他外部用户凭据；读取与传递秘密仍须合法来源、授权和真实调用需要。
6. **明文存储不授权明文回显。** 日志、诊断、错误、事件及普通 Query/UI DTO 禁止携带秘密；UI 只表达已配置/未配置/不可用及保留、替换、清除意图，不读取、显示或复制已存原密钥，不把掩码当值提交。受信任 worker 为经授权的外部调用取得秘密，不构成 UI 查询许可。

## 明确接受的风险与边界

- 数据库正文、`-wal`、rollback `-journal`、保全副本和备份都可能包含 API key、access/refresh token 等秘密，须按秘密文件处理。导出普通设置不包含秘密；数据库备份不能因普通导出规则被描述为无秘密。
- 删除是逻辑删除/停止后续正常读取，**不是取证擦除**。旧页、WAL/journal、文件系统快照或备份可能保留旧值；不以 checkpoint、VACUUM 或文件删除声称已安全擦除所有副本。
- OS 文件/目录访问限制只减少其他账户等未授权访问，不是加密，不隔离同一 OS 用户下的进程，也不能抵御管理员、恶意同用户进程或被攻陷 Runtime。用户明确接受这部分风险；不得将应用模块或 `secret_ref` 称为 OS 安全边界。
- 本次不降低 TLS 证书验证、授权、环境白名单、路径检查、不泄日志或不回显合同。已保存与已生效必须分开，不可用时明确拒绝，而不是引入另一套后端掩盖失败。

## 替代方案与理由

- **系统凭据库/Keychain/Secret Service 或应用加密**：与本会话用户明确选择不同，本次不实现、不保留自动回退，不继续列为凭据落地前置条件。将来改选须另立 ADR、明确迁移与密钥生命周期。
- **Pi 式明文 auth.json**：可表达凭据，但本地存储已采用 SQLite；采用 OMP 的 JSON→TEXT 路径可复用事务/CAS和既定依赖，避免并行文件存储约定。
- **把秘密直接放普通配置值**：拒绝；物理明文不取消逻辑分区、引用、授权及 DTO/日志限制。

## 实施与验收交接

ENG-07-C 按本决策进行中，必须以隔离临时数据库和合成 API key/OAuth 数据验收保存/重开、revision 冲突、替换/删除、受限 JSON、版本/损坏/不可写错误及无秘密回显；不得使用实际用户凭据。依赖落地、平台行为及运行结果由实际工程记录证明，本 ADR 不声明 build/tests 通过。

ENG-07 的网络/HTTP 超时、TLS、取消及大小限制仍须单独实现和验收；凭据切片完成不等于 ENG-07、DATA-01 至 DATA-04、配置 UI 或 Runtime 认证已完成。当前详细接口以 [INTERFACE_CONTRACTS](../INTERFACE_CONTRACTS.md) 及工程公开头文件为准。
