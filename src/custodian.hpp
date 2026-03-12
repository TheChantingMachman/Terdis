#pragma once

#include <map>
#include <string>
#include <vector>

struct Resource {
    std::string id;
    double      value;
    double      cap;
    double      rate;
    double      max;
};

enum class ConditionField { VALUE, RATE };

struct Milestone {
    std::string    id;
    std::string    condition_resource;
    double         condition_value;
    ConditionField condition_field;
    bool           triggered;
};

struct GateCondition {
    std::string    resource_id;
    double         min_value;
    ConditionField condition_field;
};

struct PhaseGate {
    int                        phase;
    std::vector<GateCondition> conditions;
};

struct GameState {
    std::map<std::string, Resource> resources;
    int                             current_phase;
    std::vector<Milestone>          milestones;
    std::vector<PhaseGate>          phase_gates;
    std::vector<std::string>        pending_events;
    double                          total_time;
    double                          click_power;
    double                          click_cost;
    int                             solar_panel_count;
    bool                            first_robot_awoken;
    int                             colony_unit_count;
    int                             stasis_pod_count;
    bool                            compute_amplified;
    double                          scrap_boost;
    double                          power_deficit_timer;
    int                             awoken_humans;
    int                             pod_death_count;
    bool                            game_over;
    bool                            robots_cap_expanded;
    bool                            fabricator_built;
    bool                            reactor_built;
    double                          solar_decay_accumulator;
    int                             tech_scrap_vault_tier;
};

GameState   game_init();
void        game_tick(GameState& state, double delta_seconds);

std::string save_game(const GameState& state);
GameState   load_game(const std::string& data);

double player_click(GameState& state);
bool   wake_first_robot(GameState& state);
bool   buy_solar_panel(GameState& state);
bool   buy_colony_unit(GameState& state);
bool   buy_compute_amplifier(GameState& state);

double current_solar_cost(const GameState& state);
double current_colony_cost(const GameState& state);
double current_robot_cost(const GameState& state);
bool   buy_robot(GameState& state);
bool   apply_scrap_boost(GameState& state);
bool   awaken_human(GameState& state);
double scrap_boost_click_delta(const GameState& state);
void   recompute_rates(GameState& state);
void   check_phase_gates(GameState& state);
void   check_milestones(GameState& state);
bool   buy_scrap_vault(GameState& state);
bool   buy_robot_capacity(GameState& state);
bool   build_fabricator(GameState& state);
bool   build_reactor(GameState& state);
