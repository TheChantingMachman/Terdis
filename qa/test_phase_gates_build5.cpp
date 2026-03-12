// Build 5 (updated Build 17) — test_phase_gates_build5.cpp
// Tests for data-driven PhaseGate / GateCondition refactor.
// All values derived from spec/core-spec.md and the build-5/build-17 work orders.
//
// Scope:
//   1. game_init() populates state.phase_gates with the Phase 1→2 gate
//   2. check_phase_gates() is data-driven (reads from state.phase_gates)
//   3. save_game / load_game round-trip preserves phase_gates
//
// Phase 1→2 gate per spec (Build 17):
//   PhaseGate{ phase=2, conditions={ {power, RATE, 0.0}, {colony_capacity, VALUE, 10.0} } }
//
// NOTE: Behavioural tests for check_phase_gates (does/does-not advance phase) already
// exist in test_gates_and_milestones.cpp — this file tests the data layer and
// data-driven dispatch, not the outcomes again.

#include <catch2/catch_all.hpp>
#include <algorithm>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Helper: find a GateCondition by resource_id within a PhaseGate.
// Returns nullptr if not found.
// ---------------------------------------------------------------------------
static const GateCondition* find_condition(const PhaseGate& gate, const std::string& rid) {
    for (const auto& c : gate.conditions) {
        if (c.resource_id == rid) return &c;
    }
    return nullptr;
}

// ===========================================================================
// Section 1 — game_init populates phase_gates
// ===========================================================================

TEST_CASE("game_init: phase_gates vector has exactly 2 gates", "[phase_gates][init]") {
    GameState s = game_init();
    REQUIRE(s.phase_gates.size() == 2);
}

TEST_CASE("game_init: phase_gate[0] targets phase 2", "[phase_gates][init]") {
    GameState s = game_init();
    REQUIRE(s.phase_gates[0].phase == 2);
}

TEST_CASE("game_init: phase_gate[0] has exactly 2 conditions", "[phase_gates][init]") {
    GameState s = game_init();
    REQUIRE(s.phase_gates[0].conditions.size() == 2);
}

TEST_CASE("game_init: phase_gate[0] contains power condition with min_value 0.0 and RATE field", "[phase_gates][init]") {
    GameState s = game_init();
    const GateCondition* c = find_condition(s.phase_gates[0], "power");
    REQUIRE(c != nullptr);
    REQUIRE(c->min_value == Catch::Approx(0.0));
    REQUIRE(c->condition_field == ConditionField::RATE);
}

TEST_CASE("game_init: phase_gate[0] power condition has condition_field RATE", "[phase_gates][init]") {
    GameState s = game_init();
    const GateCondition* c = find_condition(s.phase_gates[0], "power");
    REQUIRE(c != nullptr);
    REQUIRE(c->condition_field == ConditionField::RATE);
}

TEST_CASE("game_init: phase_gate[0] contains colony_capacity condition with VALUE field, min_value 2.0", "[phase_gates][init]") {
    GameState s = game_init();
    const GateCondition* c = find_condition(s.phase_gates[0], "colony_capacity");
    REQUIRE(c != nullptr);
    REQUIRE(c->min_value == Catch::Approx(2.0));
    REQUIRE(c->condition_field == ConditionField::VALUE);
}

TEST_CASE("game_init: all condition resource_ids in phase_gate[0] are distinct", "[phase_gates][init]") {
    GameState s = game_init();
    const auto& conds = s.phase_gates[0].conditions;
    REQUIRE(conds.size() == 2);
    std::vector<std::string> ids;
    for (const auto& c : conds) ids.push_back(c.resource_id);
    std::sort(ids.begin(), ids.end());
    auto it = std::unique(ids.begin(), ids.end());
    REQUIRE(it == ids.end()); // no duplicates
}

// ===========================================================================
// Section 2 — check_phase_gates is data-driven (reads state.phase_gates)
// ===========================================================================

