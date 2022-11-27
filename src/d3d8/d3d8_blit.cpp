#include "d3d8_device.h"

/**
 * Implements all cases of CopyRects
 */

namespace dxvk {

  static constexpr bool isDXT(d3d9::D3DFORMAT fmt) {
      return fmt == d3d9::D3DFMT_DXT1
          || fmt == d3d9::D3DFMT_DXT2
          || fmt == d3d9::D3DFMT_DXT3
          || fmt == d3d9::D3DFMT_DXT4
          || fmt == d3d9::D3DFMT_DXT5;
  }

  // Compute number of bytes in a compressed texture to copy for a given locked rect
  static constexpr UINT getDXTCopySize(const RECT& rect, UINT textureWidth, UINT lockPitch) {
    
    // Assume that DXT blocks are 4x4 pixels.
    // This may not always be correct.
    constexpr UINT blockWidth  = 4;
    constexpr UINT blockHeight = 4;

    // Rect dimensions in blocks
    UINT rectWidthBlocks  = ((rect.right-rect.left) / blockWidth);
    UINT rectHeightBlocks = ((rect.bottom-rect.top) / blockHeight);

    // Compute bytes per block
    UINT blocksPerRow  = std::max(textureWidth / blockWidth, 1u);
    UINT bytesPerBlock = lockPitch / blocksPerRow;

    return bytesPerBlock * (rectHeightBlocks * rectWidthBlocks);
  }

  // Copies texture rect in system mem using memcpy.
  // Rects must be congruent, but need not be aligned.
  HRESULT copyTextureBuffers(
      D3D8Surface*                  src,
      D3D8Surface*                  dst,
      const d3d9::D3DSURFACE_DESC&  srcDesc,
      const d3d9::D3DSURFACE_DESC&  dstDesc,
      const RECT&                   srcRect,
      const RECT&                   dstRect) {
    HRESULT res = D3D_OK;
    D3DLOCKED_RECT srcLocked, dstLocked;

    // CopyRects cannot perform format conversions.
    if (srcDesc.Format != dstDesc.Format)
      return D3DERR_INVALIDCALL;

    res = src->LockRect(&srcLocked, &srcRect, D3DLOCK_READONLY);
    if (FAILED(res))
      return res;
    
    res = dst->LockRect(&dstLocked, &dstRect, 0);
    if (FAILED(res)) {
      src->UnlockRect();
      return res;
    }

    auto rows = srcRect.bottom  - srcRect.top;
    auto cols = srcRect.right   - srcRect.left;

    if (isDXT(srcDesc.Format)) {
      
      // Copy compressed textures.
      auto copySize = getDXTCopySize(srcRect, srcDesc.Width, srcLocked.Pitch);
      std::memcpy(dstLocked.pBits, srcLocked.pBits, copySize);
    
    } else {
      auto bpp  = srcLocked.Pitch / srcDesc.Width;

      if (srcRect.left    == 0
       && srcRect.right   == LONG(srcDesc.Width)
       && srcDesc.Width   == dstDesc.Width
       && srcLocked.Pitch == dstLocked.Pitch) {

        // If copying the entire texture into a congruent destination,
        // we can do this in one continuous copy.
        std::memcpy(dstLocked.pBits, srcLocked.pBits, srcLocked.Pitch * rows);

      } else {
        // Copy one row at a time
        size_t srcOffset = 0, dstOffset = 0;
        for (auto i = 0; i < rows; i++) {
          std::memcpy(
            (uint8_t*)dstLocked.pBits + dstOffset,
            (uint8_t*)srcLocked.pBits + srcOffset,
            cols * bpp);
          srcOffset += srcLocked.Pitch;
          dstOffset += dstLocked.Pitch;
        }
      }
    }

    res = dst->UnlockRect();
    res = src->UnlockRect();
    return res;
  }

