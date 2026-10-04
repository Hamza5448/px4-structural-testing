/*
 * Student-authored structural tests of the real production Failsafe.
 * Q = (A || B) && C && M; pairs: A 01/02, B 02/03, C 03/04, M 03/05.
 */
#include <gtest/gtest.h>
#include "failsafe.h"
#include <parameters/param.h>
#include <uORB/topics/vehicle_status.h>

using namespace time_literals;

// PX4's parameter owner propagates refreshes to its registered children.
class StructuralParameterOwner : public ModuleParams
{
public:
	StructuralParameterOwner() : ModuleParams(nullptr) {}
	void refresh() { updateParams(); }
};

class FailsafeStructuralTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		param_control_autosave(false);
		for (unsigned i = 0; i < 3; ++i) {
			_handles[i] = param_find(_names[i]);
			ASSERT_NE(_handles[i], PARAM_INVALID);
			ASSERT_EQ(param_get(_handles[i], &_saved[i]), 0);
		}
	}

	void TearDown() override
	{
		for (unsigned i = 0; i < 3; ++i) {
			if (_handles[i] != PARAM_INVALID) {
				EXPECT_EQ(param_set(_handles[i], &_saved[i]), 0);
			}
		}
	}

	void runRow(int32_t rc_mode, int32_t rc_except, int32_t dll_action,
		    bool mission_finished, FailsafeBase::Action expected)
	{
		StructuralParameterOwner owner;
		Failsafe failsafe(&owner);
		const int32_t values[] = {rc_mode, rc_except, dll_action};

		for (unsigned i = 0; i < 3; ++i) {
			ASSERT_EQ(param_set(_handles[i], &values[i]), 0);
		}

		// Invokes the real child's virtual updateParams() through the supported owner.
		owner.refresh();

		FailsafeBase::State state{};
		state.armed = true;
		state.user_intended_mode = vehicle_status_s::NAVIGATION_STATE_AUTO_MISSION;
		state.vehicle_type = vehicle_status_s::VEHICLE_TYPE_ROTARY_WING;
		state.mission_finished = mission_finished;

		failsafe_flags_s flags{};
		flags.gcs_connection_lost = true;

		// Preserve the default action delay; observe its value instead of overriding it.
		float delay_s = 0.f;
		const param_t delay_handle = param_find("COM_FAIL_ACT_T");
		ASSERT_NE(delay_handle, PARAM_INVALID);
		ASSERT_EQ(param_get(delay_handle, &delay_s), 0);
		ASSERT_GE(delay_s, 0.f);

		failsafe.update(1_s, state, false, false, flags);
		const auto initial = failsafe.selectedAction();
		const hrt_abstime after_delay = 1_s + static_cast<hrt_abstime>((delay_s + 1.f) * 1_s);
		failsafe.update(after_delay, state, false, false, flags);
		failsafe.update(after_delay + 1_ms, state, false, false, flags);

		RecordProperty("initial_action", FailsafeBase::actionStr(initial));
		RecordProperty("settled_action", FailsafeBase::actionStr(failsafe.selectedAction()));
		printf("MC/DC runtime: initial=%s settled=%s\n",
		       FailsafeBase::actionStr(initial), FailsafeBase::actionStr(failsafe.selectedAction()));
		EXPECT_EQ(failsafe.selectedAction(), expected);
	}

private:
	const char *_names[3] {"COM_RC_IN_MODE", "COM_RCL_EXCEPT", "NAV_DLL_ACT"};
	param_t _handles[3] {PARAM_INVALID, PARAM_INVALID, PARAM_INVALID};
	int32_t _saved[3] {};
};

TEST_F(FailsafeStructuralTest, FS_MCDC_01_DisabledManualControl)
{
	// A=T B=F C=T M=T -> Q=T.
	runRow(4, 0, 0, true, FailsafeBase::Action::RTL);
}

TEST_F(FailsafeStructuralTest, FS_MCDC_02_NoManualControlException)
{
	// A=F B=F C=T M=T -> Q=F.
	runRow(0, 0, 0, true, FailsafeBase::Action::None);
}

