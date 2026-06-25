# 人形机器人 EtherCAT 从站管理设计

## 1. 文档定位

本文档细化《人形机器人 EtherCAT 主站功能规划》中的从站管理与抽象部分，用于定义在主站生命周期状态机之下，如何单独管理每个 EtherCAT 从站、如何组织从站集合、如何处理关键从站与可选从站、以及从站状态如何与 CiA402、轴组和安全策略协同。

本文档不替代主站状态机设计。主站状态机负责全局生命周期，从站管理负责每个从站的真实状态、配置状态、健康状态和设备语义。

## 2. 核心结论

推荐采用四层管理策略：

| 层级 | 责任 |
|------|------|
| 主站状态机 | 管全局生命周期和状态门禁 |
| `SlaveNodeManager` | 管从站集合、拓扑、批量状态切换、全局健康汇总 |
| `SlaveNode` | 管单个从站身份、EtherCAT 状态、配置状态、运行健康和 IOmap 绑定 |
| `DeviceProfile` / `ServoProfile` | 管设备语义、PDO 字段解释、SDO 初始化、CiA402 伺服逻辑 |

关键原则：

1. 主站状态不是所有从站状态的简单合集。
2. 每个从站必须有独立状态模型。
3. 单站操作必须通过统一主站底层接口执行，避免多个模块直接操作 SOEM。
4. 伺服、IO、安全模块等设备差异应由 Profile 层适配。
5. 人形机器人控制粒度应支持全局、单站、轴组三种层级。

## 3. 从站对象模型

每个扫描到的 EtherCAT 从站对应一个 `SlaveNode`。

`SlaveNode` 建议包含：

| 分类 | 字段 |
|------|------|
| 身份信息 | station address、alias、position、vendor id、product code、revision、serial |
| 角色信息 | required、optional、safety critical、diagnostic only |
| EtherCAT 状态 | actual state、expected state、AL status code |
| 配置状态 | identity、profile、mailbox、PDO、IOmap、DC、ready-to-run |
| 运行健康 | WKC、掉线、DC 偏差、状态丢失、设备故障 |
| IOmap 绑定 | input/output offset、bit length、PDO field mapping |
| Profile | 设备类型、PDO 语义、SDO 初始化、错误码解释 |

建议代码骨架：

```cpp
enum class SlaveRole {
    kRequired,
    kOptional,
    kSafetyCritical,
    kDiagnosticOnly,
};

enum class DeviceKind {
    kServoDrive,
    kIoModule,
    kSafetyModule,
    kSensor,
    kUnknown,
};

struct SlaveIdentity {
    uint16_t station_address{0};
    uint16_t alias{0};
    uint16_t position{0};
    uint32_t vendor_id{0};
    uint32_t product_code{0};
    uint32_t revision{0};
    uint32_t serial{0};
};

class SlaveNode {
public:
    const SlaveIdentity& Identity() const;
    SlaveRole Role() const;
    DeviceKind Kind() const;

    bool EnterMaintenance(EcMaster& master);
    bool PrepareForRun(EcMaster& master);
    bool RefreshRuntimeStatus(EcMaster& master);
    bool RequestState(EcMaster& master, EtherCatSlaveState state);

    bool IsRequiredForRun() const;
    bool IsReadyToRun() const;
    bool IsHealthy() const;
    bool HasFatalFault() const;

private:
    SlaveIdentity identity_;
    SlaveRole role_{SlaveRole::kRequired};
    DeviceKind kind_{DeviceKind::kUnknown};
    EtherCatSlaveState actual_state_{EtherCatSlaveState::kUnknown};
    EtherCatSlaveState expected_state_{EtherCatSlaveState::kUnknown};
    SlaveConfigStatus config_status_{SlaveConfigStatus::kUnverified};
    SlaveHealth health_{SlaveHealth::kUnknown};
    std::shared_ptr<DeviceProfile> profile_;
};
```

## 4. 从站状态模型

从站状态不建议只用一个枚举表达。应至少拆成四类状态。

### 4.1 EtherCAT 原生状态

该状态来自真实总线读取，用于描述 ESC / EtherCAT 协议层状态。

```cpp
enum class EtherCatSlaveState {
    kUnknown,
    kInit,
    kPreOp,
    kSafeOp,
    kOperational,
    kBootstrap,
    kError,
    kLost,
};
```

要求：

- `actual_state` 必须来自总线读取。
- `expected_state` 由主站或管理器设置。
- 状态不一致时必须记录 AL status code。
- `kBootstrap` 用于单站维护活动，不作为主站全局状态。

