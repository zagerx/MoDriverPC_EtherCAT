#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "ecat_application.h"
#include "stdin_command_reader.h"
#include "mo_ecat/master.h"
#include "utils/logger.h"

namespace
{

std::atomic<bool> g_running{true};

void OnSignal(int /*signal*/) { g_running.store(false); }

void RegisterShutdownSignals()
{
	std::signal(SIGINT, OnSignal);
	std::signal(SIGTERM, OnSignal);
}

} // namespace

int main(int argc, char *argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: " << argv[0] << " <interface>\n";
		return 1;
	}

	mo_ecat::EcMasterConfig config;
	config.ifname = argv[1];
	config.cycle_time_us = 1000;
	config.use_dc = true;

	mo_ecat::MoEcatMaster master;
	auto app = std::make_unique<mo_ecat::EcatApplication>(
		std::make_unique<mo_ecat::StdinCommandReader>(), master);

	if (!app->Initialize(config)) {
		LOG_ERROR << "Failed to initialize application";
		return 1;
	}

	RegisterShutdownSignals();

	LOG_INFO << "Application running. Type 'help' for commands, 'exit' to quit.";

	while (g_running.load()) {
		if (!app->Run()) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	LOG_INFO << "Shutting down...";
	app->Shutdown();

	return 0;
}
