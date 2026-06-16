#include "ec_controller/ec_controller.h"

#include "utils/logger.h"

namespace mo_ecat
{

EcatController::EcatController()
{
}

EcatController::~EcatController()
{
	Stop();
}

bool EcatController::Initialize(const EcMasterConfig &config)
{
	if (initialized_) {
		LOG_WARN << "EcatController already initialized";
		return false;
	}

	if (!master_.Initialize(config)) {
		return false;
	}

	if (!master_.ScanAndConfigure()) {
		return false;
	}

	if (!master_.RequestSafeOpState()) {
		LOG_ERROR << "Failed to enter SAFE_OP";
		return false;
	}

	initialized_ = true;
	return true;
}

bool EcatController::StartOperation()
{
	if (!initialized_) {
		LOG_WARN << "EcatController not initialized, call Initialize() first";
		return false;
	}

	if (operational_) {
		LOG_WARN << "EcatController already operational";
		return false;
	}

	if (!master_.RequestOperationalState()) {
		LOG_ERROR << "Failed to enter OPERATIONAL";
		return false;
	}

	operational_ = true;
	LOG_INFO << "EcatController operational";
	return true;
}

void EcatController::Stop()
{
	if (!initialized_) {
		return;
	}

	master_.RequestSafeOpState();
	master_.RequestInitState();
	operational_ = false;
	initialized_ = false;
}

void EcatController::RunOneCycle()
{
	master_.RunOneCycle();
}

void EcatController::CheckSlaveStates()
{
	master_.CheckSlaveStates();
}

bool EcatController::IsInitialized() const
{
	return initialized_;
}

bool EcatController::IsOperational() const
{
	return operational_;
}

} // namespace mo_ecat
