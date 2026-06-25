#include "ecat_application.h"

#include <iostream>
#include <sstream>
#include <vector>

#include "utils/logger.h"

namespace mo_ecat
{

EcatApplication::EcatApplication(std::unique_ptr<CommandReader> command_reader,
				 MoEcatMaster &master)
	: command_reader_(std::move(command_reader)), master_(master)
{
}

EcatApplication::~EcatApplication() = default;

bool EcatApplication::Initialize(const EcMasterConfig &config)
{
	return master_.InitializeAdapter(config);
}

void EcatApplication::Shutdown()
{
	master_.Stop();
}

bool EcatApplication::Run()
{
	std::string command;
	const auto result = command_reader_->Read(command, 0);

	if (result == ReadResult::kEof) {
		LOG_INFO << "EOF detected, requesting shutdown";
		return false;
	}

	const bool has_command = (result == ReadResult::kOk);

	if (has_command && (command == "exit" || command == "quit")) {
		LOG_INFO << "Exit requested";
		return false;
	}

	// 全局命令：loglevel
	if (has_command && command.rfind("loglevel", 0) == 0) {
		std::istringstream iss(command);
		std::string token;
		std::vector<std::string> args;
		while (iss >> token) {
			args.push_back(token);
		}
		OnLogLevel(args);
		return true;
	}

	// 先执行一次主站周期服务
	master_.Service();

	switch (master_.GetState()) {
	case MasterState::kAdapterReady:
		HandleAdapterReadyState(has_command ? &command : nullptr);
		break;
	case MasterState::kScanned:
		HandleScannedState(has_command ? &command : nullptr);
		break;
	case MasterState::kMaintenance:
		HandleMaintenanceState(has_command ? &command : nullptr);
		break;
	case MasterState::kReadyToRun:
		HandleReadyToRunState(has_command ? &command : nullptr);
		break;
	case MasterState::kOperational:
		HandleOperationalState(has_command ? &command : nullptr);
		break;
	case MasterState::kFault:
	case MasterState::kEmergencyStop:
		HandleErrorState(has_command ? &command : nullptr);
		break;
	default:
		break;
	}

	return true;
}

void EcatApplication::HandleAdapterReadyState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "scan") {
		LOG_INFO << "Command: scan";
		if (!master_.Scan()) {
			LOG_ERROR << "Scan failed";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in AdapterReady state";
	}
}

void EcatApplication::HandleScannedState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "config") {
		LOG_INFO << "Command: config";
		if (!master_.EnterMaintenance()) {
			LOG_ERROR << "Failed to enter Maintenance";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in Scanned state";
	}
}

void EcatApplication::HandleMaintenanceState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "prepare") {
		LOG_INFO << "Command: prepare";
		if (!master_.PrepareRun()) {
			LOG_ERROR << "Failed to prepare operation";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Unknown command: " << *command;
	}
}

void EcatApplication::HandleReadyToRunState(const std::string *command)
{
	if (command == nullptr) {
		return;
	}

	if (*command == "start") {
		LOG_INFO << "Command: start";
		if (!master_.StartOperation()) {
			LOG_ERROR << "Failed to start operation";
		}
	} else if (*command == "back") {
		LOG_INFO << "Command: back";
		if (!master_.BackToMaintenance()) {
			LOG_ERROR << "Failed to back to Maintenance";
		}
	} else if (*command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (*command == "help") {
		OnHelp();
	} else {
		LOG_ERROR << "Command '" << *command
			  << "' not allowed in ReadyToRun state";
	}
}

void EcatApplication::HandleOperationalState(const std::string *command)
{
	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	}
}

void EcatApplication::HandleErrorState(const std::string *command)
{
	if (command != nullptr && *command == "stop") {
		LOG_INFO << "Command: stop";
		master_.Stop();
	} else if (command != nullptr) {
		LOG_ERROR << "In Fault/EmergencyStop state, only 'stop' is allowed";
	}
}

void EcatApplication::OnLogLevel(const std::vector<std::string> &args)
{
	if (args.size() != 2) {
		LOG_ERROR << "Usage: loglevel debug|info|warn|error";
		return;
	}

	LogLevel level;
	if (args[1] == "debug") {
		level = LogLevel::Debug;
	} else if (args[1] == "info") {
		level = LogLevel::Info;
	} else if (args[1] == "warn") {
		level = LogLevel::Warn;
	} else if (args[1] == "error") {
		level = LogLevel::Error;
	} else {
		LOG_ERROR << "Unknown log level: " << args[1]
			  << "\nUsage: loglevel debug|info|warn|error";
		return;
	}

	Logger::GetInstance().SetLogLevel(level);
	LOG_INFO << "Log level set to " << args[1];
}

void EcatApplication::OnHelp()
{
	LOG_INFO << "Available commands by state:\n"
		 << "  [AdapterReady] scan\n"
		 << "  [Scanned]      config\n"
		 << "  [Maintenance]  prepare\n"
		 << "  [ReadyToRun]   start / back\n"
		 << "  [Any]          loglevel <level>\n"
		 << "  [Any]          stop\n"
		 << "  [Any]          exit / quit";
}

} // namespace mo_ecat
