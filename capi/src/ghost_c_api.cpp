#include "ghost_c_api.h"

#include "global_constraints/all_different.hpp"
#include "global_constraints/linear_equation_eq.hpp"
#include "global_constraints/linear_equation_geq.hpp"
#include "global_constraints/linear_equation_leq.hpp"
#include "model_builder.hpp"
#include "objective.hpp"
#include "options.hpp"
#include "solver.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

namespace {

struct VariableSpec {
    std::vector<int> domain;
    std::string name;
    int start_index = 0;
};

enum class ConstraintKind { linear_eq, linear_le, linear_ge, all_different };

struct ConstraintSpec {
    ConstraintKind kind;
    std::vector<int> variable_ids;
    std::vector<double> coefficients;
    double rhs = 0.0;
};

struct ObjectiveSpec {
    bool present = false;
    bool maximize = false;
    std::vector<int> variable_ids;
    std::vector<double> coefficients;
    double constant = 0.0;
};

struct LinearMinimize final : ghost::Minimize {
    std::vector<double> coefficients;
    double constant;

    LinearMinimize(
        const std::vector<int> &variable_ids,
        std::vector<double> coefficients_,
        double constant_
    ) : ghost::Minimize(variable_ids, "C API linear minimization"),
        coefficients(std::move(coefficients_)),
        constant(constant_) {}

    double required_cost(const std::vector<ghost::Variable *> &variables) const override {
        double value = constant;
        for (std::size_t i = 0; i < variables.size(); ++i) {
            value += coefficients[i] * variables[i]->get_value();
        }
        return value;
    }
};

struct LinearMaximize final : ghost::Maximize {
    std::vector<double> coefficients;
    double constant;

    LinearMaximize(
        const std::vector<int> &variable_ids,
        std::vector<double> coefficients_,
        double constant_
    ) : ghost::Maximize(variable_ids, "C API linear maximization"),
        coefficients(std::move(coefficients_)),
        constant(constant_) {}

    double required_cost(const std::vector<ghost::Variable *> &variables) const override {
        double value = constant;
        for (std::size_t i = 0; i < variables.size(); ++i) {
            value += coefficients[i] * variables[i]->get_value();
        }
        return value;
    }
};

class DynamicModelBuilder final : public ghost::ModelBuilder {
    std::vector<VariableSpec> variable_specs_;
    std::vector<ConstraintSpec> constraint_specs_;
    ObjectiveSpec objective_spec_;

public:
    DynamicModelBuilder(
        bool permutation_problem,
        const std::vector<VariableSpec> &variables,
        const std::vector<ConstraintSpec> &constraints,
        const ObjectiveSpec &objective
    ) : ghost::ModelBuilder(permutation_problem),
        variable_specs_(variables),
        constraint_specs_(constraints),
        objective_spec_(objective) {}

    void declare_variables() override {
        for (const auto &spec : variable_specs_) {
            if (spec.domain.size() == 1) {
                auto expanded_domain = spec.domain;
                const int fixed_value = spec.domain.front();
                expanded_domain.push_back(
                    fixed_value == std::numeric_limits<int>::max()
                        ? fixed_value - 1
                        : fixed_value + 1);
                std::sort(expanded_domain.begin(), expanded_domain.end());
                const int start_index = expanded_domain.front() == fixed_value ? 0 : 1;
                create_variable(expanded_domain, spec.name, start_index);
            } else {
                create_variable(spec.domain, spec.name, spec.start_index);
            }
        }
        // The current local-search kernel assumes at least two model variables.
        // Keep this implementation detail outside the public C model.
        if (variable_specs_.size() == 1) {
            create_variable(std::vector<int>{0, 1}, "__ghost_c_dummy", 0);
        }
    }

