#pragma once

/*
 * =============================================================================
 * CLM — Cross Language Modules
 * 
 * https://github.com/deeperlevel/clm
 * Contact: [deeparchive@outlook.com](mailto:deeparchive@outlook.com)
 *
 * A collection of reusable, composable foundational modules providing
 * consistent capabilities and behavior across programming languages.
 * Designed to eliminate repetitive groundwork while preserving the
 * flexibility, independence, and native strengths of each language.
 *
 * Unless a file states otherwise, CLM-authored code is dedicated to the
 * public domain under CC0 1.0 Universal.
 * https://creativecommons.org/publicdomain/zero/1.0/
 *
 * Free to use, modify, redistribute, and bundle for any purpose.
 * Attribution and retention of this notice are appreciated, not required.
 *
 * Third-party code under ext/ or vendor/ retains its original licenses,
 * copyright notices, and disclaimers. CLM's CC0 dedication does not
 * apply to such code. Refer to the accompanying vendor license files.
 *
 * CLM is provided AS IS, without warranty or liability.
 * CLM maintainers and contributors make no warranties regarding
 * third-party code and disclaim liability for its use to the extent
 * permitted by applicable law. Original vendor terms remain applicable.
 *
 * =============================================================================
 */

#include "math_core.h"

/* 2D dynamics uses +Y as up. Gravity and acceleration are world units per
 * second squared; position and velocity are read and written by storage ID. */
#include <math.h>
#include <stdlib.h>

/** Advances position and velocity with semi-implicit Euler integration.
 * Acceleration and gravity are applied before position is advanced. dt must be
 * finite and nonnegative. Position and velocity IDs must be distinct.
 * @param position Input parameter described above.
 * @param velocity Input parameter described above.
 * @param acceleration Input parameter described above.
 * @param gravity_x Input parameter described above.
 * @param gravity_y Input parameter described above.
 * @param dt Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_physics2d_step(
    clm_id position, clm_id velocity, clm_id acceleration, double gravity_x,
    double gravity_y, double dt);

/** Sets upward velocity for a jump; ground-state checks belong to the caller. 
 * @param velocity Input parameter described above.
 * @param jump_speed Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_physics2d_jump(clm_id velocity,
                                                      double jump_speed);

/** Scales positive vertical velocity to support variable-height jumps.
 * factor must be in [0,1]; descending velocity is unchanged.
 * @param velocity Input parameter described above.
 * @param factor Input parameter described above.
 * @return Success or error status, as described in the summary above.
 */
clm_math_status clm_math_physics2d_cut_jump(clm_id velocity,
                                                          double factor);
