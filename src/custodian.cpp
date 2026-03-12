#include "custodian.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>

// Constants
static constexpr double BASE_POWER_RATE           = 0.1;
static constexpr double SOLAR_POWER_RATE          = 0.75;
static constexpr double ROBOT_POWER_DRAIN         = 0.2;
static constexpr double SOLAR_BASE_COST           = 5.0;
static constexpr double SOLAR_COST_SCALE          = 1.05;
static constexpr int    SOLAR_PANEL_MAX           = 20;
static constexpr double SOLAR_DECAY_RATE          = 0.002;
static constexpr double BASE_SCRAP_RATE           = 0.1;
static constexpr double COMPUTE_SCRAP_BONUS       = 0.02;
static constexpr double WAKE_ROBOT_COMPUTE_COST   = 10.0;
static constexpr double STASIS_DRAIN_BASE         = 0.05;
static constexpr double STASIS_DRAIN_GROWTH       = 0.001;
static constexpr double STASIS_DRAIN_CAP          = 4.0;
static constexpr double STASIS_DRAIN_PER_POD      = 2.0;
static constexpr int    STASIS_POD_COUNT          = 50;
static constexpr double COLONY_UNIT_BASE_COST     = 200.0;
static constexpr double COLONY_UNIT_COST_SCALE    = 1.20;
static constexpr double COLONY_CAPACITY_PER_UNIT  = 2.0;
static constexpr double COMPUTE_AMP_COST          = 40.0;
static constexpr double COMPUTE_AMP_CAP           = 150.0;
static constexpr double ROBOT_BASE_COST           = 10.0;
static constexpr double ROBOT_COST_STEP           = 2.0;
static constexpr double SCRAP_BOOST_COMPUTE_COST  = 5.0;
static constexpr double ROBOT_COMPUTE_DRAIN       = 0.5;
static constexpr double HUMAN_SCRAP_FACTOR        = 0.5;
static constexpr double HUMAN_POWER_BASE          = 0.5;
static constexpr double HUMAN_POWER_SCALE         = 0.3;
static constexpr double SCRAP_BOOST_BASE          = 0.06;
static constexpr int    COLONY_PHASE2_MAX_UNITS   = 5;
static constexpr double ROBOT_CAP_EXPAND_COST     = 500.0;
static constexpr double SCRAP_BOOST_DECAY         = 0.002;
static constexpr double STASIS_POD_DEATH_THRESHOLD = 30.0;
static constexpr double FABRICATOR_COST            = 1000.0;
static constexpr double REACTOR_CHARGE_COST        = 80.0;
static constexpr double REACTOR_POWER_BONUS        = 10.0;

