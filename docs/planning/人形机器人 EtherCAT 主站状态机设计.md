# 人形机器人 EtherCAT 主站状态机设计

## 1. 文档定位

本文档细化《人形机器人 EtherCAT 主站功能规划》中的主站生命周期部分，用于定义人形机器人 EtherCAT 主站的稳定状态、状态迁移条件、异常分支和验收要求。

本文档只定义**主站生命周期状态机**。EtherCAT 从站状态、CiA402 伺服状态、轴组状态和安全状态应作为独立状态模型管理，不应全部塞进主站状态机。

## 2. 设计原则

1. **主站状态只表达稳定工况**：PDO 配置、DC 配置、状态确认等短暂步骤作为迁移动作，不作为主状态。
2. **正常流程清晰线性**：主站从未初始化逐步进入维护、准备运行和运行状态。
3. **故障分支独立**：普通故障进入 `Fault`，紧急安全事件进入 `EmergencyStop`。
4. **禁止隐式跳转**：所有状态迁移必须经过统一入口、检查迁移表并记录日志。
5. **状态与动作分离**：状态表示“当前处于什么稳定工况”，动作表示“正在执行什么操作”。
6. **支持人形机器人分组维护**：轴组、抱闸、伺服使能等流程不扩展为主站状态，而是由轴组状态机和活动任务管理。

## 3. 主站状态集合

推荐主站生命周期设计为 8 个稳定状态：

```text
Uninitialized
  -> AdapterReady
  -> Scanned
  -> Maintenance
  -> ReadyToRun
  -> Operational

任意状态 -> Fault
任意状态 -> EmergencyStop
Fault / EmergencyStop -> Uninitialized
Fault -> Maintenance（受限恢复路径）
```

| 状态 | 含义 | 是否允许实时控制 |
|------|------|------------------|
| `Uninitialized` | 主站进程存在，但网卡、SOEM、从站资源未初始化 | 否 |
| `AdapterReady` | 网卡绑定成功，底层 EtherCAT 通信环境可用 | 否 |
| `Scanned` | 已扫描从站，完成身份读取、拓扑识别和基础对象创建 | 否 |
| `Maintenance` | 从站处于 PREOP 或维护可访问状态，可执行 SDO、参数、诊断、PDO 映射配置 | 否 |
| `ReadyToRun` | PDO、IOmap、FMMU/SM、DC、初始输出已准备，从站处于 SAFEOP 或等价准备状态 | 否 |
| `Operational` | 从站进入 OP，周期 PDO 正常运行，上层实时控制生效 | 是 |
| `Fault` | 发生可诊断、可复位或需重新初始化的普通故障 | 否 |
| `EmergencyStop` | 外部急停、硬限位、STO、严重跟随误差等紧急安全事件 | 否 |

## 4. 状态职责

### 4.1 `Uninitialized`

职责：

- 持有最小进程资源。
- 不持有有效网卡绑定或 EtherCAT 上下文。
- 允许加载配置、选择网卡、准备日志系统。

允许动作：

- `InitializeAdapter`
- `LoadConfig`
- `Exit`

禁止动作：

- 扫描从站。
- SDO/PDO 访问。
- 伺服使能。

### 4.2 `AdapterReady`

职责：

- 表示网卡绑定和底层主站初始化完成。
- 可执行从站扫描。

进入条件：

- 网卡存在。
- 权限满足。
- SOEM 或底层驱动初始化成功。
- 基础配置加载成功。

允许动作：

- `ScanSlaves`
- `Stop`

### 4.3 `Scanned`

职责：

- 表示主站已掌握当前总线从站列表和拓扑。
- 建立从站节点、身份信息和初始 Profile 匹配结果。

进入条件：

- 扫描成功。
- 从站数量满足配置要求。
- Vendor / Product / Revision / Serial 已读取。
- 拓扑顺序和配置策略不冲突。

允许动作：

- `EnterMaintenance`
- `InspectTopology`
- `Stop`

### 4.4 `Maintenance`

职责：

- 作为非实时维护主战场。
- 支持 SDO 读写、批量参数配置、PDO 映射配置、诊断、固件升级准备、单站恢复。

进入条件：

- 必要从站进入 PREOP。
- 邮箱通信可用。
- 从站身份与配置匹配。
- 不存在阻止维护的紧急安全事件。

