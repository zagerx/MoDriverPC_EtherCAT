#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "mo_ecat/config.h"
#include "mo_ecat/types.h"

namespace mo_ecat
{

// EtherCAT 主站公共 API。
// 内部实现通过 PImpl 隐藏，不暴露 EcatController / EcMaster / SlaveNode 等内部类。
class MoEcatMaster {
public:
	MoEcatMaster();
	~MoEcatMaster();

	MoEcatMaster(const MoEcatMaster &) = delete;
	MoEcatMaster &operator=(const MoEcatMaster &) = delete;

	// 生命周期方法
	bool InitializeAdapter(const EcMasterConfig &config);
	bool Scan();
	bool EnterMaintenance();
	bool PrepareRun();
	bool StartOperation();
	bool BackToMaintenance();
	void Stop();

	// 故障请求
	void RequestFault(const std::string &reason);
	void RequestEmergencyStop(const std::string &reason);

	// 周期性服务：根据当前状态执行 PDO 收发或从站状态检查。
	// 调用方应每个主循环周期调用一次，非阻塞。
	void Service();

	// 查询
	MasterState GetState() const;
	std::size_t GetSlaveCount() const;
	std::vector<SlaveInfo> GetSlaveInfos();
	MasterSnapshot GetSnapshot();

	// SDO 诊断接口
	bool ReadSdo(int slave_id, uint16_t index, uint8_t subindex,
		     std::vector<uint8_t> &data, int timeout_ms = 1000);
	bool WriteSdo(int slave_id, uint16_t index, uint8_t subindex,
		      const std::vector<uint8_t> &data, int timeout_ms = 1000);

	// 读取并格式化指定从站的 PDO 映射，用于调试显示。
	std::string DumpPdoMapping(int slave_id);

	// 回调接口（由核心库内部线程调用，调用方不应阻塞）。
	// 为避免数据竞争，建议在 InitializeAdapter() 之前设置；若运行中需要替换，
	// 调用方需自行保证与 Service() / 回调触发线程的同步。
	std::function<void(MasterState old_state, MasterState new_state)> on_state_changed;
	std::function<void(const std::string &reason)> on_fault;
	std::function<void(const std::string &level, const std::string &source,
			   const std::string &message)>
		on_log_message;
	std::function<void(const std::vector<SlaveFeedback> &feedback)> on_feedback;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace mo_ecat
