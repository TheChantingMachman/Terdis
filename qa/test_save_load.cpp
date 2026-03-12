// Build 4 — test_save_load.cpp
// Tests for save_game() and load_game() — serialization round-trip.
// All values derived from spec/core-spec.md and the build-4 work order scope.
//
// Public API under test:
//   std::string save_game(const GameState& state);
//   GameState   load_game(const std::string& data);
//
// Core invariant: load_game(save_game(state)) reproduces the original state
// exactly (within floating-point representation) for every GameState field.

#include <catch2/catch_all.hpp>
#include <algorithm>
#include <map>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Helper: build a fully-modified GameState so round-trip tests exercise
// non-default values for every field.
// All values are within spec-defined ranges.
// ---------------------------------------------------------------------------

static GameState make_modified_state() {
    GameState s = game_init();

    // Scalars
    s.current_phase      = 2;
    s.total_time         = 123.456;
    s.click_power        = 2.5;
    s.click_cost         = 0.75;
    s.solar_panel_count  = 3;
    s.first_robot_awoken = true;

    // Resources — values within [0, cap]
    s.resources.at("power").value      = 42.5;
    s.resources.at("power").cap        = 200.0;
    s.resources.at("power").rate       = 1.6;

    s.resources.at("compute").value    = 30.0;
    s.resources.at("compute").cap      = 75.0;
    s.resources.at("compute").rate     = 0.5;

    s.resources.at("robots").value     = 1.0;
    s.resources.at("robots").cap       = 5.0;
    s.resources.at("robots").rate      = 0.0;

    s.resources.at("tech_scrap").value = 99.9;
    s.resources.at("tech_scrap").cap   = 500.0;
    s.resources.at("tech_scrap").rate  = 0.3;

    // Milestones: mark two as triggered
    for (auto& m : s.milestones) {
        if (m.id == "first_compute" || m.id == "first_robot")
            m.triggered = true;
    }

    // pending_events: two events queued
    s.pending_events.push_back("first_compute");
    s.pending_events.push_back("first_robot");

    return s;
}

// ===========================================================================
// save_game basic contract
// ===========================================================================

TEST_CASE("save_game returns a non-empty string", "[save_load]") {
    GameState s = game_init();
    std::string data = save_game(s);
    REQUIRE(!data.empty());
}

TEST_CASE("save_game is deterministic: two calls on the same state produce equal output", "[save_load]") {
    GameState s = game_init();
    REQUIRE(save_game(s) == save_game(s));
}

// ===========================================================================
// Round-trip: default (game_init) state — scalar fields
// ===========================================================================

TEST_CASE("round-trip default state: current_phase preserved", "[save_load][init]") {
    GameState orig   = game_init();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.current_phase == 1);
}

TEST_CASE("round-trip default state: total_time preserved", "[save_load][init]") {
    GameState orig   = game_init();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.total_time == Catch::Approx(0.0));
}

TEST_CASE("round-trip default state: click_power preserved", "[save_load][init]") {
    GameState orig   = game_init();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.click_power == Catch::Approx(1.0));
}

TEST_CASE("round-trip default state: click_cost preserved", "[save_load][init]") {
    GameState orig   = game_init();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.click_cost == Catch::Approx(1.0));
}

TEST_CASE("round-trip default state: solar_panel_count preserved", "[save_load][init]") {
    GameState orig   = game_init();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.solar_panel_count == 0);
}

TEST_CASE("round-trip default state: first_robot_awoken preserved", "[save_load][init]") {
    GameState orig   = game_init();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.first_robot_awoken == false);
}

TEST_CASE("round-trip default state: pending_events is empty", "[save_load][init]") {
    GameState orig   = game_init();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.pending_events.empty());
}

// ===========================================================================
// Round-trip: non-default scalar fields (one at a time)
// ===========================================================================

TEST_CASE("round-trip: current_phase=2 is preserved", "[save_load][scalars]") {
    GameState s     = game_init();
    s.current_phase  = 2;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.current_phase == 2);
}

TEST_CASE("round-trip: total_time is preserved", "[save_load][scalars]") {
    GameState s  = game_init();
    s.total_time  = 999.999;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.total_time == Catch::Approx(999.999));
}

TEST_CASE("round-trip: click_power is preserved", "[save_load][scalars]") {
    GameState s   = game_init();
    s.click_power  = 3.14;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.click_power == Catch::Approx(3.14));
}

TEST_CASE("round-trip: click_cost is preserved", "[save_load][scalars]") {
    GameState s  = game_init();
    s.click_cost  = 2.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.click_cost == Catch::Approx(2.0));
}

