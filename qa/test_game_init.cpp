// Build 3 — test_game_init.cpp
// Tests for game_init(): verifies every field of the returned GameState
// against spec/core-spec.md (Phase 1 Resources, Game State, Milestones).

#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ---------------------------------------------------------------------------
// Scalar GameState fields
// ---------------------------------------------------------------------------

TEST_CASE("game_init returns current_phase 1", "[init]") {
    GameState s = game_init();
    REQUIRE(s.current_phase == 1);
}

TEST_CASE("game_init returns click_power 1.0", "[init]") {
    GameState s = game_init();
    REQUIRE(s.click_power == Catch::Approx(1.0));
}

TEST_CASE("game_init returns click_cost 1.0", "[init]") {
    GameState s = game_init();
    REQUIRE(s.click_cost == Catch::Approx(1.0));
}

TEST_CASE("game_init returns solar_panel_count 0", "[init]") {
    GameState s = game_init();
    REQUIRE(s.solar_panel_count == 0);
}

TEST_CASE("game_init returns first_robot_awoken false", "[init]") {
    GameState s = game_init();
    REQUIRE(s.first_robot_awoken == false);
}

TEST_CASE("game_init returns total_time 0.0", "[init]") {
    GameState s = game_init();
    REQUIRE(s.total_time == Catch::Approx(0.0));
}

TEST_CASE("game_init returns empty pending_events", "[init]") {
    GameState s = game_init();
    REQUIRE(s.pending_events.empty());
}

// ---------------------------------------------------------------------------
// Resource: power
// ---------------------------------------------------------------------------

TEST_CASE("game_init power initial value 15.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.count("power") == 1);
    REQUIRE(s.resources.at("power").value == Catch::Approx(15.0));
}

TEST_CASE("game_init power cap 100.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("power").cap == Catch::Approx(100.0));
}

TEST_CASE("game_init power rate 0.1", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("power").rate == Catch::Approx(0.1));
}

// ---------------------------------------------------------------------------
// Resource: compute
// ---------------------------------------------------------------------------

TEST_CASE("game_init compute initial value 0.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.count("compute") == 1);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(0.0));
}

TEST_CASE("game_init compute cap 50.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(50.0));
}

TEST_CASE("game_init compute rate 0.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("compute").rate == Catch::Approx(0.0));
}

// ---------------------------------------------------------------------------
// Resource: robots
// ---------------------------------------------------------------------------

TEST_CASE("game_init robots initial value 0.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.count("robots") == 1);
    REQUIRE(s.resources.at("robots").value == Catch::Approx(0.0));
}

TEST_CASE("game_init robots cap 5.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("robots").cap == Catch::Approx(5.0));
}

TEST_CASE("game_init robots rate 0.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("robots").rate == Catch::Approx(0.0));
}

// ---------------------------------------------------------------------------
// Resource: tech_scrap
// ---------------------------------------------------------------------------

TEST_CASE("game_init tech_scrap initial value 0.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.count("tech_scrap") == 1);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(0.0));
}

TEST_CASE("game_init tech_scrap cap 500.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("tech_scrap").cap == Catch::Approx(500.0));
}

TEST_CASE("game_init tech_scrap rate 0.0", "[init][resources]") {
    GameState s = game_init();
    REQUIRE(s.resources.at("tech_scrap").rate == Catch::Approx(0.0));
}

// ---------------------------------------------------------------------------
// Milestones
// ---------------------------------------------------------------------------

TEST_CASE("game_init produces exactly 7 milestones", "[init][milestones]") {
    // first_compute, first_robot, solar_online, scrap_flowing, stasis_warning, colony_started, all_robots_online
    GameState s = game_init();
    REQUIRE(s.milestones.size() == 7);
}

TEST_CASE("game_init milestone all_robots_online exists with correct spec", "[init][milestones]") {
    GameState s = game_init();
    bool found = false;
    for (const auto& m : s.milestones) {
        if (m.id == "all_robots_online") {
            found = true;
            REQUIRE(m.condition_resource == "robots");
            REQUIRE(m.condition_value == Catch::Approx(5.0));
            REQUIRE(m.condition_field == ConditionField::VALUE);
            REQUIRE(m.triggered == false);
        }
    }
    REQUIRE(found);
}

TEST_CASE("game_init all milestones start untriggered", "[init][milestones]") {
    GameState s = game_init();
    for (const auto& m : s.milestones) {
        INFO("Milestone: " << m.id);
        REQUIRE(m.triggered == false);
    }
}

TEST_CASE("game_init milestone first_compute exists with correct spec", "[init][milestones]") {
    GameState s = game_init();
    bool found = false;
    for (const auto& m : s.milestones) {
        if (m.id == "first_compute") {
            found = true;
            REQUIRE(m.condition_resource == "compute");
            REQUIRE(m.condition_value == Catch::Approx(1.0));
            REQUIRE(m.condition_field == ConditionField::VALUE);
            REQUIRE(m.triggered == false);
        }
    }
    REQUIRE(found);
}

TEST_CASE("game_init milestone first_robot exists with correct spec", "[init][milestones]") {
    GameState s = game_init();
    bool found = false;
    for (const auto& m : s.milestones) {
        if (m.id == "first_robot") {
            found = true;
            REQUIRE(m.condition_resource == "robots");
            REQUIRE(m.condition_value == Catch::Approx(1.0));
            REQUIRE(m.condition_field == ConditionField::VALUE);
            REQUIRE(m.triggered == false);
        }
    }
    REQUIRE(found);
}

TEST_CASE("game_init milestone solar_online exists with correct spec", "[init][milestones]") {
    GameState s = game_init();
    bool found = false;
    for (const auto& m : s.milestones) {
        if (m.id == "solar_online") {
            found = true;
            REQUIRE(m.condition_resource == "power");
            REQUIRE(m.condition_value == Catch::Approx(0.6));
            REQUIRE(m.condition_field == ConditionField::RATE);
            REQUIRE(m.triggered == false);
        }
    }
    REQUIRE(found);
}

TEST_CASE("game_init milestone scrap_flowing exists with correct spec", "[init][milestones]") {
    GameState s = game_init();
    bool found = false;
    for (const auto& m : s.milestones) {
        if (m.id == "scrap_flowing") {
            found = true;
            REQUIRE(m.condition_resource == "tech_scrap");
            REQUIRE(m.condition_value == Catch::Approx(20.0));
            REQUIRE(m.condition_field == ConditionField::VALUE);
            REQUIRE(m.triggered == false);
        }
    }
    REQUIRE(found);
}

// ---------------------------------------------------------------------------
// Resource id fields
// ---------------------------------------------------------------------------

TEST_CASE("game_init resource id fields match map keys", "[init][resources]") {
    GameState s = game_init();
    for (const auto& [key, res] : s.resources) {
        REQUIRE(res.id == key);
    }
}