允许动作：

- `ReadSdo`
- `WriteSdo`
- `ApplyStartupConfig`
- `ConfigurePdoMapping`
- `DiagnoseSlave`
- `PrepareRun`
- `Stop`

### 4.5 `ReadyToRun`

职责：

- 作为 `Maintenance` 与 `Operational` 之间的稳定检查点。
- 表示进入 OP 前的所有关键资源已准备完成。

进入条件：

- SDO 启动配置完成。
- PDO 映射与 Profile 校验通过。
- IOmap 构建完成。
- FMMU/SM 配置完成。
- DC 配置完成或按配置明确禁用。
- 初始输出为安全值。
- 必要从站进入 SAFEOP。
- 轴组上电/抱闸/使能前置条件检查通过。

允许动作：

- `StartOperation`
- `BackToMaintenance`
- `Stop`

设计说明：

- `ReadyToRun` 是人形机器人主站建议保留的状态。多轴系统启动失败时，它能清晰区分“维护配置已完成”与“运行周期已启动”。
- 如果不设置该状态，`Maintenance -> Operational` 会成为过大的黑盒，不利于定位 PDO、DC、SAFEOP、初始输出等问题。

### 4.6 `Operational`

职责：

- 运行周期 PDO。
- 接收上层实时命令。
- 输出轴反馈、总线状态、周期统计和故障状态。
- 执行 WKC、DC、从站状态、命令超时、伺服状态监控。

进入条件：

- 周期线程启动成功。
- 从站全部或必要从站进入 OP。
- 首周期或启动窗口内 WKC 正常。
- DC 偏差进入配置窗口。
- 上层实时接口准备就绪，或配置允许先进入受控保持状态。

允许动作：

- `RunCycle`
- `UpdateCommand`
- `PublishFeedback`
- `RequestControlledStop`
- `BackToMaintenance`
- `Stop`

### 4.7 `Fault`

职责：

- 表示普通故障后的受控诊断状态。
- 保留故障快照。
- 允许部分诊断和受控恢复。

典型触发：

- 扫描失败。
- 配置失败。
- SDO 超时超过重试次数。
- PDO 映射不匹配。
- 连续 WKC 异常。
- 从站状态异常。
- DC 同步异常。
- 伺服 Fault。
- 上层命令超时。

允许动作：

- `DumpSnapshot`
- `Diagnose`
- `FaultReset`
- `RecoverToMaintenance`
- `Stop`

恢复约束：

- 只有确认故障原因已消除，且不会破坏安全边界时，才允许 `Fault -> Maintenance`。
- 涉及链路掉线、拓扑变化、配置不匹配的故障，默认走 `Fault -> Uninitialized` 后重新初始化。

### 4.8 `EmergencyStop`

职责：

- 表示紧急安全事件后的锁定状态。
- 禁止自动恢复。
- 保留最高优先级安全事件快照。

典型触发：

- 外部 E-stop。
- STO 触发。
- 硬限位触发。
- 严重跟随误差。
- 明确失控风险。
- 安全控制器要求停机。

允许动作：

- `DumpSnapshot`
- `AcknowledgeEmergency`
- `Stop`

恢复约束：

- 不允许自动回到 `Operational`。
- 默认只允许 `EmergencyStop -> Uninitialized`。
- 如需允许 `EmergencyStop -> Maintenance`，必须有独立安全论证、人工确认和硬件状态确认。

## 5. 正常迁移表

