float unormalize(uint value, int bits) {
  const int range = (1 << bits) - 1;

  return float(value) / float(range);
}

float snormalize(int value, int bits) {
  const int range = (1 << (bits - 1)) - 1;

  // Min because, -32 and -31 map to -1.0f, and we
  // divide by 31.
  return max(float(value) / float(range), -1.0f);
}