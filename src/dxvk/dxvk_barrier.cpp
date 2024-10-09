#include "dxvk_barrier.h"

namespace dxvk {
  
  DxvkBarrierTracker::DxvkBarrierTracker() {
    // Having an accessible 0 node makes certain things easier to
    // implement and allows us to use 0 as an invalid node index.
    m_nodes.emplace_back();

    // Pre-allocate root nodes for the implicit hash table
    for (uint32_t i = 0; i < 2u * HashTableSize; i++)
      allocateNode();
  }


  DxvkBarrierTracker::~DxvkBarrierTracker() {

  }


  bool DxvkBarrierTracker::findRange(
    const DxvkAddressRange&           range,
          DxvkAccess                  accessType) const {
    uint32_t rootIndex = computeRootIndex(range, accessType);
    return findNode(range, rootIndex);
  }


  void DxvkBarrierTracker::insertRange(
    const DxvkAddressRange&           range,
          DxvkAccess                  accessType) {
    uint32_t rootIndex = computeRootIndex(range, accessType);

    // If we can just insert the node with no conflicts,
    // we don't have to do anything.
    uint32_t nodeIndex = insertNode(range, rootIndex);

    if (likely(!nodeIndex))
      return;

    // If there's an existing node and it contains the entire
    // range we want to add already, also don't do anything.
    auto& node = m_nodes[nodeIndex];

    if (node.addressRange.contains(range))
      return;

    // Otherwise, check if there are any other overlapping ranges.
    // If that is not the case, simply update the range we found.
    bool hasOverlap = false;

    if (range.rangeStart < node.addressRange.rangeStart) {
      DxvkAddressRange testRange;
      testRange.resource = range.resource;
      testRange.rangeStart = range.rangeStart;
      testRange.rangeEnd = node.addressRange.rangeStart - 1u;

      hasOverlap = findNode(testRange, rootIndex);
    }

    if (range.rangeEnd > node.addressRange.rangeEnd && !hasOverlap) {
      DxvkAddressRange testRange;
      testRange.resource = range.resource;
      testRange.rangeStart = node.addressRange.rangeEnd + 1u;
      testRange.rangeEnd = range.rangeEnd;

      hasOverlap = findNode(testRange, rootIndex);
    }

    if (!hasOverlap) {
      node.addressRange.rangeStart = std::min(node.addressRange.rangeStart, range.rangeStart);
      node.addressRange.rangeEnd = std::max(node.addressRange.rangeEnd, range.rangeEnd);
      return;
    }

    // If there are multiple ranges overlapping the one being
    // inserted, remove them all and insert the merged range.
    DxvkAddressRange mergedRange = range;

    while (nodeIndex) {
      auto& node = m_nodes[nodeIndex];
      mergedRange.rangeStart = std::min(mergedRange.rangeStart, node.addressRange.rangeStart);
      mergedRange.rangeEnd = std::max(mergedRange.rangeEnd, node.addressRange.rangeEnd);

      removeNode(nodeIndex, rootIndex);

      nodeIndex = findNode(range, rootIndex);
    }

    insertNode(mergedRange, rootIndex);
  }


  void DxvkBarrierTracker::clear() {
    m_rootMaskValid = 0u;

    while (m_rootMaskSubtree) {
      // Free subtrees if any, but keep the root node intact
      uint32_t rootIndex = bit::tzcnt(m_rootMaskSubtree) + 1u;

      auto& root = m_nodes[rootIndex];

      if (root.header) {
        freeNode(root.child(0));
        freeNode(root.child(1));

        root.header = 0u;
      }

      m_rootMaskSubtree &= m_rootMaskSubtree - 1u;
    }
  }


  uint32_t DxvkBarrierTracker::allocateNode() {
    if (!m_free.empty()) {
      uint32_t nodeIndex = m_free.back();
      m_free.pop_back();

      // Free any subtree that the node might still have
      auto& node = m_nodes[nodeIndex];
      freeNode(node.child(0));
      freeNode(node.child(1));

      node.header = 0u;
      return nodeIndex;
    } else {
      // Allocate entirely new node in the array
      uint32_t nodeIndex = m_nodes.size();
      m_nodes.emplace_back();
      return nodeIndex;
    }
  }


  void DxvkBarrierTracker::freeNode(uint32_t node) {
    if (node)
      m_free.push_back(node);
  }


