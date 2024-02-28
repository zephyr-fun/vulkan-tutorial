#include "GlfwGeneral.hpp"
#include "EasyVulkan.hpp"
using namespace vulkan;

pipelineLayout pipelineLayout_triangle; // 管线布局
pipeline pipeline_triangle; // 管线

// 调用easyVulkan::CreateRpwf_Screen()并存储返回的引用到静态变量，避免重复调用easyVulkan::CreateRpwf_Screen()
const auto& RenderPassAndFramebuffers() {
	static const auto& rpwf_screen = easyVulkan::CreateRpwf_Screen();
	return rpwf_screen;
}

// 创建管线布局
void CreateLayout() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
	pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
}

// 创建管线
void CreatePipeline() {
	static shaderModule vert_triangle("shader/FirstTriangle.vert.spv");
	static shaderModule frag_triangle("shader/FirstTriangle.frag.spv");

	// 管线阶段创建信息
	static VkPipelineShaderStageCreateInfo shaderStageCreateInfos_triangle[2] = {
		vert_triangle.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
		frag_triangle.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	auto Create = [] {
		// 图形管线创建信息
		graphicsPipelineCreateInfoPack pipelineCiPack;
		pipelineCiPack.createInfo.layout = pipelineLayout_triangle;
		pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;
		pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
		pipelineCiPack.viewports.emplace_back(0.f, 0.f, float(windowSize.width), float(windowSize.height), 0.f, 1.f);
		pipelineCiPack.scissors.emplace_back(VkOffset2D{}, windowSize);
		pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		pipelineCiPack.colorBlendAttachmentStates.push_back({ .colorWriteMask = 0b1111 });
		pipelineCiPack.UpdateAllArrays();
		pipelineCiPack.createInfo.stageCount = 2;
		pipelineCiPack.createInfo.pStages = shaderStageCreateInfos_triangle;
		pipeline_triangle.Create(pipelineCiPack);
		};

	auto Destroy = [] {
		pipeline_triangle.~pipeline();
		};

	graphicsBase::Base().PushCallback_CreateSwapchain(Create);
	graphicsBase::Base().PushCallback_DestroySwapchain(Destroy);

	// 首次需要手动调用创建函数
	Create();
}

int main() {
	if (!InitializeWindow({ 1280, 720 }))
		return -1;

	const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();
	CreateLayout();
	CreatePipeline();

	fence fence(VK_FENCE_CREATE_SIGNALED_BIT); // 以置位状态初始化一个栅栏, 在渲染完成后被置位, 开始录制命令缓冲区前需要在CPU一侧手动等待fence被置位以确保先前的命令已完成执行
	semaphore semaphore_imageIsAvailable; // 
	semaphore semaphore_renderingIsOver; // 在渲染完成后被置位, 在呈现图像前等待它

	commandBuffer commandBuffer;
	commandPool commandPool(graphicsBase::Base().QueueFamilyIndex_Graphics(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT); // 从命令池中分配的命令缓冲区可以被重置，从而支持多次向同一命令缓冲区录制命令
	commandPool.AllocateBuffers(commandBuffer);

	VkClearValue clearColor = { .color = { 0.f, 0.f, 0.f, 0.f } };

	// 在当前渲染循环代码中，每一帧所用的同步对象是相同的，这意味着必须渲染完上一帧才能渲染当前帧
	// 通过给交换链中的每张图像创建一套专用的同步对象、命令缓冲区、帧缓冲，以及其他一切在循环的单帧中会被更新、写入的Vulkan对象，以此在渲染每一帧图像的过程中避免资源竞争，减少阻塞，这种做法叫做即时帧（frames in flight）
	while (!glfwWindowShouldClose(pWindow)) {
		// 出于节省CPU和GPU占用的考量，有必要在窗口最小化时阻塞渲染循环
		while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED))
			glfwWaitEvents();
		TitleFps();

		fence.WaitAndReset();
		graphicsBase::Base().SwapImage(semaphore_imageIsAvailable);
		auto i = graphicsBase::Base().CurrentImageIndex();

		commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		renderPass.CmdBegin(commandBuffer, framebuffers[i], { {}, windowSize }, clearColor);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		renderPass.CmdEnd(commandBuffer);
		commandBuffer.End();

		// 将命令缓冲区提交到图形队列时，最迟可以在VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT阶段等待获取交换链图像索引，渲染结果在该阶段被写入到交换链图像
		graphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable, semaphore_renderingIsOver, fence);
		graphicsBase::Base().PresentImage(semaphore_renderingIsOver);

		glfwPollEvents();
	}
	TerminateWindow();
	return 0;
}
