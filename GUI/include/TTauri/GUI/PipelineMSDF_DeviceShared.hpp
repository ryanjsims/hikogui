// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/GUI/PipelineMSDF_TextureMap.hpp"
#include "TTauri/GUI/PipelineMSDF_AtlasRect.hpp"
#include "TTauri/GUI/Device_forward.hpp"
#include "TTauri/Foundation/geometry.hpp"
#include "TTauri/Foundation/required.hpp"
#include "TTauri/Foundation/logger.hpp"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <mutex>

namespace TTauri {
template<typename T> struct PixelMap;
}

namespace TTauri::GUI::PipelineMSDF {

struct Image;

struct DeviceShared final {
    // Studies in China have shown that literate individuals know and use between 3,000 and 4,000 characters.
    // Handle up to 4096 characters with a 16 x 512 x 512, 16 x 1 MByte
    static constexpr int atlasImageWidth = 512; // 16 characters, of 32 pixels wide.
    static constexpr int atlasImageHeight = 512; // 16 characters, of 32 pixels height.
    static constexpr int atlasMaximumNrImages = 16; // 16 * 512 characters, of 32x32 pixels.
    static constexpr int stagingImageWidth = 64; // maximum size of character that can be uploaded is 64x64
    static constexpr int stagingImageHeight = 64;

    Device const &device;

    vk::Buffer indexBuffer;
    VmaAllocation indexBufferAllocation = {};

    vk::ShaderModule vertexShaderModule;
    vk::ShaderModule fragmentShaderModule;
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    TextureMap stagingTexture;
    std::vector<TextureMap> atlasTextures;

    std::array<vk::DescriptorImageInfo, atlasMaximumNrImages> atlasDescriptorImageInfos;
    vk::Sampler atlasSampler;
    vk::DescriptorImageInfo atlasSamplerDescriptorImageInfo;

    glm::ivec3 atlasAllocationPosition = {0, 0, 0};
    /// During allocation on a row, we keep track of the tallest glyph.
    int atlasAllocationMaxHeight = 0;


    DeviceShared(Device const &device);
    ~DeviceShared();

    DeviceShared(DeviceShared const &) = delete;
    DeviceShared &operator=(DeviceShared const &) = delete;
    DeviceShared(DeviceShared &&) = delete;
    DeviceShared &operator=(DeviceShared &&) = delete;

    /*! Deallocate vulkan resources.
    * This is called in the destructor of Device_vulkan, therefor we can not use our `std::weak_ptr<Device_vulkan> device`.
    */
    void destroy(gsl::not_null<Device *> vulkanDevice);

    /** Allocate an glyph in the atlas.
     * This may allocate an atlas texture, up to atlasMaximumNrImages.
     */
    [[nodiscard]] AtlasRect allocateGlyph(iextent2 extent) noexcept;

    void drawInCommandBuffer(vk::CommandBuffer &commandBuffer);

    /** Get a pixmap overlaying the mapping of the staging pixmap.
     * This will transition the staging texture to 'general' for writing by the CPU.
     */
    [[nodiscard]] TTauri::PixelMap<MSD10> &getStagingPixelMap();

    /** Once drawing in the staging pixmap is completed, you can upload it to the atlas.
     * This will transition the stating texture to 'source' and the atlas to 'destination'.
     */
    void uploadStagingPixmapToAtlas(AtlasRect location);

    /** This will transition the atlas to 'shader-read'.
     */
    void prepareAtlasForRendering();

private:

    void buildIndexBuffer();
    void teardownIndexBuffer(gsl::not_null<Device_vulkan *> vulkanDevice);
    void buildShaders();
    void teardownShaders(gsl::not_null<Device_vulkan *> vulkanDevice);
    void addAtlasImage();
    void buildAtlas();
    void teardownAtlas(gsl::not_null<Device_vulkan *> vulkanDevice);
};

}