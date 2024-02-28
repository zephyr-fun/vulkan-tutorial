#include "VKBase.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#pragma comment(lib, "glfw3.lib")

// 窗口指针，全局变量自动初始化为NULL
GLFWwindow* pWindow;
// 显示器对象指针
GLFWmonitor* pMonitor;
// 窗口标题
const char* windowTitle = "EasyVK";

// 初始化窗口
bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true) {
	using namespace vulkan;

	if (!glfwInit()) {
		std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n");
		return false;
	}
	// 向GLFW说明不需要OpenGL的API, 不需要在创建窗口时创建OpenGL的上下文
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// 指定窗口可否拉伸
	glfwWindowHint(GLFW_RESIZABLE, isResizable);
	// 实现透明窗口背景
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);
	// 获取显示器指针，它的作用之一是在创建全屏窗口时作为glfwCreateWindow的第四个参数
	pMonitor = glfwGetPrimaryMonitor();
	// 获取显示器当前的视频模式, 视频模式可能因为用户的操作而在程序运行过程中发生变更，因此总是在需要时获取，而不将其存储到全局变量
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	// 创建窗口
	pWindow = fullScreen ?
		glfwCreateWindow(pMode->width, pMode->height, windowTitle, pMonitor, nullptr) :
		glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);
	// 创建失败时, 用glfwTerminate()来清理GLFW并让函数返回false
	if (!pWindow) {
		std::cout << std::format("[ InitializeWindow ]\nFailed to create a glfw window!\n");
		glfwTerminate();
		return false;
	}

#ifdef _WIN32
	graphicsBase::Base().PushInstanceExtension("VK_KHR_surface");
	graphicsBase::Base().PushInstanceExtension("VK_KHR_win32_surface");
#else
	uint32_t extensionCount = 0;
	const char** extensionNames;
	// 获取平台所需的扩展，若执行成功，返回一个指针，指向一个由所需扩展的名称为元素的数组，失败则返回nullptr，并意味着此设备不支持Vulkan
	extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
	if (!extensionNames) {
		std::cout << std::format("[ InitializeWindow ]\nVulkan is not available on this machine!\n");
		glfwTerminate();
		return false;
	}
	for (size_t i = 0; i < extensionCount; i++)
		graphicsBase::Base().PushInstanceExtension(extensionNames[i]);
#endif
	graphicsBase::Base().PushDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	// 获取最新api版本
	graphicsBase::Base().UseLatestApiVersion();
	// 创建vulkan instance
	if (graphicsBase::Base().CreateInstance())
		return false;

	// 创建window surface
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	// glfwCreateWindowSurface需要vulkan实例对象, window surface会在后续创建swap chain时被使用
	if (VkResult result = glfwCreateWindowSurface(vulkan::graphicsBase::Base().Instance(), pWindow, nullptr, &surface)) {
		std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n", int32_t(result));
		glfwTerminate();
		return false;
	}
	graphicsBase::Base().Surface(surface);

	// 选择物理设备
	if (vulkan::graphicsBase::Base().GetPhysicalDevices() ||
		vulkan::graphicsBase::Base().DeterminePhysicalDevice(0, true, false) ||
		vulkan::graphicsBase::Base().CreateDevice())
		return false;

	// 创建swap chain
	if (graphicsBase::Base().CreateSwapchain(limitFrameRate))
		return false;

	return true;
}

// 终止窗口
void TerminateWindow() {
	vulkan::graphicsBase::Base().WaitIdle();
	glfwTerminate();
}

// 全屏化
void MakeWindowFullScreen() {
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	glfwSetWindowMonitor(pWindow, pMonitor, 0, 0, pMode->width, pMode->height, pMode->refreshRate);
}

// 窗口化
void MakeWindowWindowed(VkOffset2D position, VkExtent2D size) {
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	glfwSetWindowMonitor(pWindow, nullptr, position.x, position.y, size.width, size.height, pMode->refreshRate);
}

// 窗口标题显示fps
void TitleFps() {
	static double time0 = glfwGetTime();
	static double time1;
	static double dt;
	static int dframe = -1;
	static std::stringstream info;
	time1 = glfwGetTime();
	dframe++;
	if ((dt = time1 - time0) >= 1) {
		info.precision(1);
		info << windowTitle << "    " << std::fixed << dframe / dt << " FPS";
		// glfwSetWindowTitle用于设置窗口标题
		glfwSetWindowTitle(pWindow, info.str().c_str());
		info.str("");
		time0 = time1;
		dframe = 0;
	}
}
