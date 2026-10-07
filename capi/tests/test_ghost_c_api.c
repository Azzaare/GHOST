#include "ghost_c_api.h"

#include <math.h>
#include <stdio.h>

static int callback_calls;

static int sum_error(const int *values, size_t count, double *result, void *userdata) {
    if (count != 2 || userdata == NULL) return GHOST_ERROR_INVALID_ARG;
    ++callback_calls;
    *result = fabs(values[0] + values[1] - *(double *)userdata);
    return GHOST_SUCCESS;
}

static int cost_callback(const int *values, size_t count, double *result, void *userdata) {
    (void)userdata;
    if (count != 2) return GHOST_ERROR_INVALID_ARG;
    *result = 2.0 * values[0] + values[1];
    return GHOST_SUCCESS;
}

static int failing_callback(const int *values, size_t count, double *result, void *userdata) {
    (void)values; (void)count; (void)result; (void)userdata;
    return GHOST_ERROR_SOLVER;
}

static int invalid_callback(const int *values, size_t count, double *result, void *userdata) {
    (void)values; (void)count;
    *result = *(double *)userdata;
    return GHOST_SUCCESS;
}

static int test_callbacks(void) {
    GhostSessionHandle session = ghost_create_session(false);
    GhostOptionsHandle options = ghost_create_options();
    int ids[2], values[2];
    double target = 5.0, cost;
    if (ghost_c_api_version() != 0x00010000u || !session || !options) return 41;
    ids[0] = ghost_add_variable(session, 0, 5, "x");
    ids[1] = ghost_add_variable(session, 0, 5, "y");
    if (ghost_add_callback_constraint(session, ids, 2, sum_error, &target) < 0) return 42;
    if (ghost_set_option_num_threads(options, 1) != GHOST_SUCCESS) return 43;
    if (ghost_set_option_parallel(options, true) != GHOST_SUCCESS) return 44;
    callback_calls = 0;
    if (ghost_solve(session, options, 1000.0) != GHOST_ERROR_API_USAGE || callback_calls != 0) return 45;
    ghost_set_option_parallel(options, false);
    ghost_set_option_num_threads(options, 2);
    if (ghost_solve(session, options, 1000.0) != GHOST_ERROR_API_USAGE || callback_calls != 0) return 46;
    ghost_set_option_num_threads(options, 1);
    if (ghost_solve(session, options, 100000.0) != GHOST_SAT_FOUND || callback_calls == 0) return 47;
    if (ghost_get_variable_values(session, values, 2) != GHOST_SUCCESS || values[0] + values[1] != 5) return 48;
    if (ghost_set_callback_objective(session, false, ids, 2, cost_callback, NULL) != GHOST_SUCCESS) return 49;
    if (ghost_solve(session, options, 10000.0) != GHOST_FEASIBLE_FOUND) return 50;
    if (ghost_get_variable_values(session, values, 2) != GHOST_SUCCESS || values[0] + values[1] != 5) return 51;
    if (ghost_get_objective_value(session, &cost) != GHOST_SUCCESS || cost != 2.0 * values[0] + values[1]) return 52;
    ghost_destroy_session(session);
    ghost_destroy_options(options);
    return 0;
}

static int test_callback_failures(void) {
    const double invalid_values[] = {-1.0, NAN, INFINITY};
    for (int trial = 0; trial < 4; ++trial) {
        GhostSessionHandle session = ghost_create_session(false);
        GhostOptionsHandle options = ghost_create_options();
        int id = ghost_add_variable(session, 0, 1, NULL);
        ghost_set_option_num_threads(options, 1);
        ghost_set_option_parallel(options, false);
        if (trial == 0)
            ghost_add_callback_constraint(session, &id, 1, failing_callback, NULL);
        else
            ghost_add_callback_constraint(session, &id, 1, invalid_callback, (void *)&invalid_values[trial - 1]);
        if (ghost_solve(session, options, 1000.0) != GHOST_ERROR_SOLVER) return 61 + trial;
        ghost_destroy_session(session);
        ghost_destroy_options(options);
    }
    return 0;
}