  uint32_t DxvkBarrierTracker::findNode(
    const DxvkAddressRange&           range,
          uint32_t                    rootIndex) const {
    // Check if the given root is valid at all
    uint64_t rootBit = uint64_t(1u) << (rootIndex - 1u);

    if (!(m_rootMaskValid & rootBit))
      return false;

    // Traverse search tree normally
    uint32_t nodeIndex = rootIndex;

    while (nodeIndex) {
      auto& node = m_nodes[nodeIndex];

      if (node.addressRange.overlaps(range))
        return nodeIndex;

      nodeIndex = node.child(uint32_t(node.addressRange.lt(range)));
    }

    return 0u;
  }


  uint32_t DxvkBarrierTracker::insertNode(
    const DxvkAddressRange&           range,
          uint32_t                    rootIndex) {
    // Check if the given root is valid at all
    uint64_t rootBit = uint64_t(1u) << (rootIndex - 1u);

    if (!(m_rootMaskValid & rootBit)) {
      m_rootMaskValid |= rootBit;

      // Update root node as necessary. Also reset
      // its red-ness if we set it during deletion.
      auto& node = m_nodes[rootIndex];
      node.header = 0;
      node.addressRange = range;
      return 0;
    } else {
      // Traverse tree and abort if we find any range
      // overlapping the one we're trying to insert.
      uint32_t parentIndex = rootIndex;
      uint32_t childIndex = 0u;

      while (true) {
        auto& parent = m_nodes[parentIndex];

        if (parent.addressRange.overlaps(range))
          return parentIndex;

        childIndex = parent.addressRange.lt(range);

        if (!parent.child(childIndex))
          break;

        parentIndex = parent.child(childIndex);
      }

      // Create and insert new node into the tree
      uint32_t nodeIndex = allocateNode();

      auto& parent = m_nodes[parentIndex];
      parent.setChild(childIndex, nodeIndex);

      auto& node = m_nodes[nodeIndex];
      node.setRed(true);
      node.setParent(parentIndex);
      node.addressRange = range;

      // Only do the fixup to maintain red-black properties if
      // we haven't marked the root node as red in a deletion.
      if (parentIndex != rootIndex && !m_nodes[rootIndex].isRed())
        rebalancePostInsert(nodeIndex, rootIndex);

      m_rootMaskSubtree |= rootBit;
      return 0u;
    }
  }


  void DxvkBarrierTracker::removeNode(
          uint32_t                    nodeIndex,
          uint32_t                    rootIndex) {
    auto& node = m_nodes[nodeIndex];

    uint32_t l = node.child(0);
    uint32_t r = node.child(1);

    if (l && r) {
      // Both children are valid. Take the payload from the smallest
      // node in the right subtree and delete that node instead.
      uint32_t childIndex = r;

      while (m_nodes[childIndex].child(0))
        childIndex = m_nodes[childIndex].child(0);

      node.addressRange = m_nodes[childIndex].addressRange;
      removeNode(childIndex, rootIndex);
    } else {
      // Deletion is expected to be exceptionally rare, to the point of
      // being irrelevant in practice since it can only ever happen if an
      // app reads multiple disjoint blocks of a resource and then reads
      // another range covering multiple of those blocks again. Instead
      // of implementing a complex post-delete fixup, mark the root as
      // red and allow the tree to go unbalanced until the next reset.
      if (!node.isRed() && (nodeIndex != rootIndex))
        m_nodes[rootIndex].setRed(true);

      // We're deleting the a node with one or no children. To avoid
      // special-casing the root node, copy the child node to it and
      // update links as necessary.
      uint32_t childIndex = std::max(l, r);
      uint32_t parentIndex = node.parent();

      if (childIndex) {
        auto& child = m_nodes[childIndex];

        uint32_t cl = child.child(0);
        uint32_t cr = child.child(1);

        node.setChild(0, cl);
        node.setChild(1, cr);

        if (nodeIndex != rootIndex)
          node.setRed(child.isRed());

        node.addressRange = child.addressRange;

        if (cl) m_nodes[cl].setParent(nodeIndex);
        if (cr) m_nodes[cr].setParent(nodeIndex);

        child.header = 0u;
        freeNode(childIndex);
      } else if (nodeIndex != rootIndex) {
        // Removing leaf node, update parent link and move on.
        auto& parent = m_nodes[parentIndex];

        uint32_t which = uint32_t(parent.child(1) == nodeIndex);
        parent.setChild(which, 0u);

        node.header = 0;
        freeNode(nodeIndex);
      } else {
        // Removing root with no children, mark tree as invalid
        uint64_t rootBit = uint64_t(1u) << (rootIndex - 1u);

        m_rootMaskSubtree &= ~rootBit;
        m_rootMaskValid &= ~rootBit;
      }
    }
  }