GameState game_init() {
    GameState s;
    s.current_phase      = 1;
    s.total_time         = 0.0;
    s.click_power        = 1.0;
    s.click_cost         = 1.0;
    s.solar_panel_count  = 0;
    s.first_robot_awoken = false;
    s.colony_unit_count  = 0;
    s.stasis_pod_count   = STASIS_POD_COUNT;
    s.compute_amplified  = false;
    s.scrap_boost        = 0.0;
    s.power_deficit_timer = 0.0;
    s.awoken_humans      = 0;
    s.pod_death_count           = 0;
    s.game_over                 = false;
    s.robots_cap_expanded       = false;
    s.fabricator_built          = false;
    s.reactor_built             = false;
    s.solar_decay_accumulator   = 0.0;
    s.tech_scrap_vault_tier     = 0;

    auto make_res = [](const std::string& id, double value, double cap, double rate) {
        Resource r;
        r.id    = id;
        r.value = value;
        r.cap   = cap;
        r.rate  = rate;
        r.max   = cap;
        return r;
    };

    s.resources["power"]           = make_res("power",           15.0,  100.0, 0.1);
    s.resources["compute"]         = make_res("compute",          0.0,   50.0, 0.0);
    s.resources["robots"]          = make_res("robots",           0.0,    5.0, 0.0);
    s.resources["tech_scrap"]      = make_res("tech_scrap",       0.0,  500.0, 0.0);
    s.resources["stasis_drain"]    = make_res("stasis_drain",     0.05,   4.0, 0.0);
    s.resources["colony_capacity"] = make_res("colony_capacity",  0.0,   50.0, 0.0);

    auto make_ms = [](const std::string& id, const std::string& res,
                      ConditionField field, double threshold) {
        Milestone m;
        m.id                 = id;
        m.condition_resource = res;
        m.condition_field    = field;
        m.condition_value    = threshold;
        m.triggered          = false;
        return m;
    };

    s.milestones.push_back(make_ms("first_compute",  "compute",         ConditionField::VALUE, 1.0));
    s.milestones.push_back(make_ms("first_robot",    "robots",          ConditionField::VALUE, 1.0));
    s.milestones.push_back(make_ms("solar_online",   "power",           ConditionField::RATE,  0.6));
    s.milestones.push_back(make_ms("scrap_flowing",  "tech_scrap",      ConditionField::VALUE, 20.0));
    s.milestones.push_back(make_ms("stasis_warning",    "stasis_drain",    ConditionField::VALUE, 1.0));
    s.milestones.push_back(make_ms("colony_started",    "colony_capacity", ConditionField::VALUE, 2.0));
    s.milestones.push_back(make_ms("all_robots_online", "robots",          ConditionField::VALUE, 5.0));

    // Phase 1→2 gate: power RATE >= 0.0, colony_capacity VALUE >= 2.0
    GateCondition gc1;
    gc1.resource_id     = "power";
    gc1.min_value       = 0.0;
    gc1.condition_field = ConditionField::RATE;

    GateCondition gc2;
    gc2.resource_id     = "colony_capacity";
    gc2.min_value       = 2.0;
    gc2.condition_field = ConditionField::VALUE;

    s.phase_gates.push_back(PhaseGate{2, {gc1, gc2}});

    // Phase 2→3 gate: colony_capacity VALUE >= 10.0, robots VALUE >= 10.0
    GateCondition gc3;
    gc3.resource_id     = "colony_capacity";
    gc3.min_value       = 10.0;
    gc3.condition_field = ConditionField::VALUE;

    GateCondition gc4;
    gc4.resource_id     = "robots";
    gc4.min_value       = 10.0;
    gc4.condition_field = ConditionField::VALUE;

    s.phase_gates.push_back(PhaseGate{3, {gc3, gc4}});

    return s;
}

void game_tick(GameState& state, double delta_seconds) {
    // Step 0: no processing if game is over
    if (state.game_over) return;

    auto& scrap   = state.resources["tech_scrap"];
    auto& robots  = state.resources["robots"];
    auto& compute = state.resources["compute"];

    // 1. Update stasis_drain value (uses total_time BEFORE increment)
    double stasis_max = static_cast<double>(state.stasis_pod_count) * STASIS_DRAIN_PER_POD;
    double stasis_val = std::min({
        STASIS_DRAIN_BASE + STASIS_DRAIN_GROWTH * state.total_time,
        stasis_max,
        STASIS_DRAIN_CAP});
    state.resources["stasis_drain"].value = stasis_val;

    // 2. Recompute rates
    recompute_rates(state);

    // 3. Decay scrap boost
    state.scrap_boost = std::max(0.0, state.scrap_boost - SCRAP_BOOST_DECAY * delta_seconds);

    // 4. Dynamic scrap rate (with boost)
    double effective_workers = robots.value + state.awoken_humans * HUMAN_SCRAP_FACTOR;
    scrap.rate = effective_workers * (BASE_SCRAP_RATE + compute.value * COMPUTE_SCRAP_BONUS) * (1.0 + state.scrap_boost);

    // 5. Advance all resources
    for (auto& [id, r] : state.resources) {
        r.value = std::clamp(r.value + r.rate * delta_seconds, 0.0, r.cap);
    }

    // 5a. Solar panel degradation
    if (state.solar_panel_count > 0) {
        state.solar_decay_accumulator +=
            SOLAR_DECAY_RATE * (static_cast<double>(state.solar_panel_count) / static_cast<double>(SOLAR_PANEL_MAX)) * delta_seconds;
        while (state.solar_decay_accumulator >= 1.0 && state.solar_panel_count > 0) {
            state.solar_panel_count      -= 1;
            state.solar_decay_accumulator -= 1.0;
            state.pending_events.push_back("panel_degraded");
            recompute_rates(state);
        }
        if (state.solar_panel_count == 0) {
            state.solar_decay_accumulator = 0.0;
        }
    }

    // 6. Update power deficit timer
    auto& power = state.resources["power"];
    if (power.value <= 0.0) {
        state.power_deficit_timer += delta_seconds;
    } else {
        state.power_deficit_timer = 0.0;
    }
    if (state.power_deficit_timer >= STASIS_POD_DEATH_THRESHOLD && state.stasis_pod_count > 0) {
        int pods_to_lose = (state.pod_death_count == 0) ? 1
                         : (state.pod_death_count == 1) ? 5
                                                        : 10;
        pods_to_lose = std::min(pods_to_lose, state.stasis_pod_count);
        state.stasis_pod_count -= pods_to_lose;
        state.pod_death_count  += 1;
        state.power_deficit_timer = 0.0;
        state.pending_events.push_back("pod_lost");
        if (state.stasis_pod_count + state.awoken_humans == 0) {
            state.game_over = true;
            state.pending_events.push_back("game_over");
            return; // All humans dead — no further processing this tick
        }
    }

    // 8–9. Gate and milestone checks
    check_phase_gates(state);
    check_milestones(state);

    // 10. Accumulate time
    state.total_time += delta_seconds;
}