    void declare_constraints() override {
        using namespace ghost::global_constraints;
        // GHOST's local-search neighborhood assumes at least two values. Model
        // singleton domains as a two-value internal domain plus an exact fix.
        for (std::size_t id = 0; id < variable_specs_.size(); ++id) {
            if (variable_specs_[id].domain.size() == 1) {
                constraints.emplace_back(std::make_shared<LinearEquationEq>(
                    std::vector<int>{static_cast<int>(id)},
                    static_cast<double>(variable_specs_[id].domain.front()),
                    std::vector<double>{1.0}));
            }
        }
        for (const auto &spec : constraint_specs_) {
            switch (spec.kind) {
            case ConstraintKind::linear_eq:
                constraints.emplace_back(std::make_shared<LinearEquationEq>(
                    spec.variable_ids, spec.rhs, spec.coefficients));
                break;
            case ConstraintKind::linear_le:
                constraints.emplace_back(std::make_shared<LinearEquationLeq>(
                    spec.variable_ids, spec.rhs, spec.coefficients));
                break;
            case ConstraintKind::linear_ge:
                constraints.emplace_back(std::make_shared<LinearEquationGeq>(
                    spec.variable_ids, spec.rhs, spec.coefficients));
                break;
            case ConstraintKind::all_different:
                constraints.emplace_back(
                    std::make_shared<AllDifferent>(spec.variable_ids));
                break;
            }
        }
    }

    void declare_objective() override {
        if (!objective_spec_.present) {
            ghost::ModelBuilder::declare_objective();
        } else if (objective_spec_.maximize) {
            objective = std::make_shared<LinearMaximize>(
                objective_spec_.variable_ids,
                objective_spec_.coefficients,
                objective_spec_.constant);
        } else {
            objective = std::make_shared<LinearMinimize>(
                objective_spec_.variable_ids,
                objective_spec_.coefficients,
                objective_spec_.constant);
        }
    }
};

} // namespace

struct GhostSessionHandle_t {
    bool permutation_problem = false;
    std::vector<VariableSpec> variables;
    std::vector<ConstraintSpec> constraints;
    ObjectiveSpec objective;
    std::string last_error;
    std::string raw_status = "OPTIMIZE_NOT_CALLED";
    GhostSolutionStatus solution_status = GHOST_SOLUTION_STATUS_UNKNOWN;
    std::vector<int> solution;
    double objective_value = std::numeric_limits<double>::quiet_NaN();
    double best_candidate_error = std::numeric_limits<double>::quiet_NaN();
    double solve_time_seconds = 0.0;
};

struct GhostOptionsHandle_t {
    ghost::Options options;
};

namespace {

void clear_result(GhostSessionHandle handle) {
    handle->raw_status = "OPTIMIZE_NOT_CALLED";
    handle->solution_status = GHOST_SOLUTION_STATUS_UNKNOWN;
    handle->solution.clear();
    handle->objective_value = std::numeric_limits<double>::quiet_NaN();
    handle->best_candidate_error = std::numeric_limits<double>::quiet_NaN();
    handle->solve_time_seconds = 0.0;
}

bool valid_variable_id(const GhostSessionHandle handle, int id) {
    return id >= 0 && static_cast<std::size_t>(id) < handle->variables.size();
}

bool validate_scope(
    GhostSessionHandle handle,
    const int *variable_ids,
    std::size_t number_variables,
    const double *coefficients,
    bool require_coefficients
) {
    if (number_variables == 0 || variable_ids == nullptr ||
        (require_coefficients && coefficients == nullptr)) {
        handle->last_error = "A non-empty variable scope is required.";
        return false;
    }
    for (std::size_t i = 0; i < number_variables; ++i) {
        if (!valid_variable_id(handle, variable_ids[i])) {
            handle->last_error = "A variable ID is outside the current session.";
            return false;
        }
        if (coefficients != nullptr && !std::isfinite(coefficients[i])) {
            handle->last_error = "Linear coefficients must be finite.";
            return false;
        }
    }
    return true;
}

int add_linear_constraint(
    GhostSessionHandle handle,
    ConstraintKind kind,
    const int *variable_ids,
    const double *coefficients,
    std::size_t number_variables,
    double rhs
) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (!std::isfinite(rhs) ||
        !validate_scope(handle, variable_ids, number_variables, coefficients, false)) {
        if (!std::isfinite(rhs)) {
            handle->last_error = "The right-hand side must be finite.";
        }
        return GHOST_ERROR_INVALID_ARG;
    }
    try {
        ConstraintSpec spec;
        spec.kind = kind;
        spec.variable_ids.assign(variable_ids, variable_ids + number_variables);
        if (coefficients == nullptr) {
            spec.coefficients.assign(number_variables, 1.0);
        } else {
            spec.coefficients.assign(coefficients, coefficients + number_variables);
        }
        spec.rhs = rhs;
        handle->constraints.emplace_back(std::move(spec));
        clear_result(handle);
        return static_cast<int>(handle->constraints.size() - 1);
    } catch (const std::bad_alloc &) {
        handle->last_error = "Unable to allocate a linear constraint.";
        return GHOST_ERROR_MEMORY;
    }
}