  void DxvkBarrierTracker::rebalancePostInsert(
          uint32_t                    nodeIndex,
          uint32_t                    rootIndex) {
    while (nodeIndex != rootIndex) {
      auto& node = m_nodes[nodeIndex];
      auto& p = m_nodes[node.parent()];

      if (!p.isRed())
        break;

      auto& g = m_nodes[p.parent()];

      if (g.child(1) == node.parent()) {
        auto& u = m_nodes[g.child(0)];

        if (g.child(0) && u.isRed()) {
          g.setRed(true);
          u.setRed(false);
          p.setRed(false);

          nodeIndex = p.parent();
        } else {
          if (p.child(0) == nodeIndex)
            rotateRight(node.parent(), rootIndex);

          p.setRed(false);
          g.setRed(true);

          rotateLeft(p.parent(), rootIndex);
        }
      } else {
        auto& u = m_nodes[g.child(1)];

        if (g.child(1) && u.isRed()) {
          g.setRed(true);
          u.setRed(false);
          p.setRed(false);

          nodeIndex = p.parent();
        } else {
          if (p.child(1) == nodeIndex)
            rotateLeft(node.parent(), rootIndex);

          p.setRed(false);
          g.setRed(true);

          rotateRight(p.parent(), rootIndex);
        }
      }
    }

    m_nodes[rootIndex].setRed(false);
  }


  void DxvkBarrierTracker::rotateLeft(
          uint32_t                    nodeIndex,
          uint32_t                    rootIndex) {
    // This implements rotations in such a way that the node to
    // rotate around does not move. This is important to avoid
    // having a special case for the root node, and avoids having
    // to access the parent or special-case the root node.
    auto& node = m_nodes[nodeIndex];

    auto l = node.child(0);
    auto r = node.child(1);

    auto rl = m_nodes[r].child(0);
    auto rr = m_nodes[r].child(1);

    m_nodes[l].setParent(r);

    bool isRed = m_nodes[r].isRed();
    m_nodes[r].setRed(node.isRed());
    m_nodes[r].setChild(0, l);
    m_nodes[r].setChild(1, rl);

    m_nodes[rr].setParent(nodeIndex);

    node.setRed(isRed && nodeIndex != rootIndex);
    node.setChild(0, r);
    node.setChild(1, rr);

    std::swap(node.addressRange, m_nodes[r].addressRange);
  }


  void DxvkBarrierTracker::rotateRight(
          uint32_t                    nodeIndex,
          uint32_t                    rootIndex) {
    auto& node = m_nodes[nodeIndex];

    auto l = node.child(0);
    auto r = node.child(1);

    auto ll = m_nodes[l].child(0);
    auto lr = m_nodes[l].child(1);

    m_nodes[r].setParent(l);

    bool isRed = m_nodes[l].isRed();
    m_nodes[l].setRed(node.isRed());
    m_nodes[l].setChild(0, lr);
    m_nodes[l].setChild(1, r);

    m_nodes[ll].setParent(nodeIndex);

    node.setRed(isRed && nodeIndex != rootIndex);
    node.setChild(0, ll);
    node.setChild(1, l);

    std::swap(node.addressRange, m_nodes[l].addressRange);
  }




  DxvkBarrierSet:: DxvkBarrierSet(DxvkCmdBuffer cmdBuffer)
  : m_cmdBuffer(cmdBuffer) {

  }


  DxvkBarrierSet::~DxvkBarrierSet() {

  }

  
  void DxvkBarrierSet::accessMemory(
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);

    m_allBarrierSrcStages |= srcStages;
    m_memBarrier.srcStageMask  |= srcStages & vk::StageDeviceMask;
    m_memBarrier.srcAccessMask |= srcAccess & vk::AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages & vk::StageDeviceMask;

