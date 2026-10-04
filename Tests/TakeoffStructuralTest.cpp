/****************************************************************************
 *
 * Student-authored structural tests for TakeoffHandling.
 *
 * These tests target production branches and state transitions not relied
 * upon from the existing upstream TakeoffTest.cpp suite.
 *
 ****************************************************************************/

#include <gtest/gtest.h>

#include <Takeoff.hpp>
#include <lib/geo/geo.h>

using namespace time_literals;

class TakeoffStructuralTest : public ::testing::Test
{
protected:
	TakeoffHandling takeoff;

	void enterFlightUsingSkipTakeoff()
	{
		takeoff.setSpoolupTime(0_s);

		// Arm the vehicle and allow the state machine to leave DISARMED.
		takeoff.updateTakeoffState(true, false, false, 0.f, false, 0_us);

		// Exercise the skip-takeoff override.
		takeoff.updateTakeoffState(true, false, false, 0.f, true, 1_us);

		ASSERT_EQ(takeoff.getTakeoffState(), TakeoffState::flight);
	}
};

TEST_F(TakeoffStructuralTest, SkipTakeoffOverrideAndDisarmReset)
{
	takeoff.setSpoolupTime(0_s);

	// skip_takeoff must not force flight while the vehicle is disarmed.
	takeoff.updateTakeoffState(false, false, false, 0.f, true, 0_us);
	EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::disarmed);

	// Once armed, skip_takeoff forces the state machine directly to flight.
	takeoff.updateTakeoffState(true, false, false, 0.f, true, 1_us);
	EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::flight);

	// Disarming has priority and resets the state machine.
	takeoff.updateTakeoffState(false, false, false, 0.f, true, 2_us);
	EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::disarmed);
}

TEST_F(TakeoffStructuralTest, LandingTransitionsFlightBackToReady)
{
	enterFlightUsingSkipTakeoff();

	// Remaining airborne must preserve FLIGHT.
	takeoff.updateTakeoffState(true, false, false, 0.f, false, 2_us);
	EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::flight);

	// Landing while armed transitions FLIGHT -> READY.
	takeoff.updateTakeoffState(true, true, false, 0.f, false, 3_us);
	EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::ready_for_takeoff);

	// A new takeoff request can start another ramp.
	takeoff.updateTakeoffState(true, false, true, 0.f, false, 4_us);
	EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::rampup);
}

TEST(TakeoffStructuralBoundaryTest, RampTimeBoundary)
{
	constexpr float ramp_time = 2.f;
	constexpr float desired_vz = 2.f;

	// Case 1: dt < ramp_time exercises incremental ramp progress.
	TakeoffHandling incremental_takeoff;
	incremental_takeoff.setSpoolupTime(0.f);
	incremental_takeoff.setTakeoffRampTime(ramp_time);

	incremental_takeoff.updateTakeoffState(
		true, false, true, desired_vz, false, 0_us);

	ASSERT_EQ(incremental_takeoff.getTakeoffState(), TakeoffState::rampup);

	const float incremental_output =
		incremental_takeoff.updateRamp(1.f, desired_vz);

	EXPECT_NEAR(incremental_output, 1.f, 1e-5f);

	// Case 2: dt == ramp_time exercises the equality/else boundary.
	TakeoffHandling boundary_takeoff;
	boundary_takeoff.setSpoolupTime(0.f);
	boundary_takeoff.setTakeoffRampTime(ramp_time);

	boundary_takeoff.updateTakeoffState(
		true, false, true, desired_vz, false, 0_us);

	ASSERT_EQ(boundary_takeoff.getTakeoffState(), TakeoffState::rampup);

	const float boundary_output =
		boundary_takeoff.updateRamp(ramp_time, desired_vz);

	EXPECT_NEAR(boundary_output, desired_vz, 1e-5f);
}

TEST(TakeoffStructuralBoundaryTest, InitialRampGainLowerBound)
{
	constexpr float below_minimum_gain = 0.005f;
	constexpr float normal_gain = 0.5f;
	constexpr float desired_vz = 2.f;

	// A gain below 0.01 must be clamped to 0.01.
	TakeoffHandling clamped_takeoff;
	clamped_takeoff.generateInitialRampValue(below_minimum_gain);

	const float clamped_output =
		clamped_takeoff.updateRamp(0.f, desired_vz);

	EXPECT_NEAR(
		clamped_output,
		-CONSTANTS_ONE_G / 0.01f,
		1e-3f);

	// A gain above the lower bound must be used unchanged.
	TakeoffHandling normal_takeoff;
	normal_takeoff.generateInitialRampValue(normal_gain);

	const float normal_output =
		normal_takeoff.updateRamp(0.f, desired_vz);

	EXPECT_NEAR(
		normal_output,
		-CONSTANTS_ONE_G / normal_gain,
		1e-3f);
}

TEST(TakeoffStructuralLifecycleTest, SpoolupWaitAndNormalRampCompletion)
{
        TakeoffHandling takeoff;

        constexpr float spoolup_time = 1.f;
        constexpr float ramp_time = 2.f;
        constexpr float desired_vz = 2.f;

        takeoff.setSpoolupTime(spoolup_time);
        takeoff.setTakeoffRampTime(ramp_time);

        // Arm at t = 0. Spool-up time has not elapsed.
        takeoff.updateTakeoffState(
                true, false, false, desired_vz, false, 0_us);

        EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::spoolup);

        // Still before the one-second spool-up threshold.
        takeoff.updateTakeoffState(
                true, false, false, desired_vz, false, 500_ms);

        EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::spoolup);

        // At the threshold, spoolup transitions to ready.
        takeoff.updateTakeoffState(
                true, false, false, desired_vz, false, 1_s);

        EXPECT_EQ(
                takeoff.getTakeoffState(),
                TakeoffState::ready_for_takeoff);

        // Request takeoff: ready -> rampup.
        takeoff.updateTakeoffState(
                true, false, true, desired_vz, false, 1_s);

        ASSERT_EQ(takeoff.getTakeoffState(), TakeoffState::rampup);

        // Complete the configured ramp.
        const float ramp_output =
                takeoff.updateRamp(ramp_time, desired_vz);

        EXPECT_NEAR(ramp_output, desired_vz, 1e-5f);

        // With ramp progress complete, rampup -> flight.
        takeoff.updateTakeoffState(
                true, false, true, desired_vz, false, 1_s);

        EXPECT_EQ(takeoff.getTakeoffState(), TakeoffState::flight);
}