static int test_satisfaction(void) {
    GhostSessionHandle session = ghost_create_session(false);
    GhostOptionsHandle options = ghost_create_options();
    int ids[3];
    int values[3];
    GhostStatus status;

    if (session == NULL || options == NULL) return 1;
    for (int i = 0; i < 3; ++i) {
        ids[i] = ghost_add_variable(session, 1, 3, NULL);
        if (ids[i] < 0) return 2;
    }
    if (ghost_add_alldifferent_constraint(session, ids, 3) < 0) return 3;
    if (ghost_set_option_parallel(options, true) != GHOST_SUCCESS) return 4;
    if (ghost_set_option_num_threads(options, 2) != GHOST_SUCCESS) return 5;

    status = ghost_solve(session, options, 100000.0);
    if (status != GHOST_SAT_FOUND) {
        fprintf(stderr, "CSP failed: %s\n", ghost_get_last_error(session));
        return 6;
    }
    if (ghost_get_variable_values(session, values, 3) != GHOST_SUCCESS) return 7;
    if (values[0] == values[1] || values[0] == values[2] || values[1] == values[2]) return 8;

    ghost_destroy_options(options);
    ghost_destroy_session(session);
    return 0;
}

static int test_optimization(void) {
    GhostSessionHandle session = ghost_create_session(false);
    int ids[2];
    double coefficients[2] = {1.0, 1.0};
    double objective_coefficients[2] = {2.0, 1.0};
    double objective_value = NAN;
    int values[2];
    GhostStatus status;

    if (session == NULL) return 11;
    ids[0] = ghost_add_variable(session, 0, 5, "x");
    ids[1] = ghost_add_variable(session, 0, 5, "y");
    if (ghost_add_linear_ge_constraint(session, ids, coefficients, 2, 5.0) < 0) return 12;
    if (ghost_set_linear_objective(
            session, false, ids, objective_coefficients, 2, 7.0) != GHOST_SUCCESS) return 13;

    status = ghost_solve(session, NULL, 100000.0);
    if (status != GHOST_FEASIBLE_FOUND) {
        fprintf(stderr, "COP failed: %s\n", ghost_get_last_error(session));
        return 14;
    }
    if (ghost_get_variable_values(session, values, 2) != GHOST_SUCCESS) return 15;
    if (values[0] + values[1] < 5) return 16;
    if (ghost_get_objective_value(session, &objective_value) != GHOST_SUCCESS) return 17;
    if (fabs(objective_value - (2.0 * values[0] + values[1] + 7.0)) > 1e-9) return 18;

    ghost_destroy_session(session);
    return 0;
}

static int test_timeout_is_not_infeasible(void) {
    GhostSessionHandle session = ghost_create_session(false);
    int ids[2];
    double coefficients[2] = {1.0, 1.0};
    GhostStatus status;

    if (session == NULL) return 21;
    ids[0] = ghost_add_variable(session, 1, 2, "x");
    ids[1] = ghost_add_variable(session, 1, 2, "y");
    if (ghost_add_alldifferent_constraint(session, ids, 2) < 0) return 22;
    if (ghost_add_linear_eq_constraint(session, ids, coefficients, 2, 4.0) < 0) return 23;
    status = ghost_solve(session, NULL, 1000.0);
    if (status != GHOST_TIME_LIMIT) return 24;
    if (ghost_get_solution_status(session) != GHOST_SOLUTION_STATUS_NO_SOLUTION) return 25;
    ghost_destroy_session(session);
    return 0;
}

static int test_singleton_domain(void) {
    GhostSessionHandle session = ghost_create_session(false);
    int id;
    int value = 0;
    GhostStatus status;

    if (session == NULL) return 31;
    id = ghost_add_variable(session, 7, 7, "fixed");
    if (id < 0) return 32;
    status = ghost_solve(session, NULL, 10000.0);
    if (status != GHOST_SAT_FOUND) return 33;
    if (ghost_get_variable_value(session, id, &value) != GHOST_SUCCESS) return 34;
    if (value != 7) return 35;
    ghost_destroy_session(session);
    return 0;
}

int main(void) {
    puts("Running satisfaction test...");
    fflush(stdout);
    int result = test_satisfaction();
    if (result != 0) return result;
    puts("Running optimization test...");
    fflush(stdout);
    result = test_optimization();
    if (result != 0) return result;
    puts("Running timeout semantics test...");
    fflush(stdout);
    result = test_timeout_is_not_infeasible();
    if (result != 0) return result;
    puts("Running singleton-domain test...");
    fflush(stdout);
    result = test_singleton_domain();
    if (result != 0) return result;
    result = test_callbacks();
    if (result != 0) return result;
    result = test_callback_failures();
    if (result != 0) return result;
    puts("GHOST C API tests passed.");
    return 0;
}
