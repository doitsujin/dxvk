#pragma once

/**
 * \brief Reserves a hot-patchable function entry
 *
 * Emits padding at the start of a function so that an inline-hook engine can
 * overwrite the first five bytes with a `jmp rel32` and relocate what it
 * displaced. This is the GCC/clang equivalent of MSVC's /hotpatch.
 *
 * Third-party overlays hook the DXGI entry points this way. Without the
 * padding they must length-decode whatever the compiler happened to emit, and
 * their decoders generally only cover the instruction forms MSVC produces --
 * a frame-pointer build of DXVK starts CreateSwapChainForHwnd with
 * `mov rax,[rbp+disp8]`, which such a decoder does not recognise, leaving it
 * one byte short of a patchable prologue and silently unhooked.
 *
 * Five bytes is the minimum a `jmp rel32` needs. GCC emits five one-byte NOPs,
 * clang a single five-byte NOP; both are ordinary padding that costs nothing at
 * run time and is trivially relocatable.
 */
#if defined(__i386__) || defined(__x86_64__)
  #if defined(__GNUC__) || defined(__clang__)
    #define DXVK_HOTPATCHABLE __attribute__((patchable_function_entry(5)))
  #endif
#endif

#ifndef DXVK_HOTPATCHABLE
  #define DXVK_HOTPATCHABLE
#endif
