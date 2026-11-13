/* SPDX-License-Identifier: MIT */
#ifndef TITAN_NETWORK_FILTER_CSDK_H
#define TITAN_NETWORK_FILTER_CSDK_H
#include <stdint.h>
#if !defined(_WIN32) || !defined(_WIN64)
#error This SDK requires Windows x64.
#endif
#ifdef __cplusplus
#define TITAN_NETWORK_FILTER_CSDK_NOEXCEPT noexcept
extern "C" {
#else
#define TITAN_NETWORK_FILTER_CSDK_NOEXCEPT
#endif
#if defined(TITAN_NETWORK_FILTER_CSDK_BUILD)
#define TITAN_NETWORK_FILTER_CSDK_API __declspec(dllexport)
#else
#define TITAN_NETWORK_FILTER_CSDK_API __declspec(dllimport)
#endif
/* open is inert. One active session per loaded product DLL.
   call accepts length-delimited UTF-8 JSON and returns an allocated UTF-8
   response, or NULL on allocation failure. Always free with this same DLL.
   Session/operation handles are scoped to the client that returned them. */
TITAN_NETWORK_FILTER_CSDK_API uint32_t titan_network_filter_json_open(uint64_t *session) TITAN_NETWORK_FILTER_CSDK_NOEXCEPT;
TITAN_NETWORK_FILTER_CSDK_API uint32_t titan_network_filter_json_close(uint64_t session) TITAN_NETWORK_FILTER_CSDK_NOEXCEPT;
TITAN_NETWORK_FILTER_CSDK_API char *titan_network_filter_json_call(uint64_t session,const char *request,uint32_t length) TITAN_NETWORK_FILTER_CSDK_NOEXCEPT;
TITAN_NETWORK_FILTER_CSDK_API void titan_network_filter_json_free(char *response) TITAN_NETWORK_FILTER_CSDK_NOEXCEPT;
#ifdef __cplusplus
}
#endif
#undef TITAN_NETWORK_FILTER_CSDK_NOEXCEPT
#endif