  HRESULT STDMETHODCALLTYPE D3D8DeviceEx::CopyRects(
          IDirect3DSurface8* pSourceSurface,
          CONST RECT* pSourceRectsArray,
          UINT cRects,
          IDirect3DSurface8* pDestinationSurface,
          CONST POINT* pDestPointsArray) {

    if (pSourceSurface == NULL || pDestinationSurface == NULL) {
      return D3DERR_INVALIDCALL;
    }

    // TODO: No format conversion, no stretching, no clipping.
    // All src/dest rectangles must fit within the dest surface. 

    Com<D3D8Surface> src = static_cast<D3D8Surface*>(pSourceSurface);
    Com<D3D8Surface> dst = static_cast<D3D8Surface*>(pDestinationSurface);

    d3d9::D3DSURFACE_DESC srcDesc, dstDesc;
    src->GetD3D9()->GetDesc(&srcDesc);
    dst->GetD3D9()->GetDesc(&dstDesc);

    // If pSourceRectsArray is NULL, then the entire surface is copied
    RECT rect;
    POINT point = { 0, 0 };
    if (pSourceRectsArray == NULL) {
      cRects = 1;
      rect.top    = rect.left = 0;
      rect.right  = srcDesc.Width;
      rect.bottom = srcDesc.Height;
      pSourceRectsArray = &rect;

      pDestPointsArray = &point;
    }

    HRESULT res = D3DERR_INVALIDCALL;

    for (UINT i = 0; i < cRects; i++) {

      RECT srcRect, dstRect;

      srcRect = pSourceRectsArray[i];

      // True if the copy is asymmetric
      bool asymmetric = true;
      // True if the copy requires stretching (not technically supported)
      bool stretch = true;
      // True if the copy is not perfectly aligned (supported)
      bool offset = true;

      if (pDestPointsArray != NULL) {
        dstRect.left    = pDestPointsArray[i].x;
        dstRect.right   = dstRect.left + (srcRect.right - srcRect.left);
        dstRect.top     = pDestPointsArray[i].y;
        dstRect.bottom  = dstRect.top + (srcRect.bottom - srcRect.top);
        asymmetric  = dstRect.left  != srcRect.left  || dstRect.top    != srcRect.top
                   || dstRect.right != srcRect.right || dstRect.bottom != srcRect.bottom;
        
        stretch     = (dstRect.right-dstRect.left) != (srcRect.right-srcRect.left)
                   || (dstRect.bottom-dstRect.top) != (srcRect.bottom-srcRect.top);
        
        offset      = !stretch && asymmetric;
      } else {
        dstRect     = srcRect;
        asymmetric  = stretch = offset = false;
      }

      POINT dstPt = { dstRect.left, dstRect.top };

      res = D3DERR_INVALIDCALL;

      switch (dstDesc.Pool) {
        
        // Dest: DEFAULT
        case D3DPOOL_DEFAULT:
          switch (srcDesc.Pool) {
            case D3DPOOL_DEFAULT: {
              // default -> default: use StretchRect
              res = GetD3D9()->StretchRect(
                src->GetD3D9(),
                &srcRect,
                dst->GetD3D9(),
                &dstRect,
                d3d9::D3DTEXF_NONE
              );
              goto done;
            }
            case D3DPOOL_MANAGED: {
              // MANAGED -> DEFAULT: UpdateTextureFromBuffer
              res = m_bridge->UpdateTextureFromBuffer(
                src->GetD3D9(),
                dst->GetD3D9(),
                &srcRect,
                &dstPt
              );
              goto done;
            }
            case D3DPOOL_SYSTEMMEM: {
              // system mem -> default: use UpdateSurface
              res = GetD3D9()->UpdateSurface(
                src->GetD3D9(),
                &srcRect,
                dst->GetD3D9(),
                &dstPt
              );
              goto done;
            }
            case D3DPOOL_SCRATCH:
            default: {
              // TODO: Unhandled case.
              goto unhandled;
            }
          } break;
        
        // Dest: MANAGED
        case D3DPOOL_MANAGED:
          switch (srcDesc.Pool) {
            case  D3DPOOL_DEFAULT: {
              // TODO: (copy on GPU)
              goto unhandled;
            }
            case D3DPOOL_MANAGED:
            case D3DPOOL_SYSTEMMEM: {
              // SYSTEMMEM -> MANAGED: LockRect / memcpy
              
              if (stretch) {
                res = D3DERR_INVALIDCALL;
                goto done;
              }

              res = copyTextureBuffers(src.ptr(), dst.ptr(), srcDesc, dstDesc, srcRect, dstRect);

              goto done;
            }
            case D3DPOOL_SCRATCH:
            default: {
              // TODO: Unhandled case.
              goto unhandled;
            }
          } break;
        
        // DEST: SYSTEMMEM
        case D3DPOOL_SYSTEMMEM: {

          // RT (DEFAULT) -> SYSTEMMEM: Use GetRenderTargetData as fast path if possible
          if ((srcDesc.Usage & D3DUSAGE_RENDERTARGET || m_renderTarget == src)) {

            // GetRenderTargetData works if the formats and sizes match
            if (srcDesc.MultiSampleType == d3d9::D3DMULTISAMPLE_NONE
                && srcDesc.Width  == dstDesc.Width
                && srcDesc.Height == dstDesc.Height
                && srcDesc.Format == dstDesc.Format
                && !asymmetric) {
              res = GetD3D9()->GetRenderTargetData(src->GetD3D9(), dst->GetD3D9());
              goto done;
            }
          }

          switch (srcDesc.Pool) {
            case D3DPOOL_DEFAULT: {
              // Get temporary off-screen surface for stretching.
              Com<d3d9::IDirect3DSurface9> pBlitImage = dst->GetBlitImage();

              // Stretch the source RT to the temporary surface.
              res = GetD3D9()->StretchRect(
                src->GetD3D9(),
                &srcRect,
                pBlitImage.ptr(),
                &dstRect,
                d3d9::D3DTEXF_NONE);
              if (FAILED(res)) {
                goto done;
              }

              // Now sync the rendertarget data into main memory.
              res = GetD3D9()->GetRenderTargetData(pBlitImage.ptr(), dst->GetD3D9());
              goto done;
            }

            // SYSMEM/MANAGED -> SYSMEM: LockRect / memcpy
            case D3DPOOL_MANAGED:
            case D3DPOOL_SYSTEMMEM: {
              if (stretch) {
                res = D3DERR_INVALIDCALL;
                goto done;
              }

              res = copyTextureBuffers(src.ptr(), dst.ptr(), srcDesc, dstDesc, srcRect, dstRect);
            }
            case D3DPOOL_SCRATCH:
            default: {
              // TODO: Unhandled case.
              goto unhandled;
            }
          } break;
        }

        // DEST: SCRATCH
        case D3DPOOL_SCRATCH:
        default: {
          // TODO: Unhandled case.
          goto unhandled;
        }
      }

    unhandled:
      Logger::debug(str::format("CopyRects: Hit unhandled case from src pool ", srcDesc.Pool, " to dst pool ", dstDesc.Pool));
      return D3DERR_INVALIDCALL;
    
    done:
      if (FAILED(res)) {
        Logger::debug(str::format("CopyRects: FAILED to copy from src pool ", srcDesc.Pool, " to dst pool ", dstDesc.Pool));
        return res;
      }

    }

    return res;
    
  }
}