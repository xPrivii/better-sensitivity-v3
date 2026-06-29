#pragma once
/**
 * dobby.h — stub for IDE autocomplete.
 * The real Dobby is fetched by CMake (FetchContent).
 * Do NOT ship this file in the final build; CMake uses the real one.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Hook a function at `address`, redirect it to `replace_call`.
 * If `origin_call` is non-null, it will be written with a trampoline
 * that calls the original function.
 *
 * Returns 0 on success.
 */
int DobbyHook(void* address, void* replace_call, void** origin_call);

/** Destroy a previously installed hook. */
int DobbyDestroy(void* address);

#ifdef __cplusplus
}
#endif
