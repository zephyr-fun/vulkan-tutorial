#pragma once
// cpp std libs
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <map>
#include <unordered_map>
#include <span>
#include <memory>
#include <functional>
#include <concepts>
#include <format>
#include <chrono>
#include <numeric>
#include <numbers>

// GLM, 用于OpenGL的数学库，也适用于Vulkan
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
// 如果你惯用左手坐标系，在此定义GLM_FORCE_LEFT_HANDED
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

// stb_image.h, 用于读取贴图，支持bmp、tga、png、jpeg、hdr等常见格式
#include <stb_image.h>

// Vulkan
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")

template<typename T>
class arrayRef {
    T* const pArray = nullptr;
    size_t count = 0;
public:
    arrayRef() = default;
    arrayRef(T& data) :pArray(&data), count(1) {}
    template<size_t elementCount>
    arrayRef(T(&data)[elementCount]) : pArray(data), count(elementCount) {}
    arrayRef(T* pData, size_t elementCount) :pArray(pData), count(elementCount) {}
    arrayRef(const arrayRef<std::remove_const_t<T>>& other) :pArray(other.Pointer()), count(other.Count()) {}
    //Getter
    T* Pointer() const { return pArray; }
    size_t Count() const { return count; }
    //Const Function
    T& operator[](size_t index) const { return pArray[index]; }
    T* begin() const { return pArray; }
    T* end() const { return pArray + count; }
    //Non-const Function
    arrayRef& operator=(const arrayRef&) = delete;
};
#define ExecuteOnce(...) { static bool executed = false; if (executed) return __VA_ARGS__; executed = true; }
