#pragma once
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "mo_ecat/config.h"
#include "mo_ecat/types.h"
#include "soem/soem.h"

namespace mo_ecat
{

struct CyclicStats {
	uint32_t cycle_count = 0;
	uint32_t wkc_mismatch_count = 0;
	uint32_t consecutive_wkc_mismatch = 0; // 连续 WKC 不匹配次数
	uint32_t min_cycle_us = 0;             // 最小周期时间（微秒）
	uint32_t max_cycle_us = 0;             // 最大周期时间（微秒）
	uint32_t avg_cycle_us = 0;             // 平均周期时间（微秒）
	int64_t last_dc_time = 0;
};


class EcMaster
{
      public:
	EcMaster();
	~EcMaster();

	bool Initialize(const EcMasterConfig &config);
	void Close();

	// 扫描从站，填充 ctx_.slavelist，返回所有从站的静态信息
	std::vector<SlaveInfo> ScanSlaves();

	// 配置 PDO 映射和 IOmap
	bool ConfigureProcessData();

	// 配置 Distributed Clock
	bool ConfigureDc();

	int GetSlaveCount() const;
	SlaveInfo GetSlaveInfo(int slave_id) const;

	// 全局状态切换（广播到所有从站）
	bool RequestOperationalState();
	bool RequestSafeOpState();
	bool RequestInitState();
	bool RequestPreOpState();
	bool RequestStateWithRetry(int slave, uint16_t state, int max_retries = 3);
	bool CheckAllSlavesInState(uint16_t state);
	uint16_t ReadActualState(int slave);

	// 单站状态切换
	bool RequestState(int slave, uint16_t state);
	bool RequestBootstrapState(int slave);
	uint16_t GetCurrentState(int slave) const;

	// 实时读取单个从站的 AL status code。
	uint16_t ReadAlStatusCode(int slave);

	// 单步运行：执行一次 PDO 收发
	void RunOneCycle();

	// 单步运行：检查一次从站状态
	void CheckSlaveStates();

	// PDO 读写（按字节偏移）
	void WriteOutput(int slave, int offset, const uint8_t *data, int len);
	void ReadInput(int slave, int offset, uint8_t *data, int len);

	// SDO 通信（基于 SOEM ecx_SDOread / ecx_SDOwrite）
	bool SdoRead(uint16_t slave, uint16_t index, uint8_t subindex, void *data, int len,
		     int timeout_us);
	bool SdoWrite(uint16_t slave, uint16_t index, uint8_t subindex, const void *data, int len,
		      int timeout_us);

	const CyclicStats &GetStats() const;

      private:
	ecx_contextt ctx_{};
	uint8_t iomap_[4096] = {0};

	EcMasterConfig config_;

	int expected_wkc_ = 0;
	CyclicStats stats_;

	// 保护 SOEM 上下文，防止多线程同时访问
	mutable std::mutex soem_mutex_;

	// true 表示 SOEM 上下文未打开；Initialize() 成功后置为 false，Close() 后恢复为 true。
	// 用于防止对象构造后、Initialize() 成功前调用 Close() 导致未定义行为。
	bool closed_ = true;

	std::chrono::steady_clock::time_point last_cycle_time_;
};

} // namespace mo_ecat
