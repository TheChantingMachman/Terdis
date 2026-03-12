// Build 3 — test_gates_and_milestones.cpp
// Tests for check_phase_gates() and check_milestones().
// All values derived from spec/core-spec.md.
//
// Phase 1 → 2 gate conditions:
//   power          RATE  >= 0.0
//   colony_capacity VALUE >= 10.0
//
// Phase 1 milestones:
//   first_compute  : compute VALUE >= 1.0
//   first_robot    : robots  VALUE >= 1.0
//   solar_online   : power   RATE  >= 0.6
//   scrap_flowing  : tech_scrap VALUE >= 20.0

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// Helper: find a milestone by id in the state
static Milestone* find_milestone(GameState& s, const std::string& id) {
    for (auto& m : s.milestones) {
        if (m.id == id) return &m;
    }
    return nullptr;
}

static bool in_pending_events(const GameState& s, const std::string& id) {
    for (const auto& e : s.pending_events) {
        if (e == id) return true;
    }
    return false;
}

// ===========================================================================
// check_phase_gates
// ===========================================================================

TEST_CASE("check_phase_gates does not advance phase when all below threshold", "[gates]") {
    GameState s = game_init();
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates does not advance when only power condition met", "[gates]") {
    GameState s = game_init();
    s.resources.at("power").value = 50.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates does not advance when only tech_scrap condition met", "[gates]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 50.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates does not advance when only robots condition met", "[gates]") {
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates does not advance when power and tech_scrap met but not robots", "[gates]") {
    GameState s = game_init();
    s.resources.at("power").value      = 50.0;
    s.resources.at("tech_scrap").value = 50.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates does not advance when power and robots met but not tech_scrap", "[gates]") {
    GameState s = game_init();
    s.resources.at("power").value  = 50.0;
    s.resources.at("robots").value = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates does not advance when tech_scrap and robots met but not power", "[gates]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 50.0;
    s.resources.at("robots").value     = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates advances to phase 2 when all conditions met exactly", "[gates]") {
    // New gate: power RATE >= 0.0, colony_capacity VALUE >= 2.0
    GameState s = game_init();
    s.resources.at("power").rate = 0.0;
    s.resources.at("colony_capacity").value = 2.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("check_phase_gates advances to phase 2 when all conditions exceeded", "[gates]") {
    // New gate: power RATE >= 0.0, colony_capacity VALUE >= 10.0
    GameState s = game_init();
    s.resources.at("power").rate = 0.5;
    s.resources.at("colony_capacity").value = 20.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("check_phase_gates does not advance past phase 2 on repeated calls", "[gates]") {
    // New gate: power RATE >= 0.0, colony_capacity VALUE >= 2.0
    GameState s = game_init();
    s.resources.at("power").rate = 0.5;
    s.resources.at("colony_capacity").value = 2.0;
    check_phase_gates(s); // → phase 2
    check_phase_gates(s); // should stay at 2
    REQUIRE(s.current_phase == 2);
}

TEST_CASE("check_phase_gates boundary: power at 49.9 is insufficient", "[gates]") {
    GameState s = game_init();
    s.resources.at("power").value      = 49.9;
    s.resources.at("tech_scrap").value = 50.0;
    s.resources.at("robots").value     = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates boundary: tech_scrap at 49.9 is insufficient", "[gates]") {
    GameState s = game_init();
    s.resources.at("power").value      = 50.0;
    s.resources.at("tech_scrap").value = 49.9;
    s.resources.at("robots").value     = 1.0;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("check_phase_gates boundary: robots at 0.9 is insufficient", "[gates]") {
    GameState s = game_init();
    s.resources.at("power").value      = 50.0;
    s.resources.at("tech_scrap").value = 50.0;
    s.resources.at("robots").value     = 0.9;
    check_phase_gates(s);
    REQUIRE(s.current_phase == 1);
}

// ===========================================================================
// check_milestones — first_compute (compute VALUE >= 1.0)
// ===========================================================================

TEST_CASE("check_milestones: first_compute not triggered below threshold", "[milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 0.9;
    check_milestones(s);
    Milestone* m = find_milestone(s, "first_compute");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
    REQUIRE_FALSE(in_pending_events(s, "first_compute"));
}

TEST_CASE("check_milestones: first_compute triggers at exactly 1.0", "[milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 1.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "first_compute");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
    REQUIRE(in_pending_events(s, "first_compute"));
}

TEST_CASE("check_milestones: first_compute triggers above threshold", "[milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "first_compute");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

TEST_CASE("check_milestones: first_compute does not re-trigger once set", "[milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    check_milestones(s); // fires once
    size_t events_after_first = s.pending_events.size();
    check_milestones(s); // should not fire again
    REQUIRE(s.pending_events.size() == events_after_first);
}

// ===========================================================================
// check_milestones — first_robot (robots VALUE >= 1.0)
// ===========================================================================

TEST_CASE("check_milestones: first_robot not triggered when robots = 0", "[milestones]") {
    GameState s = game_init();
    check_milestones(s);
    Milestone* m = find_milestone(s, "first_robot");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
    REQUIRE_FALSE(in_pending_events(s, "first_robot"));
}

TEST_CASE("check_milestones: first_robot triggers at robots = 1.0", "[milestones]") {
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "first_robot");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
    REQUIRE(in_pending_events(s, "first_robot"));
}

TEST_CASE("check_milestones: first_robot triggers above 1.0", "[milestones]") {
    GameState s = game_init();
    s.resources.at("robots").value = 2.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "first_robot");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

// ===========================================================================
// check_milestones — solar_online (power RATE >= 0.6)
// ===========================================================================

TEST_CASE("check_milestones: solar_online not triggered at initial power rate 0.1", "[milestones]") {
    GameState s = game_init();
    // power.rate = 0.1 at init
    check_milestones(s);
    Milestone* m = find_milestone(s, "solar_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
    REQUIRE_FALSE(in_pending_events(s, "solar_online"));
}

TEST_CASE("check_milestones: solar_online not triggered at power rate 0.5 (below 0.6)", "[milestones]") {
    GameState s = game_init();
    s.resources.at("power").rate = 0.5;
    check_milestones(s);
    Milestone* m = find_milestone(s, "solar_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
}

TEST_CASE("check_milestones: solar_online triggers at power rate exactly 0.6", "[milestones]") {
    // 1 solar panel: power.rate = 0.1 + 0.5 = 0.6 → threshold exactly met
    GameState s = game_init();
    s.resources.at("power").rate = 0.6;
    check_milestones(s);
    Milestone* m = find_milestone(s, "solar_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
    REQUIRE(in_pending_events(s, "solar_online"));
}

TEST_CASE("check_milestones: solar_online checks RATE not VALUE", "[milestones]") {
    GameState s = game_init();
    // High power value but low rate — should NOT trigger solar_online
    s.resources.at("power").value = 99.0;
    s.resources.at("power").rate  = 0.1; // below 0.6 threshold
    check_milestones(s);
    Milestone* m = find_milestone(s, "solar_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
}

TEST_CASE("check_milestones: solar_online triggers above 0.6 rate", "[milestones]") {
    GameState s = game_init();
    s.resources.at("power").rate = 1.1; // 2 panels, 0 robots
    check_milestones(s);
    Milestone* m = find_milestone(s, "solar_online");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

// ===========================================================================
// check_milestones — scrap_flowing (tech_scrap VALUE >= 20.0)
// ===========================================================================

TEST_CASE("check_milestones: scrap_flowing not triggered at 0 tech_scrap", "[milestones]") {
    GameState s = game_init();
    check_milestones(s);
    Milestone* m = find_milestone(s, "scrap_flowing");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
    REQUIRE_FALSE(in_pending_events(s, "scrap_flowing"));
}

TEST_CASE("check_milestones: scrap_flowing not triggered at 19.9 tech_scrap", "[milestones]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 19.9;
    check_milestones(s);
    Milestone* m = find_milestone(s, "scrap_flowing");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == false);
}

TEST_CASE("check_milestones: scrap_flowing triggers at exactly 20.0 tech_scrap", "[milestones]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 20.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "scrap_flowing");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
    REQUIRE(in_pending_events(s, "scrap_flowing"));
}

TEST_CASE("check_milestones: scrap_flowing triggers above 20.0", "[milestones]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 50.0;
    check_milestones(s);
    Milestone* m = find_milestone(s, "scrap_flowing");
    REQUIRE(m != nullptr);
    REQUIRE(m->triggered == true);
}

// ===========================================================================
// check_milestones — general: pending_events and re-triggering
// ===========================================================================

TEST_CASE("check_milestones: multiple milestones can fire in one call", "[milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value    = 1.0;  // fires first_compute
    s.resources.at("tech_scrap").value = 20.0; // fires scrap_flowing
    check_milestones(s);
    REQUIRE(in_pending_events(s, "first_compute"));
    REQUIRE(in_pending_events(s, "scrap_flowing"));
}

TEST_CASE("check_milestones: already triggered milestone does not push duplicate event", "[milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    check_milestones(s);
    size_t count_after_first = s.pending_events.size();
    // Call again with same conditions
    check_milestones(s);
    REQUIRE(s.pending_events.size() == count_after_first);
}

TEST_CASE("check_milestones: no events pushed when no conditions met", "[milestones]") {
    GameState s = game_init();
    // All resources at defaults: compute=0, robots=0, power.rate=0.1, scrap=0
    check_milestones(s);
    REQUIRE(s.pending_events.empty());
}

TEST_CASE("check_milestones: triggered flag persists after condition drops below threshold", "[milestones]") {
    GameState s = game_init();
    s.resources.at("compute").value = 5.0;
    check_milestones(s); // fires first_compute
    // Drop compute below threshold
    s.resources.at("compute").value = 0.0;
    check_milestones(s); // should not re-fire
    // Count occurrences of "first_compute" in pending_events
    int count = 0;
    for (const auto& e : s.pending_events) {
        if (e == "first_compute") count++;
    }
    REQUIRE(count == 1); // fired exactly once
}
