#pragma once

#include "test.hpp"
#include "../clm/async_pool.hpp"

#include <atomic>
#include <cstring>
#include <thread>

static clm_id clm_async_test_scheduler_id;
static int clm_async_test_steps[16];
static int clm_async_test_cleanups[16];
static std::atomic<int> clm_async_test_pool_steps[16];
static std::atomic<int> clm_async_test_pool_cleanups[16];

static clm_async_step_result clm_async_test_yield_once(
	const clm_id task_id,
	const clm_id context_id)
{
	(void)task_id;
	if (clm_async_test_steps[context_id]++ == 0)
		return CLM_ASYNC_STEP_YIELD;
	return CLM_ASYNC_STEP_COMPLETE;
}

static clm_async_step_result clm_async_test_wake_during_step(
	const clm_id task_id,
	const clm_id context_id)
{
	if (clm_async_test_steps[context_id]++ == 0) {
		(void)clm_async_wake(clm_async_test_scheduler_id, task_id);
		return CLM_ASYNC_STEP_WAIT;
	}
	return CLM_ASYNC_STEP_COMPLETE;
}

static clm_async_step_result clm_async_test_wait(
	const clm_id task_id,
	const clm_id context_id)
{
	(void)task_id;
	(void)context_id;
	return CLM_ASYNC_STEP_WAIT;
}

static void clm_async_test_cleanup(const clm_id context_id)
{
	++clm_async_test_cleanups[context_id];
}

static clm_async_step_result clm_async_test_pool_complete(
	const clm_id task_id,
	const clm_id context_id)
{
	(void)task_id;
	return clm_async_test_pool_steps[context_id].fetch_add(1, std::memory_order_relaxed) == 0
		? CLM_ASYNC_STEP_WAIT : CLM_ASYNC_STEP_COMPLETE;
}

static void clm_async_test_pool_cleanup(const clm_id context_id)
{
	clm_async_test_pool_cleanups[context_id].fetch_add(1, std::memory_order_relaxed);
}

static bool clm_async_test_wait_pool_state(
	const clm_id pool_id,
	const clm_id task_id,
	const clm_async_task_state expected)
{
	for (std::size_t attempt = 0; attempt < 100000; ++attempt) {
		clm_async_task_state state;
		if (clm_async_pool_get_state(pool_id, task_id, &state) ==
			CLM_ASYNC_OPERATION_SUCCESS && state == expected)
			return true;
		std::this_thread::yield();
	}
	return false;
}

