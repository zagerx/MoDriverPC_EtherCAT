#include "cyclic/process_data_engine.h"

#include "utils/logger.h"

namespace mo_ecat
{

ProcessDataEngine::ProcessDataEngine(EcatController &controller, EcMaster &master,
				     SlaveNodeManager &node_manager)
	: controller_(controller), master_(master), node_manager_(node_manager)
{
}

void ProcessDataEngine::RunOnce()
{
	if (controller_.GetState() != ControllerState::kOperational) {
		LOG_WARN << "RunOnce ignored: not operational";
		return;
	}

	node_manager_.UpdateAllOutputs();
	master_.RunOneCycle();
	node_manager_.UpdateAllInputs();
}

void ProcessDataEngine::CheckSlaveStates()
{
	master_.CheckSlaveStates();

	switch (controller_.GetState()) {
	case ControllerState::kMaintenance:
		if (!master_.CheckAllSlavesInState(EC_STATE_PRE_OP)) {
			controller_.RequestErrorState("Slave dropped out of PREOP");
		}
		break;
	case ControllerState::kOperational:
		if (!master_.CheckAllSlavesInState(EC_STATE_OPERATIONAL)) {
			controller_.RequestErrorState("Slave dropped out of OPERATIONAL");
		}
		break;
	default:
		break;
	}
}

} // namespace mo_ecat