template <typename Setter>
GhostStatus set_option(GhostOptionsHandle handle, Setter &&setter) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    setter(handle->options);
    return GHOST_SUCCESS;
}

} // namespace

extern "C" {

GhostSessionHandle ghost_create_session(bool permutation_problem) {
    try {
        auto *handle = new GhostSessionHandle_t;
        handle->permutation_problem = permutation_problem;
        return handle;
    } catch (...) {
        return nullptr;
    }
}

void ghost_destroy_session(GhostSessionHandle handle) {
    delete handle;
}

const char *ghost_get_last_error(GhostSessionHandle handle) {
    return handle == nullptr ? nullptr : handle->last_error.c_str();
}

int ghost_add_variable(
    GhostSessionHandle handle,
    int min_value,
    int max_value,
    const char *name
) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (max_value < min_value) {
        handle->last_error = "The upper bound must be at least the lower bound.";
        return GHOST_ERROR_INVALID_ARG;
    }
    const auto size = static_cast<std::uint64_t>(
        static_cast<std::int64_t>(max_value) - static_cast<std::int64_t>(min_value)) + 1;
    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        handle->last_error = "The variable domain is too large.";
        return GHOST_ERROR_INVALID_ARG;
    }
    try {
        VariableSpec spec;
        spec.domain.reserve(static_cast<std::size_t>(size));
        for (std::int64_t value = min_value; value <= static_cast<std::int64_t>(max_value); ++value) {
            spec.domain.push_back(static_cast<int>(value));
        }
        spec.name = name == nullptr ? "" : name;
        handle->variables.emplace_back(std::move(spec));
        clear_result(handle);
        return static_cast<int>(handle->variables.size() - 1);
    } catch (const std::bad_alloc &) {
        handle->last_error = "Unable to allocate a variable domain.";
        return GHOST_ERROR_MEMORY;
    }
}

int ghost_add_variable_domain(
    GhostSessionHandle handle,
    const int *domain_values,
    size_t domain_size,
    const char *name
) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (domain_size == 0 || domain_values == nullptr) {
        handle->last_error = "A variable domain must be non-empty.";
        return GHOST_ERROR_INVALID_ARG;
    }
    try {
        VariableSpec spec;
        spec.domain.assign(domain_values, domain_values + domain_size);
        std::sort(spec.domain.begin(), spec.domain.end());
        spec.domain.erase(std::unique(spec.domain.begin(), spec.domain.end()), spec.domain.end());
        spec.name = name == nullptr ? "" : name;
        handle->variables.emplace_back(std::move(spec));
        clear_result(handle);
        return static_cast<int>(handle->variables.size() - 1);
    } catch (const std::bad_alloc &) {
        handle->last_error = "Unable to allocate a variable domain.";
        return GHOST_ERROR_MEMORY;
    }
}

