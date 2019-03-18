
#include "Window_vulkan.hpp"

#include "Device_vulkan.hpp"

#include "TTauri/Logging.hpp"
#include "TTauri/utils.hpp"

#include <boost/numeric/conversion/cast.hpp>

namespace TTauri {
namespace GUI {

Window_vulkan::Window_vulkan(std::shared_ptr<Window::Delegate> delegate, const std::string &title, vk::SurfaceKHR surface) :
    Window(delegate, title),
    intrinsic(surface)
{
}

Window_vulkan::~Window_vulkan()
{
}

void Window_vulkan::waitIdle()
{
    lock_dynamic_cast<Device_vulkan>(device)->intrinsic.waitForFences(1, &renderFinishedFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
}

std::pair<uint32_t, vk::Extent2D> Window_vulkan::getImageCountAndImageExtent()
{
    auto surfaceCapabilities = lock_dynamic_cast<Device_vulkan>(device)->physicalIntrinsic.getSurfaceCapabilitiesKHR(intrinsic);

    uint32_t imageCount;
    if (surfaceCapabilities.maxImageCount) {
        imageCount = std::clamp(defaultNumberOfSwapchainImages, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);

    } else {
        imageCount = std::max(defaultNumberOfSwapchainImages, surfaceCapabilities.minImageCount);
    }

    vk::Extent2D imageExtent;
    if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max() &&
        surfaceCapabilities.currentExtent.height == std::numeric_limits<uint32_t>::max()) {
        LOG_WARNING("getSurfaceCapabilitiesKHR() does not supply currentExtent");
        imageExtent.width =
            std::clamp(windowRectangle.extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
        imageExtent.height =
            std::clamp(windowRectangle.extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

    } else {
        imageExtent = surfaceCapabilities.currentExtent;
    }

    return { imageCount, imageExtent };
}

bool Window_vulkan::isOnScreen()
{
    auto imageCountAndImageExtent = getImageCountAndImageExtent();

    return imageCountAndImageExtent.second.width > 0 && imageCountAndImageExtent.second.height > 0;
}

void Window_vulkan::buildForDeviceChange()
{
    std::scoped_lock lock(mutex);

    if (state == State::LINKED_TO_DEVICE) {
        auto swapchainAndOnScreen = buildSwapchain();
        swapchain = swapchainAndOnScreen.first;
        auto onScreen = swapchainAndOnScreen.second;

        buildRenderPasses();
        buildFramebuffers();
        buildSemaphores();
        backingPipeline->buildForDeviceChange(firstRenderPass, swapchainCreateInfo.imageExtent, swapchainFramebuffers.size());

        state = onScreen ? State::READY_TO_DRAW : State::MINIMIZED;

    } else {
        BOOST_THROW_EXCEPTION(Window::StateError());
    }
}

void Window_vulkan::teardownForDeviceChange()
{
    std::scoped_lock lock(mutex);

    if (state == State::READY_TO_DRAW || state == State::SWAPCHAIN_OUT_OF_DATE || state == State::MINIMIZED) {
        waitIdle();
        backingPipeline->teardownForDeviceChange();
        teardownSemaphores();
        teardownFramebuffers();
        teardownRenderPasses();
        teardownSwapchain();

        state = State::LINKED_TO_DEVICE;
    } else {
        BOOST_THROW_EXCEPTION(Window::StateError());
    }
}

bool Window_vulkan::rebuildForSwapchainChange()
{
    if (!isOnScreen()) {
        // Early exit when window is minimized.
        return false;
    }

    waitIdle();

    backingPipeline->teardownForSwapchainChange();
    teardownFramebuffers();

    auto swapChainAndOnScreen = buildSwapchain(swapchain);
    swapchain = swapChainAndOnScreen.first;
    auto onScreen = swapChainAndOnScreen.second;

    buildFramebuffers();
    backingPipeline->buildForSwapchainChange(firstRenderPass, swapchainCreateInfo.imageExtent, swapchainFramebuffers.size());

    return onScreen;
}

std::pair<vk::SwapchainKHR, bool> Window_vulkan::buildSwapchain(vk::SwapchainKHR oldSwapchain)
{
    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);

    // Figure out the best way of sharing data between the present and graphic queues.
    vk::SharingMode sharingMode;
    uint32_t sharingQueueFamilyCount;
    uint32_t sharingQueueFamilyIndices[2] = { vulkanDevice->graphicQueue->queueFamilyIndex, vulkanDevice->presentQueue->queueFamilyIndex };
    uint32_t *sharingQueueFamilyIndicesPtr;

    if (vulkanDevice->presentQueue->queueCapabilities.handlesGraphicsAndPresent()) {
        sharingMode = vk::SharingMode::eExclusive;
        sharingQueueFamilyCount = 0;
        sharingQueueFamilyIndicesPtr = nullptr;
    } else {
        sharingMode = vk::SharingMode::eConcurrent;
        sharingQueueFamilyCount = 2;
        sharingQueueFamilyIndicesPtr = sharingQueueFamilyIndices;
    }

retry:
    auto imageCountAndImageExtent = getImageCountAndImageExtent();
    auto imageCount = imageCountAndImageExtent.first;
    auto imageExtent = imageCountAndImageExtent.second;

    if (imageExtent.width == 0 || imageExtent.height == 0) {
        return { oldSwapchain, false };
    }

    swapchainCreateInfo = vk::SwapchainCreateInfoKHR(
        vk::SwapchainCreateFlagsKHR(),
        intrinsic,
        imageCount,
        vulkanDevice->bestSurfaceFormat.format,
        vulkanDevice->bestSurfaceFormat.colorSpace,
        imageExtent,
        1, // imageArrayLayers
        vk::ImageUsageFlagBits::eColorAttachment,
        sharingMode,
        sharingQueueFamilyCount,
        sharingQueueFamilyIndicesPtr,
        vk::SurfaceTransformFlagBitsKHR::eIdentity,
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vulkanDevice->bestSurfacePresentMode,
        VK_TRUE, // clipped
        oldSwapchain);

    vk::SwapchainKHR newSwapchain;
    vk::Result result = vulkanDevice->intrinsic.createSwapchainKHR(&swapchainCreateInfo, nullptr, &newSwapchain);
    // No matter what, the oldSwapchain has been retired after createSwapchainKHR().
    vulkanDevice->intrinsic.destroy(oldSwapchain);
    oldSwapchain = vk::SwapchainKHR();

    if (result != vk::Result::eSuccess) {
        LOG_WARNING("Could not create swapchain, retrying.");
        goto retry;
    }

    auto checkImageCountAndImageExtent = getImageCountAndImageExtent();
    auto checkImageExtent = checkImageCountAndImageExtent.second;
    if (imageExtent != checkImageExtent) {
        LOG_WARNING("Surface extent changed while creating swapchain, retrying.");
        // The newSwapchain was created succesfully, it is just of the wrong size so use it as the next oldSwapchain.
        oldSwapchain = newSwapchain;
        goto retry;
    }

    view->setRectangle({ 0.0, 0.0, 0.0 }, { swapchainCreateInfo.imageExtent.width, swapchainCreateInfo.imageExtent.height, 0.0 });

    LOG_INFO("Building swap chain");
    LOG_INFO(" - extent=%i x %i") % swapchainCreateInfo.imageExtent.width % swapchainCreateInfo.imageExtent.height;
    LOG_INFO(" - colorSpace=%s, format=%s") % vk::to_string(swapchainCreateInfo.imageColorSpace) % vk::to_string(swapchainCreateInfo.imageFormat);
    LOG_INFO(" - presentMode=%s, imageCount=%i") % vk::to_string(swapchainCreateInfo.presentMode) % swapchainCreateInfo.minImageCount;

    return { newSwapchain, true };
}

void Window_vulkan::teardownSwapchain()
{
    lock_dynamic_cast<Device_vulkan>(device)->intrinsic.destroy(swapchain);
}

void Window_vulkan::buildFramebuffers()
{
    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);
    
    swapchainImages = vulkanDevice->intrinsic.getSwapchainImagesKHR(swapchain);
    for (auto image : swapchainImages) {
        uint32_t baseMipLlevel = 0;
        uint32_t levelCount = 1;
        uint32_t baseArrayLayer = 0;
        uint32_t layerCount = 1;
        auto imageSubresourceRange =
            vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, baseMipLlevel, levelCount, baseArrayLayer, layerCount);

        auto imageViewCreateInfo = vk::ImageViewCreateInfo(
            vk::ImageViewCreateFlags(),
            image,
            vk::ImageViewType::e2D,
            swapchainCreateInfo.imageFormat,
            vk::ComponentMapping(),
            imageSubresourceRange);

        auto imageView = vulkanDevice->intrinsic.createImageView(imageViewCreateInfo);
        swapchainImageViews.push_back(imageView);

        std::vector<vk::ImageView> attachments = { imageView };

        auto framebufferCreateInfo = vk::FramebufferCreateInfo(
            vk::FramebufferCreateFlags(),
            firstRenderPass,
            boost::numeric_cast<uint32_t>(attachments.size()),
            attachments.data(),
            swapchainCreateInfo.imageExtent.width,
            swapchainCreateInfo.imageExtent.height,
            1 // layers
        );

        LOG_INFO("createFramebuffer (%i, %i)") % swapchainCreateInfo.imageExtent.width % swapchainCreateInfo.imageExtent.height;

        auto framebuffer = vulkanDevice->intrinsic.createFramebuffer(framebufferCreateInfo);
        swapchainFramebuffers.push_back(framebuffer);
    }