    if (access.test(DxvkAccess::Write)) {
      m_memBarrier.dstAccessMask |= dstAccess & vk::AccessDeviceMask;

      if (dstAccess & vk::AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
        m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
      }
    }
  }


  void DxvkBarrierSet::accessBuffer(
    const DxvkBufferSliceHandle&    bufSlice,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);
    
    m_allBarrierSrcStages |= srcStages;
    m_memBarrier.srcStageMask  |= srcStages & vk::StageDeviceMask;
    m_memBarrier.srcAccessMask |= srcAccess & vk::AccessWriteMask;
    m_memBarrier.dstStageMask  |= dstStages & vk::StageDeviceMask;
    
    if (access.test(DxvkAccess::Write)) {
      m_memBarrier.dstAccessMask |= dstAccess & vk::AccessDeviceMask;

      if (dstAccess & vk::AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
        m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
      }
    }

    m_bufSlices.insert(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, access));
  }
  
  
  void DxvkBarrierSet::accessImage(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  subresources,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    DxvkAccessFlags access = this->getAccessTypes(srcAccess);

    m_allBarrierSrcStages |= srcStages & vk::StageDeviceMask;

    if (srcLayout == dstLayout) {
      m_memBarrier.srcStageMask  |= srcStages & vk::StageDeviceMask;
      m_memBarrier.srcAccessMask |= srcAccess & vk::AccessWriteMask;
      m_memBarrier.dstStageMask  |= dstStages & vk::StageDeviceMask;

      if (access.test(DxvkAccess::Write)) {
        m_memBarrier.dstAccessMask |= dstAccess;

        if (dstAccess & vk::AccessHostMask) {
          m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
          m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
        }
      }
    } else {
      VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
      barrier.srcStageMask                = srcStages & vk::StageDeviceMask;
      barrier.srcAccessMask               = srcAccess & vk::AccessWriteMask;
      barrier.dstStageMask                = dstStages & vk::StageDeviceMask;
      barrier.dstAccessMask               = dstAccess & vk::AccessDeviceMask;
      barrier.oldLayout                   = srcLayout;
      barrier.newLayout                   = dstLayout;
      barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
      barrier.image                       = image->handle();
      barrier.subresourceRange            = subresources;
      barrier.subresourceRange.aspectMask = image->formatInfo()->aspectMask;
      m_imgBarriers.push_back(barrier);

      if (dstAccess & vk::AccessHostMask) {
        m_hostBarrierSrcStages |= srcStages;
        m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
      }

      access.set(DxvkAccess::Write);
    }

    m_imgSlices.insert(image->handle(),
      DxvkBarrierImageSlice(subresources, access));
  }


  void DxvkBarrierSet::releaseImage(
          DxvkBarrierSet&           acquire,
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  subresources,
          uint32_t                  srcQueue,
          VkImageLayout             srcLayout,
          VkPipelineStageFlags      srcStages,
          VkAccessFlags             srcAccess,
          uint32_t                  dstQueue,
          VkImageLayout             dstLayout,
          VkPipelineStageFlags      dstStages,
          VkAccessFlags             dstAccess) {
    auto& release = *this;

    m_allBarrierSrcStages |= srcStages;

    VkImageMemoryBarrier2 barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
    barrier.srcStageMask                = srcStages & vk::StageDeviceMask;
    barrier.srcAccessMask               = srcAccess & vk::AccessWriteMask;
    barrier.dstStageMask                = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
    barrier.dstAccessMask               = 0;
    barrier.oldLayout                   = srcLayout;
    barrier.newLayout                   = dstLayout;
    barrier.srcQueueFamilyIndex         = srcQueue;
    barrier.dstQueueFamilyIndex         = dstQueue;
    barrier.image                       = image->handle();
    barrier.subresourceRange            = subresources;
    barrier.subresourceRange.aspectMask = image->formatInfo()->aspectMask;
    release.m_imgBarriers.push_back(barrier);

    if (srcQueue == dstQueue)
      barrier.oldLayout = dstLayout;

    barrier.srcStageMask                = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    barrier.srcAccessMask               = 0;
    barrier.dstStageMask                = dstStages;
    barrier.dstAccessMask               = dstAccess;
    acquire.m_imgBarriers.push_back(barrier);

    if (dstAccess & vk::AccessHostMask) {
      acquire.m_hostBarrierSrcStages |= srcStages & vk::StageDeviceMask;
      acquire.m_hostBarrierDstAccess |= dstAccess & vk::AccessHostMask;
    }

    DxvkAccessFlags access(DxvkAccess::Read, DxvkAccess::Write);
    release.m_imgSlices.insert(image->handle(),
      DxvkBarrierImageSlice(subresources, access));
    acquire.m_imgSlices.insert(image->handle(),
      DxvkBarrierImageSlice(subresources, access));
  }


  bool DxvkBarrierSet::isBufferDirty(
    const DxvkBufferSliceHandle&    bufSlice,
          DxvkAccessFlags           bufAccess) {
    return m_bufSlices.isDirty(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, bufAccess));
  }


  bool DxvkBarrierSet::isImageDirty(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  imgSubres,
          DxvkAccessFlags           imgAccess) {
    return m_imgSlices.isDirty(image->handle(),
      DxvkBarrierImageSlice(imgSubres, imgAccess));
  }


  DxvkAccessFlags DxvkBarrierSet::getBufferAccess(
    const DxvkBufferSliceHandle&    bufSlice) {
    return m_bufSlices.getAccess(bufSlice.handle,
      DxvkBarrierBufferSlice(bufSlice.offset, bufSlice.length, 0));
  }

  
  DxvkAccessFlags DxvkBarrierSet::getImageAccess(
    const Rc<DxvkImage>&            image,
    const VkImageSubresourceRange&  imgSubres) {
    return m_imgSlices.getAccess(image->handle(),
      DxvkBarrierImageSlice(imgSubres, 0));
  }


  void DxvkBarrierSet::finalize(const Rc<DxvkCommandList>& commandList) {
    // Emit host barrier if necessary
    if (m_hostBarrierSrcStages) {
      m_memBarrier.srcStageMask |= m_hostBarrierSrcStages;
      m_memBarrier.srcAccessMask |= VK_ACCESS_MEMORY_WRITE_BIT;
      m_memBarrier.dstStageMask |= VK_PIPELINE_STAGE_HOST_BIT;
      m_memBarrier.dstAccessMask |= m_hostBarrierDstAccess;

      m_hostBarrierSrcStages = 0;
      m_hostBarrierDstAccess = 0;
    }

    this->recordCommands(commandList);
  }


  void DxvkBarrierSet::recordCommands(const Rc<DxvkCommandList>& commandList) {
    VkDependencyInfo depInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

    if (m_memBarrier.srcStageMask | m_memBarrier.dstStageMask) {
      depInfo.memoryBarrierCount = 1;
      depInfo.pMemoryBarriers = &m_memBarrier;
    }

    if (!m_imgBarriers.empty()) {
      depInfo.imageMemoryBarrierCount = m_imgBarriers.size();
      depInfo.pImageMemoryBarriers = m_imgBarriers.data();
    }

    uint32_t totalBarrierCount = depInfo.memoryBarrierCount
      + depInfo.bufferMemoryBarrierCount
      + depInfo.imageMemoryBarrierCount;

    if (!totalBarrierCount)
      return;

    // AMDVLK (and -PRO) will just crash if they encounter a very large structure
    // in one vkCmdPipelineBarrier2 call, so we need to split the barrier into parts.
    constexpr uint32_t MaxBarriersPerCall = 512;

    if (unlikely(totalBarrierCount > MaxBarriersPerCall)) {
      VkDependencyInfo splitDepInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

      for (uint32_t i = 0; i < depInfo.memoryBarrierCount; i += MaxBarriersPerCall) {
        splitDepInfo.memoryBarrierCount = std::min(depInfo.memoryBarrierCount - i, MaxBarriersPerCall);
        splitDepInfo.pMemoryBarriers = depInfo.pMemoryBarriers + i;
        commandList->cmdPipelineBarrier(m_cmdBuffer, &splitDepInfo);
      }

      splitDepInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

      for (uint32_t i = 0; i < depInfo.bufferMemoryBarrierCount; i += MaxBarriersPerCall) {
        splitDepInfo.bufferMemoryBarrierCount = std::min(depInfo.bufferMemoryBarrierCount - i, MaxBarriersPerCall);
        splitDepInfo.pBufferMemoryBarriers = depInfo.pBufferMemoryBarriers + i;
        commandList->cmdPipelineBarrier(m_cmdBuffer, &splitDepInfo);
      }

      splitDepInfo = { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

      for (uint32_t i = 0; i < depInfo.imageMemoryBarrierCount; i += MaxBarriersPerCall) {
        splitDepInfo.imageMemoryBarrierCount = std::min(depInfo.imageMemoryBarrierCount - i, MaxBarriersPerCall);
        splitDepInfo.pImageMemoryBarriers = depInfo.pImageMemoryBarriers + i;
        commandList->cmdPipelineBarrier(m_cmdBuffer, &splitDepInfo);
      }
    } else {
      // Otherwise, issue the barrier as-is
      commandList->cmdPipelineBarrier(m_cmdBuffer, &depInfo);
    }

    commandList->addStatCtr(DxvkStatCounter::CmdBarrierCount, 1);

    this->reset();
  }
  
  
  void DxvkBarrierSet::reset() {
    m_allBarrierSrcStages = 0;

    m_memBarrier.srcStageMask = 0;
    m_memBarrier.srcAccessMask = 0;
    m_memBarrier.dstStageMask = 0;
    m_memBarrier.dstAccessMask = 0;

    m_imgBarriers.clear();

    m_bufSlices.clear();
    m_imgSlices.clear();
  }
  
  
  DxvkAccessFlags DxvkBarrierSet::getAccessTypes(VkAccessFlags flags) {
    DxvkAccessFlags result;
    if (flags & vk::AccessReadMask)  result.set(DxvkAccess::Read);
    if (flags & vk::AccessWriteMask) result.set(DxvkAccess::Write);
    return result;
  }
  
}