GhostStatus ghost_set_variable_start(GhostSessionHandle handle, int variable_id, int value) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (!valid_variable_id(handle, variable_id)) {
        handle->last_error = "The variable ID is outside the current session.";
        return GHOST_ERROR_INVALID_ID;
    }
    auto &spec = handle->variables[static_cast<std::size_t>(variable_id)];
    const auto iterator = std::find(spec.domain.begin(), spec.domain.end(), value);
    if (iterator == spec.domain.end()) {
        handle->last_error = "The starting value is outside the variable domain.";
        return GHOST_ERROR_INVALID_ARG;
    }
    spec.start_index = static_cast<int>(std::distance(spec.domain.begin(), iterator));
    clear_result(handle);
    return GHOST_SUCCESS;
}

int ghost_add_linear_eq_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double rhs
) {
    return add_linear_constraint(
        handle, ConstraintKind::linear_eq, variable_ids, coefficients, number_variables, rhs);
}

int ghost_add_linear_le_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double rhs
) {
    return add_linear_constraint(
        handle, ConstraintKind::linear_le, variable_ids, coefficients, number_variables, rhs);
}

int ghost_add_linear_ge_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double rhs
) {
    return add_linear_constraint(
        handle, ConstraintKind::linear_ge, variable_ids, coefficients, number_variables, rhs);
}

int ghost_add_alldifferent_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    size_t number_variables
) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (!validate_scope(handle, variable_ids, number_variables, nullptr, false)) {
        return GHOST_ERROR_INVALID_ARG;
    }
    try {
        ConstraintSpec spec;
        spec.kind = ConstraintKind::all_different;
        spec.variable_ids.assign(variable_ids, variable_ids + number_variables);
        handle->constraints.emplace_back(std::move(spec));
        clear_result(handle);
        return static_cast<int>(handle->constraints.size() - 1);
    } catch (const std::bad_alloc &) {
        handle->last_error = "Unable to allocate an AllDifferent constraint.";
        return GHOST_ERROR_MEMORY;
    }
}

GhostStatus ghost_set_linear_objective(
    GhostSessionHandle handle,
    bool maximize,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double constant
) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (!std::isfinite(constant) ||
        !validate_scope(handle, variable_ids, number_variables, coefficients, true)) {
        if (!std::isfinite(constant)) {
            handle->last_error = "The objective constant must be finite.";
        }
        return GHOST_ERROR_INVALID_ARG;
    }
    try {
        handle->objective.present = true;
        handle->objective.maximize = maximize;
        handle->objective.variable_ids.assign(variable_ids, variable_ids + number_variables);
        handle->objective.coefficients.assign(coefficients, coefficients + number_variables);
        handle->objective.constant = constant;
        clear_result(handle);
        return GHOST_SUCCESS;
    } catch (const std::bad_alloc &) {
        handle->last_error = "Unable to allocate the objective.";
        return GHOST_ERROR_MEMORY;
    }
}

GhostOptionsHandle ghost_create_options(void) {
    try {
        return new GhostOptionsHandle_t;
    } catch (...) {
        return nullptr;
    }
}

void ghost_destroy_options(GhostOptionsHandle handle) {
    delete handle;
}

GhostStatus ghost_set_option_parallel(GhostOptionsHandle handle, bool value) {
    return set_option(handle, [value](auto &options) { options.parallel_runs = value; });
}

GhostStatus ghost_set_option_num_threads(GhostOptionsHandle handle, int value) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (value < 1) {
        return GHOST_ERROR_INVALID_ARG;
    }
    handle->options.number_threads = value;
    return GHOST_SUCCESS;
}

GhostStatus ghost_set_option_custom_starting_point(GhostOptionsHandle handle, bool value) {
    return set_option(handle, [value](auto &options) { options.custom_starting_point = value; });
}

GhostStatus ghost_set_option_resume_search(GhostOptionsHandle handle, bool value) {
    return set_option(handle, [value](auto &options) { options.resume_search = value; });
}

GhostStatus ghost_set_option_optimization_guidance(GhostOptionsHandle handle, bool value) {
    return set_option(handle, [value](auto &options) { options.enable_optimization_guidance = value; });
}

