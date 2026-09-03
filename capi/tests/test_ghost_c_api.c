#include "ghost_c_api.h"

#include <math.h>
#include <stdio.h>

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
    puts("GHOST C API tests passed.");
    return 0;
}