TEST_F(FailsafeStructuralTest, FS_MCDC_03_MissionManualControlException)
{
	// A=F B=T C=T M=T -> Q=T.
	runRow(0, 1, 0, true, FailsafeBase::Action::RTL);
}

TEST_F(FailsafeStructuralTest, FS_MCDC_04_EnabledDatalinkHold)
{
	// A=F B=T C=F M=T -> Q=F; ordinary GCS-loss handling requests Hold.
	// Hold distinguishes this row from the mission-control-loss RTL in row 03.
	runRow(0, 1, 1, true, FailsafeBase::Action::Hold);
}

TEST_F(FailsafeStructuralTest, FS_MCDC_05_UnfinishedMission)
{
	// A=F B=T C=T M=F -> Q=F.
	runRow(0, 1, 0, false, FailsafeBase::Action::None);
}

#include <uORB/topics/battery_status.h>
#include <lib/circuit_breaker/circuit_breaker.h>
#include <vector>

class FailsafeSafetyScenarioTest : public ::testing::Test
{
protected:
	using Action = FailsafeBase::Action;
	using State = FailsafeBase::State;
	struct SavedParameter { param_t handle; int32_t value; };
	std::vector<SavedParameter> saved;

	void SetUp() override { param_control_autosave(false); }
	void TearDown() override
	{
		for (auto it = saved.rbegin(); it != saved.rend(); ++it) {
			EXPECT_EQ(param_set(it->handle, &it->value), 0);
		}
	}
	void set(const char *name, int32_t value)
	{
		const param_t handle = param_find(name);
		ASSERT_NE(handle, PARAM_INVALID);
		int32_t previous{};
		ASSERT_EQ(param_get(handle, &previous), 0);
		saved.push_back({handle, previous});
		ASSERT_EQ(param_set(handle, &value), 0);
	}
	float getFloat(const char *name)
	{
		float value{};
		const param_t handle = param_find(name);
		EXPECT_NE(handle, PARAM_INVALID);
		EXPECT_EQ(param_get(handle, &value), 0);
		return value;
	}
	State state(uint8_t mode = vehicle_status_s::NAVIGATION_STATE_AUTO_MISSION)
	{
		State result{};
		result.armed = true;
		result.vehicle_type = vehicle_status_s::VEHICLE_TYPE_ROTARY_WING;
		result.user_intended_mode = mode;
		return result;
	}
	void expect(Failsafe &failsafe, const State &state, const failsafe_flags_s &flags,
		    hrt_abstime time, Action expected)
	{
		failsafe.update(time, state, false, false, flags);
		const hrt_abstime settled = time + static_cast<hrt_abstime>((getFloat("COM_FAIL_ACT_T") + 1.f) * 1_s);
		failsafe.update(settled, state, false, false, flags);
		failsafe.update(settled + 1_ms, state, false, false, flags);
		EXPECT_EQ(failsafe.selectedAction(), expected);
		EXPECT_EQ(failsafe.inFailsafe(), expected > Action::Warn);
	}
	void scenario(uint8_t mode, const failsafe_flags_s &flags, Action expected)
	{
		StructuralParameterOwner owner;
		Failsafe failsafe(&owner);
		owner.refresh();
		const State vehicle = state(mode);
		failsafe_flags_s healthy{};
		failsafe.update(1_s, vehicle, false, false, healthy);
		expect(failsafe, vehicle, flags, 20_s, expected);
	}
};

TEST_F(FailsafeSafetyScenarioTest, ManualAndGcsLossPolicies)
{
	set("COM_RC_IN_MODE", 0);
	set("COM_RCL_EXCEPT", 0);
	set("COM_DLL_EXCEPT", 0);
	set("NAV_RCL_ACT", 3); // Land, deliberately distinct from GCS RTL.
	set("NAV_DLL_ACT", 2);
	failsafe_flags_s flags{};
	flags.manual_control_signal_lost = true;
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_MISSION, flags, Action::Land);
	flags = {};
	flags.gcs_connection_lost = true;
	// Independent ordinary GCS-loss Return policy (MC/DC row 04 uses Hold).
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_MISSION, flags, Action::RTL);
}