TEST_CASE("check_phase_gates: no advance when phase_gates is empty (no hardcoded logic)", "[phase_gates][data_driven]") {
    // If the implementation were hardcoded it would ignore phase_gates and still
    // advance. A data-driven implementation finds no gate for phase 2 and stays.
    GameState s = game_init();
    s.phase_gates.clear();
    // Satisfy old conditions and new conditions
    s.resources.at("power").value           = 50.0;
    s.resources.at("tech_scrap").value      = 50.0;
    s.resources.at("robots").value          = 1.0;
    s.resources.at("colony_capacity").value = 10.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates: no advance when gate targets wrong phase (phase=3 only)", "[phase_gates][data_driven]") {
    // current_phase=1, so we look for phase==2. A gate with phase==3 should be ignored.
    GameState s = game_init();
    s.phase_gates.clear();
    PhaseGate gate3;
    gate3.phase = 3;
    gate3.conditions.push_back({"power",      50.0});
    gate3.conditions.push_back({"tech_scrap", 50.0});
    gate3.conditions.push_back({"robots",     1.0});
    s.phase_gates.push_back(gate3);
    s.resources.at("power").value      = 50.0;
    s.resources.at("tech_scrap").value = 50.0;
    s.resources.at("robots").value     = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates: advances when custom gate with lower power threshold is satisfied", "[phase_gates][data_driven]") {
    // Replace the gate with a custom VALUE-based power threshold (10.0).
    // A hardcoded implementation would fail; a data-driven one reads min_value=10.0.
    // condition_field defaults to VALUE (zero-initialized) for 2-field init.
    GameState s = game_init();
    s.phase_gates.clear();
    PhaseGate gate;
    gate.phase = 2;
    gate.conditions.push_back({"power",      10.0}); // VALUE, defaults to ConditionField(0)=VALUE
    gate.conditions.push_back({"tech_scrap", 50.0});
    gate.conditions.push_back({"robots",     1.0});
    s.phase_gates.push_back(gate);
    s.resources.at("power").value      = 10.0; // meets custom threshold
    s.resources.at("tech_scrap").value = 50.0;
    s.resources.at("robots").value     = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("check_phase_gates: does not advance when custom gate condition not fully satisfied", "[phase_gates][data_driven]") {
    // Custom gate requires tech_scrap >= 100. Supply only 50 — should not advance.
    GameState s = game_init();
    s.phase_gates.clear();
    PhaseGate gate;
    gate.phase = 2;
    gate.conditions.push_back({"power",      50.0});
    gate.conditions.push_back({"tech_scrap", 100.0}); // stricter than default
    gate.conditions.push_back({"robots",     1.0});
    s.phase_gates.push_back(gate);
    s.resources.at("power").value      = 50.0;
    s.resources.at("tech_scrap").value = 50.0; // below 100 requirement
    s.resources.at("robots").value     = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates: advances using the gate whose phase matches current_phase+1 when multiple gates present", "[phase_gates][data_driven]") {
    // Two gates: one for phase 2 (conditions met) and one for phase 3 (blocked).
    // Should advance to phase 2 only.
    GameState s = game_init();
    // Keep the existing phase-2 gate; add a blocking phase-3 gate
    PhaseGate gate3;
    gate3.phase = 3;
    gate3.conditions.push_back({"power", 200.0}); // blocks phase 3 (far above cap)
    s.phase_gates.push_back(gate3);
    // Satisfy the new phase-2 gate: power RATE >= 0.0, colony_capacity VALUE >= 10.0
    s.resources.at("power").rate = 0.5;
    s.resources.at("colony_capacity").value = 10.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2); // advanced past phase 1 only
}

// ===========================================================================
// Section 3 — save/load round-trip for phase_gates
// ===========================================================================

TEST_CASE("round-trip: phase_gates vector count is preserved for default state (2 gates)", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.phase_gates.size() == 2);
}

TEST_CASE("round-trip: phase_gate[0].phase is preserved", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.phase_gates[0].phase == 2);
}

TEST_CASE("round-trip: phase_gate[0].conditions count is preserved (2 conditions)", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.phase_gates[0].conditions.size() == 2);
}

TEST_CASE("round-trip: phase_gate[0] power condition resource_id is preserved", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    const GateCondition* c = find_condition(loaded.phase_gates[0], "power");
    REQUIRE(c != nullptr);
}

