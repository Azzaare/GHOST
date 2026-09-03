#ifndef GHOST_C_API_H
#define GHOST_C_API_H

#include <stdbool.h>
#include <stddef.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(GHOST_C_API_BUILD)
#    define GHOST_C_API __declspec(dllexport)
#  else
#    define GHOST_C_API __declspec(dllimport)
#  endif
#else
#  define GHOST_C_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct GhostSessionHandle_t;
typedef struct GhostSessionHandle_t *GhostSessionHandle;

struct GhostOptionsHandle_t;
typedef struct GhostOptionsHandle_t *GhostOptionsHandle;

typedef enum {
    GHOST_SUCCESS = 0,
    GHOST_SAT_FOUND = 1,
    GHOST_OPTIMAL_FOUND = 2,
    GHOST_FEASIBLE_FOUND = 3,
    GHOST_TIME_LIMIT = 4,
    GHOST_INFEASIBLE = -1,
    GHOST_ERROR_UNKNOWN = -2,
    GHOST_ERROR_NULL_HANDLE = -3,
    GHOST_ERROR_INVALID_ARG = -4,
    GHOST_ERROR_INVALID_ID = -5,
    GHOST_ERROR_MEMORY = -6,
    GHOST_ERROR_SOLVER = -7,
    GHOST_ERROR_API_USAGE = -8
} GhostStatus;

typedef enum {
    GHOST_SOLUTION_STATUS_UNKNOWN = 0,
    GHOST_SOLUTION_STATUS_SAT = 1,
    GHOST_SOLUTION_STATUS_OPTIMAL = 2,
    GHOST_SOLUTION_STATUS_FEASIBLE = 3,
    GHOST_SOLUTION_STATUS_NO_SOLUTION = 4,
    GHOST_SOLUTION_STATUS_INFEASIBLE = -1
} GhostSolutionStatus;

GHOST_C_API GhostSessionHandle ghost_create_session(bool permutation_problem);
GHOST_C_API void ghost_destroy_session(GhostSessionHandle handle);
GHOST_C_API const char *ghost_get_last_error(GhostSessionHandle handle);

/* Integer variables. IDs returned here are zero-based C API identifiers. */
GHOST_C_API int ghost_add_variable(
    GhostSessionHandle handle,
    int min_value,
    int max_value,
    const char *name
);
GHOST_C_API int ghost_add_variable_domain(
    GhostSessionHandle handle,
    const int *domain_values,
    size_t domain_size,
    const char *name
);
GHOST_C_API GhostStatus ghost_set_variable_start(
    GhostSessionHandle handle,
    int variable_id,
    int value
);

GHOST_C_API int ghost_add_linear_eq_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double rhs
);
GHOST_C_API int ghost_add_linear_le_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double rhs
);
GHOST_C_API int ghost_add_linear_ge_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double rhs
);
GHOST_C_API int ghost_add_alldifferent_constraint(
    GhostSessionHandle handle,
    const int *variable_ids,
    size_t number_variables
);

/* The constant is included in the objective value reported after solve. */
GHOST_C_API GhostStatus ghost_set_linear_objective(
    GhostSessionHandle handle,
    bool maximize,
    const int *variable_ids,
    const double *coefficients,
    size_t number_variables,
    double constant
);

GHOST_C_API GhostOptionsHandle ghost_create_options(void);
GHOST_C_API void ghost_destroy_options(GhostOptionsHandle handle);
GHOST_C_API GhostStatus ghost_set_option_parallel(GhostOptionsHandle handle, bool value);
GHOST_C_API GhostStatus ghost_set_option_num_threads(GhostOptionsHandle handle, int value);
GHOST_C_API GhostStatus ghost_set_option_custom_starting_point(GhostOptionsHandle handle, bool value);
GHOST_C_API GhostStatus ghost_set_option_resume_search(GhostOptionsHandle handle, bool value);
GHOST_C_API GhostStatus ghost_set_option_optimization_guidance(GhostOptionsHandle handle, bool value);
GHOST_C_API GhostStatus ghost_set_option_tabu_time_local_min(GhostOptionsHandle handle, int value);
GHOST_C_API GhostStatus ghost_set_option_tabu_time_selected(GhostOptionsHandle handle, int value);
GHOST_C_API GhostStatus ghost_set_option_plateau_probability(GhostOptionsHandle handle, int value);
GHOST_C_API GhostStatus ghost_set_option_reset_threshold(GhostOptionsHandle handle, int value);
GHOST_C_API GhostStatus ghost_set_option_restart_threshold(GhostOptionsHandle handle, int value);
GHOST_C_API GhostStatus ghost_set_option_variables_to_reset(GhostOptionsHandle handle, int value);
GHOST_C_API GhostStatus ghost_set_option_start_samplings(GhostOptionsHandle handle, int value);

/* Heuristic solve. A timeout without a solution does not prove infeasibility. */
GHOST_C_API GhostStatus ghost_solve(
    GhostSessionHandle handle,
    GhostOptionsHandle options,
    double timeout_microseconds
);

GHOST_C_API GhostSolutionStatus ghost_get_solution_status(GhostSessionHandle handle);
GHOST_C_API const char *ghost_get_raw_status_string(GhostSessionHandle handle);
GHOST_C_API size_t ghost_get_num_variables(GhostSessionHandle handle);
GHOST_C_API GhostStatus ghost_get_variable_value(
    GhostSessionHandle handle,
    int variable_id,
    int *value
);
GHOST_C_API GhostStatus ghost_get_variable_values(
    GhostSessionHandle handle,
    int *values,
    size_t buffer_size
);
GHOST_C_API GhostStatus ghost_get_objective_value(GhostSessionHandle handle, double *value);
GHOST_C_API GhostStatus ghost_get_best_candidate_error(GhostSessionHandle handle, double *value);
GHOST_C_API GhostStatus ghost_get_solve_time(GhostSessionHandle handle, double *seconds);

#ifdef __cplusplus
}
#endif

#endif
