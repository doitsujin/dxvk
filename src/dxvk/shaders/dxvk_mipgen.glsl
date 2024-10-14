// Convenience macro for bit tests
#define csTest(a, b) (((a) & b) != 0u)


// Reduces the top mip level with appropriate interpolation.
f32vec4 csReduceSource(
        u32vec2                       dstLocation,
        uint32_t                      layer) {
  f32vec2 fcoord = csComputeSampleCoord(dstLocation, 0u);
  i32vec3 icoord = i32vec3(fcoord, layer);

  // No srgb correction needed here since the source view can use an sRGB format
  f32vec4 t00 = texelFetch(rSrcImage, icoord, 0);
  f32vec4 t10 = texelFetchOffset(rSrcImage, icoord, 0, ivec2(1, 0));
  f32vec4 t01 = texelFetchOffset(rSrcImage, icoord, 0, ivec2(0, 1));
  f32vec4 t11 = texelFetchOffset(rSrcImage, icoord, 0, ivec2(1, 1));

  // Only consider fractional part for interpolation
  fcoord -= f32vec2(icoord.xy);

  f32vec4 r0 = mix(t00, t10, fcoord.x);
  f32vec4 r1 = mix(t01, t11, fcoord.x);

  f32vec4 value = mix(r0, r1, fcoord.y);
  value = csScalePixelValue(value);
  value = csQuantizePixelValue(value);
  return value;
}


// Reduces an already processed mip level with interpolation.
f32vec4 csReduceTail(
        u32vec2                       dstLocation,
        uint32_t                      mip,
        uint32_t                      layer) {
  f32vec2 fcoord = csComputeSampleCoord(dstLocation, mip);
  i32vec3 icoord = i32vec3(fcoord, layer);

  // Need srgb correction here though since we read from the storage image
  f32vec4 t00 = imageLoad(rDstImages[mip - 1u], icoord);
  f32vec4 t10 = imageLoad(rDstImages[mip - 1u], icoord + ivec3(1, 0, 0));
  f32vec4 t01 = imageLoad(rDstImages[mip - 1u], icoord + ivec3(0, 1, 0));
  f32vec4 t11 = imageLoad(rDstImages[mip - 1u], icoord + ivec3(1, 1, 0));

  if (c_format == VK_FORMAT_R8G8B8A8_SRGB || c_format == VK_FORMAT_B8G8R8A8_SRGB) {
    t00.xyz = srgb_to_linear(t00.xyz);
    t01.xyz = srgb_to_linear(t01.xyz);
    t10.xyz = srgb_to_linear(t10.xyz);
    t11.xyz = srgb_to_linear(t11.xyz);
  }

  // Only consider fractional part for interpolation
  fcoord -= f32vec2(icoord.xy);

  f32vec4 r0 = mix(t00, t10, fcoord.x);
  f32vec4 r1 = mix(t01, t11, fcoord.x);

  f32vec4 value = mix(r0, r1, fcoord.y);
  value = csScalePixelValue(value);
  value = csQuantizePixelValue(value);
  return value;
}



// Performs reduction using either the source image, or an
// already fully processed mip level.
f32vec4 csReduceImage(
        u32vec2                       dstOffset,
        u32vec2                       dstCoord,
        u32vec2                       dstReadSize,
        u32vec2                       dstWriteSize,
        uint32_t                      mip,
        uint32_t                      layer) {
  f32vec4 value = f32vec4(0.0f);

  if (all(lessThan(dstCoord, dstReadSize))) {
    u32vec2 dstLocation = dstOffset + dstCoord;

    if (mip == 0u)
      value = csReduceSource(dstLocation, layer);
    else
      value = csReduceTail(dstLocation, mip, layer);

    if (all(lessThan(dstCoord, dstWriteSize)))
      csStoreImage(mip + 1u, dstLocation, layer, value);
  }

  return value;
}


