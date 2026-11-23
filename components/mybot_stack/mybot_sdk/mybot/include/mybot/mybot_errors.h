/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_ERRORS_H_
#define MYBOT_ERRORS_H_

/**
 * Returned by a KV-store get callback only when the requested key is absent.
 * Any other negative result is an error and must not be treated as a benign
 * first-boot state.
 */
#define MYBOT_ERR_NOT_FOUND (-3)

#endif /* MYBOT_ERRORS_H_ */
