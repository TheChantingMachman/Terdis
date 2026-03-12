#include <catch2/catch_all.hpp>
#include "custodian.hpp"

// ── Section 1: GameState field initialization ─────────────────────────────────

TEST_CASE("game_init: compute_amplified starts false", "[init][compute_amplifier]") {
    GameState s = game_init();
    REQUIRE(s.compute_amplified == false);
}

// ── Section 2: buy_compute_amplifier success path ─────────────────────────────

TEST_CASE("buy_compute_amplifier returns true when affordable and not yet done", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    REQUIRE(buy_compute_amplifier(s) == true);
}

TEST_CASE("buy_compute_amplifier deducts COMPUTE_AMP_COST=40.0 from tech_scrap", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 50.0;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(10.0));
}

TEST_CASE("buy_compute_amplifier sets compute.cap to COMPUTE_AMP_CAP=150.0", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(150.0));
}

TEST_CASE("buy_compute_amplifier sets compute_amplified to true", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    REQUIRE(s.compute_amplified == true);
}

TEST_CASE("buy_compute_amplifier pushes compute_amplified event", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    bool found = false;
    for (const auto& ev : s.pending_events) {
        if (ev == "compute_amplified") { found = true; break; }
    }
    REQUIRE(found == true);
}

// ── Section 3: guard — already done ──────────────────────────────────────────

TEST_CASE("buy_compute_amplifier returns false when already amplified", "[compute_amplifier]") {
    GameState s = game_init();
    s.compute_amplified = true;
    s.resources.at("tech_scrap").value = 100.0;
    REQUIRE(buy_compute_amplifier(s) == false);
}

TEST_CASE("buy_compute_amplifier does not change state when already amplified", "[compute_amplifier]") {
    GameState s = game_init();
    s.compute_amplified = true;
    s.resources.at("tech_scrap").value = 100.0;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(100.0));
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(50.0));
    REQUIRE(s.pending_events.empty());
}

// ── Section 4: guard — cannot afford ─────────────────────────────────────────

TEST_CASE("buy_compute_amplifier returns false when tech_scrap < 40.0", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 39.9;
    REQUIRE(buy_compute_amplifier(s) == false);
}

TEST_CASE("buy_compute_amplifier does not change state when cannot afford", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 39.9;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(39.9));
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(50.0));
    REQUIRE(s.compute_amplified == false);
    REQUIRE(s.pending_events.empty());
}

// ── Section 5: idempotency ────────────────────────────────────────────────────

TEST_CASE("buy_compute_amplifier second call returns false", "[compute_amplifier]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 100.0;
    buy_compute_amplifier(s); // succeeds: scrap -> 60.0
    REQUIRE(buy_compute_amplifier(s) == false);
    REQUIRE(s.resources.at("tech_scrap").value == Catch::Approx(60.0));
}

// ── Section 6: save/load round-trip ──────────────────────────────────────────

TEST_CASE("round-trip: compute_amplified=false is preserved", "[compute_amplifier][save_load]") {
    GameState loaded = load_game(save_game(game_init()));
    REQUIRE(loaded.compute_amplified == false);
}

TEST_CASE("round-trip: compute_amplified=true is preserved", "[compute_amplifier][save_load]") {
    GameState s = game_init();
    s.compute_amplified = true;
    GameState loaded = load_game(save_game(s));
    REQUIRE(loaded.compute_amplified == true);
}

// ── Section 7: integration with game_tick / player_click ─────────────────────

TEST_CASE("game_tick: compute cap 150.0 after amplifier allows higher compute accumulation", "[compute_amplifier][integration]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    REQUIRE(s.resources.at("compute").cap == Catch::Approx(150.0));
    s.resources.at("compute").value = 149.0;
    player_click(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(150.0));
}

TEST_CASE("player_click clamps compute at amplified cap 150.0", "[compute_amplifier][integration]") {
    GameState s = game_init();
    s.resources.at("tech_scrap").value = 40.0;
    buy_compute_amplifier(s);
    s.resources.at("compute").value = 149.5;
    double added = player_click(s);
    REQUIRE(s.resources.at("compute").value == Catch::Approx(150.0));
    REQUIRE(added == Catch::Approx(0.5));
}