// Reduces a mip in shared memory. This must be used for all odd-sized
// source mip levels, as well as once after reducing a full chain of
// even mips.
f32vec4 csReduceLdsMip(
        uint32_t                        srcMip,
        u32vec2                         srcSize,
        u32vec2                         dstOffset,
        u32vec2                         dstCoord,
        u32vec2                         dstSize,
        uint32_t                        layer) {
  u32vec2 srcCoord = dstOffset * 2u;
  f32vec2 fcoord = csComputeSampleCoord(dstOffset + dstCoord, srcMip);
  u32vec2 icoord = u32vec2(fcoord) - srcCoord;

  f32vec4 result = f32vec4(0.0f);

  if (all(lessThan(icoord, srcSize))) {
    f32vec4 t00 = csLoadLds(icoord + u32vec2(0u, 0u));
    f32vec4 t10 = csLoadLds(icoord + u32vec2(1u, 0u));
    f32vec4 t01 = csLoadLds(icoord + u32vec2(0u, 1u));
    f32vec4 t11 = csLoadLds(icoord + u32vec2(1u, 1u));

    f32vec4 r0 = mix(t00, t10, fract(fcoord.x));
    f32vec4 r1 = mix(t01, t11, fract(fcoord.x));

    result = mix(r0, r1, fract(fcoord.y));
    result = csQuantizePixelValue(result);

    if (all(lessThan(dstCoord, dstSize)))
      csStoreImage(srcMip + 1u, dstOffset + dstCoord, layer, result);
  }

  return result;
}


// Reduces up to 3 even-sized mip levels at once, using subgroup operations.
// The final mip level will be written to LDS for subsequent iterations.
// Returns the number of mip levels written.
uint32_t csReduceEvenMips(
        uint32_t                        tid,
        uint32_t                        mipCount,
        uint32_t                        srcMip,
        u32vec2                         srcCoord,
        u32vec2                         srcSize,
        u32vec2                         srcOffset,
        u32vec2                         dstSize,
        uint32_t                        layer,
        f32vec4                         valueIn) {
  bool storeLds = all(lessThan(srcCoord, srcSize));
  bool storeMem = all(lessThan(srcCoord, dstSize));

  uint32_t oddMipBits = srcSize.x | srcSize.y;
  oddMipBits |= 1u << mipCount;

  if (gl_SubgroupSize < 4u || csTest(oddMipBits, 1u)) {
    if (storeLds)
      csStoreLds(srcCoord, valueIn);
    return 0u;
  }

  precise f32vec4 value = valueIn;
  value += subgroupShuffleXor(value, 0x1);
  value += subgroupShuffleXor(value, 0x2);
  value = csQuantizePixelValue(value * 0.25f);

  bool elected = !csTest(tid, 0x3u);

  if (storeMem && elected)
    csStoreImage(srcMip + 1u, (srcOffset + srcCoord) >> 1u, layer, value);

  if (gl_SubgroupSize < 16u || csTest(oddMipBits, 2u)) {
    if (elected && storeLds)
      csStoreLds(srcCoord >> 1u, value);
    return 1u;
  }

  // Shuffling across a small number of lanes is generally fast,
  // just read the data directly.
  value += subgroupShuffleXor(value, 0x4);
  value += subgroupShuffleXor(value, 0x8);
  value = csQuantizePixelValue(value * 0.25f);

  elected = !csTest(tid, 0xfu);

  if (storeMem && elected)
    csStoreImage(srcMip + 2u, (srcOffset + srcCoord) >> 2u, layer, value);

  if (gl_SubgroupSize < 64u || csTest(oddMipBits, 4u)) {
    if (elected && storeLds)
      csStoreLds(srcCoord >> 2u, value);
    return 2u;
  }

  value += subgroupShuffleXor(value, 0x10);

  if (gl_SubgroupSize == 64u) {
    // We only need the correct result on the first lane, and
    // this is faster than shuffling across 32 lanes on AMD.
    value += subgroupBroadcast(value, 0x20);
  } else {
    value += subgroupShuffleXor(value, 0x20);
  }

  value = csQuantizePixelValue(value * 0.25f);

  elected = !csTest(tid, 0x3fu);

  if (elected) {
    if (storeMem)
      csStoreImage(srcMip + 3u, (srcOffset + srcCoord) >> 3u, layer, value);

    if (storeLds)
      csStoreLds(srcCoord >> 3u, value);
  }

  return 3u;
}