TEST_CASE("round-trip: phase_gate[0] power condition min_value 0.0 is preserved", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    const GateCondition* c = find_condition(loaded.phase_gates[0], "power");
    REQUIRE(c != nullptr);
    REQUIRE(c->min_value == Catch::Approx(0.0));
}

TEST_CASE("round-trip: phase_gate[0] power condition_field RATE is preserved", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    const GateCondition* c = find_condition(loaded.phase_gates[0], "power");
    REQUIRE(c != nullptr);
    REQUIRE(c->condition_field == ConditionField::RATE);
}

TEST_CASE("round-trip: phase_gate[0] colony_capacity condition resource_id is preserved", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    const GateCondition* c = find_condition(loaded.phase_gates[0], "colony_capacity");
    REQUIRE(c != nullptr);
}

TEST_CASE("round-trip: phase_gate[0] colony_capacity condition min_value 2.0 is preserved", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    const GateCondition* c = find_condition(loaded.phase_gates[0], "colony_capacity");
    REQUIRE(c != nullptr);
    REQUIRE(c->min_value == Catch::Approx(2.0));
}

TEST_CASE("round-trip: phase_gate[0] colony_capacity condition_field VALUE is preserved", "[phase_gates][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    const GateCondition* c = find_condition(loaded.phase_gates[0], "colony_capacity");
    REQUIRE(c != nullptr);
    REQUIRE(c->condition_field == ConditionField::VALUE);
}

TEST_CASE("round-trip: empty phase_gates vector is preserved", "[phase_gates][save_load]") {
    GameState s = game_init();
    s.phase_gates.clear();
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.phase_gates.empty());
}

TEST_CASE("round-trip: custom gate with modified min_value preserves floating-point precision", "[phase_gates][save_load]") {
    // Use a value with significant fractional precision to catch lossy serialization.
    GameState s = game_init();
    s.phase_gates[0].conditions[0].min_value = 37.125; // exact in IEEE 754
    GameState loaded = load_game(save_game(s));
    const GateCondition* c = find_condition(loaded.phase_gates[0], s.phase_gates[0].conditions[0].resource_id);
    REQUIRE(c != nullptr);
    REQUIRE(c->min_value == Catch::Approx(37.125));
}

TEST_CASE("round-trip: multiple gates are all preserved", "[phase_gates][save_load]") {
    // game_init now provides 2 gates (phase 2 and phase 3); add a custom phase-4 gate
    GameState s = game_init();
    PhaseGate gate4;
    gate4.phase = 4;
    gate4.conditions.push_back({"power",      75.0});
    gate4.conditions.push_back({"tech_scrap", 200.0});
    s.phase_gates.push_back(gate4);
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.phase_gates.size() == 3);
    // Phase 2 gate: 2 conditions; Phase 3 gate: 2 conditions; Phase 4 gate: 2 conditions
    bool found2 = false, found3 = false, found4 = false;
    for (const auto& g : loaded.phase_gates) {
        if (g.phase == 2) { found2 = true; REQUIRE(g.conditions.size() == 2); }
        if (g.phase == 3) { found3 = true; REQUIRE(g.conditions.size() == 2); }
        if (g.phase == 4) { found4 = true; REQUIRE(g.conditions.size() == 2); }
    }
    REQUIRE(found2);
    REQUIRE(found3);
    REQUIRE(found4);
}

TEST_CASE("round-trip: phase_gates functional after load — loaded gate still drives check_phase_gates", "[phase_gates][save_load]") {
    // After a round-trip the loaded state must still work correctly with check_phase_gates.
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    // Conditions not met — should stay at 1 (colony_capacity.value=0 < 10.0)
    check_phase_gates(loaded);
    REQUIRE(loaded.current_phase == 1);
    // Now satisfy new gate: power RATE >= 0.0, colony_capacity VALUE >= 10.0
    loaded.resources.at("power").rate = 0.5;
    loaded.resources.at("colony_capacity").value = 10.0;
    check_phase_gates(loaded);
    REQUIRE(loaded.current_phase == 2);
}