### 4.2 配置状态

配置状态描述从站是否已经与主站配置和 Profile 对齐。

```cpp
enum class SlaveConfigStatus {
    kUnverified,
    kIdentityVerified,
    kProfileMatched,
    kMailboxReady,
    kStartupSdoApplied,
    kPdoMapped,
    kIoMapReady,
    kDcReady,
    kReadyToRun,
    kConfigError,
};
```

要求：

- 配置状态主要在 `Maintenance -> ReadyToRun` 期间推进。
- 配置失败时应记录失败对象、索引、子索引、期望值和实际值。
- `kReadyToRun` 不等于从站已 OP，只表示进入 OP 前条件满足。

### 4.3 运行健康状态

运行健康状态用于汇总通信和设备运行风险。

```cpp
enum class SlaveHealth {
    kUnknown,
    kHealthy,
    kWarning,
    kRecoverableFault,
    kFatalFault,
    kLost,
};
```

典型来源：

- WKC 异常。
- 从站实际状态偏离期望状态。
- AL status code 异常。
- DC 偏差超限。
- 伺服 Fault。
- SDO/PDO 通信异常。
- 设备温度、电流、电压、编码器等厂商状态。

### 4.4 设备语义状态

设备语义由 Profile 层定义。

例如伺服驱动器使用 CiA402 状态：

```cpp
enum class CiA402State {
    kNotReadyToSwitchOn,
    kSwitchOnDisabled,
    kReadyToSwitchOn,
    kSwitchedOn,
    kOperationEnabled,
    kQuickStopActive,
    kFaultReactionActive,
    kFault,
};
```

要求：

- CiA402 状态不直接塞进主站状态机。
- `SlaveNode` 可持有设备语义对象，但解析逻辑应在 `ServoProfile` 或 `ServoDrive` 中。
- IO、安全、传感器类从站应有各自 Profile。

## 5. `SlaveNodeManager` 职责

`SlaveNodeManager` 是从站集合管理器，负责批量操作和全局判断。

建议职责：

- 从扫描结果构建从站节点。
- 校验拓扑和配置。
- 按角色筛选 required / optional / safety critical 从站。
- 批量请求 PREOP / SAFEOP / OP / INIT。
- 批量执行 `PrepareForRun`。
- 汇总从站健康状态。
- 将关键故障上报主站状态机。
- 提供按 alias、position、joint name、group name 查找从站能力。

建议代码骨架：

```cpp
class SlaveNodeManager {
public:
    bool BuildFromScanResult(const EcScanResult& result,
                             const RobotBusConfig& config);

    bool VerifyTopology() const;
    bool VerifyProfiles() const;

    bool EnterMaintenance(EcMaster& master);
    bool PrepareAllForRun(EcMaster& master);
    bool RequestRequiredSafeOp(EcMaster& master);
    bool RequestRequiredOperational(EcMaster& master);
    bool RequestAllInit(EcMaster& master);

    void RefreshRuntimeHealth(EcMaster& master);

    bool AllRequiredReadyToRun() const;
    bool AllRequiredOperational() const;
    bool AllRequiredHealthy() const;
    bool HasSafetyCriticalFault() const;

    SlaveNode* FindByPosition(uint16_t position);
    SlaveNode* FindByName(std::string_view name);
    std::vector<SlaveNode*> FindByGroup(std::string_view group_name);

private:
    std::vector<SlaveNode> slaves_;
};
```

## 6. 主站状态下的从站行为

### 6.1 `Scanned`

目标：

- 建立从站节点。
- 读取身份信息。
- 完成拓扑初步校验。
- 匹配配置中的从站定义。

典型流程：

```cpp
bool SlaveNodeManager::BuildFromScanResult(const EcScanResult& result,
                                           const RobotBusConfig& config) {
    slaves_.clear();

    for (const auto& scanned : result.slaves) {
        SlaveNode node;
        node.LoadIdentity(scanned.identity);
        node.AssignRole(config.ResolveRole(scanned.identity));
        node.AttachProfile(config.ResolveProfile(scanned.identity));
        slaves_.push_back(std::move(node));
    }

    return VerifyTopology() && VerifyProfiles();
}
```

失败策略：

- 关键从站缺失：主站进入 `Fault`。
- 可选从站缺失：记录 Warning，按配置决定是否继续。
- 安全关键从站缺失：主站进入 `Fault` 或 `EmergencyStop`，取决于安全策略。

### 6.2 `Maintenance`

目标：

- 让必要从站进入 PREOP。
- 建立邮箱通信。
- 执行 SDO 诊断、参数读写、Profile 校验。