TEST_F(FailsafeSafetyScenarioTest, QuadchuteAndModeDependentNavigation)
{
	set("COM_QC_ACT", 0); // Quadchute Return.
	set("COM_POS_LOW_ACT", 2); // Low position accuracy Hold.
	failsafe_flags_s flags{};
	flags.vtol_fixed_wing_system_failure = true;
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_LOITER, flags, Action::RTL);
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_TAKEOFF, flags, Action::RTL);
	flags = {};
	flags.position_accuracy_low = true;
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_LOITER, flags, Action::Hold);
	flags = {};
	flags.navigator_failure = true;
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_RTL, flags, Action::Land);
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_LOITER, flags, Action::Hold);
}

TEST_F(FailsafeSafetyScenarioTest, BatteryEscalationAndArmingBaseline)
{
	set("COM_LOW_BAT_ACT", 3); // Return at critical, Land at emergency.
	StructuralParameterOwner owner;
	Failsafe failsafe(&owner);
	owner.refresh();
	const State vehicle = state();
	failsafe_flags_s flags{};
	failsafe.update(1_s, vehicle, false, false, flags); // Healthy arming baseline.
	flags.battery_warning = battery_status_s::WARNING_LOW;
	expect(failsafe, vehicle, flags, 20_s, Action::Warn);
	flags.battery_warning = battery_status_s::WARNING_CRITICAL;
	expect(failsafe, vehicle, flags, 40_s, Action::RTL);
	flags.battery_warning = battery_status_s::WARNING_EMERGENCY;
	expect(failsafe, vehicle, flags, 60_s, Action::Land);

	Failsafe low_at_arming(&owner);
	owner.refresh();
	flags.battery_warning = battery_status_s::WARNING_CRITICAL;
	expect(low_at_arming, vehicle, flags, 1_s, Action::Warn);
}

TEST_F(FailsafeSafetyScenarioTest, CriticalFailureTimingAndCircuitBreaker)
{
	set("CBRK_FLIGHTTERM", 0);
	const hrt_abstime post_takeoff = 1_s + static_cast<hrt_abstime>(
		(getFloat("COM_SPOOLUP_TIME") + getFloat("COM_LKDOWN_TKO") + 1.f) * 1_s);
	const State vehicle = state();
	failsafe_flags_s flags{};
	StructuralParameterOwner owner;
	Failsafe airborne(&owner);
	owner.refresh();
	airborne.update(1_s, vehicle, false, false, flags);
	flags.fd_critical_failure = true;
	expect(airborne, vehicle, flags, post_takeoff, Action::Terminate);
	// Termination remains latched after the failure clears.
	flags = {};
	expect(airborne, vehicle, flags, post_takeoff + 20_s, Action::Terminate);

	Failsafe early_failure(&owner);
	owner.refresh();
	early_failure.update(1_s, vehicle, false, false, flags);
	flags.fd_critical_failure = true;
	early_failure.update(1_s + 1_ms, vehicle, false, false, flags);
	EXPECT_EQ(early_failure.selectedAction(), Action::Disarm);

	set("CBRK_FLIGHTTERM", CBRK_FLIGHTTERM_KEY);
	Failsafe inhibited(&owner);
	owner.refresh();
	flags = {};
	inhibited.update(1_s, vehicle, false, false, flags);
	flags.fd_critical_failure = true;
	expect(inhibited, vehicle, flags, post_takeoff, Action::Warn);
}

