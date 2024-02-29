# vulkan-tutorial

**什么是Vulkan实例？**
应用程序必须显式地告诉操作系统和显卡驱动，说明其需要使用Vulkan的功能，这一步是由创建Vulkan实例（VkInstance）来完成的。
Vulkan实例是你的应用程序与显卡驱动所提供的Vulkan实现之间的联系，其底层是一系列Vulkan运行所必需的用于记录状态和信息的变量。

**什么是debug messenger？**
Debug messenger用来输出验证层所捕捉到的debug信息。若没有这东西，Vulkan编程可谓寸步难行。

**什么是window surface？**
Vulkan是平台无关的API，你必须向其提供一个window surface（VkSurfaceKHR），以和平台特定的窗口对接。

**什么是物理设备和逻辑设备？**
物理设备即图形处理器，通常为GPU。Vulkan中所谓的物理设备虽称物理（physical），但并非必须是实体设备。
VkPhysicalDevice类型指代物理设备，从这类handle只能获取物理设备信息。VkDevice类型是逻辑设备的handle，逻辑设备是编程层面上用来与物理设备交互的对象，关于分配显存、创建Vulkan相关对象的命令大都会被提交给逻辑设备。

**什么是队列？**
队列（VkQueue）类似于线程，命令被提交到队列执行，[Vulkan官方标准](https://renderdoc.org/vkspec_chunked/chap3.html#fundamentals-queueoperation)中将队列描述为 命令执行引擎的接口：
*Vulkan queues provide an interface to the execution engines of a device.*
Vulkan核心功能中规定队列支持的操作类型包括图形、计算、数据传送、稀疏绑定四种，图形和计算队列必定也支持数据传送。一组功能相同的队列称为队列族。
任何支持Vulkan的显卡驱动确保你能找到**至少一个**同时支持图形和计算操作的队列族。

**什么是交换链？**
在将一张图像用于渲染或其他类型的写入时，已渲染好的图像可以被呈现引擎读取，如此交替呈现在窗口中的数张图像的集合即为交换链。

**什么是层和扩展？**
层和扩展在Vulkan的核心功能之外，提供了对特定需求、特定平台的支持，还包括特定厂商提供的功能。“扩展”一词顾名思义，而层与扩展的主要区别在于，层有显著的作用范围，比如验证层会检查几乎所有Vulkan相关的函数。此外，有些扩展需要特定的层才能使用。
层只有实例级别的（Vulkan1.0中定义过设备级别的层，但这一概念之后被废弃，Vulkan1.0版本中亦不存在设备级别独占的层），而扩展则分实例级别和设备级别，前者与特定设备无关，可能作用于所有Vulkan相关的内容，而后者则作用于与特定设备相关的内容。

**什么是image view？**
Vulkan中，VkImage引用一片物理设备内存（指显存），将该片内存上的数据用作图像，而VkImageView指定图像的使用方式，比如，一个6层的二维图像，可以用作6层的二维图像数组，也可用作天空盒。

**什么是渲染通道？**
渲染通道（render pass）指渲染过程中，所绑定帧缓冲的参数（格式、内存布局）及各个渲染步骤之间的关系。
举例而言，实现延迟渲染会涉及两个步骤，各为一个子通道（subpass），第一个通道中生成G-buffer，第二个通道中渲染到屏幕。渲染通道是对渲染流程的抽象。

**什么是子通道？**
渲染通道是对渲染流程的抽象，相应地，子通道就是渲染流程中的一个环节。举例而言，直接渲染到屏幕缓冲（由交换链图像构成的帧缓冲）的做法只有一个子通道。而延迟渲染至少要经历两个子通道，一个生成G-buffer，一个用G-buffer在屏幕缓冲上进行光照计算，如果还要做正向透明度的话，还得再多一个子通道。

**什么是子通道依赖**
子通道依赖是一种同步措施，它能确保附件的内存布局转换在正确的时机发生。

**什么是帧缓冲？**
帧缓冲是在一个渲染通道中（特定渲染流程）所必要的一组图像附件（attachment）的集合。

**什么是着色器？**
着色器是在渲染过程中的可编程阶段运行的程序，书写完后在Vulkan程序中将其读取为着色器模组，然后提供给管线使用。即着色器是在管线的可编程阶段运行的GPU程序。

**什么是管线？**
这里只解释图形管线，一条图形管线指定渲染过程中所用的着色器模组及各种状态参数（混色方式、模板和深度测试方式、视口等）。即管线（VkPipeline）是数据处理流程的抽象，它由可编程着色器阶段、固定管线阶段、以及一系列状态参数构成。

**什么是管线布局？**
管线布局（VkPipelineLayout）包含了管线如何使用描述符（uniform缓冲区、storage缓冲区、贴图和采样器等，//TODO 详见）以及push constant的信息。

**什么是图像附件？**
图像附件就是渲染目标，图形命令向图像附件上输出颜色/深度/模板值。

### 初始化流程

一个呈现图像的Vulkan应用程序需经历以下的步骤以初始化：

1. 创建Vulkan实例
2. 创建debug messenger（若编译选项为DEBUG）
3. 创建window surface
4. 选择物理设备并创建逻辑设备，取得队列
5. 创建交换链

### 创建Vulkan实例的步骤

1. 确定所需的实例级别层及扩展，不检查是否可用
2. 用[vkCreateInstance](https://renderdoc.org/vkspec_chunked/chap4.html#vkCreateInstance)(...)创建Vulkan实例

1.在创建Vulkan实例前，用PushInstanceLayer(...)和PushInstanceExtension(...)向对应的vector中添加指向层和扩展名称的指针。
2.然后尝试用CreateInstance(...)创建Vulkan实例。
3.若创建Vulkan实例失败，若[vkCreateInstance](https://renderdoc.org/vkspec_chunked/chap4.html#vkCreateInstance)(...)返回VK_ERROR_LAYER_NOT_PRESENT，从InstanceLayers()复制一份**instanceLayers**，用CheckInstanceLayers(...)检查可用性，若不可用的仅为非必要的层，创建一份去除该层后的vector，用InstanceLayers(...)复制给**instanceLayers**。返回VK_ERROR_EXTENSION_NOT_PRESENT的情况亦类似。然后重新尝试创建Vulkan实例。

### 创建逻辑设备的步骤

创建逻辑设备的步骤依序为：

1. 获取物理设备列表
2. 检查物理设备是否满足所需的队列族类型，从中选择能满足要求的设备并顺便取得队列族索引
3. 确定所需的设备级别扩展，不检查是否可用
4. 用[vkCreateDevice](https://renderdoc.org/vkspec_chunked/chap5.html#vkCreateDevice)(...)创建逻辑设备，取得队列
5. 取得物理设备属性、物理设备内存属性，以备之后使用

**根据情况，一共需要三种类型的队列：图形、呈现、计算。**
呈现队列并非是Vulkan核心功能中规定的队列类型。几乎可以肯定，GPU必定会有一个同时支持图形和呈现的队列族（该说法来自谷歌的搜索结果，我认为是准确的，考虑到没找到与此相悖的报告），但标准中并没有作此规定，因此保险起见，我将其单独列出来。

- 如果你的程序没有图形界面（比如，仅仅用于对图像做某种处理的控制台程序），那么呈现队列非必须。
- 如果你不需要GPU计算或间接渲染（将CPU上的一些计算扔到GPU上然后再从计算结果做渲染），那么计算队列非必须。
- 如果你只打算搞计算（GPU是高度并行的计算设备）而不搞渲染，那么图形队列非必须。

1.用GetPhysicalDevices()获取可用物理设备的列表。
2.然后，用DeterminePhysicalDevice(...)判断将要使用的物理设备是否满足要求，如满足则将其记录为所用设备。这一步需要判断物理设备是否支持window surface，正是因此创建window surface一步须在此之前。
3.然后，用PushDeviceExtension(...)向**deviceExtensions**中添加指向各扩展名称的指针。
4.然后尝试用CreateDevice(...)创建逻辑设备，若创建成功，该函数会取得队列。
5.若创建逻辑设备失败，尝试用DeterminePhysicalDevice(...)更换物理设备。到这一步为止仍没有检查扩展，如果你首先选用的是性能较好的物理设备，且优先看重性能（而非支持的扩展），那么这一步与下一步可互换。
6.若遍历所有物理设备后仍无法创建逻辑设备且[vkCreateDevice](https://renderdoc.org/vkspec_chunked/chap5.html#vkCreateDevice)(...)返回VK_ERROR_EXTENSION_NOT_PRESENT，从DeviceExtensions()复制一份**deviceExtensions**，嵌套循环中用DeterminePhysicalDevice(...)遍历物理设备（会有代码防止反复取得队列族索引），用CheckDeviceExtensions(...)检查扩展可用性，对于某物理设备，若不可用的仅为非必要的扩展（不过为应对没有某个扩展而书写两套代码可能会很麻烦），创建一份去除该扩展后的vector，用DeviceExtensions(...)复制给**deviceExtensions**。然后重新尝试创建逻辑设备。
上述流程优先考虑在最佳情况下获得最快的初始化速度，因而显得有些繁琐（最佳情况下不需要检查扩展，最糟糕的情况为遍历所有物理设备一次后，因全数无法满足所需扩展，再次遍历并对每个设备检查扩展可用性），如果你的应用程序用到了显卡未必支持但对于程序运行非必要的扩展，且程序对显卡性能没有要求，可以先检查扩展（最佳情况下需要检查扩展一次，最糟糕的情况下需要遍历所有物理设备一次并对每个设备检查扩展可用性一次）。

### 创建交换链的步骤

创建交换链的步骤依序为：

1. 填写创建信息**createInfo结构体
2. 创建交换链并取得交换链图像，为交换链图像创建image view

1.若需要指定交换链的图像格式和色彩空间，用GetSurfaceFormats()来取得surface的可用格式到**availableSurfaceFormats**，用SetSurfaceFormat(...)验证并指定图像格式和色彩空间。
2.用CreateSwapchain(...)创建交换链。
3.在之后的运行中，窗口大小改变时（可通过GLFW的回调函数，或WindowsAPI中的message得知），调用RecreateSwapchain()。

### 最基本的渲染循环（rendering loop）

就我的写法而言，一个最基本的渲染循环应该是：

1. 通过等待一个栅栏，确保前一次的命令已执行完毕（以便接下来覆写同一命令缓冲区）
2. 获取交换链图像索引，然后置位信号量A
3. 录制命令缓冲区
4. 提交命令缓冲区，等待信号量A，命令执行完后置位信号量B和栅栏
5. 等待信号量B，确保本次的命令执行完毕后，呈现图像

### 命令缓冲区录制的基本流程

构建完渲染循环后，直到渲染出三角形为止需经历以下步骤：

1. 创建渲染通道 render pass
2. 为每张交换链图像创建对应的帧缓冲 swap chain -> image -> image view（attachments） -> framebuffer
3. 书写顶点和片段着色器 vertex shader fragment shader
4. 创建着色器模组 shader module
5. 创建渲染管线 pipeline
6. 在命令缓冲区中录制命令 commandbuffer begin commandbuffer end

在命令缓冲区中录制命令时，首先开始一个渲染通道并同时指定所用的帧缓冲，然后绑定渲染管线，由此便指定了渲染所必须的所有参数。

### 着色器与Vulkan之间的交互流程

1. 首先使用适合人类阅读和书写的编程语言GLSL编写着色器（也可以用微软的HLSL）
2. 然后将着色器编译到SPIR-V这一中间语言，后缀名为.spv
3. 之后再由Vulkan程序读取.spv文件，由显卡驱动提供的Vulkan实现将其编译为着色器模组（VkShaderModule）
4. 不事先编译的原因在于不同的显卡驱动所提供的Vulkan实现的编译结果可能不同

### 从GLSL编译到SPIR-V

Vulkan SDK中附带的*Bin/glslc.exe*是谷歌提供的工具（[官方Github文档见此](https://github.com/google/shaderc/tree/main/glslc)），用于将由GLSL书写的着色器编译到.spv文件

### 着色器类型判别

编译GLSL着色器到SPIR-V时，需要告诉*glslc.exe*该着色器对应的可编程阶段。
以下三种方式之一即可：

1. 通过文件扩展名表示着色器类型
2. 通过着色器中的预编译指令表示着色器类型，语法为`#pragma shader_stage(着色器对应的阶段名称)`
3. 编译时显式指定，语法为`-fshader-stage=着色器对应的阶段名称`

以上三条按优先级从低到高排序，即显式指定的优先级最高，预编译指令其次。

| 管线阶段 | 文件扩展名 | 预编译指令/显式指定的阶段名称 |
| -------- | ---------- | ----------------------------- |
| 顶点     | .vert      | vertex                        |
| 片段     | .frag      | fragment                      |
| 细分控制 | .tesc      | tesscontrol                   |
| 细分求值 | .tese      | tesseval                      |
| 几何     | .geom      | geometry                      |
| 计算     | .comp      | compute                       |

基础编译命令

指定文件名

```
glslc.exe路径 着色器文件路径 -o 输出文件路径
//若显式指定着色器对应的阶段名称
glslc.exe路径 着色器文件路径 -fshader-stage=阶段名称 -o 输出文件路径
```

自动命名

```
glslc.exe路径 着色器文件路径 -c
//若显式指定着色器对应的阶段名称
glslc.exe路径 着色器文件路径 -fshader-stage=阶段名称 -c
```

- 在`-fshader-stage=阶段名称`和`-c`或`-o 输出文件路径`参数之间不分先后
- 若着色器文件的后缀为着色器类型（.vert等），自动命名的方式是在其后添加.spv，比如foo.vert变为foo.vert.spv。否则，自动命名的方式是替代扩展名为.spv，比如foo.vert.shader变为foo.vert.spv。