典型流程：

```cpp
bool SlaveNode::EnterMaintenance(EcMaster& master) {
    if (!RequestState(master, EtherCatSlaveState::kPreOp)) {
        return MarkConfigError("failed to enter PREOP");
    }

    if (!VerifyIdentity(master)) {
        return MarkConfigError("identity mismatch");
    }

    if (!profile_->VerifyMailbox(master, *this)) {
        return MarkConfigError("mailbox unavailable");
    }

    config_status_ = SlaveConfigStatus::kMailboxReady;
    health_ = SlaveHealth::kHealthy;
    return true;
}
```

允许单站操作：

- SDO 读写。
- 对象字典诊断。
- PDO 映射查看。
- 单站状态切换。
- 单站固件升级准备。

### 6.3 `ReadyToRun`

目标：

- 所有关键从站完成运行前准备。
- PDO、IOmap、DC、初始输出全部就绪。

典型流程：

```cpp
bool SlaveNode::PrepareForRun(EcMaster& master) {
    if (config_status_ < SlaveConfigStatus::kMailboxReady) {
        return MarkConfigError("mailbox not ready");
    }

    if (!profile_->ApplyStartupSdo(master, *this)) {
        return MarkConfigError("startup SDO failed");
    }

    if (!profile_->ConfigurePdoMapping(master, *this)) {
        return MarkConfigError("PDO mapping failed");
    }

    if (!profile_->VerifyPdoMapping(master, *this)) {
        return MarkConfigError("PDO mapping mismatch");
    }

    if (!BindIoMap(master)) {
        return MarkConfigError("IOmap bind failed");
    }

    if (profile_->RequiresDc() && !profile_->ConfigureDc(master, *this)) {
        return MarkConfigError("DC config failed");
    }

    config_status_ = SlaveConfigStatus::kReadyToRun;
    return true;
}
```

主站只判断集合结果：

```cpp
bool MasterContext::PrepareRun() {
    if (!slave_manager_.PrepareAllForRun(master_)) {
        return false;
    }

    if (!slave_manager_.RequestRequiredSafeOp(master_)) {
        return false;
    }

    return slave_manager_.AllRequiredReadyToRun();
}
```

### 6.4 `Operational`

目标：

- 周期刷新输入输出。
- 监控从站健康。
- 将关键故障上报主站。

运行时检查建议分两类：

| 类型 | 周期 | 内容 |
|------|------|------|
| 实时周期检查 | 每个 EtherCAT 周期 | WKC、PDO 输入输出、关键状态字、命令超时 |
| 低频诊断检查 | 10ms~100ms | AL status code、SDO 诊断、温度、电压、厂商状态 |

典型流程：

```cpp
void SlaveNodeManager::RefreshRuntimeHealth(EcMaster& master) {
    for (auto& slave : slaves_) {
        slave.RefreshActualState(master);
        slave.RefreshAlStatus(master);
        slave.RefreshDeviceStatus(master);

        if (slave.HasFatalFault() || slave.IsLost()) {
            ReportSlaveFault(slave);
        }
    }
}
```

要求：

- 运行中不允许随意修改 PDO 映射。
- 运行中 SDO 操作必须通过非实时队列，并受主站状态和实时周期保护。
- 单站故障是否拉停全身由 `SlaveRole`、轴组和安全策略共同决定。

## 7. 从站角色与故障影响

人形机器人中不同从站的重要性不同，必须配置角色。

| 角色 | 示例 | 故障默认影响 |
|------|------|--------------|
| `kSafetyCritical` | 安全 IO、急停模块、STO 状态模块 | 进入 `EmergencyStop` 或 `Fault` |
| `kRequired` | 腿部、腰部、主臂关节伺服 | 进入 `Fault`，通常全身停机 |
| `kOptional` | 手指、非关键末端模块 | 按配置降级或进入相关轴组故障 |
| `kDiagnosticOnly` | 调试 IO、非关键传感器 | 记录 Warning，不影响 OP |

判断示例：

```cpp
bool SlaveNodeManager::AllRequiredHealthy() const {
    for (const auto& slave : slaves_) {
        if (!slave.IsRequiredForRun()) {
            continue;
        }

        if (!slave.IsHealthy()) {
            return false;
        }
    }

    return true;
}
```

## 8. 控制粒度

### 8.1 全局控制

用于主站生命周期迁移：

- `RequestAllPreOp`
- `RequestRequiredSafeOp`
- `RequestRequiredOperational`
- `RequestAllInit`

### 8.2 单站维护