| 迁移 | 触发动作 | 迁移条件 | 失败去向 |
|------|----------|----------|----------|
| `Uninitialized -> AdapterReady` | `InitializeAdapter` | 网卡存在、权限满足、底层主站初始化成功、配置加载成功 | `Fault` |
| `AdapterReady -> Scanned` | `ScanSlaves` | 扫描成功、从站数量满足要求、身份读取成功、拓扑可接受 | `Fault` |
| `Scanned -> Maintenance` | `EnterMaintenance` | 必要从站进入 PREOP、邮箱通信可用、Profile 匹配通过 | `Fault` |
| `Maintenance -> ReadyToRun` | `PrepareRun` | SDO 初始化、PDO 映射、IOmap、FMMU/SM、DC、初始输出全部完成 | `Fault` |
| `ReadyToRun -> Operational` | `StartOperation` | 周期线程启动、从站进入 OP、WKC 正常、DC 偏差达标 | `Fault` |
| `Operational -> Maintenance` | `BackToMaintenance` | 轴组安全停止、周期线程停止、从站退回 PREOP、邮箱可用 | `Fault` |
| `ReadyToRun -> Maintenance` | `BackToMaintenance` | 放弃进入 OP，从站回到 PREOP 或保持可维护状态 | `Fault` |
| `Maintenance -> Uninitialized` | `Stop` | 停止活动、从站回 INIT 或释放资源 | `Uninitialized` |
| `ReadyToRun -> Uninitialized` | `Stop` | 停止启动流程、释放 IOmap 和主站资源 | `Uninitialized` |
| `Operational -> Uninitialized` | `Stop` | 轴组安全停止、周期线程停止、从站降级、释放资源 | `Uninitialized` |

## 6. 异常迁移表

| 迁移 | 触发条件 | 处理要求 |
|------|----------|----------|
| 任意状态 -> `Fault` | 普通错误、配置错误、通信错误、伺服 Fault、命令超时 | 记录故障原因、当前状态、从站快照和轴状态快照 |
| 任意状态 -> `EmergencyStop` | E-stop、STO、硬限位、严重跟随误差、安全控制器要求停机 | 最高优先级处理，禁止自动恢复 |
| `Fault -> Maintenance` | 故障已清除，拓扑未变化，邮箱可用，安全条件满足 | 只允许受控恢复，用于诊断和重新准备运行 |
| `Fault -> Uninitialized` | 用户 Stop/Reset，或故障需要完整重启 | 释放资源并允许重新初始化 |
| `EmergencyStop -> Uninitialized` | 急停解除、人工确认、硬件安全状态确认 | 重新初始化前必须保留事件记录 |

## 7. 不建议作为主站状态的内容

以下内容应由迁移动作、活动任务或子状态机表达，不建议扩展为主站主状态：

| 内容 | 推荐归属 | 原因 |
|------|----------|------|
| `ConfiguringPdo` | `PrepareRun` 内部步骤 | 过程短暂，失败进入 `Fault` 即可 |
| `ConfiguringDc` | `PrepareRun` 内部步骤 | 属于进入 `ReadyToRun` 的条件 |
| `SafeOp` | EtherCAT 从站状态 | 主站只关心是否达到 `ReadyToRun` |
| `Boot` / `Bootstrap` | 单站维护 Activity | 固件升级通常只影响单个从站 |
| `Homing` | 轴或轴组状态机 | 不是主站总线生命周期 |
| `ServoEnable` | CiA402 / 轴组状态机 | 多轴可分组使能，不适合做全局主站状态 |
| `Braking` | 轴组状态机 | 抱闸控制与机械结构相关 |
| `Recovering` | 活动任务或动作状态 | 可作为动作进度，不应污染稳定状态 |

## 8. 与其他状态机的关系

### 8.1 EtherCAT 从站状态

EtherCAT 从站状态包括 INIT、PREOP、SAFEOP、OP、BOOT。主站状态与从站状态是映射关系，不是一一对应关系。

| 主站状态 | 典型从站状态 |
|----------|--------------|
| `Uninitialized` | 不要求 |
| `AdapterReady` | 不要求 |
| `Scanned` | INIT / PREOP 过渡 |
| `Maintenance` | PREOP |
| `ReadyToRun` | SAFEOP |
| `Operational` | OP |
| `Fault` | 取决于故障点 |
| `EmergencyStop` | 取决于停机路径和硬件安全链路 |

### 8.2 CiA402 伺服状态

CiA402 状态机应独立管理，例如 Switch On Disabled、Ready to Switch On、Switched On、Operation Enabled、Fault 等。主站进入 `Operational` 只表示 PDO 通信运行，不等于所有伺服轴都已 `Operation Enabled`。

### 8.3 轴组状态

人形机器人建议为腿、臂、腰、头、手等轴组建立轴组状态机，例如：

```text
Disabled -> PowerReady -> BrakeReleased -> ServoEnabled -> Active -> Stopping -> Fault
```

轴组状态由机器人结构和安全策略决定，不应与主站生命周期混为一个状态机。

### 8.4 安全状态

