/* SPDX-License-Identifier: Apache-2.0 */
#ifndef MYBOT_PLATFORM_MCP_TRACE_H_
#define MYBOT_PLATFORM_MCP_TRACE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Process-lifetime RTM diagnostics through the public AOSL logging interface. */
void mybot_mcp_trace_start(void);

#ifdef __cplusplus
}
#endif

#endif /* MYBOT_PLATFORM_MCP_TRACE_H_ */
