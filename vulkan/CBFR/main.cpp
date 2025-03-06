#include <exception>
#include <stdexcept>

#include "Manipulator.h"

#include "SDL2/SDL.h"
#include "SDL2/SDL_vulkan.h"

#include "VulkanDebug.h"
#include "VulkanView.h"
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanTools.h"
#include "VulkanInitializers.hpp"

#include "SimpleShape.h"

#define WM_PAINT 1

constexpr float fov = 60;


VulkanInstance inst;

struct {
  tg::mat4 prj;
  tg::mat4 view;
  tg::mat4 model;
  tg::vec4 cam;
} matrix_ubo;

struct{
  struct alignas(16) aligned_vec3 : vec3 {};
  aligned_vec3 light_pos[4] = {vec3(10, -10, 10), vec3(-10, -10, 10), vec3(-10, -10, -10), vec3(10, -10, -10)};
  aligned_vec3 light_color[4] = {vec3(300), vec3(300), vec3(300), vec3(300)};
} lights_ubo;

struct alignas(16){
  struct alignas(16) aligned_vec3 : vec3 {};

	float metallic;
	float roughness;
	float ao;
	aligned_vec3 albedo;
} material_ubo;

class Test {
public:
  Test(const std::shared_ptr<VulkanDevice> &dev) : _device(dev)
  {
    _manip.set_home(vec3(0, -30, 0), vec3(0), vec3(0, 0, 1));

    _swapchain = std::make_shared<VulkanSwapChain>(dev);
    _queue = dev->graphic_queue(0);

    create_sphere();
    create_pipe_layout();
  }

  ~Test()
  {
    vkDeviceWaitIdle(*_device);

    free_resource();
    for (auto fence : _fences)
      vkDestroyFence(*_device, fence, nullptr);

    _device->destroy_command_buffers(_cmd_bufs);

    vkDestroyRenderPass(*_device, _render_pass, nullptr);

    vkDestroySemaphore(*_device, _presentSemaphore, nullptr);
    vkDestroySemaphore(*_device, _renderSemaphore, nullptr);

    auto surface = _swapchain->surface();
    _swapchain.reset();

    if (surface) {
      vkDestroySurfaceKHR(inst, surface, nullptr);
    }

    if (_vert_buf) {
      vkDestroyBuffer(*_device, _vert_buf, nullptr);
      _vert_buf = VK_NULL_HANDLE;
    }

    if (_vert_mem) {
      vkFreeMemory(*_device, _vert_mem, nullptr);
      _vert_mem = VK_NULL_HANDLE;
    }

    if (_index_buf) {
      vkDestroyBuffer(*_device, _index_buf, nullptr);
      _index_buf = VK_NULL_HANDLE;
    }

    if (_index_mem) {
      vkFreeMemory(*_device, _index_mem, nullptr);
      _index_mem = VK_NULL_HANDLE;
    }

    if (_descript_pool) {
      vkDestroyDescriptorPool(*_device, _descript_pool, nullptr);
      _descript_pool = VK_NULL_HANDLE;
    }

    if (_pipe_layout) {
      vkDestroyPipelineLayout(*_device, _pipe_layout, nullptr);
      _pipe_layout = VK_NULL_HANDLE;
    }

    if (_pipeline) {
      vkDestroyPipeline(*_device, _pipeline, nullptr);
      _pipeline = VK_NULL_HANDLE;
    }
  }

  void set_window(SDL_Window *win)
  {
    SDL_GetWindowSize(win, &_w, &_h);

    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(win, inst, &surface))
      throw std::runtime_error("could not create vk surface.");

    _swapchain->set_surface(surface);
    _swapchain->realize(_w, _h, true);

    create_sync_object();

    int count = _swapchain->image_count();

    _render_pass = _swapchain->create_render_pass();

    _cmd_bufs = _device->create_command_buffers(count);

    create_pipeline();

    apply_resource();

