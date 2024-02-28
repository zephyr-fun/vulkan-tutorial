#pragma once
#include "EasyVKStart.h"
#define VK_RESULT_THROW

#define DestroyHandleBy(Func) if (handle) { Func(graphicsBase::Base().Device(), handle, nullptr); handle = VK_NULL_HANDLE; }
#define MoveHandle handle = other.handle; other.handle = VK_NULL_HANDLE;
#define DefineHandleTypeOperator operator decltype(handle)() const { return handle; }
#define DefineAddressFunction const decltype(handle)* Address() const { return &handle; }

#ifndef NDEBUG
#define ENABLE_DEBUG_MESSENGER true
#else
#define ENABLE_DEBUG_MESSENGER false
#endif

// vulkan命名空间中主要封装了一些vulkan中的基本对象
namespace vulkan {
	constexpr VkExtent2D defaultWindowSize = { 1280, 720 };
	inline auto& outStream = std::cout;

#ifdef VK_RESULT_THROW
	class result_t {
		VkResult result;
	public:
		static void(*callback_throw)(VkResult);
		result_t(VkResult result) :result(result) {}
		result_t(result_t&& other) noexcept :result(other.result) { other.result = VK_SUCCESS; }
		~result_t() noexcept(false) {
			if (uint32_t(result) < VK_RESULT_MAX_ENUM)
				return;
			if (callback_throw)
				callback_throw(result);
			throw result;
		}
		operator VkResult() {
			VkResult result = this->result;
			this->result = VK_SUCCESS;
			return result;
		}
	};
	inline void(*result_t::callback_throw)(VkResult);

#elif defined VK_RESULT_NODISCARD
	struct [[nodiscard]] result_t {
		VkResult result;
		result_t(VkResult result) :result(result) {}
		operator VkResult() const { return result; }
	};
#pragma warning(disable:4834)
#pragma warning(disable:6031)
#else
	using result_t = VkResult;
#endif

	class graphicsBase {
		uint32_t apiVersion = VK_API_VERSION_1_0;
		// 单例类对象是静态的，未设定初始值亦无构造函数的成员会被零初始化
		VkInstance instance;
		VkPhysicalDevice physicalDevice;
		VkPhysicalDeviceProperties physicalDeviceProperties;
		VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
		std::vector<VkPhysicalDevice> availablePhysicalDevices;

		VkDevice device;
		// 有效的索引从0开始，因此使用特殊值VK_QUEUE_FAMILY_IGNORED（为UINT32_MAX）为队列族索引的默认值
		uint32_t queueFamilyIndex_graphics = VK_QUEUE_FAMILY_IGNORED;
		uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
		uint32_t queueFamilyIndex_compute = VK_QUEUE_FAMILY_IGNORED;
		VkQueue queue_graphics;
		VkQueue queue_presentation;
		VkQueue queue_compute;

		VkSurfaceKHR surface;
		std::vector <VkSurfaceFormatKHR> availableSurfaceFormats;

		VkSwapchainKHR swapchain;
		std::vector <VkImage> swapchainImages;
		std::vector <VkImageView> swapchainImageViews;
		// 当前swap chain图像索引
		uint32_t currentImageIndex = 0;
		// 保存交换链的创建信息以便重建交换链
		VkSwapchainCreateInfoKHR swapchainCreateInfo = {};

		std::vector<const char*> instanceLayers;
		std::vector<const char*> instanceExtensions;
		std::vector<const char*> deviceExtensions;

		VkDebugUtilsMessengerEXT debugUtilsMessenger;

		std::vector<void(*)()> callbacks_createSwapchain;
		std::vector<void(*)()> callbacks_destroySwapchain;
		std::vector<void(*)()> callbacks_createDevice;
		std::vector<void(*)()> callbacks_destroyDevice;

