#pragma once

#ifndef __has_attribute
#define __has_attribute(attr) 0
#endif

#if __has_attribute(ms_hook_prologue) && (defined(__i386__) || defined(__x86_64__))
#define DXVK_HOTPATCHABLE __attribute__((__ms_hook_prologue__))
#elif defined(DECLSPEC_CHPE_PATCHABLE)
#define DXVK_HOTPATCHABLE DECLSPEC_CHPE_PATCHABLE
#elif defined(__arm64ec__)
#define DXVK_HOTPATCHABLE __attribute__((hybrid_patchable))
#else
#define DXVK_HOTPATCHABLE
#endif