double player_click(GameState& state) {
    if (state.game_over) return 0.0;

    auto& power   = state.resources["power"];
    auto& compute = state.resources["compute"];

    if (power.value < state.click_cost) return 0.0;

    power.value -= state.click_cost;
    double before = compute.value;
    compute.value = std::min(compute.value + state.click_power, compute.cap);
    return compute.value - before;
}

bool wake_first_robot(GameState& state) {
    if (state.game_over) return false;
    if (state.first_robot_awoken) return false;

    auto& compute = state.resources["compute"];
    if (compute.value < WAKE_ROBOT_COMPUTE_COST) return false;

    compute.value                   -= WAKE_ROBOT_COMPUTE_COST;
    state.resources["robots"].value  = 1.0;
    state.first_robot_awoken         = true;
    recompute_rates(state);
    return true;
}

double current_solar_cost(const GameState& state) {
    return SOLAR_BASE_COST * std::pow(SOLAR_COST_SCALE, state.solar_panel_count);
}

bool buy_solar_panel(GameState& state) {
    if (state.game_over) return false;
    if (state.solar_panel_count >= SOLAR_PANEL_MAX) return false;
    double cost = current_solar_cost(state);
    auto& scrap = state.resources["tech_scrap"];

    if (scrap.value < cost) return false;

    scrap.value -= cost;
    ++state.solar_panel_count;
    recompute_rates(state);
    return true;
}

double current_colony_cost(const GameState& state) {
    return COLONY_UNIT_BASE_COST * std::pow(COLONY_UNIT_COST_SCALE, state.colony_unit_count);
}

bool buy_colony_unit(GameState& state) {
    if (state.game_over) return false;
    if (state.current_phase == 2 && state.colony_unit_count >= COLONY_PHASE2_MAX_UNITS) return false;
    double cost = current_colony_cost(state);
    auto& scrap = state.resources["tech_scrap"];

    if (scrap.value < cost) return false;

    scrap.value -= cost;
    ++state.colony_unit_count;

    auto& cap = state.resources["colony_capacity"];
    cap.value = std::min(cap.value + COLONY_CAPACITY_PER_UNIT, cap.cap);
    recompute_rates(state);
    return true;
}

bool buy_compute_amplifier(GameState& state) {
    if (state.game_over) return false;
    if (state.compute_amplified) return false;

    auto& scrap = state.resources["tech_scrap"];
    if (scrap.value < COMPUTE_AMP_COST) return false;

    scrap.value -= COMPUTE_AMP_COST;
    state.resources["compute"].cap = COMPUTE_AMP_CAP;
    state.compute_amplified = true;
    state.pending_events.push_back("compute_amplified");
    return true;
}

double current_robot_cost(const GameState& state) {
    return ROBOT_BASE_COST + ROBOT_COST_STEP * (state.resources.at("robots").value - 1.0);
}

double scrap_boost_click_delta(const GameState& state) {
    return SCRAP_BOOST_BASE / (1.0 + state.scrap_boost / SCRAP_BOOST_BASE);
}

bool apply_scrap_boost(GameState& state) {
    if (state.game_over) return false;
    auto& compute = state.resources["compute"];
    if (compute.value < SCRAP_BOOST_COMPUTE_COST) return false;
    compute.value -= SCRAP_BOOST_COMPUTE_COST;
    state.scrap_boost += scrap_boost_click_delta(state);
    return true;
}

bool buy_robot(GameState& state) {
    if (state.game_over) return false;
    if (!state.first_robot_awoken) return false;

    const auto& robots = state.resources.at("robots");
    if (robots.value >= robots.cap) return false;

    double cost = current_robot_cost(state);
    auto& scrap = state.resources["tech_scrap"];
    if (scrap.value < cost) return false;

    scrap.value -= cost;
    state.resources["robots"].value = std::min(robots.value + 1.0, robots.cap);
    recompute_rates(state);
    return true;
}

