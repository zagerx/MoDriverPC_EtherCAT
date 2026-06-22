#include "activity/activity_scheduler.h"

#include "ec_controller/ec_controller.h"
#include "utils/logger.h"

namespace mo_ecat
{

ActivityScheduler::ActivityScheduler(EcatController &controller) : controller_(controller)
{
}

bool ActivityScheduler::Execute(std::unique_ptr<EcatActivity> activity)
{
	if (!activity) {
		LOG_ERROR << "Null activity";
		return false;
	}

	if (running_.load()) {
		LOG_ERROR << "Activity already running";
		return false;
	}

	const auto state = controller_.GetState();
	if (!activity->CanStart(state)) {
		LOG_ERROR << "Activity " << activity->GetName()
			  << " cannot start in state " << EcatController::StateToString(state);
		return false;
	}

	running_.store(true);
	LOG_INFO << "Activity started: " << activity->GetName();

	const bool ok = activity->Execute();

	LOG_INFO << "Activity finished: " << activity->GetName()
		 << ", result=" << (ok ? "ok" : "failed");
	running_.store(false);

	if (!ok) {
		switch (activity->GetFailurePolicy()) {
		case ActivityFailurePolicy::kEnterError:
			controller_.RequestErrorState(std::string("Activity failed: ") +
						      activity->GetName());
			break;
		case ActivityFailurePolicy::kShutdown:
			controller_.Stop();
			break;
		case ActivityFailurePolicy::kKeepControllerState:
		default:
			LOG_WARN << "Activity failed but keeping controller state";
			break;
		}
	}

	return ok;
}

bool ActivityScheduler::IsRunning() const
{
	return running_.load();
}

} // namespace mo_ecat
