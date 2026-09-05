#include <exception>
#include <stdexcept>

#include "config.h"
#include "Manipulator.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include "VulkanDebug.h"
#include "VulkanView.h"
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "VulkanImage.h"
#include "VulkanInitializers.hpp"

#include "SimpleShape.h"

#define WM_PAINT 1
#define SHADER_DIR ROOT_DIR##"/vulkan/basic_pbr"

constexpr float fov = 60;


struct {
  tg::mat4 prj;
  tg::mat4 view;
  tg::mat4 model;
  tg::vec4 cam;
} matrixUbo;

float lightIntensity = 300;

struct{
  struct alignas(16) aligned_vec3 : vec3 {};
  aligned_vec3 lightPos[4] = {vec3(10, -10, 10), vec3(-10, -10, 10), vec3(-10, -10, -10), vec3(10, -10, -10)};
  aligned_vec3 lightColor[4] = {vec3(lightIntensity), vec3(lightIntensity), vec3(lightIntensity), vec3(1)};
} lightsUbo;

struct alignas(16){
  struct alignas(16) aligned_vec3 : vec3 {};

	float metallic;
	float roughness;
	float ao;
	aligned_vec3 albedo;
} materialUbo;

class Test {
public:
  Test(const std::shared_ptr<VulkanDevice> &dev) : _device(dev)
  {
    _manip.setHome(vec3(0, 0, 30), vec3(0), vec3(0, 1, 0));

    _swapchain = std::make_shared<VulkanSwapChain>(dev);

    createSphere();
    createPipeLayout();
  }

  ~Test()
  {
    vkDeviceWaitIdle(*_device);

    freeResource();
    for (auto fence : _fences)
      vkDestroyFence(*_device, fence, nullptr);

    _device->destroyCommandBuffers(_cmdBufs);

    vkDestroyRenderPass(*_device, _renderPass, nullptr);

    vkDestroySemaphore(*_device, _presentSemaphore, nullptr);
    vkDestroySemaphore(*_device, _renderSemaphore, nullptr);

    auto surface = _swapchain->surface();
    _swapchain.reset();

    if (_vertBuf) {
      vkDestroyBuffer(*_device, _vertBuf, nullptr);
      _vertBuf = VK_NULL_HANDLE;
    }

    if (_vertMem) {
      vkFreeMemory(*_device, _vertMem, nullptr);
      _vertMem = VK_NULL_HANDLE;
    }

    if (_indexBuf) {
      vkDestroyBuffer(*_device, _indexBuf, nullptr);
      _indexBuf = VK_NULL_HANDLE;
    }

    if (_indexMem) {
      vkFreeMemory(*_device, _indexMem, nullptr);
      _indexMem = VK_NULL_HANDLE;
    }

    if (_descriptPool) {
      vkDestroyDescriptorPool(*_device, _descriptPool, nullptr);
      _descriptPool = VK_NULL_HANDLE;
    }

    if (_pipeLayout) {
      vkDestroyPipelineLayout(*_device, _pipeLayout, nullptr);
      _pipeLayout = VK_NULL_HANDLE;
    }

    if (_pipeline) {
      vkDestroyPipeline(*_device, _pipeline, nullptr);
      _pipeline = VK_NULL_HANDLE;
    }
  }

  void setWindow(SDL_Window *win)
  {
    SDL_GetWindowSizeInPixels(win, &_w, &_h);

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(win, VulkanInstance::instance(), nullptr, &surface))
      throw std::runtime_error("could not create vk surface.");

    _swapchain->setSurface(surface);
    _swapchain->realize(_w, _h, true);

    createSyncObject();

    int count = _swapchain->imageCount();

    _renderPass = _device->createRenderPass(_swapchain->colorFormat());

    _cmdBufs = _device->createCommandBuffers(count);

    createPipeline();

    reqResource();

