#include "ShadowView.h"

#include "VulkanDebug.h"
#include "VulkanView.h"
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanTools.h"
#include "VulkanPass.h"
#include "Manipulator.h"
#include "DepthPass.h"
#include "VulkanInitializers.hpp"

#include "VulkanPipeline.h"
#include "TexturePipeline.h"
#include "DepthPipeline.h"
#include "DepthPersPipeline.h"

#include "SimpleShape.h"
#include "RenderData.h"
#include "GLTFLoader.h"
#include "MeshInstance.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include "config.h"
#include "imgui/imgui.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#define WM_PAINT 1

constexpr float fov = 60;

PBRBase pbr;
ParallelLight light;

namespace
{
constexpr float ShadowNear = 0.1f;
const tg::boundingbox SceneShadowBounds(tg::vec3(-12, -1, -12), tg::vec3(12, 20, 12));

struct PsmWarp
{
  tg::mat4 matrix;
  std::vector<tg::vec3> warpedPoints;
  tg::vec3 warpedLightDirection;
};

struct Plane
{
  tg::vec3 normal;
  float distance;
};

constexpr uint32_t BoxEdges[12][2] = {
  {0, 1}, {2, 3}, {4, 5}, {6, 7},
  {0, 2}, {1, 3}, {4, 6}, {5, 7},
  {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

void addUniquePoint(std::vector<tg::vec3> &points, const tg::vec3 &point)
{
  for (const auto &existing : points) {
    if (tg::length(existing - point) < 0.0001f)
      return;
  }
  points.push_back(point);
}

bool pointInsideBox(const tg::vec3 &point, const tg::boundingbox &box)
{
  constexpr float epsilon = 0.0001f;
  return point.x() >= box.min().x() - epsilon && point.x() <= box.max().x() + epsilon &&
         point.y() >= box.min().y() - epsilon && point.y() <= box.max().y() + epsilon &&
         point.z() >= box.min().z() - epsilon && point.z() <= box.max().z() + epsilon;
}

Plane makePlane(const tg::vec3 &a, const tg::vec3 &b, const tg::vec3 &c,
                const tg::vec3 &insidePoint)
{
  Plane plane;
  plane.normal = tg::normalize(tg::cross(b - a, c - a));
  plane.distance = -tg::dot(plane.normal, a);
  if (tg::dot(plane.normal, insidePoint) + plane.distance < 0.0f) {
    plane.normal = -plane.normal;
    plane.distance = -plane.distance;
  }
  return plane;
}

std::array<Plane, 6> frustumPlanes(const std::array<tg::vec3, 8> &corners)
{
  tg::vec3 center(0.0f);
  for (const auto &corner : corners)
    center += corner;
  center /= float(corners.size());

  // corner 顺序：每个 near/far 平面内依次为左下、右下、左上、右上。
  return {
    makePlane(corners[0], corners[2], corners[3], center), // near
    makePlane(corners[4], corners[5], corners[7], center), // far
    makePlane(corners[0], corners[4], corners[6], center), // left
    makePlane(corners[1], corners[3], corners[7], center), // right
    makePlane(corners[0], corners[1], corners[5], center), // bottom
    makePlane(corners[2], corners[6], corners[7], center)  // top
  };
}

bool pointInsideFrustum(const tg::vec3 &point, const std::array<Plane, 6> &planes)
{
  constexpr float epsilon = 0.0001f;
  for (const auto &plane : planes) {
    if (tg::dot(plane.normal, point) + plane.distance < -epsilon)
      return false;
  }
  return true;
}

bool segmentBoxIntersection(const tg::vec3 &start, const tg::vec3 &end,
                            const tg::boundingbox &box, float &entry, float &exit)
{
  const tg::vec3 direction = end - start;
  entry = 0.0f;
  exit = 1.0f;

  for (int axis = 0; axis < 3; ++axis) {
    if (std::abs(direction[axis]) < 0.000001f) {
      if (start[axis] < box.min()[axis] || start[axis] > box.max()[axis])
        return false;
      continue;
    }

    float t0 = (box.min()[axis] - start[axis]) / direction[axis];
    float t1 = (box.max()[axis] - start[axis]) / direction[axis];
    if (t0 > t1)
      std::swap(t0, t1);
    entry = std::max(entry, t0);
    exit = std::min(exit, t1);
    if (entry > exit)
      return false;
  }
  return true;
}

std::vector<tg::vec3> frustumSceneIntersection(const std::array<tg::vec3, 8> &frustum,
                                                const tg::boundingbox &sceneBounds)
{
  const auto planes = frustumPlanes(frustum);
  std::array<tg::vec3, 8> sceneCorners;
  for (uint32_t i = 0; i < sceneCorners.size(); ++i)
    sceneCorners[i] = sceneBounds.corner(i);

  std::vector<tg::vec3> points;

  // 保留两个凸体中已经位于另一个凸体内部的原始顶点。
  for (const auto &corner : frustum) {
    if (pointInsideBox(corner, sceneBounds))
      addUniquePoint(points, corner);
  }
  for (const auto &corner : sceneCorners) {
    if (pointInsideFrustum(corner, planes))
      addUniquePoint(points, corner);
  }

  // 收集视锥边与场景 AABB 表面的交点。
  for (const auto &edge : BoxEdges) {
    const tg::vec3 start = frustum[edge[0]];
    const tg::vec3 end = frustum[edge[1]];
    float entry = 0.0f;
    float exit = 0.0f;
    if (segmentBoxIntersection(start, end, sceneBounds, entry, exit)) {
      addUniquePoint(points, start + (end - start) * entry);
      addUniquePoint(points, start + (end - start) * exit);
    }
  }

  // 收集场景 AABB 边与六个视锥平面的交点。
  for (const auto &edge : BoxEdges) {
    const tg::vec3 start = sceneCorners[edge[0]];
    const tg::vec3 end = sceneCorners[edge[1]];
    const tg::vec3 direction = end - start;
    for (const auto &plane : planes) {
      const float startDistance = tg::dot(plane.normal, start) + plane.distance;
      const float endDistance = tg::dot(plane.normal, end) + plane.distance;
      const float denominator = startDistance - endDistance;
      if (std::abs(denominator) < 0.000001f)
        continue;

      const float t = startDistance / denominator;
      if (t < 0.0f || t > 1.0f)
        continue;

      const tg::vec3 point = start + direction * t;
      if (pointInsideFrustum(point, planes) && pointInsideBox(point, sceneBounds))
        addUniquePoint(points, point);
    }
  }

  return points;
}

std::vector<tg::vec3> includePotentialCasters(const std::vector<tg::vec3> &receivers,
                                               const tg::vec3 &lightDirection,
                                               const tg::boundingbox &sceneBounds)
{
  const tg::vec3 towardLight = tg::normalize(lightDirection);
  std::vector<tg::vec3> points = receivers;
  points.reserve(receivers.size() * 2);

  // 对每个可见接收点，沿指向光源的方向延伸到场景盒边界。该线段覆盖可能
  // 位于接收点与方向光之间的 caster，同时不会把无关的整个场景盒纳入拟合。
  for (const auto &receiver : receivers) {
    float exitDistance = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
      if (towardLight[axis] > 0.000001f)
        exitDistance = std::min(exitDistance,
            (sceneBounds.max()[axis] - receiver[axis]) / towardLight[axis]);
      else if (towardLight[axis] < -0.000001f)
        exitDistance = std::min(exitDistance,
            (sceneBounds.min()[axis] - receiver[axis]) / towardLight[axis]);
    }

    if (exitDistance > 0.0f && std::isfinite(exitDistance))
      addUniquePoint(points, receiver + towardLight * exitDistance);
  }
  return points;
}

std::array<tg::vec3, 8> cameraFrustumCorners(const tg::vec3 &eye,
                                             const tg::vec3 &target,
                                             const tg::vec3 &up,
                                             float verticalFov,
                                             float aspect,
                                             float nearDistance,
                                             float farDistance)
{
  const tg::vec3 forward = tg::normalize(target - eye);
  const tg::vec3 right = tg::normalize(tg::cross(forward, up));
  const tg::vec3 cameraUp = tg::normalize(tg::cross(right, forward));
  const float tanHalfFov = std::tan(tg::radians(verticalFov * 0.5f));

  std::array<tg::vec3, 8> corners;
  int index = 0;
  for (float distance : {nearDistance, farDistance}) {
    const tg::vec3 center = eye + forward * distance;
    const float halfHeight = tanHalfFov * distance;
    const float halfWidth = halfHeight * aspect;

    for (float y : {-1.0f, 1.0f}) {
      for (float x : {-1.0f, 1.0f}) {
        corners[index++] = center + right * (x * halfWidth) + cameraUp * (y * halfHeight);
      }
    }
  }
  return corners;
}

PsmWarp buildPsmWarp(const tg::vec3 &lightDirection,
                     const tg::vec3 &cameraEye,
                     const tg::vec3 &cameraTarget,
                     float shadowFar,
                     const std::vector<tg::vec3> &fitPoints)
{
  const tg::vec3 light = tg::normalize(lightDirection);
  const tg::vec3 cameraForward = tg::normalize(cameraTarget - cameraEye);

  // PSM 在光线与视线接近平行时会退化。此时选择一个稳定的备用轴，避免
  // cross 得到零向量；这会平滑降低扭曲效果，但不会产生 NaN 矩阵。
  tg::vec3 right = tg::cross(light, cameraForward);
  if (tg::length(right) < 0.001f) {
    const tg::vec3 fallback = std::abs(light.y()) < 0.99f
        ? tg::vec3(0, 1, 0)
        : tg::vec3(1, 0, 0);
    right = tg::cross(light, fallback);
  }
  right = tg::normalize(right);
  const tg::vec3 forward = tg::normalize(tg::cross(light, right));

  tg::boundingbox bounds(fitPoints[0], fitPoints[0]);
  for (size_t i = 1; i < fitPoints.size(); ++i)
    bounds.expand(fitPoints[i]);

  const tg::vec3 center = bounds.center();
  const float radius = std::max(bounds.radius(), 0.001f);
  const float warpNear = std::max(std::sqrt(shadowFar * ShadowNear) - ShadowNear, 0.01f);
  const float distance = radius + warpNear;
  const float warpFar = warpNear + radius * 2.0f;
  const float ratio = std::clamp(radius / distance, 0.0f, 0.999f);
  const float warpFov = tg::degrees(2.0f * std::asin(ratio));

  const tg::mat4 warpView = tg::lookat(center - forward * distance, center, light);
  const tg::mat4 warpProjection = tg::perspective<float>(warpFov, 1.0f, warpNear, warpFar);

  PsmWarp result;
  result.matrix = warpProjection * warpView;
  result.warpedPoints.reserve(fitPoints.size());
  for (const auto &point : fitPoints)
    result.warpedPoints.push_back(result.matrix * point);

  // 方向向量不能直接经历透视除法。用两个相邻世界点的扭曲结果之差，得到
  // 光照方向在 post-perspective space 中的实际方向。
  const tg::vec3 warpedCenter = result.matrix * center;
  const tg::vec3 warpedAlongLight = result.matrix * (center + light);
  const tg::vec3 warpedLight = warpedAlongLight - warpedCenter;
  result.warpedLightDirection = tg::length(warpedLight) > 0.000001f
      ? tg::normalize(warpedLight)
      : light;
  return result;
}

void buildWarpedLightMatrices(const PsmWarp &warp, ShadowMatrix &shadowMatrix)
{
  tg::boundingbox warpedBounds(warp.warpedPoints[0], warp.warpedPoints[0]);
  for (size_t i = 1; i < warp.warpedPoints.size(); ++i)
    warpedBounds.expand(warp.warpedPoints[i]);

  const tg::vec3 center = warpedBounds.center();
  const tg::vec3 extent = warpedBounds.max() - warpedBounds.min();
  const float lightDistance = std::max(tg::length(extent), 0.001f);
  const tg::vec3 lightEye = center + warp.warpedLightDirection * lightDistance;
  const tg::vec3 viewForward = tg::normalize(center - lightEye);
  const tg::vec3 fallbackUp = std::abs(viewForward.y()) < 0.99f
      ? tg::vec3(0, 1, 0)
      : tg::vec3(0, 0, 1);

  // 使用固定且稳定的 up，不根据每帧投影轮廓旋转阴影相机，避免最小矩形
  // 候选边切换导致 Shadow Map texel 网格旋转和明显抖动。
  shadowMatrix.view = tg::lookat(lightEye, center, fallbackUp);

  const tg::vec3 firstLightCorner = shadowMatrix.view * warp.warpedPoints[0];
  tg::boundingbox lightBounds(firstLightCorner, firstLightCorner);
  for (size_t i = 1; i < warp.warpedPoints.size(); ++i)
    lightBounds.expand(shadowMatrix.view * warp.warpedPoints[i]);

  constexpr float padding = 0.01f;
  const float nearPlane = std::max(0.001f, -lightBounds.max().z() - padding);
  const float farPlane = std::max(nearPlane + 0.001f, -lightBounds.min().z() + padding);
  shadowMatrix.prj = tg::ortho(lightBounds.min().x() - padding,
                               lightBounds.max().x() + padding,
                               lightBounds.min().y() - padding,
                               lightBounds.max().y() + padding,
                               nearPlane,
                               farPlane);
  shadowMatrix.mvp = shadowMatrix.prj * shadowMatrix.view;
}
} // namespace

ShadowView::ShadowView(const std::shared_ptr<VulkanDevice> &dev) : VulkanView(dev, true)
{
  createSphere();

  GLTFLoader loader;
  _tree = loader.loadFile(ROOT_DIR "/data/oaktree.gltf");
  _tree->setTransform(tg::mat4(tg::translate(tg::vec3(0, 1, 0)) * tg::scale(4.0f)));

  _deer = loader.loadFile(ROOT_DIR "/data/deer.gltf");
  _deer->setTransform(tg::mat4(tg::translate(tg::vec3(3, 1, 0)) * tg::rotate(tg::radians(30.f), tg::vec3(0, 1, 0)) * tg::scale(1.f)));

  _shadowPipeline = std::make_shared<ShadowPipeline>(dev);

  _depthPipeline = std::make_shared<DepthPersPipeline>(dev, 2048, 2048);
  _depthImage = _device->createDepthImage(2048, 2048, VK_FORMAT_D32_SFLOAT);

  _depthPass = std::make_shared<DepthPass>(dev);

  {
    _basicTexture = std::make_shared<VulkanTexture>();
    _basicTexture->setImage(32, 32, tg::Tvec4<uint8_t>(128, 128, 128, 255));
    _basicTexture->realize(_device);
  }

  {
    _hudPass = std::make_shared<HUDPass>(dev);
    _hudPipeline = std::make_shared<HUDPipeline>(dev);
    _hudRect = std::make_shared<HUDRect>(dev);
    _hudRect->setGeometry(50, 50, 400, 400);
  }

  createPipeLayout();

  setUniforms();

  //manipulator()->setHome({0, 0, 30}, {0, 0, 0}, {0, 1, 0});
}

ShadowView::~ShadowView()
{
  vkDeviceWaitIdle(*device());

  if (_vertBuf) {
    vkDestroyBuffer(*device(), _vertBuf, nullptr);
    _vertBuf = VK_NULL_HANDLE;
  }

  if (_vertMem) {
    vkFreeMemory(*device(), _vertMem, nullptr);
    _vertMem = VK_NULL_HANDLE;
  }

  if (_indexBuf) {
    vkDestroyBuffer(*device(), _indexBuf, nullptr);
    _indexBuf = VK_NULL_HANDLE;
  }

  if (_indexMem) {
    vkFreeMemory(*device(), _indexMem, nullptr);
    _indexMem = VK_NULL_HANDLE;
  }

  _hudRect.reset();

  if (_shadowSampler) {
    vkDestroySampler(*device(), _shadowSampler, nullptr);
    _shadowSampler = VK_NULL_HANDLE;
  }

  if (_descriptPool) {
    vkDestroyDescriptorPool(*device(), _descriptPool, nullptr);
    _descriptPool = VK_NULL_HANDLE;
  }

  for (int i = 0; i < _depthFrames.size(); i++) {
    vkDestroyFramebuffer(*_device, _depthFrames[i], 0);
  }
  _depthFrames.clear();

  for (int i = 0; i < _hudFrames.size(); i++)
  {
    vkDestroyFramebuffer(*_device, _hudFrames[i], 0);
  }
  _hudFrames.clear();
}

void ShadowView::createSphere()
{
  Box box(vec3(0), vec3(20, 2, 20));
  box.build();
  auto &verts = box.getVertex();
  auto &norms = box.getNorms();
  auto &uv = box.getUvs();
  auto &index = box.getIndex();
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
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &vertexBufferInfo, nullptr, &vertices.buffer));
  VkMemoryRequirements memReqs;
  vkGetBufferMemoryRequirements(*device(), vertices.buffer, &memReqs);

  VkMemoryAllocateInfo memAlloc = {};
  memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &vertices.mem));

  void *data = 0;
  VK_CHECK_RESULT(vkMapMemory(*device(), vertices.mem, 0, memAlloc.allocationSize, 0, &data));
  uint64_t offset = 0;
  memcpy(data, verts.data(), verts.size() * sizeof(vec3));
  offset += verts.size() * sizeof(vec3);
  memcpy((uint8_t *)data + offset, norms.data(), norms.size() * sizeof(vec3));
  offset += norms.size() * sizeof(vec3);
  memcpy((uint8_t *)data + offset, uv.data(), uv.size() * sizeof(vec2));
  vkUnmapMemory(*device(), vertices.mem);
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), vertices.buffer, vertices.mem, 0));

  vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &vertexBufferInfo, nullptr, &_vertBuf));
  vkGetBufferMemoryRequirements(*device(), _vertBuf, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &_vertMem));
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), _vertBuf, _vertMem, 0));

  uint64_t indexSize = index.size() * sizeof(uint16_t);
  VkBufferCreateInfo indexbufferInfo = {};
  indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  indexbufferInfo.size = indexSize;
  indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  // Copy index data to a buffer visible to the host (staging buffer)
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &indexbufferInfo, nullptr, &indices.buffer));
  vkGetBufferMemoryRequirements(*device(), indices.buffer, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &indices.mem));
  VK_CHECK_RESULT(vkMapMemory(*device(), indices.mem, 0, indexSize, 0, &data));
  memcpy(data, index.data(), indexSize);
  vkUnmapMemory(*device(), indices.mem);
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), indices.buffer, indices.mem, 0));

  // Create destination buffer with device only visibility
  indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  VK_CHECK_RESULT(vkCreateBuffer(*device(), &indexbufferInfo, nullptr, &_indexBuf));
  vkGetBufferMemoryRequirements(*device(), _indexBuf, &memReqs);
  memAlloc.allocationSize = memReqs.size;
  memAlloc.memoryTypeIndex = *device()->memoryTypeIndex(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK_RESULT(vkAllocateMemory(*device(), &memAlloc, nullptr, &_indexMem));
  VK_CHECK_RESULT(vkBindBufferMemory(*device(), _indexBuf, _indexMem, 0));

  {
    VkCommandBuffer cmdBuffer;

    VkCommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocateInfo.commandPool = device()->commandPool();
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocateInfo.commandBufferCount = 1;

    VK_CHECK_RESULT(vkAllocateCommandBuffers(*device(), &cmdBufAllocateInfo, &cmdBuffer));

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
    VK_CHECK_RESULT(vkCreateFence(*device(), &fenceCreateInfo, nullptr, &fence));

    VK_CHECK_RESULT(vkQueueSubmit(device()->transferQueue(), 1, &submitInfo, fence));
    VK_CHECK_RESULT(vkWaitForFences(*device(), 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT));
    vkDestroyFence(*device(), fence, nullptr);
    vkFreeCommandBuffers(*device(), device()->commandPool(), 1, &cmdBuffer);
  }

  vkDestroyBuffer(*device(), vertices.buffer, nullptr);
  vkDestroyBuffer(*device(), indices.buffer, nullptr);
  vkFreeMemory(*device(), vertices.mem, nullptr);
  vkFreeMemory(*device(), indices.mem, nullptr);
}