TEST_F(FailsafeSafetyScenarioTest, SpoolupHealthAndOptionalManualControl)
{
	StructuralParameterOwner lifecycle_owner;
	Failsafe lifecycle(&lifecycle_owner);
	lifecycle_owner.refresh();
	State lifecycle_state = state();
	lifecycle_state.armed = false;
	failsafe_flags_s healthy{};
	expect(lifecycle, lifecycle_state, healthy, 1_s, Action::None);
	lifecycle_state.armed = true;
	expect(lifecycle, lifecycle_state, healthy, 20_s, Action::None);
	lifecycle_state.armed = false;
	expect(lifecycle, lifecycle_state, healthy, 40_s, Action::None);

	const State vehicle = state();
	for (bool esc_failure : {false, true}) {
		SCOPED_TRACE(esc_failure ? "ESC arming failure" : "Unhealthy battery");
		StructuralParameterOwner owner;
		Failsafe failsafe(&owner);
		owner.refresh();
		failsafe_flags_s flags{};
		failsafe.update(1_s, vehicle, false, false, flags);
		flags.fd_esc_arming_failure = esc_failure;
		flags.battery_unhealthy = !esc_failure;
		failsafe.update(1_s + 1_ms, vehicle, false, false, flags);
		EXPECT_EQ(failsafe.selectedAction(), Action::Disarm);
	}
	failsafe_flags_s unhealthy{};
	unhealthy.battery_unhealthy = true;
	scenario(vehicle.user_intended_mode, unhealthy, Action::Warn);

	set("COM_RC_IN_MODE", 0);
	set("COM_RCL_EXCEPT", 0);
	set("NAV_RCL_ACT", 2);
	StructuralParameterOwner owner;
	Failsafe optional_rc(&owner);
	owner.refresh();
	failsafe_flags_s flags{};
	flags.manual_control_signal_lost = true;
	expect(optional_rc, vehicle, flags, 1_s, Action::None); // Lost at arming is optional.
	flags.manual_control_signal_lost = false;
	expect(optional_rc, vehicle, flags, 20_s, Action::None);
	flags.manual_control_signal_lost = true;
	expect(optional_rc, vehicle, flags, 40_s, Action::RTL); // Regained then lost is actionable.
}

TEST_F(FailsafeSafetyScenarioTest, IndependentFlightSafetyPolicies)
{
	set("COM_WIND_MAX_ACT", 3);
	set("GF_ACTION", 3);
	set("COM_FLTT_LOW_ACT", 3);
	set("COM_ACT_FAIL_ACT", 2);
	set("COM_IMB_PROP_ACT", 0);
	struct Policy { const char *name; bool failsafe_flags_s::*flag; Action action; };
	const Policy policies[] = {
		{"wind limit", &failsafe_flags_s::wind_limit_exceeded, Action::RTL},
		{"flight time limit", &failsafe_flags_s::flight_time_limit_exceeded, Action::RTL},
		{"geofence", &failsafe_flags_s::geofence_breached, Action::RTL},
		{"remaining battery time", &failsafe_flags_s::battery_low_remaining_time, Action::RTL},
		{"motor failure", &failsafe_flags_s::fd_motor_failure, Action::Land},
		{"imbalanced propeller", &failsafe_flags_s::fd_imbalanced_prop, Action::Warn},
		{"mission failure", &failsafe_flags_s::mission_failure, Action::RTL}
	};
	for (const auto &policy : policies) {
		SCOPED_TRACE(policy.name);
		failsafe_flags_s flags{};
		flags.*(policy.flag) = true;
		scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_MISSION, flags, policy.action);
	}
}

