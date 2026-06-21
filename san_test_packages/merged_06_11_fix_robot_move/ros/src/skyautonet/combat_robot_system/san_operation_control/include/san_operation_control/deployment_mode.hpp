// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 7 - deployment_mode enum + yaml mapping.
//
// Mirrors core/deployment.py on the Python side so cross-language
// boundaries (e.g. Python mission scripts inspecting a ROS2 yaml dump)
// observe the same 5 modes.
//
// Reference: SAN-SDD-SWARM-001 v1.3 §11.

#pragma once

#include <string>

namespace san_operation_control
{

enum class DeploymentMode
{
  DEVELOPMENT,     // dev box, full simulators, watchdog disable allowed
  BENCH,           // unit-bench / CI hardware-in-loop, no kinematics
  LAB_TEST,        // lab dry run, DEMO sequencer enabled, weapons SIMULATED
  DEMO,            // public demo, DEMO sequencer enabled, weapons SIMULATED
  PRODUCTION,      // operational deployment, all safety enforced
};

DeploymentMode fromString(const std::string & s);
std::string    toString(DeploymentMode m);

// Modes that enable the DEMO 6-phase sequencer. PHASE 7 widens this
// from {DEMO} to {DEMO, LAB_TEST} so HIL demos can drive the same path
// in the lab without flipping the deployment to public-DEMO.
bool demoSequencerEnabled(DeploymentMode m);

// Modes that REQUIRE the safety watchdog regardless of yaml override.
// The yaml `hw_watchdog_enabled` flag is honored only in DEVELOPMENT;
// PRODUCTION / DEMO / LAB_TEST / BENCH always force-enable.
bool watchdogIsForceEnabled(DeploymentMode m);

}  // namespace san_operation_control