void ShadowView::setUniforms()
{
  light.lightDir = tg::normalize(vec3(0, 1, 2));
  light.lightColor = vec3(10);

  pbr.albedo = vec3(0.8);
  pbr.ao = 1;
  pbr.metallic = 0.2;
  pbr.roughness = 0.7;
  uint8_t *data = 0;

  VK_CHECK_RESULT(vkMapMemory(*device(), _material->memory(), 0, sizeof(pbr), 0, (void **)&data));
  memcpy(data, &pbr, sizeof(pbr));
  vkUnmapMemory(*device(), _material->memory());

  _shadowBuf = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sizeof(ShadowMatrix));

  updateLight();
}

void ShadowView::updateUbo()
{
  _matrix.eye = manipulator()->eye();
  _matrix.view = manipulator()->viewMatrix();
  _matrix.prj = tg::perspective<float>(fov, float(width()) / height(), 0.1, 1000);

  void *data = 0;
  {
    VK_CHECK_RESULT(vkMapMemory(*device(), _uboBuf->memory(), 0, sizeof(_matrix), 0, (void **)&data));
    memcpy(data, &_matrix, sizeof(_matrix));
    vkUnmapMemory(*device(), _uboBuf->memory());
  }

  const tg::vec3 cameraEye = manipulator()->eye();
  const tg::vec3 cameraTarget = manipulator()->target();
  const tg::vec3 cameraUp = manipulator()->up();
  const float aspect = float(width()) / std::max(float(height()), 1.0f);
  const float shadowFar = std::max(ShadowNear + 1.0f,
      tg::distance(cameraEye, SceneShadowBounds.center()) + SceneShadowBounds.radius());
  const auto frustumCorners = cameraFrustumCorners(cameraEye, cameraTarget, cameraUp,
                                                   fov, aspect, ShadowNear, shadowFar);

  // PSM warp 只拟合“当前相机视锥与场景阴影范围的交集”，避免将完整远端
  // 视锥和整个场景盒做 union 后留下大量空白区域、浪费 Shadow Map texel。
  std::vector<tg::vec3> receivers = frustumSceneIntersection(frustumCorners,
                                                             SceneShadowBounds);
  if (receivers.empty()) {
    // 相机暂时完全看不到场景盒时仍保持一个有限、有效的矩阵。
    for (uint32_t i = 0; i < 8; ++i)
      receivers.push_back(SceneShadowBounds.corner(i));
  }

  PsmWarp warp = buildPsmWarp(light.lightDir, cameraEye, cameraTarget,
                              shadowFar, receivers);

  // 最终光照正交投影还需覆盖能遮挡这些 receiver 的物体。沿光源方向延伸
  // receiver 到场景盒边界，只把潜在 caster 体积加入 light-space fitting。
  const std::vector<tg::vec3> shadowPoints = includePotentialCasters(
      receivers, light.lightDir, SceneShadowBounds);
  warp.warpedPoints.clear();
  warp.warpedPoints.reserve(shadowPoints.size());
  for (const auto &point : shadowPoints)
    warp.warpedPoints.push_back(warp.matrix * point);

  _shadowMatrix.pers = warp.matrix;
  buildWarpedLightMatrices(warp, _shadowMatrix);

  {
    VK_CHECK_RESULT(vkMapMemory(*device(), _shadowBuf->memory(), 0, sizeof(ShadowMatrix), 0, (void **)&data));
    memcpy(data, &_shadowMatrix, sizeof(ShadowMatrix));
    vkUnmapMemory(*device(), _shadowBuf->memory());
  }
}