static int test_clm_async()
{
	clm_test_result result = {0};
	clm_test_begin("CLM - Async");
	clm_async_test_scheduler_id = 92000;
	std::memset(clm_async_test_steps, 0, sizeof(clm_async_test_steps));
	std::memset(clm_async_test_cleanups, 0, sizeof(clm_async_test_cleanups));
	CLM_ASSERT(clm_async_scheduler_create(clm_async_test_scheduler_id) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_scheduler_create(clm_async_test_scheduler_id) ==
		CLM_ASYNC_OPERATION_BUSY,
		"create a unique cooperative scheduler");
	CLM_ASSERT(clm_async_spawn(clm_async_test_scheduler_id, 1,
		clm_async_test_yield_once, 1, clm_async_test_cleanup) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_spawn(clm_async_test_scheduler_id, 2,
		clm_async_test_yield_once, 2, clm_async_test_cleanup) ==
		CLM_ASYNC_OPERATION_SUCCESS,
		"submit stackless tasks with caller-owned context IDs");
	std::size_t steps = 0;
	clm_async_task_state state;
	CLM_ASSERT(clm_async_run(clm_async_test_scheduler_id, 3, &steps) ==
		CLM_ASYNC_OPERATION_SUCCESS && steps == 3 &&
		clm_async_get_state(clm_async_test_scheduler_id, 1, &state) ==
		CLM_ASYNC_OPERATION_SUCCESS && state == CLM_ASYNC_TASK_COMPLETED &&
		clm_async_get_state(clm_async_test_scheduler_id, 2, &state) ==
		CLM_ASYNC_OPERATION_SUCCESS && state == CLM_ASYNC_TASK_READY,
		"run bounded steps fairly in FIFO order");
	CLM_ASSERT(clm_async_run(clm_async_test_scheduler_id, 8, &steps) ==
		CLM_ASYNC_OPERATION_SUCCESS && steps == 1 &&
		clm_async_get_state(clm_async_test_scheduler_id, 2, &state) ==
		CLM_ASYNC_OPERATION_SUCCESS && state == CLM_ASYNC_TASK_COMPLETED,
		"retain task state after completion");
	clm_async_task_token stale_token;
	CLM_ASSERT(clm_async_spawn(clm_async_test_scheduler_id, 3,
		clm_async_test_wake_during_step, 3, clm_async_test_cleanup) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_task_get_token(clm_async_test_scheduler_id, 3,
			&stale_token) == CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_run(clm_async_test_scheduler_id, 1, &steps) ==
		CLM_ASYNC_OPERATION_SUCCESS && steps == 1 &&
		clm_async_get_state(clm_async_test_scheduler_id, 3, &state) ==
		CLM_ASYNC_OPERATION_SUCCESS && state == CLM_ASYNC_TASK_READY,
		"remember a wake issued while its task is running");
	CLM_ASSERT(clm_async_run(clm_async_test_scheduler_id, 2, &steps) ==
		CLM_ASYNC_OPERATION_SUCCESS && steps == 1 &&
		clm_async_task_release(clm_async_test_scheduler_id, 1) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_task_release(clm_async_test_scheduler_id, 2) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_task_release(clm_async_test_scheduler_id, 3) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_spawn(clm_async_test_scheduler_id, 3,
			clm_async_test_wait, 4, clm_async_test_cleanup) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_wake_token(clm_async_test_scheduler_id, stale_token) ==
		CLM_ASYNC_OPERATION_NOT_FOUND,
		"reject stale task tokens after a task ID is reused");
	CLM_ASSERT(clm_async_cancel(clm_async_test_scheduler_id, 3) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_get_state(clm_async_test_scheduler_id, 3, &state) ==
		CLM_ASYNC_OPERATION_SUCCESS && state == CLM_ASYNC_TASK_CANCELLED &&
		clm_async_test_cleanups[1] == 1 &&
		clm_async_test_cleanups[2] == 1 &&
		clm_async_test_cleanups[3] == 1 &&
		clm_async_test_cleanups[4] == 1 &&
		clm_async_task_release(clm_async_test_scheduler_id, 3) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_scheduler_destroy(clm_async_test_scheduler_id) ==
		CLM_ASYNC_OPERATION_SUCCESS,
		"cancel, clean up, and destroy scheduler tasks");

	const clm_id pool_id = 92100;
	clm_async_pool_config config = {
		2, 1, 2, 4, CLM_ASYNC_OVERFLOW_QUEUE
	};
	CLM_ASSERT(clm_async_pool_create(pool_id, &config) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_worker_count(pool_id, &steps) ==
		CLM_ASYNC_OPERATION_SUCCESS && steps == 2,
		"create a pool with one scheduler thread per worker");
	clm_async_task_token token_a;
	clm_async_task_token token_b;
	clm_async_task_token token_c;
	for (int i = 0; i < 16; ++i) {
		clm_async_test_pool_steps[i].store(0);
		clm_async_test_pool_cleanups[i].store(0);
	}
	const bool spawned_ab =
		clm_async_pool_spawn(pool_id, 101, clm_async_test_pool_complete,
			1, clm_async_test_pool_cleanup) == CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_spawn(pool_id, 102, clm_async_test_pool_complete,
			2, clm_async_test_pool_cleanup) == CLM_ASYNC_OPERATION_SUCCESS;
	const bool workers_waiting = spawned_ab &&
		clm_async_test_wait_pool_state(pool_id, 101, CLM_ASYNC_TASK_WAITING) &&
		clm_async_test_wait_pool_state(pool_id, 102, CLM_ASYNC_TASK_WAITING);
	CLM_ASSERT(workers_waiting &&
		clm_async_pool_spawn(pool_id, 103, clm_async_test_pool_complete,
			3, clm_async_test_pool_cleanup) == CLM_ASYNC_OPERATION_SUCCESS,
		"fill both workers and queue a third task");
	std::size_t worker_a = CLM_ASYNC_NO_WORKER;
	std::size_t worker_b = CLM_ASYNC_NO_WORKER;
	std::size_t worker_c = 0;
	CLM_ASSERT(clm_async_pool_task_get_worker_index(pool_id, 101, &worker_a) ==
		CLM_ASYNC_OPERATION_SUCCESS && worker_a == 0 &&
		clm_async_pool_task_get_worker_index(pool_id, 102, &worker_b) ==
		CLM_ASYNC_OPERATION_SUCCESS && worker_b == 1 &&
		clm_async_pool_task_get_worker_index(pool_id, 103, &worker_c) ==
		CLM_ASYNC_OPERATION_SUCCESS && worker_c == CLM_ASYNC_NO_WORKER,
		"assign tasks round-robin and retain unassigned queue state");
	CLM_ASSERT(clm_async_pool_task_get_token(pool_id, 101, &token_a) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_task_get_token(pool_id, 102, &token_b) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_task_get_token(pool_id, 103, &token_c) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_wake_token(pool_id, token_a) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_wake_token(pool_id, token_b) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_test_wait_pool_state(pool_id, 103, CLM_ASYNC_TASK_WAITING),
		"wake assigned tasks and dispatch the queued task");
	CLM_ASSERT(clm_async_pool_wake_token(pool_id, token_c) ==
		CLM_ASYNC_OPERATION_SUCCESS,
		"wake a queued task after it is assigned");
	clm_async_pool_stats stats = {0};
	for (std::size_t attempt = 0; attempt < 100000; ++attempt) {
		if (clm_async_pool_get_stats(pool_id, &stats) !=
			CLM_ASYNC_OPERATION_SUCCESS || stats.terminal == 3)
			break;
		std::this_thread::yield();
	}
	CLM_ASSERT(stats.pending == 0 && stats.terminal == 3 &&
		clm_async_test_pool_steps[1].load(std::memory_order_relaxed) == 2 &&
		clm_async_test_pool_steps[2].load(std::memory_order_relaxed) == 2 &&
		clm_async_test_pool_steps[3].load(std::memory_order_relaxed) == 2,
		"run worker tasks and drain the pending FIFO");
	CLM_ASSERT(clm_async_pool_task_release(pool_id, 101) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_task_release(pool_id, 102) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_pool_task_release(pool_id, 103) ==
		CLM_ASYNC_OPERATION_SUCCESS &&
		clm_async_test_pool_cleanups[1].load(std::memory_order_relaxed) == 1 &&
		clm_async_test_pool_cleanups[2].load(std::memory_order_relaxed) == 1 &&
		clm_async_test_pool_cleanups[3].load(std::memory_order_relaxed) == 1 &&
		clm_async_pool_destroy(pool_id) == CLM_ASYNC_OPERATION_SUCCESS,
		"release completed worker tasks and join the pool");

	clm_test_summary(result.passed, result.failed);
	return result.failed == 0 ? 0 : 1;
}

