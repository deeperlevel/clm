#pragma once

#include "test.hpp"
#include "../clm/math.hpp"
#include "../clm/math_number.hpp"
#include "../clm/math_random.hpp"
#include "../clm/math_vector.hpp"
#include "../clm/math_matrix.hpp"
#include "../clm/math_interpolation.hpp"
#include "../clm/math_geometry.hpp"
#include "../clm/math_quaternion.hpp"
#include "../clm/math_collision.hpp"
#include "../clm/math_physics2d.hpp"
#include "../clm/math_pathfinding.hpp"

#define CLM_MATH_TEST_STORAGE_CAPACITY 32
#define CLM_MATH_TEST_VALUE_CAPACITY 64

typedef struct clm_math_test_double_storage {
	clm_id id;
	size_t count;
	double values[CLM_MATH_TEST_VALUE_CAPACITY];
	bool used;
} clm_math_test_double_storage;

typedef struct clm_math_test_int64_storage {
	clm_id id;
	size_t count;
	int64_t values[CLM_MATH_TEST_VALUE_CAPACITY];
	bool used;
} clm_math_test_int64_storage;

typedef struct clm_math_test_storage {
	clm_math_test_double_storage doubles[CLM_MATH_TEST_STORAGE_CAPACITY];
	clm_math_test_int64_storage int64s[CLM_MATH_TEST_STORAGE_CAPACITY];
} clm_math_test_storage;

typedef struct clm_math_test_graph {
	size_t neighbor_counts[4];
	size_t neighbors[4][2];
	double costs[4][2];
	double estimates[4];
} clm_math_test_graph;

static clm_math_status clm_math_test_graph_neighbor_count(
	void* user,
	const size_t node,
	size_t* out_count)
{
	if (node >= 4 || out_count == NULL)
		return MATH_ERROR_DIMENSION;
	*out_count = ((clm_math_test_graph*)user)->neighbor_counts[node];
	return MATH_SUCCESS;
}

static clm_math_status clm_math_test_graph_neighbor(
	void* user,
	const size_t node,
	const size_t edge,
	size_t* out_neighbor,
	double* out_cost)
{
	if (node >= 4 || edge >= ((clm_math_test_graph*)user)->neighbor_counts[node] ||
		out_neighbor == NULL || out_cost == NULL)
		return MATH_ERROR_DIMENSION;
	clm_math_test_graph* graph = (clm_math_test_graph*)user;
	*out_neighbor = graph->neighbors[node][edge];
	*out_cost = graph->costs[node][edge];
	return MATH_SUCCESS;
}

static clm_math_status clm_math_test_graph_heuristic(
	void* user,
	const size_t node,
	const size_t goal,
	double* out_estimate)
{
	if (node >= 4 || goal != 3 || out_estimate == NULL)
		return MATH_ERROR_DIMENSION;
	*out_estimate = ((clm_math_test_graph*)user)->estimates[node];
	return MATH_SUCCESS;
}

static clm_math_test_double_storage* clm_math_test_find_double(
	clm_math_test_storage* storage,
	const clm_id id,
	const bool create)
{
	clm_math_test_double_storage* empty = NULL;
	for (size_t index = 0; index < CLM_MATH_TEST_STORAGE_CAPACITY; ++index) {
		clm_math_test_double_storage* slot = &storage->doubles[index];
		if (slot->used && slot->id == id)
			return slot;
		if (!slot->used && empty == NULL)
			empty = slot;
	}
	if (create && empty != NULL) {
		empty->used = true;
		empty->id = id;
		empty->count = 0;
		return empty;
	}
	return NULL;
}

static clm_math_test_int64_storage* clm_math_test_find_int64(
	clm_math_test_storage* storage,
	const clm_id id,
	const bool create)
{
	clm_math_test_int64_storage* empty = NULL;
	for (size_t index = 0; index < CLM_MATH_TEST_STORAGE_CAPACITY; ++index) {
		clm_math_test_int64_storage* slot = &storage->int64s[index];
		if (slot->used && slot->id == id)
			return slot;
		if (!slot->used && empty == NULL)
			empty = slot;
	}
	if (create && empty != NULL) {
		empty->used = true;
		empty->id = id;
		empty->count = 0;
		return empty;
	}
	return NULL;
}