void ShadowView::updateLight()
{
  _shadowMatrix.light = light.lightDir;

  uint8_t *data = 0;
  {
    VK_CHECK_RESULT(vkMapMemory(*device(), _light->memory(), 0, sizeof(light), 0, (void **)&data));
    memcpy(data, &light, sizeof(light));
  vkUnmapMemory(*device(), _light->memory());
  }
}

void ShadowView::resize(int w, int h)
{
  updateUbo();
}

void ShadowView::updateScene()
{
  auto fun = [this]() {
    tg::vec3 dir;
    auto x = tg::radians(_lightDir.x());
    auto y = tg::radians(_lightDir.y());
    dir.x() = cos(x) * cos(y);
    dir.y() = sin(y);
    dir.z() = sin(x) * cos(y);

    tg::normalize(dir);
    light.lightDir = dir;

    updateLight();
    updateUbo();
  };

  if (_imgui) {
    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_Once);

    ImGui::Begin("test");

    if (ImGui::SliderFloat("x", &_lightDir.x(), -180, 180)) {
      fun();
    }

    if (ImGui::SliderFloat("y", &_lightDir.y(), 0, 90)) {
      fun();
    }

    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
  }
}

void ShadowView::keyUp(int key)
{
  if (key == SDL_SCANCODE_UP)
    manipulator()->rotate(0, 1);
  else if (key == SDL_SCANCODE_DOWN)
    manipulator()->rotate(0, -1);
  else if (key == SDL_SCANCODE_LEFT)
    manipulator()->rotate(-1, 0);
  else if (key == SDL_SCANCODE_RIGHT)
    manipulator()->rotate(1, 0);

  updateUbo();
}