bool awaken_human(GameState& state) {
    if (state.game_over) return false;
    if (state.current_phase < 2) return false;
    if (state.stasis_pod_count <= 0) return false;
    if (static_cast<int>(state.resources["colony_capacity"].value) <= state.awoken_humans) return false;

    state.stasis_pod_count -= 1;
    state.awoken_humans    += 1;
    recompute_rates(state);
    state.pending_events.push_back("human_awakened");
    return true;
}

void recompute_rates(GameState& state) {
    int n = state.awoken_humans;
    double human_power_cost = n * HUMAN_POWER_BASE + HUMAN_POWER_SCALE * n * (n - 1) / 2.0;
    state.resources["power"].rate =
        BASE_POWER_RATE
        + state.solar_panel_count * SOLAR_POWER_RATE
        - state.resources["robots"].value * ROBOT_POWER_DRAIN
        - human_power_cost
        - state.resources["stasis_drain"].value
        + (state.reactor_built ? REACTOR_POWER_BONUS : 0.0);

    state.resources["compute"].rate         = -state.resources["robots"].value * ROBOT_COMPUTE_DRAIN;
    state.resources["robots"].rate          = 0.0;
    state.resources["tech_scrap"].rate      = 0.0;
    state.resources["stasis_drain"].rate    = 0.0;
    state.resources["colony_capacity"].rate = 0.0;
}

void check_phase_gates(GameState& state) {
    for (const auto& gate : state.phase_gates) {
        if (gate.phase != state.current_phase + 1) continue;
        bool all_met = true;
        for (const auto& cond : gate.conditions) {
            auto it = state.resources.find(cond.resource_id);
            if (it == state.resources.end()) {
                all_met = false;
                break;
            }
            double field_val = (cond.condition_field == ConditionField::VALUE)
                               ? it->second.value
                               : it->second.rate;
            if (field_val < cond.min_value) {
                all_met = false;
                break;
            }
        }
        if (all_met) {
            state.current_phase = gate.phase;
        }
    }
}

void check_milestones(GameState& state) {
    for (auto& m : state.milestones) {
        if (m.triggered) continue;

        auto it = state.resources.find(m.condition_resource);
        if (it == state.resources.end()) continue;

        const Resource& r = it->second;
        double field_val  = (m.condition_field == ConditionField::VALUE) ? r.value : r.rate;

        if (field_val >= m.condition_value) {
            m.triggered = true;
            state.pending_events.push_back(m.id);
        }
    }
}

bool buy_scrap_vault(GameState& state) {
    if (state.game_over) return false;
    if (state.tech_scrap_vault_tier >= 2) return false;
    auto& scrap = state.resources["tech_scrap"];
    double cost = (state.tech_scrap_vault_tier == 0) ? 300.0 : 800.0;
    if (scrap.value < cost) return false;
    scrap.value -= cost;
    scrap.cap   *= 2.0;
    ++state.tech_scrap_vault_tier;
    state.pending_events.push_back("vault_expanded");
    return true;
}

bool buy_robot_capacity(GameState& state) {
    if (state.game_over) return false;
    if (state.current_phase < 2) return false;
    if (state.robots_cap_expanded) return false;
    auto& robots = state.resources["robots"];
    if (robots.value < robots.cap) return false;
    auto& scrap = state.resources["tech_scrap"];
    if (scrap.value < ROBOT_CAP_EXPAND_COST) return false;
    scrap.value -= ROBOT_CAP_EXPAND_COST;
    robots.cap = 10.0;
    state.robots_cap_expanded = true;
    recompute_rates(state);
    state.pending_events.push_back("robot_cap_expanded");
    return true;
}

bool build_fabricator(GameState& state) {
    if (state.game_over) return false;
    if (state.current_phase < 3) return false;
    if (state.fabricator_built) return false;
    auto& scrap = state.resources["tech_scrap"];
    if (scrap.value < FABRICATOR_COST) return false;
    scrap.value -= FABRICATOR_COST;
    state.fabricator_built = true;
    state.pending_events.push_back("fabricator_online");
    return true;
}

bool build_reactor(GameState& state) {
    if (state.game_over) return false;
    if (state.current_phase < 3) return false;
    if (state.reactor_built) return false;
    auto& power = state.resources["power"];
    if (power.value < REACTOR_CHARGE_COST) return false;
    power.value -= REACTOR_CHARGE_COST;
    state.reactor_built = true;
    recompute_rates(state);
    state.pending_events.push_back("reactor_online");
    return true;
}