安全状态建议独立定义为：

```text
Normal -> Warning -> RecoverableFault -> FatalFault -> Emergency
```

安全状态可以驱动主站进入 `Fault` 或 `EmergencyStop`，但不应替代主站生命周期状态。

## 9. 关键动作定义

| 动作 | 主要职责 |
|------|----------|
| `InitializeAdapter` | 初始化配置、绑定网卡、初始化底层主站上下文 |
| `ScanSlaves` | 扫描从站、读取身份、识别拓扑、创建从站节点 |
| `EnterMaintenance` | 请求 PREOP、建立邮箱通信、执行基础身份校验 |
| `PrepareRun` | 执行 SDO 初始化、PDO 映射、IOmap、FMMU/SM、DC、SAFEOP 和初始输出 |
| `StartOperation` | 启动周期线程、请求 OP、确认 WKC/DC/从站状态 |
| `BackToMaintenance` | 受控停止轴组、停止周期线程、回到 PREOP 维护状态 |
| `Stop` | 安全停止、释放主站资源、回到未初始化 |
| `RequestFault` | 记录普通故障并进入 `Fault` |
| `RequestEmergencyStop` | 处理紧急安全事件并进入 `EmergencyStop` |

## 10. 状态迁移守卫条件

每次迁移至少检查：

- 当前状态是否允许该动作。
- 是否存在未完成的互斥活动。
- 是否存在未确认的安全事件。
- 从站数量、身份、拓扑是否符合配置策略。
- 必要从站是否达到期望 EtherCAT 状态。
- WKC、DC、周期线程状态是否满足迁移要求。
- 上层实时接口是否满足运行要求。
- 轴组、抱闸、限位、急停状态是否满足安全要求。

## 11. 日志与快照要求

每次状态迁移必须记录：

- 旧状态。
- 新状态。
- 触发动作。
- 操作者或调用来源。
- 迁移耗时。
- 成功或失败原因。

进入 `Fault` 或 `EmergencyStop` 时必须记录：

- 主站状态。
- 从站列表、EtherCAT 状态、AL status code。
- WKC、DC 偏差、周期统计。
- 轴状态、CiA402 状态字、错误码、目标值、反馈值。
- 最近 N 个周期的命令与反馈窗口。

## 12. 验收要求

| 验收项 | 要求 |
|--------|------|
| 合法迁移 | 所有正常迁移可按顺序完成，并记录日志 |
| 非法迁移 | 非法命令被拒绝，不改变当前状态 |
| 失败回滚 | 任一启动阶段失败都进入 `Fault` 并保留原因 |
| Stop 行为 | 任意非紧急状态下 `Stop` 可回到 `Uninitialized` |
| Emergency 行为 | 急停触发后进入 `EmergencyStop`，禁止自动恢复 |
| Fault 恢复 | 可恢复故障满足条件后可进入 `Maintenance` |
| 快照完整性 | 故障快照包含主站、从站、轴、周期和命令上下文 |
| 并发保护 | 状态迁移期间拒绝互斥动作或重复命令 |

## 13. 推荐实现接口

状态机建议通过统一入口实现：

```cpp
enum class MasterState {
    kUninitialized,
    kAdapterReady,
    kScanned,
    kMaintenance,
    kReadyToRun,
    kOperational,
    kFault,
    kEmergencyStop,
};

enum class MasterAction {
    kInitializeAdapter,
    kScanSlaves,
    kEnterMaintenance,
    kPrepareRun,
    kStartOperation,
    kBackToMaintenance,
    kStop,
    kRequestFault,
    kRequestEmergencyStop,
};
```

实现建议：

- 使用迁移表集中定义允许关系。
- 每个动作对应一个明确执行函数。
- 执行函数成功后才更新状态。
- 执行函数失败统一进入 `Fault` 或 `EmergencyStop`。
- 状态变更和故障原因必须结构化记录。
- 周期线程、维护 Activity、命令接口不能直接改状态，只能提交动作请求。

## 14. 结语

人形机器人 EtherCAT 主站状态机应保持克制：主站只描述总线生命周期，伺服、轴组、安全和维护任务分别由独立模型管理。推荐的 8 状态设计能兼顾启动可诊断性、运行确定性和安全恢复边界，适合作为后续代码和其他设计文档收敛的基准。