void ShadowView::createCommandBuffers()
{
  int count = MaxConcurrentFrames;
  if (count != _cmdBufs.size()) {
    _device->destroyCommandBuffers(_cmdBufs);
    _cmdBufs = _device->createCommandBuffers(count);
  }
}

void ShadowView::buildDepthCommandBuffer(VkCommandBuffer cmdBuf)
{
  tg::mat4 mt;
  mt.identity();
  if (_depthPipeline && _depthPipeline->valid()) {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_depthPipeline);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPipeline->pipeLayout(), 0, 1, &_shadowMatrixSet, 0, nullptr);

    vkCmdPushConstants(cmdBuf, _depthPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &mt);

    VkWriteDescriptorSet textureSet = {};
    textureSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    textureSet.dstSet = 0;
    textureSet.dstBinding = 0;
    textureSet.descriptorCount = 1;
    textureSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = _basicTexture->descriptor();
    textureSet.pImageInfo = &descriptor;
    _device->vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _depthPipeline->pipeLayout(), 1, 1, &textureSet);

    {
      VkDeviceSize offset[3] = {0, _vertCount * sizeof(vec3), _vertCount * (sizeof(vec3) + sizeof(vec2))};
      VkBuffer bufs[3] = {};
      bufs[0] = _vertBuf;
      bufs[1] = _vertBuf;
      bufs[2] = _vertBuf;

      vkCmdBindVertexBuffers(cmdBuf, 0, 3, bufs, offset);
      vkCmdBindIndexBuffer(cmdBuf, _indexBuf, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmdBuf, _indexCount, 1, 0, 0, 0);
    }

    _tree->buildDepthTextureCommandBuffer(cmdBuf, _depthPipeline->pipeLayout());

    _deer->buildDepthTextureCommandBuffer(cmdBuf, _depthPipeline->pipeLayout());
  }
}