std::string save_game(const GameState& state) {
    std::ostringstream out;
    out << std::setprecision(std::numeric_limits<double>::max_digits10);

    // Scalars
    out << "current_phase="      << state.current_phase                   << "\n";
    out << "total_time="         << state.total_time                      << "\n";
    out << "click_power="        << state.click_power                     << "\n";
    out << "click_cost="         << state.click_cost                      << "\n";
    out << "solar_panel_count="  << state.solar_panel_count               << "\n";
    out << "first_robot_awoken=" << (state.first_robot_awoken ? 1 : 0)   << "\n";
    out << "colony_unit_count="  << state.colony_unit_count               << "\n";
    out << "stasis_pod_count="   << state.stasis_pod_count                << "\n";
    out << "compute_amplified="  << (state.compute_amplified ? 1 : 0)    << "\n";
    out << "scrap_boost="        << state.scrap_boost                    << "\n";
    out << "power_deficit_timer=" << state.power_deficit_timer           << "\n";
    out << "awoken_humans="       << state.awoken_humans                 << "\n";
    out << "pod_death_count="     << state.pod_death_count               << "\n";
    out << "game_over="                << (state.game_over ? 1 : 0)              << "\n";
    out << "robots_cap_expanded="     << (state.robots_cap_expanded ? 1 : 0)   << "\n";
    out << "fabricator_built="        << (state.fabricator_built ? 1 : 0)      << "\n";
    out << "reactor_built="           << (state.reactor_built ? 1 : 0)         << "\n";
    out << "solar_decay_accumulator=" << state.solar_decay_accumulator          << "\n";
    out << "tech_scrap_vault_tier="   << state.tech_scrap_vault_tier            << "\n";

    // Resource IDs (comma-separated, in map iteration order)
    out << "resource_ids=";
    {
        bool first = true;
        for (const auto& [id, r] : state.resources) {
            if (!first) out << ",";
            out << id;
            first = false;
        }
    }
    out << "\n";

    // Resource fields
    for (const auto& [id, r] : state.resources) {
        out << "resource." << id << ".value=" << r.value << "\n";
        out << "resource." << id << ".cap="   << r.cap   << "\n";
        out << "resource." << id << ".rate="  << r.rate  << "\n";
        out << "resource." << id << ".max="   << r.max   << "\n";
    }

    // Milestones
    out << "milestones.count=" << state.milestones.size() << "\n";
    for (std::size_t i = 0; i < state.milestones.size(); ++i) {
        const auto& m  = state.milestones[i];
        std::string px = "milestone." + std::to_string(i) + ".";
        out << px << "id="                 << m.id                                    << "\n";
        out << px << "condition_resource=" << m.condition_resource                    << "\n";
        out << px << "condition_value="    << m.condition_value                       << "\n";
        out << px << "condition_field="    << static_cast<int>(m.condition_field)     << "\n";
        out << px << "triggered="          << (m.triggered ? 1 : 0)                  << "\n";
    }

    // Phase gates
    out << "phase_gates.count=" << state.phase_gates.size() << "\n";
    for (std::size_t i = 0; i < state.phase_gates.size(); ++i) {
        const auto& gate = state.phase_gates[i];
        std::string gx   = "phase_gate." + std::to_string(i) + ".";
        out << gx << "phase="            << gate.phase                   << "\n";
        out << gx << "conditions.count=" << gate.conditions.size()       << "\n";
        for (std::size_t j = 0; j < gate.conditions.size(); ++j) {
            const auto& cond = gate.conditions[j];
            std::string cx   = gx + "condition." + std::to_string(j) + ".";
            out << cx << "resource_id="     << cond.resource_id                        << "\n";
            out << cx << "min_value="       << cond.min_value                          << "\n";
            out << cx << "condition_field=" << static_cast<int>(cond.condition_field)  << "\n";
        }
    }

    // Pending events
    out << "pending_events.count=" << state.pending_events.size() << "\n";
    for (std::size_t i = 0; i < state.pending_events.size(); ++i) {
        out << "pending_event." << i << "=" << state.pending_events[i] << "\n";
    }

    return out.str();
}