void csPerformPass(
        uint32_t                      tid,
        uint32_t                      baseMip,
        u32vec2                       dstLocation,
        uint32_t                      layer) {
  uint32_t mipIndex = baseMip + 1u;

  // Compute properties of the image area we're reading and writing this pass
  CsMipArea mipArea = csComputeMipArea(dstLocation, baseMip + c_mipsPerPass, mipIndex);

  // Total number of mipmaps to process in this pass
  uint32_t mipCount = min(globals.mipCount - baseMip, c_mipsPerPass);

  // Generate outputs in blocks of 8x8 pixels, needed for the subgroup path.
  u32vec3 blocks = csComputeBlockTopology(mipArea.readSize);

  for (uint32_t i = 0u; i < blocks.z; i += gl_WorkGroupSize.x / 64u) {
    u32vec2 coord = csComputeBlockCoord(tid, blocks, i);
    u32vec2 dstOffset = mipArea.readOffset + coord;

    // Load first mip level from the source image and reduce it.
    f32vec4 data = csReduceImage(mipArea.readOffset, coord,
      mipArea.readSize, mipArea.writeSize, baseMip, layer);

    // Produce output for the next set of even mips right away if possible
    uint32_t evenMips = csReduceEvenMips(tid, mipCount - 1u, baseMip + 1u,
      coord, mipArea.readSize, mipArea.readOffset, mipArea.writeSize, layer, data);

    // Adjust mip index on the first iteration only. This also means that
    // we must not otherwise use this variable within this loop.
    mipIndex += (i == 0u) ? evenMips : 0u;
  }

  barrier();

  // After the initial pass, mipIndex will point to the mip level that was
  // last written, and is now going to be read. The next mip must be read
  // from LDS no matter what.
  while ((mipIndex - baseMip) < mipCount) {
    CsMipArea srcMipArea = csComputeMipArea(dstLocation, baseMip + c_mipsPerPass, mipIndex);
    CsMipArea dstMipArea = csComputeMipArea(dstLocation, baseMip + c_mipsPerPass, mipIndex + 1u);

    uint32_t mipsProcesed = 0u;

    // We might still generate outputs up to 31x31 pixels depending on
    // how many mips we can process in one iteration, hence the loop.
    u32vec3 blocks = csComputeBlockTopology(dstMipArea.readSize);

    for (uint32_t i = 0u; i < blocks.z; i += gl_WorkGroupSize.x / 64u) {
      u32vec2 coord = csComputeBlockCoord(tid, blocks, i);

      // Reduce mip level currently stored in LDS and wait for reads
      // to complete, so that we do not override any of the data.
      f32vec4 data = csReduceLdsMip(mipIndex, srcMipArea.readSize,
        dstMipArea.readOffset, coord, dstMipArea.writeSize, layer);

      barrier();

      // Now we can safely write to LDS even if there are multipe
      // blocks, since the workgroup processes blocks in order.
      uint32_t maxEvenMips = mipCount - (mipIndex - baseMip) - 1u;
      mipsProcesed = csReduceEvenMips(tid, maxEvenMips, mipIndex + 1u, coord,
        dstMipArea.readSize, dstMipArea.readOffset, dstMipArea.writeSize, layer, data) + 1u;
    }

    mipIndex += mipsProcesed;
    barrier();
  }
}


// Helper to broadcast the remaining workgroup count across the entire
// workgroup. Relevant when processing the last set of mips.
shared uint32_t csWorkgroupsShared;


void main() {
  uint32_t tid = gl_SubgroupInvocationID + gl_SubgroupSize * gl_SubgroupID;

  // Process first set of mips
  u32vec2 dstLocation = gl_WorkGroupID.xy;
  uint32_t layer = gl_WorkGroupID.z;

  csPerformPass(tid, 0u, dstLocation, layer);

  if (globals.mipCount <= c_mipsPerPass)
    return;

  // If there are any mips left to process for this shader, only
  // the last active workgroup can safely access all image data
  if (tid == 0u) {
    csWorkgroupsShared = atomicAdd(rWorkgroupCount.layers[layer], -1u,
      gl_ScopeQueueFamily, gl_StorageSemanticsImage, gl_SemanticsAcquireRelease) - 1u;
  }

  barrier();

  if (csWorkgroupsShared != 0u)
    return;

  // Process final set of mipmaps
  csPerformPass(tid, c_mipsPerPass, u32vec2(0u), layer);
}