void ShadowView::recordCommandBuffer(VkCommandBuffer cmdBuf, uint32_t i)
{
  if (!_depthPipeline->valid() || !_shadowPipeline->valid())
    return;

  auto &framebuffers = frameBuffers();
  auto &activeRenderPass = *renderPass();
  assert(i < framebuffers.size());
  assert(i < _depthFrames.size());
  assert(i < _hudFrames.size());

  VkCommandBufferBeginInfo bufInfo = {};
  bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bufInfo.pNext = nullptr;

  VkClearValue clearValues[2];

  VkRenderPassBeginInfo renderPassBeginInfo = {};
  renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassBeginInfo.pNext = nullptr;
  renderPassBeginInfo.renderArea.offset.x = 0;
  renderPassBeginInfo.renderArea.offset.y = 0;
  renderPassBeginInfo.renderArea.extent.width = _w;
  renderPassBeginInfo.renderArea.extent.height = _h;
  renderPassBeginInfo.clearValueCount = 1;
  renderPassBeginInfo.pClearValues = clearValues;

    renderPassBeginInfo.renderPass = *_depthPass;
    renderPassBeginInfo.framebuffer = _depthFrames[i];
    renderPassBeginInfo.renderArea.extent.width = _depthImage->width();
    renderPassBeginInfo.renderArea.extent.height = _depthImage->height();
    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuf, &bufInfo));

    clearValues[0].depthStencil = {1.f, 0};
    renderPassBeginInfo.clearValueCount = 1;
    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    buildDepthCommandBuffer(cmdBuf);

    vkCmdEndRenderPass(cmdBuf);

    clearValues[0].color = {{0.0, 0.0, 0.2, 1.0}};
    clearValues[1].depthStencil = {1.f, 0};
    renderPassBeginInfo.clearValueCount = 2;

    renderPassBeginInfo.renderPass = activeRenderPass;
    renderPassBeginInfo.framebuffer = framebuffers[i];
    renderPassBeginInfo.renderArea.extent.width = width();
    renderPassBeginInfo.renderArea.extent.height = height();

    vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    buildCommandBuffer(cmdBuf);

    vkCmdEndRenderPass(cmdBuf);

    {
      renderPassBeginInfo.renderPass = *_hudPass;
      renderPassBeginInfo.framebuffer = _hudFrames[i];
      renderPassBeginInfo.clearValueCount = 0;
      vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

      {
        VkViewport viewport = {};
        viewport.width = _w;
        viewport.height = _h;
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

      vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_hudPipeline);
      tg::mat4 mat;
      mat.identity();
      mat[0][0] = 2.0 / width();
      mat[1][1] = 2.0 / height();
      mat = tg::translate(-1.f, -1.f, 0.f) * mat;
      vkCmdPushConstants(cmdBuf, _hudPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat), &mat);
      _hudRect->fillCommand(cmdBuf, _hudPipeline.get());
      vkCmdEndRenderPass(cmdBuf);
    }

  VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuf));
}