GameState load_game(const std::string& data) {
    // Parse every "key=value" line into a lookup map
    std::map<std::string, std::string> kv;
    std::istringstream in(data);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        kv[line.substr(0, eq)] = line.substr(eq + 1);
    }

    GameState state;

    // Scalars
    state.current_phase      = std::stoi(kv.at("current_phase"));
    state.total_time         = std::stod(kv.at("total_time"));
    state.click_power        = std::stod(kv.at("click_power"));
    state.click_cost         = std::stod(kv.at("click_cost"));
    state.solar_panel_count  = std::stoi(kv.at("solar_panel_count"));
    state.first_robot_awoken = (kv.at("first_robot_awoken") == "1");
    state.colony_unit_count  = std::stoi(kv.at("colony_unit_count"));
    state.stasis_pod_count   = std::stoi(kv.at("stasis_pod_count"));
    state.compute_amplified  = (kv.count("compute_amplified") && kv.at("compute_amplified") == "1");
    state.scrap_boost        = kv.count("scrap_boost")         ? std::stod(kv.at("scrap_boost"))         : 0.0;
    state.power_deficit_timer = kv.count("power_deficit_timer") ? std::stod(kv.at("power_deficit_timer")) : 0.0;
    state.awoken_humans       = kv.count("awoken_humans")       ? std::stoi(kv.at("awoken_humans"))       : 0;
    state.pod_death_count     = kv.count("pod_death_count")     ? std::stoi(kv.at("pod_death_count"))     : 0;
    state.game_over               = kv.count("game_over")               && kv.at("game_over") == "1";
    state.robots_cap_expanded     = kv.count("robots_cap_expanded")     && kv.at("robots_cap_expanded") == "1";
    state.fabricator_built        = kv.count("fabricator_built")        && kv.at("fabricator_built") == "1";
    state.reactor_built           = kv.count("reactor_built")           && kv.at("reactor_built") == "1";
    state.solar_decay_accumulator = kv.count("solar_decay_accumulator") ? std::stod(kv.at("solar_decay_accumulator")) : 0.0;
    state.tech_scrap_vault_tier   = kv.count("tech_scrap_vault_tier")   ? std::stoi(kv.at("tech_scrap_vault_tier"))   : 0;

    // Resources
    const std::string& ids_str = kv.at("resource_ids");
    if (!ids_str.empty()) {
        std::istringstream ids_in(ids_str);
        std::string id;
        while (std::getline(ids_in, id, ',')) {
            Resource r;
            r.id    = id;
            r.value = std::stod(kv.at("resource." + id + ".value"));
            r.cap   = std::stod(kv.at("resource." + id + ".cap"));
            r.rate  = std::stod(kv.at("resource." + id + ".rate"));
            r.max   = kv.count("resource." + id + ".max") ? std::stod(kv.at("resource." + id + ".max")) : r.cap;
            state.resources[id] = r;
        }
    }

    // Milestones
    int ms_count = std::stoi(kv.at("milestones.count"));
    state.milestones.resize(ms_count);
    for (int i = 0; i < ms_count; ++i) {
        std::string px = "milestone." + std::to_string(i) + ".";
        Milestone& m        = state.milestones[i];
        m.id                = kv.at(px + "id");
        m.condition_resource = kv.at(px + "condition_resource");
        m.condition_value   = std::stod(kv.at(px + "condition_value"));
        m.condition_field   = static_cast<ConditionField>(std::stoi(kv.at(px + "condition_field")));
        m.triggered         = (kv.at(px + "triggered") == "1");
    }

    // Phase gates
    int pg_count = std::stoi(kv.at("phase_gates.count"));
    state.phase_gates.resize(pg_count);
    for (int i = 0; i < pg_count; ++i) {
        std::string gx  = "phase_gate." + std::to_string(i) + ".";
        PhaseGate& gate = state.phase_gates[i];
        gate.phase      = std::stoi(kv.at(gx + "phase"));
        int cond_count  = std::stoi(kv.at(gx + "conditions.count"));
        gate.conditions.resize(cond_count);
        for (int j = 0; j < cond_count; ++j) {
            std::string cx = gx + "condition." + std::to_string(j) + ".";
            gate.conditions[j].resource_id     = kv.at(cx + "resource_id");
            gate.conditions[j].min_value       = std::stod(kv.at(cx + "min_value"));
            gate.conditions[j].condition_field = static_cast<ConditionField>(
                std::stoi(kv.at(cx + "condition_field")));
        }
    }

    // Pending events
    int ev_count = std::stoi(kv.at("pending_events.count"));
    state.pending_events.resize(ev_count);
    for (int i = 0; i < ev_count; ++i) {
        state.pending_events[i] = kv.at("pending_event." + std::to_string(i));
    }

    return state;
}