GhostStatus ghost_set_option_tabu_time_local_min(GhostOptionsHandle handle, int value) {
    return set_option(handle, [value](auto &options) { options.tabu_time_local_min = value; });
}

GhostStatus ghost_set_option_tabu_time_selected(GhostOptionsHandle handle, int value) {
    return set_option(handle, [value](auto &options) { options.tabu_time_selected = value; });
}

GhostStatus ghost_set_option_plateau_probability(GhostOptionsHandle handle, int value) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (value < 0 || value > 100) {
        return GHOST_ERROR_INVALID_ARG;
    }
    handle->options.percent_chance_force_trying_on_plateau = value;
    return GHOST_SUCCESS;
}

GhostStatus ghost_set_option_reset_threshold(GhostOptionsHandle handle, int value) {
    return set_option(handle, [value](auto &options) { options.reset_threshold = value; });
}

GhostStatus ghost_set_option_restart_threshold(GhostOptionsHandle handle, int value) {
    return set_option(handle, [value](auto &options) { options.restart_threshold = value; });
}

GhostStatus ghost_set_option_variables_to_reset(GhostOptionsHandle handle, int value) {
    return set_option(handle, [value](auto &options) { options.number_variables_to_reset = value; });
}

GhostStatus ghost_set_option_start_samplings(GhostOptionsHandle handle, int value) {
    return set_option(handle, [value](auto &options) { options.number_start_samplings = value; });
}

GhostStatus ghost_solve(
    GhostSessionHandle handle,
    GhostOptionsHandle options_handle,
    double timeout_microseconds
) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (handle->variables.empty() || !std::isfinite(timeout_microseconds) || timeout_microseconds <= 0.0) {
        handle->last_error = "Solving requires variables and a finite positive timeout.";
        return GHOST_ERROR_INVALID_ARG;
    }
    clear_result(handle);
    const auto started = std::chrono::steady_clock::now();
    try {
        DynamicModelBuilder builder(
            handle->permutation_problem,
            handle->variables,
            handle->constraints,
            handle->objective);
        ghost::Solver solver(builder);
        ghost::Options options = options_handle == nullptr
            ? ghost::Options()
            : options_handle->options;
        double final_cost = std::numeric_limits<double>::quiet_NaN();
        std::vector<int> final_solution;
        const bool found = solver.fast_search(
            final_cost, final_solution, timeout_microseconds, options);
        handle->solve_time_seconds = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - started).count();
        handle->solution = std::move(final_solution);
        handle->solution.resize(handle->variables.size());
        if (found && handle->objective.present) {
            handle->solution_status = GHOST_SOLUTION_STATUS_FEASIBLE;
            handle->objective_value = final_cost;
            handle->best_candidate_error = 0.0;
            handle->raw_status = "FEASIBLE_SOLUTION_FOUND; HEURISTIC_TIME_LIMIT_REACHED";
            return GHOST_FEASIBLE_FOUND;
        }
        if (found) {
            handle->solution_status = GHOST_SOLUTION_STATUS_SAT;
            handle->best_candidate_error = final_cost;
            handle->raw_status = "SATISFYING_SOLUTION_FOUND";
            return GHOST_SAT_FOUND;
        }
        handle->solution_status = GHOST_SOLUTION_STATUS_NO_SOLUTION;
        if (!handle->objective.present) {
            handle->best_candidate_error = final_cost;
        }
        handle->raw_status = "TIME_LIMIT_REACHED; NO_FEASIBLE_SOLUTION_FOUND";
        return GHOST_TIME_LIMIT;
    } catch (const std::bad_alloc &) {
        handle->last_error = "GHOST ran out of memory while solving.";
        handle->raw_status = "MEMORY_ERROR";
        return GHOST_ERROR_MEMORY;
    } catch (const std::exception &error) {
        handle->last_error = error.what();
        handle->raw_status = "SOLVER_ERROR";
        return GHOST_ERROR_SOLVER;
    } catch (...) {
        handle->last_error = "Unknown exception raised by GHOST.";
        handle->raw_status = "UNKNOWN_SOLVER_ERROR";
        return GHOST_ERROR_UNKNOWN;
    }
}