void ShadowView::buildCommandBuffer(VkCommandBuffer cmdBuf)
{
  tg::mat4 mt;
  mt.identity();
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

  if (_shadowPipeline && _shadowPipeline->valid()) {
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, *_shadowPipeline);

    uint32_t offset[1] = {};
    VkDescriptorSet dessets[3] = {_matrixSet, _lightSet, _pbrSet};
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 0, 3, dessets, 1, offset);

    VkWriteDescriptorSet textureSet = {};
    textureSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    textureSet.dstSet = 0;
    textureSet.dstBinding = 0;
    textureSet.descriptorCount = 1;
    textureSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    auto descriptor = _basicTexture->descriptor();
    textureSet.pImageInfo = &descriptor;
    _device->vkCmdPushDescriptorSetKHR(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 3, 1, &textureSet);

    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 4, 1, &_shadowMatrixSet, 0, 0);
    vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadowPipeline->pipeLayout(), 5, 1, &_shadowTextureSet, 0, 0);

    vkCmdPushConstants(cmdBuf, _shadowPipeline->pipeLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Transform), &mt);

    {
      VkDeviceSize offset[3] = {0, _vertCount * sizeof(vec3), _vertCount * (sizeof(vec3) + sizeof(vec2))};
      VkBuffer bufs[3] = {};
      bufs[0] = _vertBuf;
      bufs[1] = _vertBuf;
      bufs[2] = _vertBuf;

      vkCmdBindVertexBuffers(cmdBuf, 0, 3, bufs, offset);
      vkCmdBindIndexBuffer(cmdBuf, _indexBuf, 0, VK_INDEX_TYPE_UINT16);
      vkCmdDrawIndexed(cmdBuf, _indexCount, 1, 0, 0, 0);
    }
  }

  _tree->buildTextureCommandBuffer(cmdBuf, _shadowPipeline->pipeLayout());

  _deer->buildTextureCommandBuffer(cmdBuf, _shadowPipeline->pipeLayout());
}

