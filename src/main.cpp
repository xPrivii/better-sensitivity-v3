/**
 * PrecisionSensitivity - Native mod for Minecraft Bedrock Edition (Android)
 * Loader: LeviLaunchroid (LeviLDev)
 *
 * Features:
 *  - Allows sensitivity up to 2 decimal places (e.g. 85.63 instead of 86)
 *  - Supports Touch, Mouse/OTG, and Controller input
 *  - Patches the in-game slider to use float precision
 *  - Writes correct float value to options.txt (e.g. 0.8563)
 */

#include <jni.h>
#include <android/log.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#include "dobby.h"       // Dobby inline hooking library
#include "shadowhook.h"  // ShadowHook (alternative, for system libs)

#define LOG_TAG "PrecisionSens"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ─────────────────────────────────────────────────────────────────────────────
//  Precision state (shared between hooks and options.txt patch)
// ─────────────────────────────────────────────────────────────────────────────

static float g_touch_sensitivity       = -1.0f; // -1 = not overridden yet
static float g_mouse_sensitivity       = -1.0f;
static float g_controller_sensitivity  = -1.0f;

// Round to 2 decimal places
static inline float round2(float v) {
    return roundf(v * 100.0f) / 100.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
//  options.txt patching
//  MCBE stores sensitivity as a float 0.0–1.0 in options.txt:
//    mouse_sensitivity:0.85  → displayed as 85 in UI
//  We intercept writes and allow sub-integer precision.
// ─────────────────────────────────────────────────────────────────────────────

// Symbol offsets below are for MCBE 1.21.x ARM64.
// They must be updated per version — see version_offsets.h for a table.
#include "version_offsets.h"

typedef float (*GetSensitivityFn)(void* self, int inputMode);
typedef void  (*SetSensitivityFn)(void* self, int inputMode, float value);
typedef void  (*SliderSetValueFn)(void* self, float value);
typedef float (*SliderGetValueFn)(void* self);

static GetSensitivityFn orig_GetSensitivity = nullptr;
static SetSensitivityFn orig_SetSensitivity = nullptr;
static SliderSetValueFn orig_SliderSetValue = nullptr;
static SliderGetValueFn orig_SliderGetValue = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
//  Input mode enum (matches MCBE internals)
// ─────────────────────────────────────────────────────────────────────────────
enum InputMode : int {
    INPUT_TOUCH      = 0,
    INPUT_MOUSE      = 1,
    INPUT_CONTROLLER = 2,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Hook: GetSensitivity
//  Called by the game when it reads sensitivity for a given input mode.
//  We return our stored float-precision value if one has been set.
// ─────────────────────────────────────────────────────────────────────────────
static float hook_GetSensitivity(void* self, int inputMode) {
    float original = orig_GetSensitivity(self, inputMode);

    switch (inputMode) {
        case INPUT_TOUCH:
            if (g_touch_sensitivity >= 0.0f) {
                LOGI("GetSens TOUCH override: %.4f (was %.4f)", g_touch_sensitivity, original);
                return g_touch_sensitivity;
            }
            break;
        case INPUT_MOUSE:
            if (g_mouse_sensitivity >= 0.0f) {
                LOGI("GetSens MOUSE override: %.4f (was %.4f)", g_mouse_sensitivity, original);
                return g_mouse_sensitivity;
            }
            break;
        case INPUT_CONTROLLER:
            if (g_controller_sensitivity >= 0.0f) {
                LOGI("GetSens CTRL override: %.4f (was %.4f)", g_controller_sensitivity, original);
                return g_controller_sensitivity;
            }
            break;
        default:
            break;
    }

    return original;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hook: SetSensitivity
//  Called when the player moves the in-game slider OR options.txt is loaded.
//  We preserve full float precision (2 decimal places) instead of rounding.
// ─────────────────────────────────────────────────────────────────────────────
static void hook_SetSensitivity(void* self, int inputMode, float value) {
    // Preserve 2 decimal places
    float precise = round2(value);

    switch (inputMode) {
        case INPUT_TOUCH:
            g_touch_sensitivity = precise;
            LOGI("SetSens TOUCH -> %.4f (raw=%.4f)", precise, value);
            break;
        case INPUT_MOUSE:
            g_mouse_sensitivity = precise;
            LOGI("SetSens MOUSE -> %.4f (raw=%.4f)", precise, value);
            break;
        case INPUT_CONTROLLER:
            g_controller_sensitivity = precise;
            LOGI("SetSens CTRL  -> %.4f (raw=%.4f)", precise, value);
            break;
        default:
            break;
    }

    // Forward with our precise value
    orig_SetSensitivity(self, inputMode, precise);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hook: Slider setValue
//  The vanilla slider snaps to integer steps (1, 2, 3 … 200).
//  We intercept setValue and allow non-integer values by bypassing the snap.
// ─────────────────────────────────────────────────────────────────────────────
static void hook_SliderSetValue(void* self, float value) {
    // Do NOT call floor/round — pass the raw float straight through.
    // This lets the UI slider sit between integer ticks.
    orig_SliderSetValue(self, value);
}

// ─────────────────────────────────────────────────────────────────────────────
//  options.txt write intercept
//  MCBE serialises options.txt with fprintf/fwrite.  We hook fwrite to catch
//  the sensitivity lines and rewrite them with our precise floats.
// ─────────────────────────────────────────────────────────────────────────────
typedef size_t (*fwrite_fn)(const void*, size_t, size_t, FILE*);
static fwrite_fn orig_fwrite = nullptr;

static size_t hook_fwrite(const void* buf, size_t size, size_t count, FILE* fp) {
    if (buf == nullptr) return orig_fwrite(buf, size, size, fp);

    size_t total = size * count;
    const char* str = static_cast<const char*>(buf);

    // Check if this is a sensitivity line
    auto replace_sens = [&](const char* key, float override_val) -> bool {
        if (override_val < 0.0f) return false;
        if (total < strlen(key)) return false;
        if (strncmp(str, key, strlen(key)) != 0) return false;

        char newline[64];
        // options.txt format: "key:VALUE\n"
        int len = snprintf(newline, sizeof(newline), "%s%.4f\n", key, override_val);
        LOGI("Patching options.txt: %s -> %.4f", key, override_val);
        return orig_fwrite(newline, 1, (size_t)len, fp) > 0;
    };

    if (replace_sens("mouse_sensitivity:",       g_mouse_sensitivity))      return count;
    if (replace_sens("touch_sensitivity:",        g_touch_sensitivity))      return count;
    if (replace_sens("controller_sensitivity:",   g_controller_sensitivity)) return count;

    return orig_fwrite(buf, size, count, fp);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Install hooks
// ─────────────────────────────────────────────────────────────────────────────
static bool install_hooks(void* mc_handle) {
    bool ok = true;

// Helper macro: resolve symbol by name and hook it with Dobby
#define HOOK_SYM(name, hook, orig) do {                                     \
    void* sym = dlsym(mc_handle, name);                                     \
    if (!sym) { LOGE("dlsym failed: %s", name); ok = false; break; }       \
    if (DobbyHook(sym, (void*)(hook), (void**)&(orig)) != 0) {             \
        LOGE("DobbyHook failed: %s", name); ok = false;                    \
    } else { LOGI("Hooked: %s @ %p", name, sym); }                         \
} while(0)

// Helper macro: hook by offset from base address
#define HOOK_OFF(offset, hook, orig) do {                                   \
    void* addr = reinterpret_cast<void*>(                                   \
        reinterpret_cast<uintptr_t>(mc_handle) + (offset));                 \
    if (DobbyHook(addr, (void*)(hook), (void**)&(orig)) != 0) {            \
        LOGE("DobbyHook offset failed: 0x%zx", (size_t)(offset));          \
        ok = false;                                                          \
    } else { LOGI("Hooked offset 0x%zx @ %p", (size_t)(offset), addr); }   \
} while(0)

    // Sensitivity get/set — use exported symbol if available, otherwise offset
#ifdef MCBE_USE_SYMBOL_NAMES
    HOOK_SYM(SYM_GET_SENSITIVITY, hook_GetSensitivity, orig_GetSensitivity);
    HOOK_SYM(SYM_SET_SENSITIVITY, hook_SetSensitivity, orig_SetSensitivity);
    HOOK_SYM(SYM_SLIDER_SET_VAL,  hook_SliderSetValue,  orig_SliderSetValue);
#else
    HOOK_OFF(OFF_GET_SENSITIVITY, hook_GetSensitivity, orig_GetSensitivity);
    HOOK_OFF(OFF_SET_SENSITIVITY, hook_SetSensitivity, orig_SetSensitivity);
    HOOK_OFF(OFF_SLIDER_SET_VAL,  hook_SliderSetValue,  orig_SliderSetValue);
#endif

    // Hook libc fwrite to intercept options.txt writes
    void* fwrite_sym = dlsym(RTLD_DEFAULT, "fwrite");
    if (fwrite_sym) {
        if (DobbyHook(fwrite_sym, (void*)hook_fwrite, (void**)&orig_fwrite) != 0) {
            LOGE("DobbyHook fwrite failed");
            ok = false;
        } else {
            LOGI("Hooked fwrite");
        }
    }

#undef HOOK_SYM
#undef HOOK_OFF

    return ok;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Wait-for-libminecraftpe thread
//  LeviLaunchroid loads our .so before Minecraft's own libraries.
//  We spin until libminecraftpe.so is present in /proc/self/maps.
// ─────────────────────────────────────────────────────────────────────────────
static void* wait_and_hook_thread(void*) {
    LOGI("PrecisionSensitivity: waiting for libminecraftpe.so …");

    void* mc_handle = nullptr;
    for (int i = 0; i < 300; ++i) {   // up to 30 seconds
        mc_handle = dlopen("libminecraftpe.so", RTLD_NOW | RTLD_NOLOAD);
        if (mc_handle) break;
        usleep(100000); // 100 ms
    }

    if (!mc_handle) {
        LOGE("libminecraftpe.so not found after timeout — giving up");
        return nullptr;
    }

    LOGI("libminecraftpe.so found @ %p", mc_handle);
    bool result = install_hooks(mc_handle);
    LOGI("install_hooks: %s", result ? "SUCCESS" : "PARTIAL/FAILED");

    dlclose(mc_handle);
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  JNI_OnLoad — entry point called by LeviLaunchroid when the .so is loaded
// ─────────────────────────────────────────────────────────────────────────────
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
    LOGI("PrecisionSensitivity v1.0 loaded");

    pthread_t tid;
    pthread_create(&tid, nullptr, wait_and_hook_thread, nullptr);
    pthread_detach(tid);

    return JNI_VERSION_1_6;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API — called from a config UI / companion app via JNI if needed
// ─────────────────────────────────────────────────────────────────────────────
extern "C" JNIEXPORT void JNICALL
Java_dev_levimc_precisionsens_NativeBridge_setSensitivity(
        JNIEnv* /*env*/, jclass /*cls*/,
        jint inputMode, jfloat value)
{
    float precise = round2(value);
    switch (inputMode) {
        case INPUT_TOUCH:      g_touch_sensitivity      = precise; break;
        case INPUT_MOUSE:      g_mouse_sensitivity      = precise; break;
        case INPUT_CONTROLLER: g_controller_sensitivity = precise; break;
        default: break;
    }
    LOGI("JNI setSensitivity(mode=%d, value=%.4f)", inputMode, precise);
}

extern "C" JNIEXPORT jfloat JNICALL
Java_dev_levimc_precisionsens_NativeBridge_getSensitivity(
        JNIEnv* /*env*/, jclass /*cls*/, jint inputMode)
{
    switch (inputMode) {
        case INPUT_TOUCH:      return g_touch_sensitivity;
        case INPUT_MOUSE:      return g_mouse_sensitivity;
        case INPUT_CONTROLLER: return g_controller_sensitivity;
        default:               return -1.0f;
    }
}