    update_ubo();
  }

  void apply_resource()
  {
    int count = _swapchain->image_count();
    _depth = _swapchain->create_depth_image(_w, _h);
    _frame_bufs = _swapchain->create_frame_buffer(_render_pass, _depth);

    build_command_buffers(_frame_bufs, _render_pass);

    if (_fences.size() != _cmd_bufs.size()) {
      for (auto fence : _fences)
        vkDestroyFence(*_device, fence, nullptr);
      _fences = _device->create_fences(_cmd_bufs.size());
    }
  }

  void free_resource()
  {
    for (auto &framebuf : _frame_bufs)
      vkDestroyFramebuffer(*_device, framebuf, nullptr);
    _frame_bufs.clear();

    vkDestroyImageView(*_device, _depth.view, nullptr);
    vkDestroyImage(*_device, _depth.image, nullptr);
    vkFreeMemory(*_device, _depth.mem, nullptr);
    _depth = {VK_NULL_HANDLE};
  }

  void create_sync_object()
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
    ev.type = SDL_USEREVENT;
    ev.user.code = WM_PAINT;
    SDL_PushEvent(&ev);
  }

  void draw()
  {
    // vkWaitForFences(*_device, 1, _fences[]);
    auto [result, index] = _swapchain->acquire_image(_presentSemaphore);
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
    submitInfo.pCommandBuffers = &_cmd_bufs[index];  // Command buffers(s) to execute in this batch (submission)
    submitInfo.commandBufferCount = 1;               // One command buffer

    submitInfo.pWaitSemaphores = &_presentSemaphore;   // Semaphore(s) to wait upon before the submitted command buffer starts executing
    submitInfo.pSignalSemaphores = &_renderSemaphore;  // Semaphore(s) to be signaled when command buffers have completed

    // Submit to the graphics queue passing a wait fence
    VK_CHECK_RESULT(vkQueueSubmit(_queue, 1, &submitInfo, _fences[index]));

    {
      _frame = index;
      auto present = _swapchain->queue_present(_queue, _frame, _renderSemaphore);
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
          case SDL_QUIT:
            running = false;
            break;
          case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
              vkDeviceWaitIdle(*_device);
              _w = event.window.data1;
              _h = event.window.data2;
              _swapchain->realize(_w, _h, true);
              free_resource();
              apply_resource();
              update_ubo();
              update();
            }
            break;
          case SDL_USEREVENT:
            if (event.user.code == WM_PAINT)
              draw();
            break;
          case SDL_MOUSEMOTION:
            if (event.motion.state & SDL_BUTTON_LMASK) {
              _manip.rotate(event.motion.xrel, event.motion.yrel);
              update_ubo();
            } else if (event.motion.state & SDL_BUTTON_MMASK) {
            } else if (event.motion.state & SDL_BUTTON_RMASK) {
              _manip.translate(event.motion.xrel, -event.motion.yrel);
              update_ubo();
            }
            update();
            break;
          case SDL_MOUSEWHEEL: {
            _manip.zoom(event.wheel.y);
            update_ubo();
            update();
            break;
          }
          case SDL_KEYUP: {
            if (event.key.keysym.scancode == SDL_SCANCODE_SPACE)
              _manip.home();
            update_ubo();
            update();
          } break;
          default:
            break;
        }
      }
    }
  }

  void build_command_buffers(std::vector<VkFramebuffer> &framebuffers, VkRenderPass renderPass)
  {
    assert(framebuffers.size() == _cmd_bufs.size());

    VkCommandBufferBeginInfo buf_info = {};
    buf_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    buf_info.pNext = nullptr;

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

    for (int i = 0; i < _cmd_bufs.size(); i++) {
      renderPassBeginInfo.framebuffer = framebuffers[i];
      auto &cmd_buf = _cmd_bufs[i];
      VK_CHECK_RESULT(vkBeginCommandBuffer(cmd_buf, &buf_info));

      vkCmdBeginRenderPass(cmd_buf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      {
        VkViewport viewport = {};
        viewport.y = _h;
        viewport.width = _w;
        viewport.height = -_h;
        viewport.minDepth = 0;
        viewport.maxDepth = 1;
        vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
      }

      {
        VkRect2D scissor = {};
        scissor.extent.width = _w;
        scissor.extent.height = _h;
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        vkCmdSetScissor(cmd_buf, 0, 1, &scissor);
      }

      {
        vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline);
      }

      VkDescriptorSet dessets[2] = {_matrix_set, _material_set};
      vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipe_layout, 0, 2, dessets, 0, nullptr);

      {
        VkDeviceSize offset[2] = {0, _vert_count * sizeof(vec3)};
        VkBuffer bufs[2] = {};
        bufs[0] = _vert_buf;
        bufs[1] = _vert_buf;
        vkCmdBindVertexBuffers(cmd_buf, 0, 2, bufs, offset);
        vkCmdBindIndexBuffer(cmd_buf, _index_buf, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(cmd_buf, _index_count, 49, 0, 0, 0);
      }

      vkCmdEndRenderPass(cmd_buf);

      VK_CHECK_RESULT(vkEndCommandBuffer(cmd_buf));
    }
  }

  void create_pipe_layout()
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

    VkDescriptorSetLayout matrix_lay, material_lay;
    VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &descriptorLayout, nullptr, &matrix_lay));
  
    {
      VkDescriptorSetLayoutBinding material_binding = {};
      material_binding.binding = 2;
      material_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      material_binding.descriptorCount = 1;
      material_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

      descriptorLayout.bindingCount = 1;
      descriptorLayout.pBindings = &material_binding; 

      VK_CHECK_RESULT(vkCreateDescriptorSetLayout(*_device, &descriptorLayout, nullptr, &material_lay));
    }


    VkDescriptorSetLayout layouts[2] = {matrix_lay, material_lay};

    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.pNext = nullptr;
    pPipelineLayoutCreateInfo.setLayoutCount = 2;
    pPipelineLayoutCreateInfo.pSetLayouts = layouts;

    VkPipelineLayout pipe_layout;
    VK_CHECK_RESULT(vkCreatePipelineLayout(*_device, &pPipelineLayoutCreateInfo, nullptr, &pipe_layout));
    _pipe_layout = pipe_layout;

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
    _descript_pool = desPool;

    //----------------------------------------------------------------------------------------------------

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = desPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &matrix_lay;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &_matrix_set));
    
    int sz = ((sizeof(matrix_ubo) + 63) >> 8) << 8;
    int light_sz = sizeof(lights_ubo);
    VkDescriptorBufferInfo descriptor = {};
    _ubo_buf = _device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz + light_sz);
    descriptor.buffer = *_ubo_buf;
    descriptor.offset = 0;
    descriptor.range = sizeof(matrix_ubo);

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _matrix_set;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &descriptor;
    writeDescriptorSet.dstBinding = 0;

    vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);

    {
      descriptor.offset = sz;
      descriptor.range = sizeof(lights_ubo);
      writeDescriptorSet.dstSet = _matrix_set;
      writeDescriptorSet.dstBinding = 1;
      vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);
    }

    {
      VkDescriptorSetAllocateInfo allocInfo = {};
      allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
      allocInfo.descriptorPool = desPool;
      allocInfo.descriptorSetCount = 1;
      allocInfo.pSetLayouts = &material_lay;

      VK_CHECK_RESULT(vkAllocateDescriptorSets(*_device, &allocInfo, &_material_set));

      int sz = sizeof(material_ubo) * 49;
      _material_buf = _device->create_buffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
      VkDescriptorBufferInfo descriptor = {};
      descriptor.buffer = *_material_buf;
      descriptor.offset = 0;
      descriptor.range = sz;

      VkWriteDescriptorSet writeDescriptorSet = {};
      writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      writeDescriptorSet.dstSet = _material_set;
      writeDescriptorSet.descriptorCount = 1;
      writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      writeDescriptorSet.pBufferInfo = &descriptor;
      writeDescriptorSet.dstBinding = 2;

      vkUpdateDescriptorSets(*_device, 1, &writeDescriptorSet, 0, nullptr);
    }

    vkDestroyDescriptorSetLayout(*_device, matrix_lay, nullptr);
    vkDestroyDescriptorSetLayout(*_device, material_lay, nullptr);


    uint8_t *data = 0;
    VK_CHECK_RESULT(vkMapMemory(*_device, _ubo_buf->memory(), sz, sizeof(lights_ubo), 0, (void **)&data));
    memcpy(data, &lights_ubo, sizeof(lights_ubo));
    vkUnmapMemory(*_device, _ubo_buf->memory());

    {
      decltype(material_ubo) mate_bufs[49];
      for (int i = 0; i < 49; i++) {
        mate_bufs[i].metallic = (i / 7) / 7.f;
        mate_bufs[i].roughness = std::max((i % 7) / 7.f, 0.05f);
        mate_bufs[i].ao = 1;
        mate_bufs[i].albedo.set(1, 0, 0);
      }

      uint8_t *data = 0;
      VK_CHECK_RESULT(vkMapMemory(*_device, _material_buf->memory(), 0, _material_buf->size(), 0, (void **)&data));
      memcpy(data, &mate_bufs, _material_buf->size());
      vkUnmapMemory(*_device, _material_buf->memory());
    }
  }

  void create_pipeline()
  {
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = _pipe_layout;
    pipelineCreateInfo.renderPass = _render_pass;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

    VkPipelineRasterizationStateCreateInfo rasterizationState = {};
    rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationState.cullMode = VK_CULL_MODE_NONE;
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
    shaderStages[0].module = _device->create_shader("pbr.vert.spv");
    shaderStages[0].pName = "main";
    assert(shaderStages[0].module != VK_NULL_HANDLE);

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = _device->create_shader("pbr.frag.spv");
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

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(*_device, _device->get_or_create_pipecache(), 1, &pipelineCreateInfo, nullptr, &_pipeline));

    vkDestroyShaderModule(*_device, shaderStages[0].module, nullptr);
    vkDestroyShaderModule(*_device, shaderStages[1].module, nullptr);
  }

  void update_ubo()
  {
    matrix_ubo.cam = _manip.eye();
    matrix_ubo.view = _manip.view_matrix();
    matrix_ubo.model = tg::mat4::identity();
    matrix_ubo.prj = tg::perspective<float>(fov, float(_w) / _h, 0.1, 1000);
    // tg::near_clip(matrix_ubo.prj, tg::vec4(0, 0, -1, 0.5));
    uint8_t *data = 0;
    VK_CHECK_RESULT(vkMapMemory(*_device, _ubo_buf->memory(), 0, sizeof(matrix_ubo), 0, (void **)&data));
    memcpy(data, &matrix_ubo, sizeof(matrix_ubo));
    vkUnmapMemory(*_device, _ubo_buf->memory());
  }

  void create_sphere()
  {
    Sphere sp(vec3(0), 1);
    sp.build();
    auto &verts = sp.get_vertex();
    auto &norms = sp.get_norms();
    auto &uv = sp.get_uvs();
    auto &index = sp.get_index();
    _vert_count = verts.size();
    _index_count = index.size();

    struct StageBuffer {
      VkBuffer buffer;
      VkDeviceMemory mem;
    };
    StageBuffer vertices, indices;

    uint64_t vert_size = verts.size() * (sizeof(vec3) * 2 + sizeof(vec2));
    VkBufferCreateInfo vertexBufferInfo = {};
    vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    vertexBufferInfo.size = vert_size;
    vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &vertexBufferInfo, nullptr, &vertices.buffer));
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(*_device, vertices.buffer, &memReqs);

    VkMemoryAllocateInfo memAlloc = {};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &vertices.mem));

    void *data = 0;
    VK_CHECK_RESULT(vkMapMemory(*_device, vertices.mem, 0, memAlloc.allocationSize, 0, &data));
    uint64_t offset = 0;
    memcpy(data, verts.data(), verts.size() * sizeof(vec3));
    offset += verts.size() * sizeof(vec3);
    memcpy((uint8_t *)data + offset, norms.data(), norms.size() * sizeof(vec3));
    offset += norms.size() * sizeof(vec3);
    memcpy((uint8_t *)data + offset, uv.data(), uv.size() * sizeof(vec2));
    vkUnmapMemory(*_device, vertices.mem);
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, vertices.buffer, vertices.mem, 0));

    vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &vertexBufferInfo, nullptr, &_vert_buf));
    vkGetBufferMemoryRequirements(*_device, _vert_buf, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &_vert_mem));
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, _vert_buf, _vert_mem, 0));

    uint64_t index_size = index.size() * sizeof(uint16_t);
    VkBufferCreateInfo indexbufferInfo = {};
    indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    indexbufferInfo.size = index_size;
    indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    // Copy index data to a buffer visible to the host (staging buffer)
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &indexbufferInfo, nullptr, &indices.buffer));
    vkGetBufferMemoryRequirements(*_device, indices.buffer, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &indices.mem));
    VK_CHECK_RESULT(vkMapMemory(*_device, indices.mem, 0, index_size, 0, &data));
    memcpy(data, index.data(), index_size);
    vkUnmapMemory(*_device, indices.mem);
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, indices.buffer, indices.mem, 0));

    // Create destination buffer with device only visibility
    indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VK_CHECK_RESULT(vkCreateBuffer(*_device, &indexbufferInfo, nullptr, &_index_buf));
    vkGetBufferMemoryRequirements(*_device, _index_buf, &memReqs);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = *_device->memory_type_index(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK_RESULT(vkAllocateMemory(*_device, &memAlloc, nullptr, &_index_mem));
    VK_CHECK_RESULT(vkBindBufferMemory(*_device, _index_buf, _index_mem, 0));

    {
      VkCommandBuffer cmdBuffer;

      VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
      cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      cmdBufAllocateInfo.commandPool = _device->command_pool();
      cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmdBufAllocateInfo.commandBufferCount = 1;

      VK_CHECK_RESULT(vkAllocateCommandBuffers(*_device, &cmdBufAllocateInfo, &cmdBuffer));

      VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
      VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));

      VkBufferCopy copyRegion = {};
      copyRegion.size = vert_size;
      vkCmdCopyBuffer(cmdBuffer, vertices.buffer, _vert_buf, 1, &copyRegion);

      copyRegion.size = index_size;
      vkCmdCopyBuffer(cmdBuffer, indices.buffer, _index_buf, 1, &copyRegion);

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

      VK_CHECK_RESULT(vkQueueSubmit(_queue, 1, &submitInfo, fence));
      VK_CHECK_RESULT(vkWaitForFences(*_device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
      vkDestroyFence(*_device, fence, nullptr);
      vkFreeCommandBuffers(*_device, _device->command_pool(), 1, &cmdBuffer);
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

  VkRenderPass _render_pass = VK_NULL_HANDLE;
  VulkanSwapChain::ImageUnit _depth;

  VkSemaphore _presentSemaphore;
  VkSemaphore _renderSemaphore;

  VkQueue _queue;

  std::vector<VkCommandBuffer> _cmd_bufs;
  std::vector<VkFramebuffer> _frame_bufs;
  std::vector<VkFence> _fences;

  VkBuffer _vert_buf;
  VkDeviceMemory _vert_mem;
  VkBuffer _index_buf;
  VkDeviceMemory _index_mem;

  VkDescriptorPool _descript_pool = VK_NULL_HANDLE;

  VkDescriptorSet _matrix_set = VK_NULL_HANDLE;
  VkDescriptorSet _material_set = VK_NULL_HANDLE;

  VkPipelineLayout _pipe_layout = VK_NULL_HANDLE;
  VkPipeline _pipeline = VK_NULL_HANDLE;

  std::shared_ptr<VulkanBuffer> _ubo_buf;

  std::shared_ptr<VulkanBuffer> _material_buf;

  uint32_t _vert_count = 0;
  uint32_t _index_count = 0;

  Manipulator _manip;
};


int main(int argc, char** argv)
{
  SDL_Window* win = 0;
  std::shared_ptr<Test> test;
  try {
    inst.initialize();

    if (SDL_Init(SDL_INIT_VIDEO) != 0)
      throw std::runtime_error("sdl init error.");
    win = SDL_CreateWindow("demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (win == nullptr)
      throw std::runtime_error("could not create sdl window.");

    inst.enable_debug();
    auto dev = inst.create_device();

    test = std::make_shared<Test>(dev);
    test->set_window(win);

  } catch (std::runtime_error& e) {
    printf("%s", e.what());
    return -1;
  }
  test->loop();
  return 0;
}