		// Static
		static graphicsBase singleton;
		//--------------------
		graphicsBase() = default;
		graphicsBase(graphicsBase&&) = delete;
		~graphicsBase() {
			if (!instance)
				return;
			if (device) {
				WaitIdle();
				if (swapchain) {
					for (auto& i : callbacks_destroySwapchain)
						i();
					for (auto& i : swapchainImageViews)
						if (i)
							vkDestroyImageView(device, i, nullptr);
					vkDestroySwapchainKHR(device, swapchain, nullptr);
				}
				for (auto& i : callbacks_destroyDevice)
					i();
				vkDestroyDevice(device, nullptr);
			}
			if (surface)
				vkDestroySurfaceKHR(instance, surface, nullptr);
			if (debugUtilsMessenger) {
				PFN_vkDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessenger =
					reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
				if (DestroyDebugUtilsMessenger)
					DestroyDebugUtilsMessenger(instance, debugUtilsMessenger, nullptr);
			}
			vkDestroyInstance(instance, nullptr);
		}
		// 被CreateSwapchain和RecreateSwapchain调用
		result_t CreateSwapchain_Internal() {
			// 根据swapchainCreateInfo创建swap chain
			if (VkResult result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain!\nError code: {}\n", int32_t(result));
				return result;
			}

			// 获取swap chain对应的images
			uint32_t swapchainImageCount;
			if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of swapchain images!\nError code: {}\n", int32_t(result));
				return result;
			}
			swapchainImages.resize(swapchainImageCount);
			if (VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data())) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", int32_t(result));
				return result;
			}

			// 为swap chain中的image创建image view, 图像视图（VkImageView）定义了图像的使用方式
			swapchainImageViews.resize(swapchainImageCount);
			VkImageViewCreateInfo imageViewCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = swapchainCreateInfo.imageFormat,
				//.components = {},
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			for (size_t i = 0; i < swapchainImageCount; i++) {
				imageViewCreateInfo.image = swapchainImages[i];
				if (VkResult result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i])) {
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a swapchain image view!\nError code: {}\n", int32_t(result));
					return result;
				}
			}
			return VK_SUCCESS;
		}

		// 该函数被DeterminePhysicalDevice调用，用于检查物理设备是否满足所需的队列族类型，并将对应的队列族索引返回到queueFamilyIndices，执行成功时直接将索引写入相应成员变量
		result_t GetQueueFamilyIndices(VkPhysicalDevice physicalDevice, bool enableGraphicsQueue, bool enableComputeQueue, uint32_t(&queueFamilyIndices)[3]) {
			uint32_t queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
			if (!queueFamilyCount)
				return VK_RESULT_MAX_ENUM;
			std::vector<VkQueueFamilyProperties> queueFamilyPropertieses(queueFamilyCount);
			vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyPropertieses.data());
			auto& [ig, ip, ic] = queueFamilyIndices;
			ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;
			for (uint32_t i = 0; i < queueFamilyCount; i++) {
				VkBool32
					supportGraphics = enableGraphicsQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_GRAPHICS_BIT,
					supportPresentation = false,
					supportCompute = enableComputeQueue && queueFamilyPropertieses[i].queueFlags & VK_QUEUE_COMPUTE_BIT;
				if (surface)
					// 查询队列族是否支持将图像呈现到该surface
					if (VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation)) {
						outStream << std::format("[ graphicsBase ] ERROR\nFailed to determine if the queue family supports presentation!\nError code: {}\n", int32_t(result));
						return result;
					}
				if (supportGraphics && supportCompute) {
					if (supportPresentation) {
						ig = ip = ic = i;
						break;
					}
					if (ig != ic ||
						ig == VK_QUEUE_FAMILY_IGNORED)
						ig = ic = i;
					if (!surface)
						break;
				}
				if (supportGraphics &&
					ig == VK_QUEUE_FAMILY_IGNORED)
					ig = i;
				if (supportPresentation &&
					ip == VK_QUEUE_FAMILY_IGNORED)
					ip = i;
				if (supportCompute &&
					ic == VK_QUEUE_FAMILY_IGNORED)
					ic = i;
			}
			if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue ||
				ip == VK_QUEUE_FAMILY_IGNORED && surface ||
				ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)
				return VK_RESULT_MAX_ENUM;
			queueFamilyIndex_graphics = ig;
			queueFamilyIndex_presentation = ip;
			queueFamilyIndex_compute = ic;
			return VK_SUCCESS;
		}
		result_t CreateDebugMessenger() {
			static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback = [](
				VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageTypes,
				const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
				void* pUserData)->VkBool32 {
					outStream << std::format("{}\n\n", pCallbackData->pMessage);
					return VK_FALSE;
				};
			VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = DebugUtilsMessengerCallback
			};
			PFN_vkCreateDebugUtilsMessengerEXT CreateDebugUtilsMessenger =
				reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
			if (CreateDebugUtilsMessenger) {
				VkResult result = CreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugUtilsMessenger);
				if (result)
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n", int32_t(result));
				return result;
			}
			outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
			return VK_RESULT_MAX_ENUM;
		}
		// Static Function, 用于向instanceLayers或instanceExtensions容器中添加不重复的字符串指针
		static void AddLayerOrExtension(std::vector<const char*>& container, const char* name) {
			for (auto& i : container)
				if (!strcmp(name, i))
					return;
			container.push_back(name);
		}
	public:
		//Getter
		uint32_t ApiVersion() const {
			return apiVersion;
		}
		VkInstance Instance() const {
			return instance;
		}
		VkPhysicalDevice PhysicalDevice() const {
			return physicalDevice;
		}
		const VkPhysicalDeviceProperties& PhysicalDeviceProperties() const {
			return physicalDeviceProperties;
		}
		const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const {
			return physicalDeviceMemoryProperties;
		}
		VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const {
			return availablePhysicalDevices[index];
		}
		uint32_t AvailablePhysicalDeviceCount() const {
			return uint32_t(availablePhysicalDevices.size());
		}

		VkDevice Device() const {
			return device;
		}
		uint32_t QueueFamilyIndex_Graphics() const {
			return queueFamilyIndex_graphics;
		}
		uint32_t QueueFamilyIndex_Presentation() const {
			return queueFamilyIndex_presentation;
		}
		uint32_t QueueFamilyIndex_Compute() const {
			return queueFamilyIndex_compute;
		}
		VkQueue Queue_Graphics() const {
			return queue_graphics;
		}
		VkQueue Queue_Presentation() const {
			return queue_presentation;
		}
		VkQueue Queue_Compute() const {
			return queue_compute;
		}

		VkSurfaceKHR Surface() const {
			return surface;
		}
		const VkFormat& AvailableSurfaceFormat(uint32_t index) const {
			return availableSurfaceFormats[index].format;
		}
		const VkColorSpaceKHR& AvailableSurfaceColorSpace(uint32_t index) const {
			return availableSurfaceFormats[index].colorSpace;
		}
		uint32_t AvailableSurfaceFormatCount() const {
			return uint32_t(availableSurfaceFormats.size());
		}

		VkSwapchainKHR Swapchain() const {
			return swapchain;
		}
		VkImage SwapchainImage(uint32_t index) const {
			return swapchainImages[index];
		}

		VkImageView SwapchainImageView(uint32_t index) const {
			return swapchainImageViews[index];
		}
		uint32_t SwapchainImageCount() const {
			return uint32_t(swapchainImages.size());
		}

		uint32_t CurrentImageIndex() const {
			return currentImageIndex;
		}

		const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const {
			return swapchainCreateInfo;
		}

		const std::vector<const char*>& InstanceLayers() const {
			return instanceLayers;
		}
		const std::vector<const char*>& InstanceExtensions() const {
			return instanceExtensions;
		}
		const std::vector<const char*>& DeviceExtensions() const {
			return deviceExtensions;
		}

		//Const Function
		VkResult WaitIdle() const {
			VkResult result = vkDeviceWaitIdle(device);
			if (result)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: {}\n", int32_t(result));
			return result;
		}

		//Non-const Function
		void PushCallback_CreateSwapchain(void(*function)()) {
			callbacks_createSwapchain.push_back(function);
		}
		void PushCallback_DestroySwapchain(void(*function)()) {
			callbacks_destroySwapchain.push_back(function);
		}
		void PushCallback_CreateDevice(void(*function)()) {
			callbacks_createDevice.push_back(function);
		}
		void PushCallback_DestroyDevice(void(*function)()) {
			callbacks_destroyDevice.push_back(function);
		}

		// 用于创建Vulkan实例前
		void PushInstanceLayer(const char* layerName) {
			AddLayerOrExtension(instanceLayers, layerName);
		}
		void PushInstanceExtension(const char* extensionName) {
			AddLayerOrExtension(instanceExtensions, extensionName);
		}
		result_t UseLatestApiVersion() {
			if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
				return vkEnumerateInstanceVersion(&apiVersion);
			return VK_SUCCESS;
		}

		// 用于创建Vulkan实例
		result_t CreateInstance(const void* pNext = nullptr, VkInstanceCreateFlags flags = 0) {
			if constexpr (ENABLE_DEBUG_MESSENGER)
				PushInstanceLayer("VK_LAYER_KHRONOS_validation"),
				PushInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			VkApplicationInfo applicatianInfo = {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.apiVersion = apiVersion
			};
			VkInstanceCreateInfo instanceCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
				.pNext = pNext,
				.flags = flags,
				.pApplicationInfo = &applicatianInfo,
				.enabledLayerCount = uint32_t(instanceLayers.size()),
				.ppEnabledLayerNames = instanceLayers.data(),
				.enabledExtensionCount = uint32_t(instanceExtensions.size()),
				.ppEnabledExtensionNames = instanceExtensions.data()
			};
			if (VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
				return result;
			}
			outStream << std::format(
				"Vulkan API Version: {}.{}.{}\n",
				VK_VERSION_MAJOR(apiVersion),
				VK_VERSION_MINOR(apiVersion),
				VK_VERSION_PATCH(apiVersion));
			if constexpr (ENABLE_DEBUG_MESSENGER)
				CreateDebugMessenger();
			return VK_SUCCESS;
		}

		// 检查所需layers是否可用, 不可用则将传入参数对应位置空
		result_t CheckInstanceLayers(arrayRef<const char*> layersToCheck) const {
			uint32_t layerCount;
			std::vector<VkLayerProperties> availableLayers;
			if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance layers!\n");
				return result;
			}
			if (layerCount) {
				availableLayers.resize(layerCount);
				if (VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data())) {
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: {}\n", int32_t(result));
					return result;
				}
				for (auto& i : layersToCheck) {
					bool found = false;
					for (auto& j : availableLayers)
						if (!strcmp(i, j.layerName)) {
							found = true;
							break;
						}
					if (!found)
						i = nullptr;
				}
			}
			else
				for (auto& i : layersToCheck)
					i = nullptr;
			return VK_SUCCESS;
		}

		// 检查所需extensions是否可用, 不可用则将传入参数对应位置空
		result_t CheckInstanceExtensions(arrayRef<const char*> extensionsToCheck, const char* layerName) const {
			uint32_t extensionCount;
			std::vector<VkExtensionProperties> availableExtensions;
			if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr)) {
				layerName ?
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:{}\n", layerName) :
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\n");
				return result;
			}
			if (extensionCount) {
				availableExtensions.resize(extensionCount);
				if (VkResult result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data())) {
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: {}\n", int32_t(result));
					return result;
				}
				for (auto& i : extensionsToCheck) {
					bool found = false;
					for (auto& j : availableExtensions)
						if (!strcmp(i, j.extensionName)) {
							found = true;
							break;
						}
					if (!found)
						i = nullptr;
				}
			}
			else
				for (auto& i : extensionsToCheck)
					i = nullptr;
			return VK_SUCCESS;
		}
		void InstanceLayers(const std::vector<const char*>& layerNames) {
			instanceLayers = layerNames;
		}
		void InstanceExtensions(const std::vector<const char*>& extensionNames) {
			instanceExtensions = extensionNames;
		}

		// Set Window Surface, 该函数用于选择物理设备前
		void Surface(VkSurfaceKHR surface) {
			if (!this->surface)
				this->surface = surface;
		}

		// 用于创建逻辑设备前
		void PushDeviceExtension(const char* extensionName) {
			AddLayerOrExtension(deviceExtensions, extensionName);
		}
		
		// 用于获取物理设备
		result_t GetPhysicalDevices() {
			uint32_t deviceCount;
			if (VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: {}\n", int32_t(result));
				return result;
			}
			if (!deviceCount)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!\n"),
				abort();
			availablePhysicalDevices.resize(deviceCount);
			VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
			if (result)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
			return result;
		}

		// 用于判断物理设备上的队列族是否满足要求
		result_t DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true) {
			static constexpr uint32_t notFound = INT32_MAX;//== VK_QUEUE_FAMILY_IGNORED & INT32_MAX
			struct queueFamilyIndexCombination {
				uint32_t graphics = VK_QUEUE_FAMILY_IGNORED;
				uint32_t presentation = VK_QUEUE_FAMILY_IGNORED;
				uint32_t compute = VK_QUEUE_FAMILY_IGNORED;
			};
			static std::vector<queueFamilyIndexCombination> queueFamilyIndexCombinations(availablePhysicalDevices.size());
			auto& [ig, ip, ic] = queueFamilyIndexCombinations[deviceIndex];
			if (ig == notFound && enableGraphicsQueue ||
				ip == notFound && surface ||
				ic == notFound && enableComputeQueue)
				return VK_RESULT_MAX_ENUM;
			if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue ||
				ip == VK_QUEUE_FAMILY_IGNORED && surface ||
				ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue) {
				uint32_t indices[3];
				VkResult result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex], enableGraphicsQueue, enableComputeQueue, indices);
				if (result == VK_SUCCESS ||
					result == VK_RESULT_MAX_ENUM) {
					if (enableGraphicsQueue)
						ig = indices[0] & INT32_MAX;
					if (surface)
						ip = indices[1] & INT32_MAX;
					if (enableComputeQueue)
						ic = indices[2] & INT32_MAX;
				}
				if (result)
					return result;
			}
			else {
				queueFamilyIndex_graphics = enableGraphicsQueue ? ig : VK_QUEUE_FAMILY_IGNORED;
				queueFamilyIndex_presentation = surface ? ip : VK_QUEUE_FAMILY_IGNORED;
				queueFamilyIndex_compute = enableComputeQueue ? ic : VK_QUEUE_FAMILY_IGNORED;
			}
			physicalDevice = availablePhysicalDevices[deviceIndex];
			return VK_SUCCESS;
		}

		/// 创建逻辑设备
		result_t CreateDevice(const void* pNext = nullptr, VkDeviceCreateFlags flags = 0) {
			float queuePriority = 1.f;
			VkDeviceQueueCreateInfo queueCreateInfos[3] = {
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueCount = 1,
					.pQueuePriorities = &queuePriority },
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueCount = 1,
					.pQueuePriorities = &queuePriority },
				{
					.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
					.queueCount = 1,
					.pQueuePriorities = &queuePriority } };
			uint32_t queueCreateInfoCount = 0;
			if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
				queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
			if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
				queueFamilyIndex_presentation != queueFamilyIndex_graphics)
				queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
			if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
				queueFamilyIndex_compute != queueFamilyIndex_graphics &&
				queueFamilyIndex_compute != queueFamilyIndex_presentation)
				queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;

			// test features
			VkPhysicalDeviceFeatures2 deviceFeatures = {};
			deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			VkPhysicalDeviceVulkan13Features deviceFeatures13 = {};
			deviceFeatures.pNext = &deviceFeatures13;
			vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures);
			std::cout << std::format("[ physical device features ] : dynamicRendering is {}\n", deviceFeatures13.dynamicRendering);
			// test features
			// 获取物理设备的设备特性
			VkPhysicalDeviceFeatures physicalDeviceFeatures;
			vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);
			VkDeviceCreateInfo deviceCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
				.pNext = pNext,
				.flags = flags,
				.queueCreateInfoCount = queueCreateInfoCount,
				.pQueueCreateInfos = queueCreateInfos,
				.enabledExtensionCount = uint32_t(deviceExtensions.size()),
				.ppEnabledExtensionNames = deviceExtensions.data(),
				.pEnabledFeatures = &physicalDeviceFeatures
			};
			if (VkResult result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!\nError code: {}\n", int32_t(result));
				return result;
			}
			if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
				vkGetDeviceQueue(device, queueFamilyIndex_graphics, 0, &queue_graphics);
			if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED)
				vkGetDeviceQueue(device, queueFamilyIndex_presentation, 0, &queue_presentation);
			if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED)
				vkGetDeviceQueue(device, queueFamilyIndex_compute, 0, &queue_compute);
			vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
			vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
			// 输出所用的物理设备的名称
			outStream << std::format("Renderer: {}\n", physicalDeviceProperties.deviceName);
			return VK_SUCCESS;
		}

		/// 用于创建逻辑设备失败后, 尝试去掉非必要拓展并重新创建逻辑设备
		result_t CheckDeviceExtensions(arrayRef<const char*> extensionsToCheck, const char* layerName = nullptr) const {
			uint32_t extensionCount;
			std::vector<VkExtensionProperties> availableExtensions;
			if (VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, &extensionCount, nullptr)) {
				layerName ?
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of device extensions!\nLayer name:{}\n", layerName) :
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of device extensions!\n");
				return result;
			}
			if (extensionCount) {
				availableExtensions.resize(extensionCount);
				if (VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, layerName, &extensionCount, availableExtensions.data())) {
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate device extension properties!\nError code: {}\n", int32_t(result));
					return result;
				}
				for (auto& i : extensionsToCheck) {
					bool found = false;
					for (auto& j : availableExtensions)
						if (!strcmp(i, j.extensionName)) {
							found = true;
							break;
						}
					if (!found)
						i = nullptr;//If a required extension isn't available, set it to nullptr
				}
			}
			else
				for (auto& i : extensionsToCheck)
					i = nullptr;
			return VK_SUCCESS;
		}
		void DeviceExtensions(const std::vector<const char*>& extensionNames) {
			deviceExtensions = extensionNames;
		}
		//                    Create Swapchain
		result_t GetSurfaceFormats() {
			uint32_t surfaceFormatCount;
			if (VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: {}\n", int32_t(result));
				return result;
			}
			if (!surfaceFormatCount)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any supported surface format!\n"),
				abort();
			availableSurfaceFormats.resize(surfaceFormatCount);
			VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, availableSurfaceFormats.data());
			if (result)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: {}\n", int32_t(result));
			return result;
		}
		result_t SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat) {
			// 不考虑没有指定色彩空间的情况，一是因为色彩空间VkColorSpaceKHR的零值为VK_COLOR_SPACE_SRGB_NONLINEAR_KHR，是个有效的色彩空间，二是因为不同色彩空间对后续操作（比如贴图的图像格式、片段着色器最后的色调映射）影响很大，没理由允许任意的色彩空间
			bool formatIsAvailable = false;
			if (!surfaceFormat.format) {
				for (auto& i : availableSurfaceFormats)
					if (i.colorSpace == surfaceFormat.colorSpace) {
						swapchainCreateInfo.imageFormat = i.format;
						swapchainCreateInfo.imageColorSpace = i.colorSpace;
						formatIsAvailable = true;
						break;
					}
			}
			else
				for (auto& i : availableSurfaceFormats)
					if (i.format == surfaceFormat.format &&
						i.colorSpace == surfaceFormat.colorSpace) {
						swapchainCreateInfo.imageFormat = i.format;
						swapchainCreateInfo.imageColorSpace = i.colorSpace;
						formatIsAvailable = true;
						break;
					}
			if (!formatIsAvailable)
				return VK_ERROR_FORMAT_NOT_SUPPORTED;
			if (swapchain)
				return RecreateSwapchain();
			return VK_SUCCESS;
		}
		result_t CreateSwapchain(bool limitFrameRate = true, const void* pNext = nullptr, VkSwapchainCreateFlagsKHR flags = 0) {
			// Get surface capabilities, 相关的参数有：交换链图像的数量、尺寸、视点数、变换、透明通道的方式、图像的用途。
			VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
			if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
				return result;
			}
			// Set image count, swap chain中的image数量最好不要太少，避免阻塞，同时不要太多，避免占用过多显存
			swapchainCreateInfo.minImageCount = surfaceCapabilities.minImageCount + (surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);
			// Set image extent, 设置image尺寸
			swapchainCreateInfo.imageExtent =
				surfaceCapabilities.currentExtent.width == -1 ?
				VkExtent2D{
				glm::clamp(defaultWindowSize.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
				glm::clamp(defaultWindowSize.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height) } :
				surfaceCapabilities.currentExtent;
			// Set transformation
			swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
			// Set alpha compositing mode, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR表示透明度的处理方式由应用程序的其他部分（Vulkan以外的部分）指定
			if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
				swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
			else
				for (size_t i = 0; i < 4; i++)
					if (surfaceCapabilities.supportedCompositeAlpha & 1 << i) {
						swapchainCreateInfo.compositeAlpha = VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & 1 << i);
						break;
					}
			// Set image usage, image必需能被用作颜色附件
			swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			// 如果image能够被用作transfer_src的话，就可以实现窗口截屏
			if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
				swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
			// 如果image能够被用作transfer_dst的话，就可以使用vkCmdClearColorImage清屏，使用vkCmdBlitImage将图像直接搬到屏幕上(不必渲染)
			if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
				swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			else
				outStream << std::format("[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!\n");

			// Get surface formats
			if (!availableSurfaceFormats.size())
				if (VkResult result = GetSurfaceFormats())
					return result;
			// If surface format is not determined, select a a four-component UNORM format, 在未指定的情况下使用短路&&来尝试设置两种常见格式
			if (!swapchainCreateInfo.imageFormat)
				if (SetSurfaceFormat({ VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR }) &&
					SetSurfaceFormat({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })) {
					swapchainCreateInfo.imageFormat = availableSurfaceFormats[0].format;
					swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
					outStream << std::format("[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!\n");
				}

			// Get surface present modes
			// 由于VK_PRESENT_MODE_IMMEDIATE_KHR和VK_PRESENT_MODE_FIFO_RELAXED_KHR可能导致画面撕裂
			// 在不需要限制帧率时应当选择VK_PRESENT_MODE_MAILBOX_KHR，需要限制帧率使其最大不超过屏幕刷新率时应选择VK_PRESENT_MODE_FIFO_KHR
			uint32_t surfacePresentModeCount;
			if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the count of surface present modes!\nError code: {}\n", int32_t(result));
				return result;
			}
			if (!surfacePresentModeCount)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to find any surface present mode!\n"),
				abort();
			std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
			if (VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data())) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: {}\n", int32_t(result));
				return result;
			}
			//Set present mode to mailbox if available and necessary
			swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
			if (!limitFrameRate)
				for (size_t i = 0; i < surfacePresentModeCount; i++)
					if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
						swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
						break;
					}

			swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			swapchainCreateInfo.pNext = pNext;
			swapchainCreateInfo.flags = flags;
			swapchainCreateInfo.surface = surface;
			swapchainCreateInfo.imageArrayLayers = 1;
			swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchainCreateInfo.clipped = VK_TRUE;

			if (VkResult result = CreateSwapchain_Internal())
				return result;
			for (auto& i : callbacks_createSwapchain)
				i();
			return VK_SUCCESS;
		}

		// After initialization
		void Terminate() {
			this->~graphicsBase();
			instance = VK_NULL_HANDLE;
			physicalDevice = VK_NULL_HANDLE;
			device = VK_NULL_HANDLE;
			surface = VK_NULL_HANDLE;
			swapchain = VK_NULL_HANDLE;
			swapchainImages.resize(0);
			swapchainImageViews.resize(0);
			swapchainCreateInfo = {};
			debugUtilsMessenger = VK_NULL_HANDLE;
		}

		// 重建逻辑设备
		result_t RecreateDevice(const void* pNext = nullptr, VkDeviceCreateFlags flags = 0) {
			// 等待逻辑设备空闲
			if (VkResult result = WaitIdle())
				return result;
			if (swapchain) {
				// 调用销毁交换链时的回调函数
				for (auto& i : callbacks_destroySwapchain)
					i();
				//销毁交换链图像的image view
				for (auto& i : swapchainImageViews)
					if (i)
						vkDestroyImageView(device, i, nullptr);
				swapchainImageViews.resize(0);
				// 销毁swap chain
				vkDestroySwapchainKHR(device, swapchain, nullptr);
				// 重置
				swapchain = VK_NULL_HANDLE;
				swapchainCreateInfo = {};
			}
			for (auto& i : callbacks_destroyDevice)
				i();
			if (device)
				vkDestroyDevice(device, nullptr),
				device = VK_NULL_HANDLE;
			return CreateDevice(pNext, flags);
		}

		// 在开关HDR或改变窗口大小时需要重建swap chain
		result_t RecreateSwapchain() {
			VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
			if (VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities)) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
				return result;
			}
			// 如果窗口显示区域的长或宽为0（通常发生在最小化到任务栏窗口时），不重建交换链
			if (surfaceCapabilities.currentExtent.width == 0 ||
				surfaceCapabilities.currentExtent.height == 0)
				return VK_SUCCESS;
			swapchainCreateInfo.imageExtent = surfaceCapabilities.currentExtent;
			swapchainCreateInfo.oldSwapchain = swapchain;
			// 保程序没有正在使用旧的交换链, 等待图形和呈现队列闲置
			VkResult result = vkQueueWaitIdle(queue_graphics);
			if (!result &&
				queue_graphics != queue_presentation)
				result = vkQueueWaitIdle(queue_presentation);
			if (result) {
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: {}\n", int32_t(result));
				return result;
			}

			for (auto& i : callbacks_destroySwapchain)
				i();
			for (auto& i : swapchainImageViews)
				if (i)
					vkDestroyImageView(device, i, nullptr);
			swapchainImageViews.resize(0);
			if (result = CreateSwapchain_Internal())
				return result;
			for (auto& i : callbacks_createSwapchain)
				i();
			return VK_SUCCESS;
		}

		// 用于获取交换链图像索引到currentImageIndex，以及在需要重建交换链时调用RecreateSwapchain()、重建交换链后销毁旧交换链
		result_t SwapImage(VkSemaphore semaphore_imageIsAvailable) {
			// 销毁旧swap chain, 逻辑是如果在当前帧重建交换链，那么在下一帧销毁交换链
			if (swapchainCreateInfo.oldSwapchain &&
				swapchainCreateInfo.oldSwapchain != swapchain) {
				vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
				swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
			}
			// 获取可用的swap chain图像索引
			while (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_imageIsAvailable, VK_NULL_HANDLE, &currentImageIndex))
				switch (result) {
				case VK_SUBOPTIMAL_KHR:
				case VK_ERROR_OUT_OF_DATE_KHR:
					if (VkResult result = RecreateSwapchain())
						return result;
					break; // 注意重建交换链后仍需要获取图像，通过break，再次执行while的条件判定语句
				default:
					outStream << std::format("[ graphicsBase ] ERROR\nFailed to acquire the next image!\nError code: {}\n", int32_t(result));
					return result;
				}
			return VK_SUCCESS;
		}

		// 将command buffer提交到用于图形的队列
		result_t SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const {
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			VkResult result = vkQueueSubmit(queue_graphics, 1, &submitInfo, fence);
			if (result)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
			return result;
		}

		// 一种提交command buffer的常见情形, 带需要等待的信号量，命令完成后需要置位的信号量和栅栏
		result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer,
			VkSemaphore semaphore_imageIsAvailable = VK_NULL_HANDLE, VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE) const {
			static constexpr VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			VkSubmitInfo submitInfo = {
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer
			};
			if (semaphore_imageIsAvailable)
				submitInfo.waitSemaphoreCount = 1,
				submitInfo.pWaitSemaphores = &semaphore_imageIsAvailable,
				submitInfo.pWaitDstStageMask = &waitDstStage;
			if (semaphore_renderingIsOver)
				submitInfo.signalSemaphoreCount = 1,
				submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
			return SubmitCommandBuffer_Graphics(submitInfo, fence);
		}

		// 只使用栅栏的常见情形
		result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const {
			VkSubmitInfo submitInfo = {
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer
			};
			return SubmitCommandBuffer_Graphics(submitInfo, fence);
		}

		// 将command buffer提交到用于计算的队列, 只是用栅栏的情形
		result_t SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const {
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			VkResult result = vkQueueSubmit(queue_compute, 1, &submitInfo, fence);
			if (result)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: {}\n", int32_t(result));
			return result;
		}

		// 将command buffer提交到用于计算的队列
		result_t SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const {
			VkSubmitInfo submitInfo = {
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer
			};
			return SubmitCommandBuffer_Compute(submitInfo, fence);
		}

		// 将command buffer提交到用于计算的队列, 带需要等待的信号量，命令完成后需要置位的信号量和栅栏
		result_t SubmitCommandBuffer_Presentation(VkCommandBuffer commandBuffer,
			VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE, VkSemaphore semaphore_ownershipIsTransfered = VK_NULL_HANDLE, VkFence fence = VK_NULL_HANDLE) const {
			static constexpr VkPipelineStageFlags waitDstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
			VkSubmitInfo submitInfo = {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.commandBufferCount = 1,
				.pCommandBuffers = &commandBuffer
			};
			if (semaphore_renderingIsOver)
				submitInfo.waitSemaphoreCount = 1,
				submitInfo.pWaitSemaphores = &semaphore_renderingIsOver,
				submitInfo.pWaitDstStageMask = &waitDstStage;
			if (semaphore_ownershipIsTransfered)
				submitInfo.signalSemaphoreCount = 1,
				submitInfo.pSignalSemaphores = &semaphore_ownershipIsTransfered;
			VkResult result = vkQueueSubmit(queue_presentation, 1, &submitInfo, fence);
			if (result)
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to submit the presentation command buffer!\nError code: {}\n", int32_t(result));
			return result;
		}

		void CmdTransferImageOwnership(VkCommandBuffer commandBuffer) const {
			VkImageMemoryBarrier imageMemoryBarrier_g2p = {
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
				.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask = 0,
				.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
				.srcQueueFamilyIndex = queueFamilyIndex_graphics,
				.dstQueueFamilyIndex = queueFamilyIndex_presentation,
				.image = swapchainImages[currentImageIndex],
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0,
				0, nullptr, 0, nullptr, 1, &imageMemoryBarrier_g2p);
		}

		result_t PresentImage(VkPresentInfoKHR& presentInfo) {
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			switch (VkResult result = vkQueuePresentKHR(queue_presentation, &presentInfo)) {
			case VK_SUCCESS:
				return VK_SUCCESS;
			case VK_SUBOPTIMAL_KHR:
			case VK_ERROR_OUT_OF_DATE_KHR:
				return RecreateSwapchain();
			default:
				outStream << std::format("[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError code: {}\n", int32_t(result));
				return result;
			}
		}

		result_t PresentImage(VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE) {
			VkPresentInfoKHR presentInfo = {
				.swapchainCount = 1,
				.pSwapchains = &swapchain,
				.pImageIndices = &currentImageIndex
			};
			if (semaphore_renderingIsOver)
				presentInfo.waitSemaphoreCount = 1,
				presentInfo.pWaitSemaphores = &semaphore_renderingIsOver;
			return PresentImage(presentInfo);
		}

		//Static Function
		static graphicsBase& Base() {
			return singleton;
		}
	};
	inline graphicsBase graphicsBase::singleton;
	
	// 信号量用于在队列之间进行同步
	class semaphore {
		VkSemaphore handle = VK_NULL_HANDLE;
	public:
		// semaphore() = default;
		semaphore(VkSemaphoreCreateInfo& createInfo) {
			Create(createInfo);
		}

		// 默认构造器创建未置位的信号量
		semaphore(/*reserved for future use*/) {
			Create();
		}

		semaphore(semaphore&& other) noexcept { MoveHandle; }

		~semaphore() { DestroyHandleBy(vkDestroySemaphore); }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Non-const Function
		result_t Create(VkSemaphoreCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			VkResult result = vkCreateSemaphore(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", int32_t(result));
			return result;
		}
		result_t Create(/*reserved for future use*/) {
			VkSemaphoreCreateInfo createInfo = {};
			return Create(createInfo);
		}
	};

	// 栅栏用于在cpu(app)侧和队列之间进行同步
	class fence {
		VkFence handle = VK_NULL_HANDLE;
	public:
		// fence() = default;
		fence(VkFenceCreateInfo& createInfo) {
			Create(createInfo);
		}

		// 默认构造器创建未置位的栅栏
		fence(VkFenceCreateFlags flags = 0) {
			Create(flags);
		}

		fence(fence&& other) noexcept { MoveHandle; }

		~fence() { DestroyHandleBy(vkDestroyFence); }

		// Getter, 转换为vulkan raw类型
		DefineHandleTypeOperator;

		DefineAddressFunction;

		// Const Function
		// 等待当前栅栏被置位
		result_t Wait() const {
			VkResult result = vkWaitForFences(graphicsBase::Base().Device(), 1, &handle, false, UINT64_MAX);
			if (result)
				outStream << std::format("[ fence ] ERROR\nFailed to wait for the fence!\nError code: {}\n", int32_t(result));
			return result;
		}

		// 将当前栅栏重置为未置位状态
		result_t Reset() const {
			VkResult result = vkResetFences(graphicsBase::Base().Device(), 1, &handle);
			if (result)
				outStream << std::format("[ fence ] ERROR\nFailed to reset the fence!\nError code: {}\n", int32_t(result));
			return result;
		}

		// 针对“等待后立刻重置”的情形的helper函数
		result_t WaitAndReset() const {
			VkResult result = Wait();
			result || (result = Reset());
			return result;
		}

		// vkGetFenceStatus(...)成功时有两种结果，所以不能仅仅判断result是否非0
		result_t Status() const {
			VkResult result = vkGetFenceStatus(graphicsBase::Base().Device(), handle);
			if (result < 0)
				outStream << std::format("[ fence ] ERROR\nFailed to get the status of the fence!\nError code: {}\n", int32_t(result));
			return result;
		}

		//Non-const Function
		result_t Create(VkFenceCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			VkResult result = vkCreateFence(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ fence ] ERROR\nFailed to create a fence!\nError code: {}\n", int32_t(result));
			return result;
		}

		result_t Create(VkFenceCreateFlags flags = 0) {
			VkFenceCreateInfo createInfo = {
				.flags = flags
			};
			return Create(createInfo);
		}
	};

	// 着色器模组
	class shaderModule {
		VkShaderModule handle = VK_NULL_HANDLE;
	public:
		shaderModule() = default;

		shaderModule(VkShaderModuleCreateInfo& createInfo) {
			Create(createInfo);
		}

		shaderModule(const char* filepath /*reserved for future use*/) {
			Create(filepath);
		}

		shaderModule(size_t codeSize, const uint32_t* pCode /*reserved for future use*/) {
			Create(codeSize, pCode);
		}

		shaderModule(shaderModule&& other) noexcept { MoveHandle; }

		~shaderModule() { DestroyHandleBy(vkDestroyShaderModule); }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Const Function
		VkPipelineShaderStageCreateInfo StageCreateInfo(VkShaderStageFlagBits stage, const char* entry = "main") const {
			return {
				VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,//sType
				nullptr,                                            //pNext
				0,                                                  //flags
				stage,                                              //stage
				handle,                                             //module
				entry,                                              //pName
				nullptr                                             //pSpecializationInfo, 默认shader中没有需要特化的常量
			};
		}

		//Non-const Function
		result_t Create(VkShaderModuleCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			VkResult result = vkCreateShaderModule(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ shader ] ERROR\nFailed to create a shader module!\nError code: {}\n", int32_t(result));
			return result;
		}

		result_t Create(size_t codeSize, const uint32_t* pCode /*reserved for future use*/) {
			VkShaderModuleCreateInfo createInfo = {
				.codeSize = codeSize,
				.pCode = pCode
			};
			return Create(createInfo);
		}

		result_t Create(const char* filepath /*reserved for future use*/) {
			std::ifstream file(filepath, std::ios::ate | std::ios::binary); // std::ios::ate 文件指针定位到文件末尾, std::ios::binary 以二进制格式打开
			if (!file) {
				outStream << std::format("[ shader ] ERROR\nFailed to open the file: {}\n", filepath);
				return VK_RESULT_MAX_ENUM; // No proper VkResult enum value, don't use VK_ERROR_UNKNOWN
			}
			size_t fileSize = size_t(file.tellg()); // 使用 file.tellg() 获取文件指针的当前位置（文件末尾），然后将其转换为 size_t 类型，得到文件的大小
			std::vector<uint32_t> binaries(fileSize / 4);
			file.seekg(0); // 将文件指针移动到文件的开始位置，准备读取文件内容
			file.read(reinterpret_cast<char*>(binaries.data()), fileSize);
			file.close();
			return Create(fileSize, binaries.data());
		}
	};

	// 管线布局
	class pipelineLayout {
		VkPipelineLayout handle = VK_NULL_HANDLE;
	public:
		pipelineLayout() = default;

		pipelineLayout(VkPipelineLayoutCreateInfo& createInfo) {
			Create(createInfo);
		}

		pipelineLayout(pipelineLayout&& other) noexcept { MoveHandle; }

		~pipelineLayout() { DestroyHandleBy(vkDestroyPipelineLayout); }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Non-const Function
		result_t Create(VkPipelineLayoutCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			VkResult result = vkCreatePipelineLayout(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ pipelineLayout ] ERROR\nFailed to create a pipeline layout!\nError code: {}\n", int32_t(result));
			return result;
		}
	};

	// 管线
	class pipeline {
		VkPipeline handle = VK_NULL_HANDLE;
	public:
		pipeline() = default;

		pipeline(VkGraphicsPipelineCreateInfo& createInfo) {
			Create(createInfo);
		}

		pipeline(VkComputePipelineCreateInfo& createInfo) {
			Create(createInfo);
		}

		pipeline(pipeline&& other) noexcept { MoveHandle; }

		~pipeline() { DestroyHandleBy(vkDestroyPipeline); }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Non-const Function
		result_t Create(VkGraphicsPipelineCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			VkResult result = vkCreateGraphicsPipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ pipeline ] ERROR\nFailed to create a graphics pipeline!\nError code: {}\n", int32_t(result));
			return result;
		}

		result_t Create(VkComputePipelineCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			VkResult result = vkCreateComputePipelines(graphicsBase::Base().Device(), VK_NULL_HANDLE, 1, &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ pipeline ] ERROR\nFailed to create a compute pipeline!\nError code: {}\n", int32_t(result));
			return result;
		}
	};

	// 渲染通道
	class renderPass {
		VkRenderPass handle = VK_NULL_HANDLE;
	public:
		renderPass() = default;

		renderPass(VkRenderPassCreateInfo& createInfo) {
			Create(createInfo);
		}

		renderPass(renderPass&& other) noexcept { MoveHandle; }

		~renderPass() { DestroyHandleBy(vkDestroyRenderPass); }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Const Function, 开始渲染通道
		void CmdBegin(VkCommandBuffer commandBuffer, VkRenderPassBeginInfo& beginInfo, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
			beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			beginInfo.renderPass = handle;
			vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
		}

		void CmdBegin(VkCommandBuffer commandBuffer, VkFramebuffer framebuffer, VkRect2D renderArea, arrayRef<const VkClearValue> clearValues = {}, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
			VkRenderPassBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass = handle,
				.framebuffer = framebuffer,
				.renderArea = renderArea,
				.clearValueCount = uint32_t(clearValues.Count()),
				.pClearValues = clearValues.Pointer()
			};
			vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
		}

		// 进入下一个子通道
		void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const {
			vkCmdNextSubpass(commandBuffer, subpassContents);
		}

		// 结束渲染通道
		void CmdEnd(VkCommandBuffer commandBuffer) const {
			vkCmdEndRenderPass(commandBuffer);
		}

		//Non-const Function
		result_t Create(VkRenderPassCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			VkResult result = vkCreateRenderPass(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n", int32_t(result));
			return result;
		}
	};

	// 帧缓冲区, 渲染缓冲区
	class framebuffer {
		VkFramebuffer handle = VK_NULL_HANDLE;
	public:
		framebuffer() = default;

		framebuffer(VkFramebufferCreateInfo& createInfo) {
			Create(createInfo);
		}

		framebuffer(framebuffer&& other) noexcept { MoveHandle; }

		~framebuffer() { DestroyHandleBy(vkDestroyFramebuffer); }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Non-const Function
		result_t Create(VkFramebufferCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			VkResult result = vkCreateFramebuffer(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n", int32_t(result));
			return result;
		}
	};

	class commandBuffer {
		friend class commandPool;
		VkCommandBuffer handle = VK_NULL_HANDLE;
	public:
		commandBuffer() = default;

		commandBuffer(commandBuffer&& other) noexcept { MoveHandle; }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Const Function
		result_t Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const {
			inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
			VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = usageFlags,
				.pInheritanceInfo = &inheritanceInfo
			};
			VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
			if (result)
				outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
			return result;
		}

		result_t Begin(VkCommandBufferUsageFlags usageFlags = 0) const {
			VkCommandBufferBeginInfo beginInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.flags = usageFlags,
			};
			VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
			if (result)
				outStream << std::format("[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {}\n", int32_t(result));
			return result;
		}

		result_t End() const {
			VkResult result = vkEndCommandBuffer(handle);
			if (result)
				outStream << std::format("[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: {}\n", int32_t(result));
			return result;
		}
	};

	class commandPool {
		VkCommandPool handle = VK_NULL_HANDLE;
	public:
		commandPool() = default;

		commandPool(VkCommandPoolCreateInfo& createInfo) {
			Create(createInfo);
		}

		commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
			Create(queueFamilyIndex, flags);
		}

		commandPool(commandPool&& other) noexcept { MoveHandle; }

		~commandPool() { DestroyHandleBy(vkDestroyCommandPool); }

		//Getter
		DefineHandleTypeOperator;

		DefineAddressFunction;

		//Const Function, VK_COMMAND_BUFFER_LEVEL_PRIMARY为一级命令缓冲区
		result_t AllocateBuffers(arrayRef<VkCommandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const {
			VkCommandBufferAllocateInfo allocateInfo = {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = handle,
				.level = level,
				.commandBufferCount = uint32_t(buffers.Count())
			};
			VkResult result = vkAllocateCommandBuffers(graphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
			if (result)
				outStream << std::format("[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: {}\n", int32_t(result));
			return result;
		}

		result_t AllocateBuffers(arrayRef<commandBuffer> buffers, VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const {
			return AllocateBuffers(
				{ &buffers[0].handle, buffers.Count() },
				level);
		}

		void FreeBuffers(arrayRef<VkCommandBuffer> buffers) const {
			vkFreeCommandBuffers(graphicsBase::Base().Device(), handle, buffers.Count(), buffers.Pointer());
			memset(buffers.Pointer(), 0, buffers.Count() * sizeof(VkCommandBuffer));
		}

		void FreeBuffers(arrayRef<commandBuffer> buffers) const {
			FreeBuffers({ &buffers[0].handle, buffers.Count() });
		}

		//Non-const Function
		result_t Create(VkCommandPoolCreateInfo& createInfo) {
			createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			VkResult result = vkCreateCommandPool(graphicsBase::Base().Device(), &createInfo, nullptr, &handle);
			if (result)
				outStream << std::format("[ commandPool ] ERROR\nFailed to create a command pool!\nError code: {}\n", int32_t(result));
			return result;
		}

		result_t Create(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) {
			VkCommandPoolCreateInfo createInfo = {
				.flags = flags,
				.queueFamilyIndex = queueFamilyIndex
			};
			return Create(createInfo);
		}
	};

}