TEST_CASE("round-trip: solar_panel_count=5 is preserved", "[save_load][scalars]") {
    GameState s         = game_init();
    s.solar_panel_count  = 5;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.solar_panel_count == 5);
}

TEST_CASE("round-trip: first_robot_awoken=true is preserved", "[save_load][scalars]") {
    GameState s          = game_init();
    s.first_robot_awoken  = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.first_robot_awoken == true);
}

// ===========================================================================
// Round-trip: resource fields (per-resource, non-default values)
// ===========================================================================

TEST_CASE("round-trip: all four resources are present after load", "[save_load][resources]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.resources.count("power")      == 1);
    REQUIRE(loaded.resources.count("compute")    == 1);
    REQUIRE(loaded.resources.count("robots")     == 1);
    REQUIRE(loaded.resources.count("tech_scrap") == 1);
}

TEST_CASE("round-trip: resource id fields match map keys after load", "[save_load][resources]") {
    GameState loaded = load_game(save_game(game_init()));
    for (const auto& [key, res] : loaded.resources) {
        REQUIRE(res.id == key);
    }
}

TEST_CASE("round-trip: power.value is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("power").value = 77.3;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("power").value == Catch::Approx(77.3));
}

TEST_CASE("round-trip: power.cap is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("power").cap = 250.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("power").cap == Catch::Approx(250.0));
}

TEST_CASE("round-trip: power.rate is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("power").rate = 1.1;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("power").rate == Catch::Approx(1.1));
}

TEST_CASE("round-trip: compute.value is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("compute").value = 25.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("compute").value == Catch::Approx(25.0));
}

TEST_CASE("round-trip: compute.cap is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("compute").cap = 100.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("compute").cap == Catch::Approx(100.0));
}

TEST_CASE("round-trip: compute.rate is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("compute").rate = 0.5;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("compute").rate == Catch::Approx(0.5));
}

TEST_CASE("round-trip: robots.value is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("robots").value = 1.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("robots").value == Catch::Approx(1.0));
}

TEST_CASE("round-trip: robots.cap is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("robots").cap = 10.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("robots").cap == Catch::Approx(10.0));
}

TEST_CASE("round-trip: tech_scrap.value is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 42.7;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("tech_scrap").value == Catch::Approx(42.7));
}

TEST_CASE("round-trip: tech_scrap.cap is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").cap = 750.0;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("tech_scrap").cap == Catch::Approx(750.0));
}

TEST_CASE("round-trip: tech_scrap.rate is preserved", "[save_load][resources]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").rate = 0.3;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.resources.at("tech_scrap").rate == Catch::Approx(0.3));
}

// ===========================================================================
// Round-trip: milestones
// ===========================================================================

TEST_CASE("round-trip: exactly 7 milestones present after load", "[save_load][milestones]") {
    // first_compute, first_robot, solar_online, scrap_flowing, stasis_warning, colony_started, all_robots_online
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.milestones.size() == 7);
}

TEST_CASE("round-trip: all_robots_online milestone id is present after load", "[save_load][milestones]") {
    GameState loaded = load_game(save_game(game_init()));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "all_robots_online") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: all_robots_online triggered=true is preserved", "[save_load][milestones]") {
    GameState s = game_init();
    for (auto& m : s.milestones) {
        if (m.id == "all_robots_online") m.triggered = true;
    }
    GameState loaded = load_game(save_game(s));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "all_robots_online") {
            found = true;
            REQUIRE(m.triggered == true);
        }
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: milestone triggered=false is preserved", "[save_load][milestones]") {
    // game_init starts all milestones untriggered
    GameState loaded = load_game(save_game(game_init()));
    for (const auto& m : loaded.milestones) {
        INFO("Milestone: " << m.id);
        REQUIRE(m.triggered == false);
    }
}

TEST_CASE("round-trip: milestone triggered=true is preserved", "[save_load][milestones]") {
    GameState s = game_init();
    for (auto& m : s.milestones) {
        if (m.id == "first_compute") m.triggered = true;
    }
    GameState loaded = load_game(save_game(s));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "first_compute") {
            found = true;
            REQUIRE(m.triggered == true);
        }
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: untriggered milestones stay false after partial trigger", "[save_load][milestones]") {
    GameState s = game_init();
    for (auto& m : s.milestones) {
        if (m.id == "first_compute") m.triggered = true;
    }
    GameState loaded = load_game(save_game(s));
    for (const auto& m : loaded.milestones) {
        if (m.id != "first_compute") {
            INFO("Milestone: " << m.id);
            REQUIRE(m.triggered == false);
        }
    }
}