static clm_math_status clm_math_test_double_count(
	void* user,
	const clm_id id,
	size_t* out_count)
{
	clm_math_test_double_storage* slot =
		clm_math_test_find_double((clm_math_test_storage*)user, id, false);
	if (slot == NULL || out_count == NULL)
		return MATH_ERROR_STORAGE;
	*out_count = slot->count;
	return MATH_SUCCESS;
}

static clm_math_status clm_math_test_double_read(
	void* user,
	const clm_id id,
	double* values,
	const size_t count)
{
	clm_math_test_double_storage* slot =
		clm_math_test_find_double((clm_math_test_storage*)user, id, false);
	if (slot == NULL || values == NULL || slot->count != count)
		return MATH_ERROR_STORAGE;
	memcpy(values, slot->values, count * sizeof(*values));
	return MATH_SUCCESS;
}

static clm_math_status clm_math_test_double_write(
	void* user,
	const clm_id id,
	const double* values,
	const size_t count)
{
	if (count > CLM_MATH_TEST_VALUE_CAPACITY || (count != 0 && values == NULL))
		return MATH_ERROR_DIMENSION;
	clm_math_test_double_storage* slot =
		clm_math_test_find_double((clm_math_test_storage*)user, id, true);
	if (slot == NULL)
		return MATH_ERROR_ALLOC;
	if (count != 0)
		memcpy(slot->values, values, count * sizeof(*values));
	slot->count = count;
	return MATH_SUCCESS;
}

static clm_math_status clm_math_test_int64_count(
	void* user,
	const clm_id id,
	size_t* out_count)
{
	clm_math_test_int64_storage* slot =
		clm_math_test_find_int64((clm_math_test_storage*)user, id, false);
	if (slot == NULL || out_count == NULL)
		return MATH_ERROR_STORAGE;
	*out_count = slot->count;
	return MATH_SUCCESS;
}

static clm_math_status clm_math_test_int64_read(
	void* user,
	const clm_id id,
	int64_t* values,
	const size_t count)
{
	clm_math_test_int64_storage* slot =
		clm_math_test_find_int64((clm_math_test_storage*)user, id, false);
	if (slot == NULL || (count != 0 && values == NULL) || slot->count != count)
		return MATH_ERROR_STORAGE;
	if (count != 0)
		memcpy(values, slot->values, count * sizeof(*values));
	return MATH_SUCCESS;
}

static clm_math_status clm_math_test_int64_write(
	void* user,
	const clm_id id,
	const int64_t* values,
	const size_t count)
{
	if (count > CLM_MATH_TEST_VALUE_CAPACITY || (count != 0 && values == NULL))
		return MATH_ERROR_DIMENSION;
	clm_math_test_int64_storage* slot =
		clm_math_test_find_int64((clm_math_test_storage*)user, id, true);
	if (slot == NULL)
		return MATH_ERROR_ALLOC;
	if (count != 0)
		memcpy(slot->values, values, count * sizeof(*values));
	slot->count = count;
	return MATH_SUCCESS;
}

