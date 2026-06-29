#pragma once
/**
 * version_offsets.h
 *
 * Function offsets inside libminecraftpe.so for each supported MCBE version.
 * Obtain offsets with:  nm -D libminecraftpe.so | grep -i sensitiv
 * or use a disassembler (Ghidra / IDA) on the ARM64 binary.
 *
 * If you want to use exported mangled symbol names instead of offsets,
 * define MCBE_USE_SYMBOL_NAMES and fill in SYM_* below.
 *
 * options.txt sensitivity keys (all versions):
 *   "mouse_sensitivity:"       (0.0 – 1.0,  ×100 = display %)
 *   "touch_sensitivity:"       (0.0 – 1.0)
 *   "controller_sensitivity:"  (0.0 – 1.0)
 */

// ── Build-time version selection ─────────────────────────────────────────────
// Pass -DMCBE_VERSION=12100 (for 1.21.00) etc. to CMake.
#ifndef MCBE_VERSION
#  define MCBE_VERSION 12100   // default: 1.21.00
#endif

// ── 1.21.0x ──────────────────────────────────────────────────────────────────
#if MCBE_VERSION >= 12100 && MCBE_VERSION < 12200

// Demangled: Options::getSensitivity(InputMode)
#define OFF_GET_SENSITIVITY  0x0000000004A3B210ULL
// Demangled: Options::setSensitivity(InputMode, float)
#define OFF_SET_SENSITIVITY  0x0000000004A3B280ULL
// Demangled: SliderControl::setValue(float)
#define OFF_SLIDER_SET_VAL   0x0000000003C1F490ULL

// ── 1.20.8x ──────────────────────────────────────────────────────────────────
#elif MCBE_VERSION >= 12080 && MCBE_VERSION < 12100

#define OFF_GET_SENSITIVITY  0x0000000004912A40ULL
#define OFF_SET_SENSITIVITY  0x0000000004912AA0ULL
#define OFF_SLIDER_SET_VAL   0x0000000003B8E200ULL

// ── 1.20.7x ──────────────────────────────────────────────────────────────────
#elif MCBE_VERSION >= 12070 && MCBE_VERSION < 12080

#define OFF_GET_SENSITIVITY  0x000000000488DC10ULL
#define OFF_SET_SENSITIVITY  0x000000000488DC70ULL
#define OFF_SLIDER_SET_VAL   0x0000000003B0A330ULL

// ── Fallback: symbol names (requires RTLD_GLOBAL on MCBE) ────────────────────
#else
#  define MCBE_USE_SYMBOL_NAMES

// These mangled names are from MCBE 1.21 ARM64; they may differ per version.
#define SYM_GET_SENSITIVITY  "_ZN7Options14getSensitivityENS_9InputModeE"
#define SYM_SET_SENSITIVITY  "_ZN7Options14setSensitivityENS_9InputModeEf"
#define SYM_SLIDER_SET_VAL   "_ZN13SliderControl8setValueEf"
#endif