    updateUbo();
  }

  void reqResource()
  {
    int count = _swapchain->imageCount();

    _frameBufs = _swapchain->createFrameBuffer(_renderPass);

    buildCommandBuffers(_frameBufs, _renderPass);

    if (_fences.size() != _cmdBufs.size()) {
      for (auto fence : _fences)
        vkDestroyFence(*_device, fence, nullptr);
      _fences = _device->createFences(_cmdBufs.size());
    }
  }

  void freeResource()
  {
    for (auto &framebuf : _frameBufs)
      vkDestroyFramebuffer(*_device, framebuf, nullptr);
    _frameBufs.clear();

  }

  void createSyncObject()
  {
    VkSemaphoreCreateInfo semaphoreCreateInfo = {};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreCreateInfo.pNext = nullptr;

    // Semaphore used to ensure that image presentation is complete before starting to submit again
    VK_CHECK_RESULT(vkCreateSemaphore(*_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));

    // Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
    VK_CHECK_RESULT(vkCreateSemaphore(*_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));
  }

  void update()
  {
    SDL_Event ev;
    ev.type = SDL_EVENT_USER;
    ev.user.code = WM_PAINT;
    SDL_PushEvent(&ev);
  }

  void draw()
  {
    // vkWaitForFences(*_device, 1, _fences[]);
    auto [result, index] = _swapchain->acquireImage(_presentSemaphore);
    if (!((result == VK_SUCCESS) || (result == VK_SUBOPTIMAL_KHR))) {
      VK_CHECK_RESULT(result);
    }

    VK_CHECK_RESULT(vkWaitForFences(*_device, 1, &_fences[index], VK_TRUE, UINT64_MAX));
    VK_CHECK_RESULT(vkResetFences(*_device, 1, &_fences[index]));

    VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pWaitDstStageMask = &waitStageMask;   // Pointer to the list of pipeline stages that the semaphore waits will occur at
    submitInfo.waitSemaphoreCount = 1;               // One wait semaphore
    submitInfo.signalSemaphoreCount = 1;             // One signal semaphore
    submitInfo.pCommandBuffers = &_cmdBufs[index];  // Command buffers(s) to execute in this batch (submission)
    submitInfo.commandBufferCount = 1;               // One command buffer

    submitInfo.pWaitSemaphores = &_presentSemaphore;   // Semaphore(s) to wait upon before the submitted command buffer starts executing
    submitInfo.pSignalSemaphores = &_renderSemaphore;  // Semaphore(s) to be signaled when command buffers have completed

    // Submit to the graphics queue passing a wait fence
    auto queue = _device->graphicQueue(0);
    VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, _fences[index]));

    {
      _frame = index;
      auto present = _swapchain->queuePresent(queue, _frame, _renderSemaphore);
      if (!((present == VK_SUCCESS) || (present == VK_SUBOPTIMAL_KHR))) {
        VK_CHECK_RESULT(present);
      }
    }
  }

  void loop()
  {
    bool running = true;
    while (running) {
      SDL_Event event;
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
          case SDL_EVENT_QUIT:
            running = false;
            break;
          case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            vkDeviceWaitIdle(*_device);
            _w = event.window.data1;
            _h = event.window.data2;
            freeResource();
            _swapchain->realize(_w, _h, true);
            reqResource();
            updateUbo();
            update();
            break;
          case SDL_EVENT_USER:
            if (event.user.code == WM_PAINT)
              draw();
            break;
          case SDL_EVENT_MOUSE_MOTION:
            if (event.motion.state & SDL_BUTTON_LMASK) {
              _manip.rotate(event.motion.xrel, event.motion.yrel);
              updateUbo();
            } else if (event.motion.state & SDL_BUTTON_MMASK) {
            } else if (event.motion.state & SDL_BUTTON_RMASK) {
              _manip.translate(event.motion.xrel, -event.motion.yrel);
              updateUbo();
            }
            update();
            break;
          case SDL_EVENT_MOUSE_WHEEL: {
            _manip.zoom(event.wheel.y);
            updateUbo();
            update();
            break;
          }
          case SDL_EVENT_KEY_UP: {
            if (event.key.scancode == SDL_SCANCODE_SPACE)
              _manip.home();
            updateUbo();
            update();
          } break;
          default:
            break;
        }
      }
    }
  }

  void buildCommandBuffers(std::vector<VkFramebuffer> &framebuffers, VkRenderPass renderPass)
  {
    assert(framebuffers.size() == _cmdBufs.size());

    VkCommandBufferBeginInfo bufInfo = {};
    bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bufInfo.pNext = nullptr;

    VkClearValue clearValues[2];
    clearValues[0].color = {{0.0, 0.0, 0.2, 1.0}};
    clearValues[1].depthStencil = {1, 0};

    VkRenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.pNext = nullptr;
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = _w;
    renderPassBeginInfo.renderArea.extent.height = _h;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    for (int i = 0; i < _cmdBufs.size(); i++) {
      renderPassBeginInfo.framebuffer = framebuffers[i];
      auto &cmdBuf = _cmdBufs[i];
      VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &bufInfo));

      vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      {
        VkViewport viewport = {};
        viewport.y = _h;
        viewport.width = _w;
        viewport.height = -_h;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;
        vkCmdSetViewport(cmdBuf, 0, 1, &viewport);
      }

      {
        VkRect2D scissor = {};
        scissor.extent.width = _w;
        scissor.extent.height = _h;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        vkCmdSetScissor(cmdBuf, 0, 1, &scissor);
      }

      {
        vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
      }

      VkDescriptorSet dessets[2] = {_matrixSet, _materialSet};
      vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeLayout, 0, 2, dessets, 0, nullptr);

      {
        VkDeviceSize offset[2] = {0, _vertCount * sizeof(vec3)};
        VkBuffer bufs[2] = {};
        bufs[0] = _vertBuf;
        bufs[1] = _vertBuf;
        vkCmdBindVertexBuffers(cmdBuf, 0, 2, bufs, offset);
        vkCmdBindIndexBuffer(cmdBuf, _indexBuf, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmdBuf, _indexCount, 49, 0, 0, 0);
      }

      vkCmdEndRenderPass(cmdBuf);

      VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuf));
    }
  }

  void createPipeLayout()
  {
    VkDescriptorSetLayoutBinding layoutBinding[2] = {};
    layoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding[0].descriptorCount = 1;
    layoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBinding[0].pImmutableSamplers = nullptr;

    layoutBinding[1].binding = 1;
    layoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    layoutBinding[1].descriptorCount = 1;
    layoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    layoutBinding[1].pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
    descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayout.pNext = nullptr;
    descriptorLayout.bindingCount = 2;
    descriptorLayout.pBindings = layoutBinding;

    VkDescriptorSetLayout matrixLay, materialLay;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &descriptorLayout, nullptr, &matrixLay));

    {
      VkDescriptorSetLayoutBinding materialBinding = {};
      materialBinding.binding = 2;
      materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      materialBinding.descriptorCount = 1;
      materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

      descriptorLayout.bindingCount = 1;
      descriptorLayout.pBindings = &materialBinding;

      VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &descriptorLayout, nullptr, &materialLay));
    }


    VkDescriptorSetLayout layouts[2] = {matrixLay, materialLay};

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = 2;
    pPipelineLayoutCreateInfo.pSetLayouts = layouts;

    VkPipelineLayout pipeLayout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &pPipelineLayoutCreateInfo, nullptr, &pipeLayout));
    _pipeLayout = pipeLayout;

    //----------------------------------------------------------------------------------------------------

    VkDescriptorPoolSize typeCounts[1];
    typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    typeCounts[0].descriptorCount = 10;

    VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.pNext = nullptr;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = typeCounts;
    descriptorPoolInfo.maxSets = 10;

    VkDescriptorPool desPool;
    VK_CHECK_RESULT(vkCreateDescriptorPool(*_device, &descriptorPoolInfo, nullptr, &desPool));
    _descriptPool = desPool;

    //----------------------------------------------------------------------------------------------------

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = desPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &matrixLay;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &_matrixSet));

    int sz = ((sizeof(matrixUbo) + 63) >> 8) << 8;
    int lightSz = sizeof(lightsUbo);
    VkDescriptorBufferInfo descriptor = {};
    _uboBuf = _device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz + lightSz);
    descriptor.buffer = *_uboBuf;
    descriptor.offset = 0;
    descriptor.range = sizeof(matrixUbo);

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _matrixSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &descriptor;
    writeDescriptorSet.dstBinding = 0;

    vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);

    {
      descriptor.offset = sz;
      descriptor.range = sizeof(lightsUbo);
      writeDescriptorSet.dstSet = _matrixSet;
      writeDescriptorSet.dstBinding = 1;
      vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);
    }

    {
      VkDescriptorSetAllocateInfo allocInfo = {};
      allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocInfo.descriptorPool = desPool;
      allocInfo.descriptorSetCount = 1;
      allocInfo.pSetLayouts = &materialLay;

      VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &_materialSet));

      int sz = sizeof(materialUbo) * 49;
      _materialBuf = _device->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
      VkDescriptorBufferInfo descriptor = {};
      descriptor.buffer = *_materialBuf;
      descriptor.offset = 0;
      descriptor.range = sz;

      VkWriteDescriptorSet writeDescriptorSet = {};
      writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeDescriptorSet.dstSet = _materialSet;
      writeDescriptorSet.descriptorCount = 1;
      writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writeDescriptorSet.pBufferInfo = &descriptor;
      writeDescriptorSet.dstBinding = 2;

      vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);
    }

    vkDestroyDescriptorSetLayout(*_device, matrixLay, nullptr);
    vkDestroyDescriptorSetLayout(*_device, materialLay, nullptr);


    uint8_t *data = 0;
    VK_CHECK_RESULT(vkMapMemory(*_device, _uboBuf->memory(), sz, sizeof(lightsUbo), 0, (void **)&data));
    memcpy(data, &lightsUbo, sizeof(lightsUbo));
    vkUnmapMemory(*_device, _uboBuf->memory());

    {
      decltype(materialUbo) mateBufs[49];
      for (int i = 0; i < 49; i++) {
        mateBufs[i].metallic = ((i / 7) + 1) / 7.f;
        mateBufs[i].roughness = ((i % 7) + 1) / 7.f;
        mateBufs[i].ao = 1;
        mateBufs[i].albedo.set(1, 0, 0);
      }

      uint8_t *data = 0;
      VK_CHECK_RESULT(vkMapMemory(*_device, _materialBuf->memory(), 0, _materialBuf->size(), 0, (void **)&data));
      memcpy(data, &mateBufs, _materialBuf->size());
      vkUnmapMemory(*_device, _materialBuf->memory());
    }
  }

  void createPipeline()
  {
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = _pipeLayout;
    pipelineCreateInfo.renderPass = _renderPass;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationState.depthClampEnable = VK_FALSE;
    rasterizationState.rasterizerDiscardEnable = VK_FALSE;
    rasterizationState.depthBiasEnable = VK_FALSE;
    rasterizationState.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
    blendAttachmentState[0].colorWriteMask = 0xf;
    blendAttachmentState[0].blendEnable = VK_FALSE;
    VkPipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = blendAttachmentState;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::vector<VkDynamicState> dynamicStateEnables;
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables.data();
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());

    VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = VK_TRUE;
    depthStencilState.depthWriteEnable = VK_TRUE;
    depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.front = depthStencilState.back;

    VkPipelineMultisampleStateCreateInfo multisampleState = {};
    multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleState.pSampleMask = nullptr;

    VkVertexInputBindingDescription vertexInputBindings[2] = {};
    vertexInputBindings[0].binding = 0;  // vkCmdBindVertexBuffers
    vertexInputBindings[0].stride = sizeof(vec3);
    vertexInputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    vertexInputBindings[1].binding = 1;  // vkCmdBindVertexBuffers
    vertexInputBindings[1].stride = sizeof(vec3);
    vertexInputBindings[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertexInputAttributs[2] = {};
    vertexInputAttributs[0].binding = 0;
    vertexInputAttributs[0].location = 0;
    vertexInputAttributs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[0].offset = 0;
    vertexInputAttributs[1].binding = 1;
    vertexInputAttributs[1].location = 1;
    vertexInputAttributs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    vertexInputAttributs[1].offset = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = 2;
    vertexInputState.pVertexBindingDescriptions = vertexInputBindings;
    vertexInputState.vertexAttributeDescriptionCount = 2;
    vertexInputState.pVertexAttributeDescriptions = vertexInputAttributs;

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = _device->createShader(SHADER_DIR"/pbr.vert.spv");
    shaderStages[0].pName = "main";
    assert(shaderStages[0].module != VK_NULL_HANDLE);

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = _device->createShader(SHADER_DIR"/pbr.frag.spv");
    shaderStages[1].pName = "main";
    assert(shaderStages[1].module != VK_NULL_HANDLE);

    pipelineCreateInfo.stageCount = 2;
    pipelineCreateInfo.pStages = shaderStages;

    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(*_device, _device->getOrCreatePipecache(), 1, &pipelineCreateInfo, nullptr, &_pipeline));

    vkDestroyShaderModule(*_device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(*_device, shaderStages[1].module, nullptr);
  }

  void updateUbo()
  {
    matrixUbo.cam = _manip.eye();
    matrixUbo.view = _manip.viewMatrix();
    matrixUbo.model.identity();
    matrixUbo.prj = tg::perspective<float>(fov, float(_w) / _h, 0.1, 1000);
    // tg::near_clip(matrix_ubo.prj, tg::vec4(0, 0, -1, 0.5));
    uint8_t *data = 0;
    VK_CHECK_RESULT(vkMapMemory(*_device, _uboBuf->memory(), 0, sizeof(matrixUbo), 0, (void **)&data));
    memcpy(data, &matrixUbo, sizeof(matrixUbo));
    vkUnmapMemory(*_device, _uboBuf->memory());
  }

  void createSphere()
  {
    Sphere sp(vec3(0), 1);
    sp.build();
    auto &verts = sp.getVertex();
    auto &norms = sp.getNorms();
    //auto &uv = sp.get_uvs();
    auto &index = sp.getIndex();
    _vertCount = verts.size();
    _indexCount = index.size();

    struct StageBuffer {
      VkBuffer buffer;
      VkDeviceMemory mem;
    };
    StageBuffer vertices, indices;

    uint64_t vertSize = verts.size() * (sizeof(vec3) * 2 + sizeof(vec2));
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = vertSize;
    vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &vertexBufferInfo, nullptr, &vertices.buffer));
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(*_device, vertices.buffer, &memReqs);

    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &vertices.mem));

    void *data = 0;
    VK_CHECK_RESULT(vkMapMemory(*_device, vertices.mem, 0, memAlloc.allocationSize, 0, &data));
    uint64_t offset = 0;
    memcpy(data, verts.data(), verts.size() * sizeof(vec3));
    offset += verts.size() * sizeof(vec3);
    memcpy((uint8_t *)data + offset, norms.data(), norms.size() * sizeof(vec3));
    offset += norms.size() * sizeof(vec3);
    //memcpy((uint8_t *)data + offset, uv.data(), uv.size() * sizeof(vec2));
    vkUnmapMemory(*_device, vertices.mem);
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, vertices.buffer, vertices.mem, 0));

    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &vertexBufferInfo, nullptr, &_vertBuf));
    vkGetBufferMemoryRequirements(*_device, _vertBuf, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &_vertMem));
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, _vertBuf, _vertMem, 0));

    uint64_t indexSize = index.size() * sizeof(uint16_t);
    VkBufferCreateInfo indexbufferInfo = {};
    indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexbufferInfo.size = indexSize;
    indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    // Copy index data to a buffer visible to the host (staging buffer)
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &indexbufferInfo, nullptr, &indices.buffer));
    vkGetBufferMemoryRequirements(*_device, indices.buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &indices.mem));
    VK_CHECK_RESULT(vkMapMemory(*_device, indices.mem, 0, indexSize, 0, &data));
    memcpy(data, index.data(), indexSize);
    vkUnmapMemory(*_device, indices.mem);
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, indices.buffer, indices.mem, 0));

    // Create destination buffer with device only visibility
    indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &indexbufferInfo, nullptr, &_indexBuf));
    vkGetBufferMemoryRequirements(*_device, _indexBuf, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &_indexMem));
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, _indexBuf, _indexMem, 0));

    {
      VkCommandBuffer cmdBuffer;

      VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
      cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmdBufAllocateInfo.commandPool = _device->commandPool();
      cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmdBufAllocateInfo.commandBufferCount = 1;

      VK_CHECK_RESULT(vkAllocateCommandBuffers(*_device, &cmdBufAllocateInfo, &cmdBuffer));

      VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
      VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

      VkBufferCopy copyRegion = {};
      copyRegion.size = vertSize;
      vkCmdCopyBuffer(cmdBuffer, vertices.buffer, _vertBuf, 1, &copyRegion);

      copyRegion.size = indexSize;
      vkCmdCopyBuffer(cmdBuffer, indices.buffer, _indexBuf, 1, &copyRegion);

      VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

      VkSubmitInfo submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &cmdBuffer;

      VkFenceCreateInfo fenceCreateInfo = {};
      fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceCreateInfo.flags = 0;
      VkFence fence;
      VK_CHECK_RESULT(vkCreateFence(*_device, &fenceCreateInfo, nullptr, &fence));

      VK_CHECK_RESULT(vkQueueSubmit(_device->graphicQueue(0), 1, &submitInfo, fence));
      VK_CHECK_RESULT(vkWaitForFences(*_device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
      vkDestroyFence(*_device, fence, nullptr);
      vkFreeCommandBuffers(*_device, _device->commandPool(), 1, &cmdBuffer);
    }

    vkDestroyBuffer(*_device, vertices.buffer, nullptr);
    vkDestroyBuffer(*_device, indices.buffer, nullptr);
    vkFreeMemory(*_device, vertices.mem, nullptr);
    vkFreeMemory(*_device, indices.mem, nullptr);
  }

private:
  std::shared_ptr<VulkanDevice> _device;
  std::shared_ptr<VulkanSwapChain> _swapchain;

  int _w, _h;
  uint32_t _frame = 0;

  VkRenderPass _renderPass = VK_NULL_HANDLE;

  VkSemaphore _presentSemaphore;
  VkSemaphore _renderSemaphore;

  std::vector<VkCommandBuffer> _cmdBufs;
  std::vector<VkFramebuffer> _frameBufs;
  std::vector<VkFence> _fences;

  VkBuffer _vertBuf;
  VkDeviceMemory _vertMem;
  VkBuffer _indexBuf;
  VkDeviceMemory _indexMem;

  VkDescriptorPool _descriptPool = VK_NULL_HANDLE;

  VkDescriptorSet _matrixSet = VK_NULL_HANDLE;
  VkDescriptorSet _materialSet = VK_NULL_HANDLE;

  VkPipelineLayout _pipeLayout = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;

  std::shared_ptr<VulkanBuffer> _uboBuf;

  std::shared_ptr<VulkanBuffer> _materialBuf;

  uint32_t _vertCount = 0;
  uint32_t _indexCount = 0;

  Manipulator _manip;
};


int main(int argc, char** argv)
{
  SDL_Window* win = 0;
  std::shared_ptr<Test> test;
  try {
    if (!SDL_Init(SDL_INIT_VIDEO))
      throw std::runtime_error(std::string("sdl init error: ") + SDL_GetError());
    win = SDL_CreateWindow("demo", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (win == nullptr)
      throw std::runtime_error(std::string("could not create sdl window: ") + SDL_GetError());

    auto &inst = VulkanInstance::instance();
    inst.enableDebug();
    auto dev = inst.createDevice();

    test = std::make_shared<Test>(dev);
    test->setWindow(win);

  } catch (std::runtime_error& e) {
    printf("%s", e.what());
    return -1;
  }
  test->loop();
  test.reset();
  SDL_DestroyWindow(win);
  SDL_Quit();
  return 0;
}