用于维护和调试：

- `RequestSlaveState`
- `ReadSlaveSdo`
- `WriteSlaveSdo`
- `DumpSlaveDiagnosis`
- `ReadPdoMapping`
- `CompareProfile`

单站维护应只允许在 `Maintenance` 或受控诊断状态下执行。运行中如需 SDO，应通过受限队列执行。

### 8.3 轴组控制

用于人形机器人整机操作：

- `EnableGroup`
- `QuickStopGroup`
- `FaultResetGroup`
- `DiagnoseGroup`
- `ReleaseBrakeGroup`
- `DisableGroup`

轴组控制应通过轴组状态机和设备 Profile 下发到相关从站，不应直接绕过从站管理器。

## 9. Profile 适配策略

`DeviceProfile` 是设备差异的主要隔离层。

建议接口：

```cpp
class DeviceProfile {
public:
    virtual ~DeviceProfile() = default;

    virtual bool MatchIdentity(const SlaveIdentity& identity) const = 0;
    virtual bool VerifyMailbox(EcMaster& master, SlaveNode& slave) = 0;
    virtual bool ApplyStartupSdo(EcMaster& master, SlaveNode& slave) = 0;
    virtual bool ConfigurePdoMapping(EcMaster& master, SlaveNode& slave) = 0;
    virtual bool VerifyPdoMapping(EcMaster& master, SlaveNode& slave) = 0;
    virtual bool ConfigureDc(EcMaster& master, SlaveNode& slave) = 0;
    virtual bool RequiresDc() const = 0;
    virtual void DecodeInputs(const IoMapView& io, SlaveNode& slave) = 0;
    virtual void EncodeOutputs(IoMapView& io, SlaveNode& slave) = 0;
    virtual SlaveHealth EvaluateHealth(const SlaveNode& slave) const = 0;
};
```

伺服类 Profile 应进一步支持：

- CiA402 状态字解析。
- 控制字生成。
- CSP/CSV/CST/Homing 模式配置。
- 错误码解析。
- 单位缩放。
- 方向和极性。
- 软限位和跟随误差策略。

## 10. 并发与访问规则

必须遵守：

1. 只有 `EcMaster` 可以直接访问底层 SOEM 上下文。
2. `SlaveNode` 不能绕过 `EcMaster` 操作总线。
3. 周期线程只执行确定性 PDO 路径。
4. SDO、诊断、固件升级必须避开实时路径。
5. 运行中 SDO 必须排队、限频、限时，并可被安全策略拒绝。
6. 主站状态迁移期间禁止并发执行互斥单站维护动作。
7. 故障快照读取应避免阻塞实时停机路径。

## 11. 日志与快照

每个从站至少应支持以下诊断输出：

- 从站编号、alias、物理位置、用户命名。
- Vendor / Product / Revision / Serial。
- EtherCAT actual / expected state。
- AL status code。
- Profile 匹配结果。
- PDO 映射与 IOmap offset。
- DC 配置与偏差。
- WKC 相关统计。
- 设备状态字、错误码、厂商错误信息。
- 最近一次配置失败或通信失败原因。

故障快照应同时包含：

- 单站状态。
- 所属轴组。
- 故障传播结果。
- 主站当时状态。
- 最近 N 个周期的关键 PDO 数据。

## 12. 验收要求

| 验收项 | 要求 |
|--------|------|
| 扫描建模 | 扫描后每个真实从站都有对应 `SlaveNode` |
| 身份校验 | Vendor / Product / Revision / Serial 与配置不匹配时可明确报错 |
| 拓扑校验 | 从站顺序或数量不符合配置时按角色处理 |
| 单站维护 | 在 `Maintenance` 下可对指定从站执行 SDO 诊断 |
| 批量准备 | 所有 required 从站完成 PDO、IOmap、DC 准备后才能进入 `ReadyToRun` |
| 角色策略 | optional 从站故障不应默认拉停全身，safety critical 从站故障必须触发安全策略 |
| 运行监控 | OP 期间可发现掉站、状态偏离、WKC 异常、伺服 Fault |
| Profile 隔离 | 新增设备类型不应修改主站状态机 |
| 快照完整性 | 单站故障能导出身份、状态、AL code、PDO、错误码和轴组上下文 |

## 13. 结语

从站管理的核心是把全局生命周期和单站细节拆开：主站状态机负责门禁和阶段，从站管理器负责集合一致性，每个 `SlaveNode` 负责自身状态，Profile 负责设备语义。这样既能支撑人形机器人几十轴规模，也能在故障和调试时保持清晰边界。
