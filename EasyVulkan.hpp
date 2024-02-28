#include "VkBase+.h"

using namespace vulkan;
const VkExtent2D& windowSize = graphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan {
    struct renderPassWithFramebuffers {
        renderPass renderPass;
        std::vector<framebuffer> framebuffers;
    };

    // 创建一个直接渲染到交换链图像，且不做深度测试等任何测试的渲染通道，及其对应的帧缓冲
    const auto& CreateRpwf_Screen() {
        static renderPassWithFramebuffers rpwf_screen;
        if (rpwf_screen.renderPass)
            outStream << std::format("[ easyVulkan ] WARNING\nDon't call CreateRpwf_Screen() twice!\n");
        else {
            // 图像附件描述
            // VkAttachmentDescription只是用于描述图像附件，而不指定具体的图像附件或帧缓冲，因此其中不要求填写VkImage或VkImageView或VkFramebuffer类型的handle
            VkAttachmentDescription attachmentDescription = {
                .format = graphicsBase::Base().SwapchainCreateInfo().imageFormat,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED, // 将initialLayout设定为VK_IMAGE_LAYOUT_UNDEFINED可能导致丢弃图像附件原有的内容，但鉴于会在渲染循环中每帧清空图像，因此无妨
                .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR // 以便将交换链图像用于呈现
            };

            VkAttachmentReference attachmentReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

            // 子通道描述, 当前只有一个子通道，该子通道只使用一个颜色附件
            VkSubpassDescription subpassDescription = {
                .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .colorAttachmentCount = 1,
                .pColorAttachments = &attachmentReference
            };

            // 子通道依赖, 覆盖渲染通道开始时的隐式依赖
            VkSubpassDependency subpassDependency = {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
            };

            // 渲染通道的创建信息
            VkRenderPassCreateInfo renderPassCreateInfo = {
                .attachmentCount = 1,
                .pAttachments = &attachmentDescription,
                .subpassCount = 1,
                .pSubpasses = &subpassDescription,
                .dependencyCount = 1,
                .pDependencies = &subpassDependency
            };
            rpwf_screen.renderPass.Create(renderPassCreateInfo);

            auto CreateFramebuffers = [] {
                rpwf_screen.framebuffers.resize(graphicsBase::Base().SwapchainImageCount());
                // 渲染缓冲区的create info
                VkFramebufferCreateInfo framebufferCreateInfo = {
                    .renderPass = rpwf_screen.renderPass,
                    .attachmentCount = 1,
                    .width = windowSize.width,
                    .height = windowSize.height,
                    .layers = 1
                };
                for (size_t i = 0; i < graphicsBase::Base().SwapchainImageCount(); i++) {
                    VkImageView attachment = graphicsBase::Base().SwapchainImageView(i);
                    // image imageview之间的关系是一一对应的 一组attachments对应于一个framebuffer 
                    framebufferCreateInfo.pAttachments = &attachment;
                    rpwf_screen.framebuffers[i].Create(framebufferCreateInfo);
                }
                };
            auto DestroyFramebuffers = [] {
                rpwf_screen.framebuffers.clear(); // 清空vector中的元素时会逐一调用析构函数
                };
            graphicsBase::Base().PushCallback_CreateSwapchain(CreateFramebuffers);
            graphicsBase::Base().PushCallback_DestroySwapchain(DestroyFramebuffers);
            
            // 首次初始化需要手动调用一次
            CreateFramebuffers();
        }
        return rpwf_screen;
    }
}
