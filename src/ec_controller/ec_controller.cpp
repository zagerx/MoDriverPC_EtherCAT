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

	const int slave_count = master_.GetSlaveCount();
	joint_manager_.Initialize(master_, slave_count);
	LOG_INFO << "Created " << joint_manager_.GetAxisCount() << " servo axis(es)";

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

	joint_manager_.Clear();
	operational_ = false;
	initialized_ = false;
}

void EcatController::RunOneCycle()
{
	joint_manager_.UpdateAllOutputs();
	master_.RunOneCycle();
	joint_manager_.UpdateAllInputs();
}

void EcatController::CheckSlaveStates()
{
	master_.CheckSlaveStates();
}

JointManager &EcatController::GetJointManager()
{
	return joint_manager_;
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