void ShadowView::createPipeLayout()
{
  VkDescriptorPoolSize typeCounts[3] = {};
  typeCounts[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  typeCounts[0].descriptorCount = 10;
  typeCounts[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  typeCounts[1].descriptorCount = 10;
  typeCounts[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  typeCounts[2].descriptorCount = 10;

  VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
  descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolInfo.pNext = nullptr;
  descriptorPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  descriptorPoolInfo.poolSizeCount = 3;
  descriptorPoolInfo.pPoolSizes = typeCounts;
  descriptorPoolInfo.maxSets = 10;

  VkDescriptorPool desPool;
  VK_CHECK_RESULT(vkCreateDescriptorPool(*device(), &descriptorPoolInfo, nullptr, &desPool));
  _descriptPool = desPool;

  //----------------------------------------------------------------------------------------------------
  auto mlayout = _shadowPipeline->matrixLayout();
  auto llayout = _shadowPipeline->lightLayout();
  auto playout = _shadowPipeline->pbrLayout();

  VkDescriptorSetAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = desPool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &mlayout;

  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_matrixSet));

  allocInfo.pSetLayouts = &llayout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_lightSet));

  allocInfo.pSetLayouts = &playout;
  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_pbrSet));

  VkDescriptorBufferInfo descriptor = {};
  int sz = sizeof(_matrix);
  _uboBuf = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  descriptor.buffer = *_uboBuf;
  descriptor.offset = 0;
  descriptor.range = sizeof(_matrix);

  sz = sizeof(light);
  VkDescriptorBufferInfo ldescriptor = {};
  _light = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  ldescriptor.buffer = *_light;
  ldescriptor.offset = 0;
  ldescriptor.range = sz;

  sz = sizeof(pbr);
  VkDescriptorBufferInfo mdescriptor = {};
  _material = device()->createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sz);
  mdescriptor.buffer = *_material;
  mdescriptor.offset = 0;
  mdescriptor.range = sz;

  VkWriteDescriptorSet writeDescriptorSet = {};
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = _matrixSet;
  writeDescriptorSet.descriptorCount = 1;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSet.pBufferInfo = &descriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);

  writeDescriptorSet.dstSet = _lightSet;
  writeDescriptorSet.pBufferInfo = &ldescriptor;
  writeDescriptorSet.dstBinding = 0;
  vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);

  writeDescriptorSet.dstSet = _pbrSet;
  writeDescriptorSet.pBufferInfo = &mdescriptor;
  writeDescriptorSet.dstBinding = 0;
  //writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
  vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);


  //----------------------------------------------------------------------------------------------------
  //{
  //  auto clayout = _shadow_pipeline->texture_layout();
  //  VkDescriptorSetAllocateInfo allocInfo = {};
  //  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  //  allocInfo.descriptorPool = desPool;
  //  allocInfo.descriptorSetCount = 1;
  //  allocInfo.pSetLayouts = &clayout;

  //  VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_basic_tex_set));

  //  VkDescriptorImageInfo texDescriptor = {};
  //  texDescriptor.imageView = _basic_texture->image_view();
  //  texDescriptor.sampler = _basic_texture->sampler();
  //  texDescriptor.imageLayout = _basic_texture->image_layout();

  //  VkWriteDescriptorSet wd_set = {};
  //  wd_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  //  wd_set.dstBinding = 0;
  //  wd_set.descriptorCount = 1;
  //  wd_set.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  //  wd_set.pImageInfo = &_basic_texture->descriptor();
  //  wd_set.dstSet = _basic_tex_set;

  //  vkUpdateDescriptorSets(*device(), 1, &wd_set, 0, nullptr);
  //}
}