TEST_CASE("round-trip: milestone condition_field VALUE (==0) is preserved", "[save_load][milestones]") {
    // first_compute: condition_field=VALUE
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "first_compute") {
            found = true;
            REQUIRE(m.condition_field == ConditionField::VALUE);
        }
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: milestone condition_field RATE (==1) is preserved", "[save_load][milestones]") {
    // solar_online: condition_field=RATE
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "solar_online") {
            found = true;
            REQUIRE(m.condition_field == ConditionField::RATE);
        }
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: milestone condition_resource is preserved", "[save_load][milestones]") {
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "first_compute") {
            found = true;
            REQUIRE(m.condition_resource == "compute");
        }
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: milestone condition_value is preserved", "[save_load][milestones]") {
    // scrap_flowing: condition_value=20.0
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "scrap_flowing") {
            found = true;
            REQUIRE(m.condition_value == Catch::Approx(20.0));
        }
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: milestone condition_value for solar_online (0.6) is preserved", "[save_load][milestones]") {
    GameState s = game_init();
    GameState loaded = load_game(save_game(s));
    bool found = false;
    for (const auto& m : loaded.milestones) {
        if (m.id == "solar_online") {
            found = true;
            REQUIRE(m.condition_value == Catch::Approx(0.6));
        }
    }
    REQUIRE(found);
}

TEST_CASE("round-trip: all four milestone ids are present after load", "[save_load][milestones]") {
    GameState loaded = load_game(save_game(game_init()));
    std::vector<std::string> ids;
    for (const auto& m : loaded.milestones) ids.push_back(m.id);
    REQUIRE(std::find(ids.begin(), ids.end(), "first_compute") != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "first_robot")   != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "solar_online")  != ids.end());
    REQUIRE(std::find(ids.begin(), ids.end(), "scrap_flowing") != ids.end());
}

// ===========================================================================
// Round-trip: pending_events
// ===========================================================================

TEST_CASE("round-trip: empty pending_events remains empty", "[save_load][events]") {
    GameState s = game_init(); // pending_events starts empty
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.pending_events.empty());
}

TEST_CASE("round-trip: single pending_event is preserved", "[save_load][events]") {
    GameState s = game_init();
    s.pending_events.push_back("first_compute");
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.pending_events.size() == 1);
    REQUIRE(loaded.pending_events.at(0) == "first_compute");
}

TEST_CASE("round-trip: multiple pending_events are preserved in order", "[save_load][events]") {
    GameState s = game_init();
    s.pending_events.push_back("first_compute");
    s.pending_events.push_back("first_robot");
    s.pending_events.push_back("solar_online");
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.pending_events.size() == 3);
    REQUIRE(loaded.pending_events.at(0) == "first_compute");
    REQUIRE(loaded.pending_events.at(1) == "first_robot");
    REQUIRE(loaded.pending_events.at(2) == "solar_online");
}

// ===========================================================================
// Round-trip: fully-modified state (all fields non-default simultaneously)
// ===========================================================================

TEST_CASE("round-trip full state: current_phase is preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.current_phase == orig.current_phase);
}

TEST_CASE("round-trip full state: total_time is preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.total_time == Catch::Approx(orig.total_time));
}

TEST_CASE("round-trip full state: click_power is preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.click_power == Catch::Approx(orig.click_power));
}

TEST_CASE("round-trip full state: click_cost is preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.click_cost == Catch::Approx(orig.click_cost));
}

TEST_CASE("round-trip full state: solar_panel_count is preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.solar_panel_count == orig.solar_panel_count);
}

TEST_CASE("round-trip full state: first_robot_awoken is preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.first_robot_awoken == orig.first_robot_awoken);
}

TEST_CASE("round-trip full state: all resource values, caps, and rates are preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    for (const auto& [key, res] : orig.resources) {
        INFO("Resource: " << key);
        REQUIRE(loaded.resources.count(key) == 1);
        REQUIRE(loaded.resources.at(key).value == Catch::Approx(res.value));
        REQUIRE(loaded.resources.at(key).cap   == Catch::Approx(res.cap));
        REQUIRE(loaded.resources.at(key).rate  == Catch::Approx(res.rate));
    }
}

TEST_CASE("round-trip full state: milestone triggered states are preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    // Build id→triggered maps and compare
    std::map<std::string, bool> orig_map, loaded_map;
    for (const auto& m : orig.milestones)   orig_map[m.id]   = m.triggered;
    for (const auto& m : loaded.milestones) loaded_map[m.id] = m.triggered;
    REQUIRE(orig_map == loaded_map);
}

TEST_CASE("round-trip full state: pending_events are preserved", "[save_load][full]") {
    GameState orig   = make_modified_state();
    GameState loaded = load_game(save_game(orig));
    REQUIRE(loaded.pending_events == orig.pending_events);
}