TEST_F(FailsafeSafetyScenarioTest, LinkLossExceptionsAndVtolTransition)
{
	set("COM_RC_IN_MODE", 0);
	set("COM_RCL_EXCEPT", 31); // Mission, Hold, Offboard, External, AltitudeCruise.
	set("COM_DLL_EXCEPT", 7); // Mission, Hold, Offboard.
	set("NAV_RCL_ACT", 2);
	set("NAV_DLL_ACT", 2);
	const uint8_t modes[] = {
		vehicle_status_s::NAVIGATION_STATE_AUTO_MISSION,
		vehicle_status_s::NAVIGATION_STATE_AUTO_LOITER,
		vehicle_status_s::NAVIGATION_STATE_OFFBOARD,
		vehicle_status_s::NAVIGATION_STATE_AUTO_TAKEOFF,
		vehicle_status_s::NAVIGATION_STATE_AUTO_VTOL_TAKEOFF
	};
	for (uint8_t mode : modes) {
		SCOPED_TRACE(static_cast<int>(mode));
		failsafe_flags_s flags{};
		flags.manual_control_signal_lost = true;
		flags.gcs_connection_lost = true;
		scenario(mode, flags, Action::None);
	}
	failsafe_flags_s rc_loss{};
	rc_loss.manual_control_signal_lost = true;
	scenario(vehicle_status_s::NAVIGATION_STATE_EXTERNAL1, rc_loss, Action::None);
	scenario(vehicle_status_s::NAVIGATION_STATE_ALTITUDE_CRUISE, rc_loss, Action::None);
	failsafe_flags_s gcs_loss{};
	gcs_loss.gcs_connection_lost = true;
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_LAND, gcs_loss, Action::None);
	scenario(vehicle_status_s::NAVIGATION_STATE_AUTO_PRECLAND, gcs_loss, Action::None);

	set("COM_RCL_EXCEPT", 0);
	set("COM_DLL_EXCEPT", 0);
	// Disabled exceptions must restore ordinary actionable link-loss policies.
	for (uint8_t mode : {vehicle_status_s::NAVIGATION_STATE_OFFBOARD,
			     vehicle_status_s::NAVIGATION_STATE_EXTERNAL1,
			     vehicle_status_s::NAVIGATION_STATE_ALTITUDE_CRUISE}) {
		SCOPED_TRACE(static_cast<int>(mode));
		scenario(mode, rc_loss, Action::RTL);
	}
	scenario(vehicle_status_s::NAVIGATION_STATE_OFFBOARD, gcs_loss, Action::RTL);

	StructuralParameterOwner fixedwing_owner;
	Failsafe fixedwing_takeoff(&fixedwing_owner);
	fixedwing_owner.refresh();
	State fixedwing = state(vehicle_status_s::NAVIGATION_STATE_AUTO_VTOL_TAKEOFF);
	fixedwing.vehicle_type = vehicle_status_s::VEHICLE_TYPE_FIXED_WING;
	failsafe_flags_s fixedwing_flags{};
	fixedwing_takeoff.update(1_s, fixedwing, false, false, fixedwing_flags);
	fixedwing_flags.gcs_connection_lost = true;
	expect(fixedwing_takeoff, fixedwing, fixedwing_flags, 20_s, Action::None);
	fixedwing.mission_finished = true;
	expect(fixedwing_takeoff, fixedwing, fixedwing_flags, 40_s, Action::RTL);

	StructuralParameterOwner owner;
	Failsafe transition(&owner);
	owner.refresh();
	State vehicle = state(vehicle_status_s::NAVIGATION_STATE_AUTO_VTOL_TAKEOFF);
	vehicle.vtol_in_transition_mode = true;
	failsafe_flags_s flags{};
	transition.update(1_s, vehicle, false, false, flags);
	flags.manual_control_signal_lost = true;
	flags.gcs_connection_lost = true;
	expect(transition, vehicle, flags, 20_s, Action::None);
	vehicle.mission_finished = true; // Established loiter ends link-loss suppression.
	expect(transition, vehicle, flags, 40_s, Action::RTL);
}


TEST_F(FailsafeSafetyScenarioTest, ExternalModesManualControlException)
{
	set("COM_RC_IN_MODE", 0);
	set("NAV_RCL_ACT", 2); // Production manual-control-loss Return response.
	const uint8_t modes[] = {
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL1,
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL2,
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL3,
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL4,
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL5,
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL6,
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL7,
		vehicle_status_s::NAVIGATION_STATE_EXTERNAL8
	};
	failsafe_flags_s flags{};
	flags.manual_control_signal_lost = true;
	for (uint8_t mode : modes) {
		SCOPED_TRACE(static_cast<int>(mode));
		set("COM_RCL_EXCEPT", 1 << 3); // Production ExternalMode exception bit.
		scenario(mode, flags, Action::None);
		set("COM_RCL_EXCEPT", 0);
		scenario(mode, flags, Action::RTL);
	}
}
