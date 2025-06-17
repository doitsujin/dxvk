#pragma once

#include "dxvk_latency_markers.h"
#include "../../util/sync/sync_signal.h"
#include "../../util/util_env.h"
#include <dxgi.h>

namespace dxvk {

  /*
   * /brief Abstract frame pacer mode in order to support different strategies of synchronization.
   */

  class FramePacerMode {

  public:

    enum Mode {
      MAX_FRAME_LATENCY = 0,
      LOW_LATENCY,
      LOW_LATENCY_VRR,
      MIN_LATENCY
    };

    FramePacerMode( Mode mode, LatencyMarkersStorage* markerStorage, uint32_t maxFrameLatency=1 )
    : m_mode( mode ),
      m_waitLatency( maxFrameLatency+1 ),
      m_latencyMarkersStorage( markerStorage ) {
      setFpsLimitFrametimeFromEnv();
    }

    virtual ~FramePacerMode() { }

    virtual void startFrame( uint64_t frameId ) { }
    virtual void endFrame( uint64_t frameId ) { }

    virtual void finishRender( uint64_t frameId ) { }

    virtual bool getDesiredPresentMode( uint32_t& presentMode ) const {
      return false; }

    void waitRenderFinished( uint64_t frameId ) {
      if (m_mode) m_fenceGpuFinished.wait(frameId-m_waitLatency); }

    void signalRenderFinished( uint64_t frameId ) {
      if (m_mode) m_fenceGpuFinished.signal(frameId); }

    void signalGpuStart( uint64_t frameId ) {
      if (m_mode) m_fenceGpuStart.signal(frameId); }

    void signalCsFinished( uint64_t frameId ) {
      if (m_mode) m_fenceCsFinished.signal(frameId); }

    void setTargetFrameRate( double frameRate ) {
      if (!m_fpsLimitEnvOverride && frameRate > 1.0)
        m_fpsLimitFrametime.store( 1'000'000/frameRate );
    }

    const Mode m_mode;

    static bool getDoubleFromEnv( const char* name, double* result );
    static bool getIntFromEnv( const char* name, int* result );

  protected:

    void setFpsLimitFrametimeFromEnv();

    const uint32_t m_waitLatency;
    LatencyMarkersStorage* m_latencyMarkersStorage;
    std::atomic<int32_t> m_fpsLimitFrametime = { 0 };
    bool m_fpsLimitEnvOverride = { false };

    sync::Fence m_fenceGpuStart    = { sync::Fence(DXGI_MAX_SWAP_CHAIN_BUFFERS) };
    sync::Fence m_fenceGpuFinished = { sync::Fence(DXGI_MAX_SWAP_CHAIN_BUFFERS) };
    sync::Fence m_fenceCsFinished  = { sync::Fence(DXGI_MAX_SWAP_CHAIN_BUFFERS) };

  };



  inline bool FramePacerMode::getDoubleFromEnv( const char* name, double* result ) {
    std::string env = env::getEnvVar(name);
    if (env.empty())
      return false;

    try {
      *result = std::stod(env);
      return true;
    } catch (const std::invalid_argument&) {
      return false;
    }
  }


  inline bool FramePacerMode::getIntFromEnv( const char* name, int* result ) {
    std::string env = env::getEnvVar(name);
    if (env.empty())
      return false;

    try {
      *result = std::stoi(env);
      return true;
    } catch (const std::invalid_argument&) {
      return false;
    }
  }


  inline void FramePacerMode::setFpsLimitFrametimeFromEnv() {
    double fpsLimit;
    if (!getDoubleFromEnv("DXVK_FRAME_RATE", &fpsLimit))
      return;

    m_fpsLimitEnvOverride = true;
    if (fpsLimit < 1.0)
      return;

    m_fpsLimitFrametime = 1'000'000/fpsLimit;
  }

}
