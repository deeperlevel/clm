#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 *
 * An initiative by deeperlevel, developed with community contributions.
 * https://github.com/deeperlevel/clm
 * Contact: deeparchive@outlook.com
 *
 * Public Domain — CC0 1.0 Universal
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Provided AS IS, without warranty or liability.
 *
 * Don't start from scratch, bro.
 * =============================================================================
 */

#include "math_core.h"

/* 2D dynamics uses +Y as up. Gravity and acceleration are world units per
 * second squared; position and velocity are read and written by storage ID. */
/** Advances position and velocity with semi-implicit Euler integration.
 * Acceleration and gravity are applied before position is advanced. dt must be
 * finite and nonnegative. Position and velocity IDs must be distinct.
 */
static inline clm_math_status clm_math_physics2d_step(
    clm_id position, clm_id velocity, clm_id acceleration, double gravity_x,
    double gravity_y, double dt);

/** Sets upward velocity for a jump; ground-state checks belong to the caller. */
static inline clm_math_status clm_math_physics2d_jump(clm_id velocity,
                                                      double jump_speed);

/** Scales positive vertical velocity to support variable-height jumps.
 * factor must be in [0,1]; descending velocity is unchanged.
 */
static inline clm_math_status clm_math_physics2d_cut_jump(clm_id velocity,
                                                          double factor);

#include <math.h>
#include <stdlib.h>

static inline clm_math_status clm_math_physics2d_step(
    clm_id position, clm_id velocity, clm_id acceleration, double gravity_x,
    double gravity_y, double dt) {
  if (position == velocity || !isfinite(gravity_x) || !isfinite(gravity_y) ||
      !isfinite(dt))
    return MATH_ERROR_VALUE;
  if (dt < 0)
    return MATH_ERROR_DOMAIN;

  double *p = NULL, *v = NULL, *a = NULL;
  clm_math_status status = clm_math_read_double(position, 2, &p, NULL);
  if (status)
    return status;
  status = clm_math_read_double(velocity, 2, &v, NULL);
  if (status) {
    free(p);
    return status;
  }
  status = clm_math_read_double(acceleration, 2, &a, NULL);
  if (status) {
    free(p);
    free(v);
    return status;
  }

  double next_velocity[2] = {v[0] + (a[0] + gravity_x) * dt,
                             v[1] + (a[1] + gravity_y) * dt};
  double next_position[2] = {p[0] + next_velocity[0] * dt,
                             p[1] + next_velocity[1] * dt};
  free(p);
  free(v);
  free(a);
  for (size_t axis = 0; axis < 2; ++axis)
    if (!isfinite(next_velocity[axis]) || !isfinite(next_position[axis]))
      return MATH_ERROR_VALUE;

  status = clm_math_write_double(velocity, next_velocity, 2);
  if (status)
    return status;
  return clm_math_write_double(position, next_position, 2);
}

static inline clm_math_status clm_math_physics2d_jump(clm_id velocity,
                                                      double jump_speed) {
  if (!isfinite(jump_speed))
    return MATH_ERROR_VALUE;
  if (jump_speed < 0)
    return MATH_ERROR_DOMAIN;
  double *v = NULL;
  clm_math_status status = clm_math_read_double(velocity, 2, &v, NULL);
  if (status)
    return status;
  v[1] = jump_speed;
  status = clm_math_write_double(velocity, v, 2);
  free(v);
  return status;
}

static inline clm_math_status clm_math_physics2d_cut_jump(clm_id velocity,
                                                          double factor) {
  if (!isfinite(factor))
    return MATH_ERROR_VALUE;
  if (factor < 0 || factor > 1)
    return MATH_ERROR_DOMAIN;
  double *v = NULL;
  clm_math_status status = clm_math_read_double(velocity, 2, &v, NULL);
  if (status)
    return status;
  if (v[1] > 0)
    v[1] *= factor;
  status = clm_math_write_double(velocity, v, 2);
  free(v);
  return status;
}