GhostSolutionStatus ghost_get_solution_status(GhostSessionHandle handle) {
    return handle == nullptr ? GHOST_SOLUTION_STATUS_UNKNOWN : handle->solution_status;
}

const char *ghost_get_raw_status_string(GhostSessionHandle handle) {
    return handle == nullptr ? nullptr : handle->raw_status.c_str();
}

size_t ghost_get_num_variables(GhostSessionHandle handle) {
    return handle == nullptr ? 0 : handle->variables.size();
}

GhostStatus ghost_get_variable_value(GhostSessionHandle handle, int variable_id, int *value) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (value == nullptr) {
        handle->last_error = "The output value pointer is null.";
        return GHOST_ERROR_INVALID_ARG;
    }
    if (handle->solution_status != GHOST_SOLUTION_STATUS_SAT &&
        handle->solution_status != GHOST_SOLUTION_STATUS_FEASIBLE &&
        handle->solution_status != GHOST_SOLUTION_STATUS_OPTIMAL) {
        handle->last_error = "No feasible solution is available.";
        return GHOST_ERROR_API_USAGE;
    }
    if (variable_id < 0 || static_cast<std::size_t>(variable_id) >= handle->solution.size()) {
        handle->last_error = "The variable ID is outside the solution.";
        return GHOST_ERROR_INVALID_ID;
    }
    *value = handle->solution[static_cast<std::size_t>(variable_id)];
    return GHOST_SUCCESS;
}

GhostStatus ghost_get_variable_values(
    GhostSessionHandle handle,
    int *values,
    size_t buffer_size
) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (values == nullptr || buffer_size < handle->solution.size()) {
        handle->last_error = "The solution buffer is null or too small.";
        return GHOST_ERROR_INVALID_ARG;
    }
    if (handle->solution_status != GHOST_SOLUTION_STATUS_SAT &&
        handle->solution_status != GHOST_SOLUTION_STATUS_FEASIBLE &&
        handle->solution_status != GHOST_SOLUTION_STATUS_OPTIMAL) {
        handle->last_error = "No feasible solution is available.";
        return GHOST_ERROR_API_USAGE;
    }
    std::copy(handle->solution.begin(), handle->solution.end(), values);
    return GHOST_SUCCESS;
}

GhostStatus ghost_get_objective_value(GhostSessionHandle handle, double *value) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (value == nullptr) {
        handle->last_error = "The objective output pointer is null.";
        return GHOST_ERROR_INVALID_ARG;
    }
    if (!handle->objective.present || !std::isfinite(handle->objective_value)) {
        handle->last_error = "No feasible objective value is available.";
        return GHOST_ERROR_API_USAGE;
    }
    *value = handle->objective_value;
    return GHOST_SUCCESS;
}

GhostStatus ghost_get_best_candidate_error(GhostSessionHandle handle, double *value) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (value == nullptr) {
        handle->last_error = "The candidate-error output pointer is null.";
        return GHOST_ERROR_INVALID_ARG;
    }
    if (!std::isfinite(handle->best_candidate_error)) {
        handle->last_error = "No candidate error is available for this result.";
        return GHOST_ERROR_API_USAGE;
    }
    *value = handle->best_candidate_error;
    return GHOST_SUCCESS;
}

GhostStatus ghost_get_solve_time(GhostSessionHandle handle, double *seconds) {
    if (handle == nullptr) {
        return GHOST_ERROR_NULL_HANDLE;
    }
    if (seconds == nullptr) {
        handle->last_error = "The solve-time output pointer is null.";
        return GHOST_ERROR_INVALID_ARG;
    }
    if (handle->solution_status == GHOST_SOLUTION_STATUS_UNKNOWN) {
        handle->last_error = "The solver has not been called.";
        return GHOST_ERROR_API_USAGE;
    }
    *seconds = handle->solve_time_seconds;
    return GHOST_SUCCESS;
}

} // extern "C"
