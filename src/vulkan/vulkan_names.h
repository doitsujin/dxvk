#pragma once

#include <ostream>

#include "../util/util_enum.h"

#include "vulkan_loader.h"

std::ostream& operator << (std::ostream& os, VkPipelineCacheHeaderVersion e);
std::ostream& operator << (std::ostream& os, VkResult e);
std::ostream& operator << (std::ostream& os, VkFormat e);
std::ostream& operator << (std::ostream& os, VkImageType e);
std::ostream& operator << (std::ostream& os, VkImageTiling e);
std::ostream& operator << (std::ostream& os, VkImageLayout e);
std::ostream& operator << (std::ostream& os, VkImageViewType e);
std::ostream& operator << (std::ostream& os, VkPresentModeKHR e);
std::ostream& operator << (std::ostream& os, VkColorSpaceKHR e);
std::ostream& operator << (std::ostream& os, VkOffset2D e);
std::ostream& operator << (std::ostream& os, VkOffset3D e);
std::ostream& operator << (std::ostream& os, VkExtent2D e);
std::ostream& operator << (std::ostream& os, VkExtent3D e);