void ShadowView::createFrameBuffers()
{
  {
    auto view = _depthImage->imageView();
    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = *_depthPass;
    frameBufferCreateInfo.attachmentCount = 1;
    frameBufferCreateInfo.pAttachments = &view;
    frameBufferCreateInfo.width = _depthImage->width();
    frameBufferCreateInfo.height = _depthImage->height();
    frameBufferCreateInfo.layers = 1;

    for (int i = 0; i < _depthFrames.size(); i++)
      vkDestroyFramebuffer(*device(), _depthFrames[i], 0);

    _depthFrames.resize(_swapchain->imageCount());
    for (int i = 0; i < _depthFrames.size(); i++) {
      VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &frameBufferCreateInfo, nullptr, &_depthFrames[i]));
    }
  }

  setFrameBuffers(_swapchain->createFrameBuffer(*renderPass()));

  {
    for (int i = 0; i < _hudFrames.size(); i++)
      vkDestroyFramebuffer(*device(), _hudFrames[i], 0);

    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.pNext = NULL;
    frameBufferCreateInfo.renderPass = *_hudPass;
    frameBufferCreateInfo.attachmentCount = 1;
    frameBufferCreateInfo.width = _w;
    frameBufferCreateInfo.height = _h;
    frameBufferCreateInfo.layers = 1;

    std::vector<VkFramebuffer> frameBuffers;
    frameBuffers.resize(_swapchain->imageCount());
    for (uint32_t i = 0; i < frameBuffers.size(); i++) {
      VkImageView img = _swapchain->imageView(i);
      frameBufferCreateInfo.pAttachments = &img;
      VK_CHECK_RESULT(vkCreateFramebuffer(*_device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
    _hudFrames = std::move(frameBuffers);
  }
}

void ShadowView::destroyFrameBuffers()
{
  for (auto frame : _depthFrames)
    vkDestroyFramebuffer(*device(), frame, nullptr);
  _depthFrames.clear();

  for (auto frame : _hudFrames)
    vkDestroyFramebuffer(*device(), frame, nullptr);
  _hudFrames.clear();

  VulkanView::destroyFrameBuffers();
}

void ShadowView::createPipeline()
{
  if (_depthPipeline) {
    _depthPipeline->realize(_depthPass.get());

    auto desLayout = _depthPipeline->matrixLayout();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &desLayout;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_shadowMatrixSet));

    int sz = sizeof(_shadowMatrix);
    VkDescriptorBufferInfo descriptor = {};
    descriptor.buffer = *_shadowBuf;
    descriptor.offset = 0;
    descriptor.range = sz;

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _shadowMatrixSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writeDescriptorSet.pBufferInfo = &descriptor;
    writeDescriptorSet.dstBinding = 0;
    vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);
  }

  _shadowPipeline->realize(renderPass());

  _tree->realize(_device, _shadowPipeline);

  _deer->realize(_device, _shadowPipeline);

  {
    auto slayout = _shadowPipeline->shadowTextureLayout();
    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = _descriptPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &slayout;

    VK_CHECK_RESULT(vkAllocateDescriptorSets(*device(), &allocInfo, &_shadowTextureSet));

    _shadowTexture = std::make_shared<VulkanTexture>();
    _shadowTexture->realize(_depthImage);

    if (_shadowSampler)
      vkDestroySampler(*device(), _shadowSampler, nullptr);

    VkSamplerCreateInfo samplerInfo = vks::initializers::samplerCreateInfo();
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    VK_CHECK_RESULT(vkCreateSampler(*device(), &samplerInfo, nullptr, &_shadowSampler));

    VkDescriptorImageInfo depthDescriptor = _shadowTexture->descriptor();
    depthDescriptor.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthDescriptor.sampler = _shadowSampler;

    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = _shadowTextureSet;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeDescriptorSet.pBufferInfo = 0;
    writeDescriptorSet.dstBinding = 0;
    writeDescriptorSet.pImageInfo = &depthDescriptor;
    vkUpdateDescriptorSets(*device(), 1, &writeDescriptorSet, 0, nullptr);
  }

  {
    _hudPipeline->realize(_hudPass.get());
    _hudRect->setTexture(_hudPipeline.get(), _shadowTexture.get(), _descriptPool,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
  }

}
