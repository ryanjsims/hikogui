// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/GUI/PipelineImage_TextureMap.hpp"
#include "TTauri/GUI/PipelineImage_Page.hpp"
#include "TTauri/GUI/GUIDevice_forward.hpp"
#include "ttauri/required.hpp"
#include "ttauri/R16G16B16A16SFloat.hpp"
#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>
#include <mutex>

namespace tt {
template<typename T> struct PixelMap;
}

namespace tt::PipelineImage {

struct Image;

struct DeviceShared final {
    static constexpr int atlasNrHorizontalPages = 16;
    static constexpr int atlasNrVerticalPages = 16;
    static constexpr int atlasImageWidth = atlasNrHorizontalPages * Page::widthIncludingBorder;
    static constexpr int atlasImageHeight = atlasNrVerticalPages * Page::heightIncludingBorder;
    static constexpr int atlasNrPagesPerImage = atlasNrHorizontalPages * atlasNrVerticalPages;
    static constexpr int atlasMaximumNrImages = 16;
    static constexpr int stagingImageWidth = 1024;
    static constexpr int stagingImageHeight = 1024;

    GUIDevice const &device;

    vk::ShaderModule vertexShaderModule;
    vk::ShaderModule fragmentShaderModule;
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;

    TextureMap stagingTexture;
    std::vector<TextureMap> atlasTextures;

    std::array<vk::DescriptorImageInfo, atlasMaximumNrImages> atlasDescriptorImageInfos;
    vk::Sampler atlasSampler;
    vk::DescriptorImageInfo atlasSamplerDescriptorImageInfo;

    std::vector<Page> atlasFreePages;

    DeviceShared(GUIDevice const &device);
    ~DeviceShared();

    DeviceShared(DeviceShared const &) = delete;
    DeviceShared &operator=(DeviceShared const &) = delete;
    DeviceShared(DeviceShared &&) = delete;
    DeviceShared &operator=(DeviceShared &&) = delete;

    /*! Deallocate vulkan resources.
    * This is called in the destructor of GUIDevice_vulkan, therefor we can not use our `std::weak_ptr<GUIDevice_vulkan> device`.
    */
    void destroy(GUIDevice *vulkanDevice);

    /*! Get the coordinate in the atlas from a page index.
     * \param page number in the atlas
     * \return x, y pixel coordinate in an atlasTexture and z the atlasTextureIndex.
     */
    static ivec getAtlasPositionFromPage(Page page) noexcept {
        ttlet imageIndex = page.nr / atlasNrPagesPerImage;
        ttlet pageNrInsideImage = page.nr % atlasNrPagesPerImage;

        ttlet pageY = pageNrInsideImage / atlasNrVerticalPages;
        ttlet pageX = pageNrInsideImage % atlasNrVerticalPages;

        ttlet x = pageX * Page::widthIncludingBorder + Page::border;
        ttlet y = pageY * Page::heightIncludingBorder + Page::border;

        return ivec{x, y, imageIndex, 1};
    }

    /** Allocate pages from the atlas.
     */
    std::vector<Page> allocatePages(int const nrPages) noexcept;

    /** Deallocate pages back to the atlas.
     */
    void freePages(std::vector<Page> const &pages) noexcept;

    /** Allocate an image in the atlas.
     * \param extent of the image.
     */
    Image makeImage(ivec extent) noexcept;

    void drawInCommandBuffer(vk::CommandBuffer &commandBuffer);

    tt::PixelMap<R16G16B16A16SFloat> getStagingPixelMap();

    void prepareAtlasForRendering();

private:
    tt::PixelMap<R16G16B16A16SFloat> getStagingPixelMap(ivec extent) {
        return getStagingPixelMap().submap({{0,0}, extent});
    }

    void updateAtlasWithStagingPixelMap(Image const &image);

    void buildShaders();
    void teardownShaders(GUIDevice_vulkan *vulkanDevice);
    void addAtlasImage();
    void buildAtlas();
    void teardownAtlas(GUIDevice_vulkan *vulkanDevice);

    friend Image;
};

}