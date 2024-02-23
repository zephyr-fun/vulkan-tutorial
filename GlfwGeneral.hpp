#include "VKBase.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#pragma comment(lib, "glfw3.lib")

// ����ָ�룬ȫ�ֱ����Զ���ʼ��ΪNULL
GLFWwindow* pWindow;
// ��ʾ������ָ��
GLFWmonitor* pMonitor;
// ���ڱ���
const char* windowTitle = "EasyVK";

// ��ʼ������
bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true) {
	using namespace vulkan;

	if (!glfwInit()) {
		std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n");
		return false;
	}
	// ��GLFW˵������ҪOpenGL��API, ����Ҫ�ڴ�������ʱ����OpenGL��������
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// ָ�����ڿɷ�����
	glfwWindowHint(GLFW_RESIZABLE, isResizable);
	// ʵ��͸�����ڱ���
	glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, true);
	// ��ȡ��ʾ��ָ�룬��������֮һ���ڴ���ȫ������ʱ��ΪglfwCreateWindow�ĵ��ĸ�����
	pMonitor = glfwGetPrimaryMonitor();
	// ��ȡ��ʾ����ǰ����Ƶģʽ, ��Ƶģʽ������Ϊ�û��Ĳ������ڳ������й����з�������������������Ҫʱ��ȡ����������洢��ȫ�ֱ���
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	// ��������
	pWindow = fullScreen ?
		glfwCreateWindow(pMode->width, pMode->height, windowTitle, pMonitor, nullptr) :
		glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);
	// ����ʧ��ʱ, ��glfwTerminate()������GLFW���ú�������false
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
	// ��ȡƽ̨�������չ����ִ�гɹ�������һ��ָ�룬ָ��һ����������չ������ΪԪ�ص����飬ʧ���򷵻�nullptr������ζ�Ŵ��豸��֧��Vulkan
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
	// ��ȡ����api�汾
	graphicsBase::Base().UseLatestApiVersion();
	// ����vulkan instance
	if (graphicsBase::Base().CreateInstance())
		return false;

	// ����window surface
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	// glfwCreateWindowSurface��Ҫvulkanʵ������, window surface���ں�������swap chainʱ��ʹ��
	if (VkResult result = glfwCreateWindowSurface(vulkan::graphicsBase::Base().Instance(), pWindow, nullptr, &surface)) {
		std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n", int32_t(result));
		glfwTerminate();
		return false;
	}
	graphicsBase::Base().Surface(surface);

	// ѡ�������豸
	if (vulkan::graphicsBase::Base().GetPhysicalDevices() ||
		vulkan::graphicsBase::Base().DeterminePhysicalDevice(0, true, false) ||
		vulkan::graphicsBase::Base().CreateDevice())
		return false;

	// ����swap chain
	if (graphicsBase::Base().CreateSwapchain(limitFrameRate))
		return false;

	return true;
}

// ��ֹ����
void TerminateWindow() {
	vulkan::graphicsBase::Base().WaitIdle();
	glfwTerminate();
}

// ȫ����
void MakeWindowFullScreen() {
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	glfwSetWindowMonitor(pWindow, pMonitor, 0, 0, pMode->width, pMode->height, pMode->refreshRate);
}

// ���ڻ�
void MakeWindowWindowed(VkOffset2D position, VkExtent2D size) {
	const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
	glfwSetWindowMonitor(pWindow, nullptr, position.x, position.y, size.width, size.height, pMode->refreshRate);
}

// ���ڱ�����ʾfps
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
		// glfwSetWindowTitle�������ô��ڱ���
		glfwSetWindowTitle(pWindow, info.str().c_str());
		info.str("");
		time0 = time1;
		dframe = 0;
	}
}