    BOOST_ASSERT(swapchainImageViews.size() == swapchainImages.size());
    BOOST_ASSERT(swapchainFramebuffers.size() == swapchainImages.size());
}

void Window_vulkan::teardownFramebuffers()
{
    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);

    for (auto frameBuffer : swapchainFramebuffers) {
        vulkanDevice->intrinsic.destroy(frameBuffer);
    }
    swapchainFramebuffers.clear();

    for (auto imageView : swapchainImageViews) {
        vulkanDevice->intrinsic.destroy(imageView);
    }
    swapchainImageViews.clear();
}

void Window_vulkan::buildRenderPasses()
{
    std::vector<vk::AttachmentDescription> attachmentDescriptions = { {
        vk::AttachmentDescriptionFlags(),
        swapchainCreateInfo.imageFormat,
        vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear,
        vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, // stencilLoadOp
        vk::AttachmentStoreOp::eDontCare, // stencilStoreOp
        vk::ImageLayout::eUndefined, // initialLayout
        vk::ImageLayout::ePresentSrcKHR // finalLayout
    } };

    std::vector<vk::AttachmentReference> inputAttachmentReferences = {};

    std::vector<vk::AttachmentReference> colorAttachmentReferences = { { 0, vk::ImageLayout::eColorAttachmentOptimal } };

    std::vector<vk::SubpassDescription> subpassDescriptions = { { vk::SubpassDescriptionFlags(),
                                                                  vk::PipelineBindPoint::eGraphics,
                                                                  boost::numeric_cast<uint32_t>(inputAttachmentReferences.size()),
                                                                  inputAttachmentReferences.data(),
                                                                  boost::numeric_cast<uint32_t>(colorAttachmentReferences.size()),
                                                                  colorAttachmentReferences.data() } };

    std::vector<vk::SubpassDependency> subpassDependency = { { VK_SUBPASS_EXTERNAL,
                                                               0,
                                                               vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                               vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                                               vk::AccessFlags(),
                                                               vk::AccessFlagBits::eColorAttachmentRead |
                                                                   vk::AccessFlagBits::eColorAttachmentWrite } };

    vk::RenderPassCreateInfo renderPassCreateInfo = {
        vk::RenderPassCreateFlags(), 
        boost::numeric_cast<uint32_t>(attachmentDescriptions.size()), 
        attachmentDescriptions.data(),
        boost::numeric_cast<uint32_t>(subpassDescriptions.size()), 
        subpassDescriptions.data(), boost::numeric_cast<uint32_t>(subpassDependency.size()),
        subpassDependency.data()
    };

    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);

    firstRenderPass = vulkanDevice->intrinsic.createRenderPass(renderPassCreateInfo);
    attachmentDescriptions[0].loadOp = vk::AttachmentLoadOp::eClear;
    followUpRenderPass = vulkanDevice->intrinsic.createRenderPass(renderPassCreateInfo);
}

