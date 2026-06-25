#pragma once

#include <memory>
#include <string>
#include <vector>

#include "command_reader.h"
#include "mo_ecat/master.h"

namespace mo_ecat
{

// 命令行应用层封装。
// 只使用 MoEcatMaster 公共 API，不依赖核心内部类。
class EcatApplication {
public:
	explicit EcatApplication(std::unique_ptr<CommandReader> command_reader,
				 MoEcatMaster &master);
	~EcatApplication();

	bool Initialize(const EcMasterConfig &config);
	void Shutdown();

	// 单步执行一次：服务主站 + 读取并执行命令。
	// 返回 false 表示收到退出请求。
	bool Run();

private:
	void HandleAdapterReadyState(const std::string *command);
	void HandleScannedState(const std::string *command);
	void HandleMaintenanceState(const std::string *command);
	void HandleReadyToRunState(const std::string *command);
	void HandleOperationalState(const std::string *command);
	void HandleErrorState(const std::string *command);

	void OnDiagnose();
	void OnParam(const std::vector<std::string> &args);
	void OnInspect();
	void OnPdo(const std::vector<std::string> &args);
	void OnLogLevel(const std::vector<std::string> &args);
	void OnHelp();

	std::unique_ptr<CommandReader> command_reader_;
	MoEcatMaster &master_;
};

} // namespace mo_ecat
