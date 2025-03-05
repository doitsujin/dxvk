#define VK_RESOLVE_MODE_NONE            (0)
#define VK_RESOLVE_MODE_SAMPLE_ZERO_BIT (1 << 0)
#define VK_RESOLVE_MODE_AVERAGE_BIT     (1 << 1)
#define VK_RESOLVE_MODE_MIN_BIT         (1 << 2)
#define VK_RESOLVE_MODE_MAX_BIT         (1 << 3)

#define resolve_fn(name, type, load_fn)               \
type name(ivec3 coord, int samples, uint mode) {      \
  if (mode == VK_RESOLVE_MODE_NONE)                   \
    return type(0);                                   \
  type value = load_fn(coord, 0);                     \
                                                      \
  switch (mode) {                                     \
    case VK_RESOLVE_MODE_SAMPLE_ZERO_BIT:             \
      return value;                                   \
                                                      \
    case VK_RESOLVE_MODE_AVERAGE_BIT:                 \
      for (int i = 1; i < samples; i++)               \
        value += load_fn(coord, i);                   \
      value /= type(c_samples);                       \
      break;                                          \
                                                      \
    case VK_RESOLVE_MODE_MIN_BIT:                     \
      for (int i = 1; i < samples; i++)               \
        value = min(value, load_fn(coord, i));        \
      break;                                          \
                                                      \
    case VK_RESOLVE_MODE_MAX_BIT:                     \
      for (int i = 1; i < c_samples; i++)             \
        value = min(value, load_fn(coord, i));        \
      break;                                          \
  }                                                   \
                                                      \
  return value;                                       \
}
