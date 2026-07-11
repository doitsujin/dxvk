#pragma once

#include <array>
#include <vector>

#include "dxvk_buffer.h"
#include "dxvk_include.h"

#include "../util/thread.h"

namespace dxvk {

  class DxvkDevice;
  struct DxvkDeviceQueue;

  /**
   * \brief Checkpoint for hang debugging
   */
  struct DxvkCheckpoint {
    int32_t prev = -1;
    int32_t next = -1;

    std::array<char, 120> info = {};
  };


  /**
   * \brief Checkpoint buffer for
   */
  class DxvkCheckpointBuffer {
    // Allocate ring buffer of 256k entries, should suffice
    static constexpr size_t NumCheckpoints = 1u << 18u;
  public:

    DxvkCheckpointBuffer(DxvkDevice* device);
    ~DxvkCheckpointBuffer();

    /**
     * \brief Adds checkpoint
     *
     * \param [in] queue Target queue for command buffer
     * \param [in] cmd Command buffer
     * \param [in] prev Last checkpoint on the same
     *    command buffer, or -1 if this is the frist.
     * \param [in] text String representation of checkpoint data
     * \returns Checkpoint ID
     */
    int32_t addCheckpoint(
      const DxvkDeviceQueue&      queue,
            VkCommandBuffer       cmd,
            int32_t               prev,
      const char*                 text);

    /**
     * \brief Ends command list
     *
     * If necessary, records the last checkpoint
     * associated with the command buffer.
     * \param [in] queue Target queue for command buffer
     * \param [in] cmd Command buffer
     * \param [in] prev Last checkpoint
     */
    void endCommandBuffer(
      const DxvkDeviceQueue&      queue,
            VkCommandBuffer       cmd,
            int32_t               prev);

    /**
     * \brief Prints commands associated with a hang
     *
     * Must only be called when DEVICE_LOST was detected.
     */
    void printHangInfo();

  private:

    struct Marker {
      int32_t top;
      int32_t bottom;
    };

    DxvkDevice* m_device = nullptr;

    bool        m_hung = false;

    dxvk::mutex m_mutex;
    int32_t     m_next = 0;

    Rc<DxvkBuffer> m_markerBuffer;

    std::vector<DxvkDeviceQueue> m_markerQueues;
    std::vector<DxvkCheckpoint> m_checkpoints;

    int32_t getNextCheckpoint();

    size_t getQueueIndex(
      const DxvkDeviceQueue&        queue);

    DxvkResourceBufferInfo getMarkerBuffer(
      const DxvkDeviceQueue&        queue,
            VkPipelineStageFlagBits stage);

    Rc<DxvkBuffer> createMarkerBuffer();

    void logHangCommands(
      const DxvkDeviceQueue&        queue,
            int32_t                 lastBegun,
            int32_t                 lastComplete);

    void logHangCommandSequence(
      const std::pair<int32_t, int32_t>& list,
            int32_t                 lastBegun,
            int32_t                 lastComplete);

    void logDeviceFaults();

    void dumpDeviceFaultInfo();

    std::pair<int32_t, int32_t> getCommandList(int32_t command) const;

    static std::string faultAddressToString(const VkDeviceFaultAddressInfoKHR& address);

  };

}