void Window_vulkan::teardownRenderPasses()
{
    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);
    vulkanDevice->intrinsic.destroy(firstRenderPass);
    vulkanDevice->intrinsic.destroy(followUpRenderPass);
}

void Window_vulkan::buildSemaphores()
{
    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);
    auto semaphoreCreateInfo = vk::SemaphoreCreateInfo();
    imageAvailableSemaphore = vulkanDevice->intrinsic.createSemaphore(semaphoreCreateInfo, nullptr);

    // This fence is used to wait for the Window and its Pipelines to be idle.
    // It should therefor be signed at the start so that when no rendering has been
    // done it is still idle.
    auto fenceCreateInfo = vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);
    renderFinishedFence = vulkanDevice->intrinsic.createFence(fenceCreateInfo, nullptr);
}

void Window_vulkan::teardownSemaphores()
{
    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);
    vulkanDevice->intrinsic.destroy(imageAvailableSemaphore);
    vulkanDevice->intrinsic.destroy(renderFinishedFence);
}

bool Window_vulkan::render(bool blockOnVSync)
{
    uint32_t imageIndex;
    uint64_t timeout = blockOnVSync ? std::numeric_limits<uint64_t>::max() : 0;
    auto vulkanDevice = lock_dynamic_cast<Device_vulkan>(device);

    auto result = vulkanDevice->intrinsic.acquireNextImageKHR(swapchain, timeout, imageAvailableSemaphore, vk::Fence(), &imageIndex);
    switch (result) {
    case vk::Result::eSuccess: break;
    case vk::Result::eSuboptimalKHR: LOG_INFO("acquireNextImageKHR() eSuboptimalKHR"); return false;
    case vk::Result::eErrorOutOfDateKHR: LOG_INFO("acquireNextImageKHR() eErrorOutOfDateKHR"); return false;
    case vk::Result::eTimeout:
        // Don't render, we didn't receive an image.
        return true;
    default: BOOST_THROW_EXCEPTION(Window::SwapChainError());
    }

    vk::Semaphore renderFinishedSemaphores[] = { backingPipeline->render(imageIndex, imageAvailableSemaphore) };

    // Make a fence that should be signaled when all drawing is finished.
    vulkanDevice->intrinsic.waitIdle();
    // device->intrinsic.waitForFences(1, &renderFinishedFence, VK_TRUE, std::numeric_limits<uint64_t>::max());
    vulkanDevice->intrinsic.resetFences(1, &renderFinishedFence);
    vulkanDevice->graphicQueue->intrinsic.submit(0, nullptr, renderFinishedFence);

    auto presentInfo = vk::PresentInfoKHR(1, renderFinishedSemaphores, 1, &swapchain, &imageIndex);

    // Pass present info as a pointer to get the non-throw version.
    result = vulkanDevice->presentQueue->intrinsic.presentKHR(&presentInfo);
    switch (result) {
    case vk::Result::eSuccess: break;
    case vk::Result::eSuboptimalKHR: LOG_INFO("presentKHR() eSuboptimalKHR"); return false;
    case vk::Result::eErrorOutOfDateKHR: LOG_INFO("presentKHR() eErrorOutOfDateKHR"); return false;
    default: BOOST_THROW_EXCEPTION(Window::SwapChainError());
    }
    return true;
}

}}