static int clm_math_tests(void)
{
	clm_test_result result = {0};
	clm_test_begin("CLM - Math");
	static clm_math_test_storage storage;
	memset(&storage, 0, sizeof(storage));
	const clm_math_storage_adapter adapter = {
		clm_math_test_double_count,
		clm_math_test_double_read,
		clm_math_test_double_write,
		clm_math_test_int64_count,
		clm_math_test_int64_read,
		clm_math_test_int64_write,
		&storage
	};
	clm_math_set_storage_adapter(&adapter);

	double scalar = 0.0;
	CLM_ASSERT(clm_math_clamp(5.0, 0.0, 3.0, &scalar) == MATH_SUCCESS &&
		scalar == 3.0 && clm_math_inverse_lerp(2.0, 6.0, 4.0, &scalar) ==
		MATH_SUCCESS && scalar == 0.5 &&
		clm_math_wrap(-1.0, 0.0, 4.0, &scalar) == MATH_SUCCESS && scalar == 3.0,
		"run scalar clamp, interpolation, and wrap operations");
	CLM_ASSERT(clm_math_inverse_lerp(1.0, 1.0, 2.0, &scalar) ==
		MATH_ERROR_DOMAIN && clm_math_log(4.0, 1.0, &scalar) ==
		MATH_ERROR_DOMAIN && clm_math_approximately(1.0, 1.0 + 1e-10,
		1e-9, 0.0),
		"report scalar domains and compare with tolerances");

	const double vector_a[] = {1.0, 2.0, 3.0};
	const double vector_b[] = {4.0, 5.0, 6.0};
	CLM_ASSERT(clm_math_write_double(97001, vector_a, 3) == MATH_SUCCESS &&
		clm_math_write_double(97002, vector_b, 3) == MATH_SUCCESS &&
		clm_math_vector_add(97001, 97002, 97003) == MATH_SUCCESS &&
		clm_math_vector_dot(97001, 97002, &scalar) == MATH_SUCCESS &&
		scalar == 32.0,
		"operate on storage-ID vectors");
	double* values = NULL;
	CLM_ASSERT(clm_math_read_double(97003, 3, &values, NULL) == MATH_SUCCESS &&
		values[0] == 5.0 && values[1] == 7.0 && values[2] == 9.0,
		"read vector results through the storage adapter");
	free(values);

	const double matrix_a[] = {1.0, 2.0, 3.0, 4.0};
	const double matrix_b[] = {5.0, 6.0, 7.0, 8.0};
	CLM_ASSERT(clm_math_write_double(97004, matrix_a, 4) == MATH_SUCCESS &&
		clm_math_write_double(97005, matrix_b, 4) == MATH_SUCCESS &&
		clm_math_matrix_multiply(97004, 2, 2, 97005, 2, 2, 97006) ==
		MATH_SUCCESS && clm_math_matrix_determinant(97004, 2, &scalar) ==
		MATH_SUCCESS && scalar == -2.0,
		"multiply matrices and compute determinants");

	const double point[] = {0.5, 0.5};
	const double polygon[] = {0, 0, 1, 0, 1, 1, 0, 1};
	const double aabb[] = {0, 0, 1, 1};
	const double ray_origin[] = {-1, 0.5};
	const double ray_direction[] = {1, 0};
	bool inside = false;
	bool hit = false;
	CLM_ASSERT(clm_math_write_double(97007, point, 2) == MATH_SUCCESS &&
		clm_math_write_double(97008, polygon, 8) == MATH_SUCCESS &&
		clm_math_geometry_point_in_polygon(97007, 97008, &inside) ==
		MATH_SUCCESS && inside &&
		clm_math_geometry_polygon_area(97008, &scalar) == MATH_SUCCESS &&
		scalar == 1.0 &&
		clm_math_write_double(97009, aabb, 4) == MATH_SUCCESS &&
		clm_math_write_double(97010, ray_origin, 2) == MATH_SUCCESS &&
		clm_math_write_double(97011, ray_direction, 2) == MATH_SUCCESS &&
		clm_math_collision_ray_aabb(97010, 97011, 97009, &hit, &scalar) ==
		MATH_SUCCESS && hit && scalar == 1.0,
		"test polygon geometry and ray/AABB collision");

	const double axis[] = {0, 0, 1};
	const double unit_x[] = {1, 0, 0};
	CLM_ASSERT(clm_math_write_double(97012, axis, 3) == MATH_SUCCESS &&
		clm_math_write_double(97013, unit_x, 3) == MATH_SUCCESS &&
		clm_math_quaternion_from_axis_angle(97012, CLM_MATH_PI / 2, 97014) ==
		MATH_SUCCESS && clm_math_quaternion_rotate_vector(97014, 97013, 97015) ==
		MATH_SUCCESS && clm_math_interpolation_storage_lerp(97001, 97002, 0.5,
		97016) == MATH_SUCCESS,
		"construct and apply quaternions and interpolate stored vectors");
	CLM_ASSERT(clm_math_read_double(97015, 3, &values, NULL) == MATH_SUCCESS &&
		fabs(values[0]) < 1e-12 && fabs(values[1] - 1.0) < 1e-12 &&
		fabs(values[2]) < 1e-12,
		"rotate a vector by a quarter-turn quaternion");
	free(values);

	const double initial_position[] = {0.0, 0.0};
	const double initial_velocity[] = {2.0, 0.0};
	const double no_acceleration[] = {0.0, 0.0};
	CLM_ASSERT(clm_math_write_double(97020, initial_position, 2) == MATH_SUCCESS &&
		clm_math_write_double(97021, initial_velocity, 2) == MATH_SUCCESS &&
		clm_math_write_double(97022, no_acceleration, 2) == MATH_SUCCESS &&
		clm_math_physics2d_step(97020, 97021, 97022, 0.0, -10.0, 0.1) ==
		MATH_SUCCESS && clm_math_physics2d_jump(97021, 5.0) == MATH_SUCCESS &&
		clm_math_physics2d_cut_jump(97021, 0.5) == MATH_SUCCESS,
		"integrate gravity and apply a variable-height jump");
	CLM_ASSERT(clm_math_read_double(97020, 2, &values, NULL) == MATH_SUCCESS &&
		fabs(values[0] - 0.2) < 1e-12 && fabs(values[1] + 0.1) < 1e-12,
		"advance position using updated velocity");
	free(values);
	CLM_ASSERT(clm_math_read_double(97021, 2, &values, NULL) == MATH_SUCCESS &&
		fabs(values[0] - 2.0) < 1e-12 && fabs(values[1] - 2.5) < 1e-12,
		"apply gravity before jump and scale upward velocity");
	free(values);

	clm_math_test_graph test_graph = {
		{2, 1, 1, 0},
		{{1, 2}, {3, 0}, {3, 0}, {0, 0}},
		{{1.0, 2.0}, {3.0, 0.0}, {1.0, 0.0}, {0.0, 0.0}},
		{3.0, 2.0, 1.0, 0.0}
	};
	const clm_math_pathfinding_graph graph = {
		clm_math_test_graph_neighbor_count,
		clm_math_test_graph_neighbor,
		clm_math_test_graph_heuristic,
		&test_graph
	};
	size_t* path = NULL;
	size_t path_length = 0;
	bool found = false;
	CLM_ASSERT(clm_math_pathfinding_a_star(4, 0, 3, &graph, &path,
		&path_length, &found) == MATH_SUCCESS && found && path_length == 3 &&
		path[0] == 0 && path[1] == 2 && path[2] == 3,
		"find the minimum-cost route through weighted walk and jump edges");
	free(path);

	size_t factor_count = 0;
	int64_t* factor_values = NULL;
	CLM_ASSERT(clm_math_number_prime_factors(360, 97017, &factor_count) ==
		MATH_SUCCESS && factor_count == 6 &&
		clm_math_read_int64(97017, factor_count, &factor_values, NULL) ==
		MATH_SUCCESS && factor_values[0] == 2 && factor_values[1] == 2 &&
		factor_values[2] == 2 && factor_values[3] == 3 &&
		factor_values[4] == 3 && factor_values[5] == 5 &&
		clm_math_number_is_prime(97) &&
		!clm_math_number_is_prime(1),
		"run integer number theory over fixed-width values");
	free(factor_values);
	int64_t gcd = 0;
	CLM_ASSERT(clm_math_number_gcd(84, 30, &gcd) == MATH_SUCCESS && gcd == 6 &&
		clm_math_number_mod_inverse(3, 11, &gcd) == MATH_SUCCESS && gcd == 4,
		"compute gcd and modular inverse results");

	const int64_t initial_state[] = {0, 1};
	uint32_t random_a = 0;
	uint32_t random_b = 0;
	CLM_ASSERT(clm_math_write_int64(97018, initial_state, 2) == MATH_SUCCESS &&
		clm_math_write_int64(97019, initial_state, 2) == MATH_SUCCESS &&
		clm_math_random_seed(97018, 1234, 7) == MATH_SUCCESS &&
		clm_math_random_seed(97019, 1234, 7) == MATH_SUCCESS &&
		clm_math_random_uint32(97018, &random_a) == MATH_SUCCESS &&
		clm_math_random_uint32(97019, &random_b) == MATH_SUCCESS &&
		random_a == random_b,
		"produce reproducible random sequences from equal seed and stream");

	clm_math_set_storage_adapter(NULL);
	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}

static int test_clm_math(void)
{
    return clm_math_tests();
}

