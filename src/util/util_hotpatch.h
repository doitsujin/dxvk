#pragma once

#ifndef __has_attribute
#define __has_attribute(attr) 0
#endif

// mingw-gcc has a bug where `ms_hook_prologue` can misalign vtable entries.
// This seems to occur only on some classes rather than all of them.
// Typically this manifests as vtable entries being offset forward by 32 bytes.
// Provide a patchable attribute that only declares on arm64ec/CHPE.
// llvm-mingw doesn't have this problem.
#if __has_attribute(ms_hook_prologue) && (defined(__i386__) || defined(__x86_64__))
#define DXVK_HOTPATCHABLE __attribute__((__ms_hook_prologue__))
#define DXVK_HOTPATCHABLE_ARM64EC
#elif defined(DECLSPEC_CHPE_PATCHABLE)
#define DXVK_HOTPATCHABLE DECLSPEC_CHPE_PATCHABLE
#define DXVK_HOTPATCHABLE_ARM64EC DECLSPEC_CHPE_PATCHABLE
#elif defined(__arm64ec__)
#define DXVK_HOTPATCHABLE __attribute__((hybrid_patchable))
#define DXVK_HOTPATCHABLE_ARM64EC DXVK_HOTPATCHABLE
#else
#define DXVK_HOTPATCHABLE
#define DXVK_HOTPATCHABLE_ARM64EC
#endif
