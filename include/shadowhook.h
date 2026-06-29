#pragma once
/**
 * shadowhook.h — stub for IDE autocomplete.
 * ShadowHook is an optional fallback for hooking system libraries.
 * The real library: https://github.com/bytedance/android-inline-hook
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void* shadowhook_t;

shadowhook_t shadowhook_hook_func_addr(void* func_addr,
                                       void* new_addr,
                                       void** orig_addr);
int shadowhook_unhook(shadowhook_t stub);

#ifdef __cplusplus
}
#endif
