/*
 * LunarGravity - gravityuniformbuffer.cpp
 *
 * Copyright (C) 2017 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Mark Young <marky@lunarg.com>
 */

#include <cstring>
#include <fstream>
#include <string>
#include <sstream>

#include "gravityinstanceextif.hpp"
#include "gravitydeviceextif.hpp"
#include "gravitylogger.hpp"
#include "gravitydevicememory.hpp"
#include "gravityuniformbuffer.hpp"

GravityUniformBuffer::GravityUniformBuffer(GravityInstanceExtIf *inst_ext_if, GravityDeviceExtIf *dev_ext_if,
                                           GravityDeviceMemoryManager *dev_memory_mgr, uint32_t total_reserved_size_bytes) {
    GravityLogger &logger = GravityLogger::getInstance();

    m_total_reserved_size_bytes = total_reserved_size_bytes;
    m_data_stride_bytes = 0;
    m_cpu_addr = nullptr;
    m_inst_ext_if = inst_ext_if;
    m_dev_ext_if = dev_ext_if;
    m_dev_memory_mgr = dev_memory_mgr;
    memset(&m_memory, 0, sizeof(GravityDeviceMemory));
    m_memory.vk_device_memory = VK_NULL_HANDLE;

    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.pNext = nullptr;
    buffer_create_info.flags = 0;
    buffer_create_info.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_create_info.size = total_reserved_size_bytes;
    VkResult vk_result = vkCreateBuffer(m_dev_ext_if->m_vk_device, &buffer_create_info, nullptr, &m_vk_buffer);
    if (VK_SUCCESS != vk_result) {
        std::string error_msg = "GravityUniformBuffer::GravityUniformBuffer failed call to vkCreateBuffer with error ";
        error_msg += vk_result;
        logger.LogError(error_msg);
        exit(-1);
    }

    // Get the memory requirements for this buffer
    vkGetBufferMemoryRequirements(m_dev_ext_if->m_vk_device, m_vk_buffer, &m_memory.vk_mem_reqs);
}

GravityUniformBuffer::~GravityUniformBuffer() { Cleanup(); }

void GravityUniformBuffer::Cleanup() {
    if (VK_NULL_HANDLE != m_memory.vk_device_memory) {
        Unload();
    }
    vkDestroyBuffer(m_dev_ext_if->m_vk_device, m_vk_buffer, NULL);
    m_total_reserved_size_bytes = 0;
}

bool GravityUniformBuffer::Load(uint32_t data_stride_bytes) {
    GravityLogger &logger = GravityLogger::getInstance();
    m_data_stride_bytes = data_stride_bytes;

    if (!m_dev_memory_mgr->AllocateMemory(m_memory, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        logger.LogError("GravityUniformBuffer::Load failed GravityDeviceMemoryManager->AllocateMemory call");
        return false;
    }

    return true;
}

void *GravityUniformBuffer::Map(uint32_t index, uint64_t map_mem_size) {
    GravityLogger &logger = GravityLogger::getInstance();
    uint64_t offset_bytes = index * m_data_stride_bytes;
    if (!m_dev_memory_mgr->MapMemory(m_memory, offset_bytes, map_mem_size, &m_cpu_addr)) {
        logger.LogError("GravityUniformBuffer::Map m_dev_memory_mgr->MapMemory failed");
        return nullptr;
    }
    return m_cpu_addr;
}

void GravityUniformBuffer::Unmap() {
    m_dev_memory_mgr->UnmapMemory(m_memory);
    m_cpu_addr = nullptr;
}

bool GravityUniformBuffer::Bind() {
    GravityLogger &logger = GravityLogger::getInstance();

    // Bind the uniform buffer memory at this point
    VkResult vk_result = vkBindBufferMemory(m_dev_ext_if->m_vk_device, m_vk_buffer, m_memory.vk_device_memory, 0);
    if (VK_SUCCESS != vk_result) {
        std::string error_msg = "GravityUniformBuffer::Bind failed vkBindBufferMemory with error ";
        error_msg += vk_result;
        logger.LogError(error_msg);
        return false;
    }
    return true;
}

VkDescriptorBufferInfo GravityUniformBuffer::GetDescriptorInfo(uint32_t index) {
    VkDescriptorBufferInfo desc_buffer_info = {};
    desc_buffer_info.offset = index * m_data_stride_bytes;
    desc_buffer_info.range = m_data_stride_bytes;
    desc_buffer_info.buffer = m_vk_buffer;
    return desc_buffer_info;
}

bool GravityUniformBuffer::Unload() {
    if (VK_NULL_HANDLE != m_memory.vk_device_memory) {
        m_dev_memory_mgr->FreeMemory(m_memory);
        m_memory.vk_device_memory = VK_NULL_HANDLE;
    }

    vkDestroyBuffer(m_dev_ext_if->m_vk_device, m_vk_buffer, NULL);
    m_vk_buffer = VK_NULL_HANDLE;

    return true;
}
