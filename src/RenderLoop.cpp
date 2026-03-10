#include "RenderLoop.hpp"

#include "VulkanApp.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <glfw3.h>
#include <stdexcept>

namespace {
constexpr int kChunkSize = Chunk::SIZE;
double gSmoothedFrameTimeMs = 16.6;

struct DrawChunkEntry {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
};

int floorDiv(int value, int divisor) noexcept {
    int quotient = value / divisor;
    const int remainder = value % divisor;
    if (remainder != 0 && ((remainder < 0) != (divisor < 0))) {
        --quotient;
    }
    return quotient;
}
}

void VulkanApp::drawFrame(VulkanApp& app) {
    const double currentTime = glfwGetTime();
    double deltaSeconds = currentTime - lastFrameTimeSeconds;
    lastFrameTimeSeconds = currentTime;
    deltaSeconds = std::clamp(deltaSeconds, 0.0, 0.25);
    const double frameTimeMs = deltaSeconds * 1000.0;
    gSmoothedFrameTimeMs = gSmoothedFrameTimeMs * 0.9 + frameTimeMs * 0.1;

    if (app.swapchainExtent.width == 0 || app.swapchainExtent.height == 0) {
        return;
    }

    inputController.update(deltaSeconds);

    if (!cameraPlacedOnTerrain && !activeChunkMeshes.empty()) {
        placeCameraOnTerrain();
    }

    if (!app.meshNeedsRebuild.load(std::memory_order_relaxed)) {
        const ChunkCoord cameraChunk = app.chunkForPosition(app.camera.position());

        bool shouldRequest = false;
        {
            std::lock_guard<std::mutex> lock(app.chunkQueueMutex);
            const int deltaX = std::abs(cameraChunk.x - app.requestedTerrainCenterChunk.x);
            const int deltaZ = std::abs(cameraChunk.z - app.requestedTerrainCenterChunk.z);
            if ((std::max)(deltaX, deltaZ) >= 1) {
                app.requestedTerrainCenterChunk.x = cameraChunk.x;
                app.requestedTerrainCenterChunk.z = cameraChunk.z;
                shouldRequest = true;
            }
        }

        if (shouldRequest) {
            app.meshNeedsRebuild.store(true, std::memory_order_relaxed);
        }
    }

    if (!app.meshNeedsRebuild.load(std::memory_order_relaxed)) {
        static uint32_t refillCheckFrameCounter = 0;
        ++refillCheckFrameCounter;

        if ((refillCheckFrameCounter % 8u) == 0u) {
            bool needsRefill = false;

            {
                std::lock_guard<std::mutex> lock(app.chunkQueueMutex);

                const size_t queueHeadroomGeneration = app.kMaxGenerationJobs > app.generationJobQueue.size()
                    ? (app.kMaxGenerationJobs - app.generationJobQueue.size())
                    : 0;
                const size_t queueHeadroomMeshing = app.kMaxMeshJobs > app.meshJobQueue.size()
                    ? (app.kMaxMeshJobs - app.meshJobQueue.size())
                    : 0;

                if (!app.desiredChunkSet.empty() && (queueHeadroomGeneration > 64 || queueHeadroomMeshing > 64)) {
                    size_t missingCount = 0;
                    const size_t desiredCount = app.desiredChunkSet.size();
                    const size_t coverageSlack = desiredCount > 64 ? 64 : 16;
                    for (const ChunkCoord& coord : app.desiredChunkSet) {
                        const bool isActive = app.activeChunkMeshes.find(coord) != app.activeChunkMeshes.end();
                        const bool isEmptyComplete = app.completedEmptyChunkSet.find(coord) != app.completedEmptyChunkSet.end();
                        const bool isGenerating = app.queuedOrGeneratingChunkSet.find(coord) != app.queuedOrGeneratingChunkSet.end();
                        const bool isMeshing = app.queuedOrMeshingChunkSet.find(coord) != app.queuedOrMeshingChunkSet.end();

                        if (!isActive && !isEmptyComplete && !isGenerating && !isMeshing) {
                            ++missingCount;
                            if (missingCount > coverageSlack) {
                                needsRefill = true;
                                break;
                            }
                        }
                    }
                }
            }

            if (needsRefill) {
                app.meshNeedsRebuild.store(true, std::memory_order_relaxed);
            }
        }
    }
    
    if (app.meshNeedsRebuild.exchange(false, std::memory_order_relaxed)) {
        app.rebuildVoxelMesh();
    }

    vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    if (completedSubmissionCount < submittedSubmissionCount) {
        ++completedSubmissionCount;
    }
    processCompletedUploadBatches();
    processDeferredDestroyQueue();
    uploadCompletedChunkMeshes();

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    updateUniformBuffer(currentFrame);

    vkResetFences(device, 1, &inFlightFences[currentFrame]);

    vkResetCommandBuffer(app.commandBuffers[currentFrame], 0);
    app.recordCommandBuffer(app.commandBuffers[currentFrame], imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {app.imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = waitSemaphores;
    submitInfo.pWaitDstStageMask  = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &app.commandBuffers[currentFrame];

    VkSemaphore signalSemaphores[] = {app.renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(app.graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer");
    }
    ++submittedSubmissionCount;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapchains[] = {app.swapchain};
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapchains;
    presentInfo.pImageIndices   = &imageIndex;

    presentInfo.pResults = nullptr;
    result = vkQueuePresentKHR(app.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanApp::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = renderPass;
    renderPassInfo.framebuffer       = swapchainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues    = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    if (!descriptorSets.empty()) {
        const uint32_t activeFrame = currentFrame % static_cast<uint32_t>(descriptorSets.size());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[activeFrame], 0, nullptr);
    }

    const ChunkCoord renderCenter = chunkForPosition(camera.position());
    const int drawKeepRadius = renderDistanceChunks.load(std::memory_order_relaxed) + kKeepRadiusExtra + 2;

    std::vector<DrawChunkEntry> drawEntries;
    drawEntries.reserve(activeChunkMeshes.size());
    {
        std::lock_guard<std::mutex> lock(chunkQueueMutex);
        drawEntries.reserve(activeChunkMeshes.size());
        for (const auto& [coord, mesh] : activeChunkMeshes) {
            if (mesh.indexCount == 0 || mesh.vertexBuffer == VK_NULL_HANDLE || mesh.indexBuffer == VK_NULL_HANDLE) {
                continue;
            }

            const int dx = std::abs(coord.x - renderCenter.x);
            const int dz = std::abs(coord.z - renderCenter.z);
            if ((std::max)(dx, dz) > drawKeepRadius) {
                continue;
            }

            drawEntries.push_back(DrawChunkEntry{mesh.vertexBuffer, mesh.indexBuffer, mesh.indexCount});
        }
    }

    for (const DrawChunkEntry& drawEntry : drawEntries) {
        VkBuffer vertexBuffers[] = {drawEntry.vertexBuffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, drawEntry.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, drawEntry.indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

void VulkanApp::requestTerrainWindow(const ChunkCoord& centerChunk) {
    const int minTerrainHeight = static_cast<int>(std::floor(terrainSettings.baseHeight - terrainSettings.elevationAmplitude)) - 1;
    const int maxTerrainHeight = static_cast<int>(std::ceil(terrainSettings.baseHeight + terrainSettings.elevationAmplitude)) + 1;
    const int maxFilledHeight = (std::max)(maxTerrainHeight, terrainSettings.waterLevel + 1);
    const int minTerrainChunkY = floorDiv(minTerrainHeight, kChunkSize);
    const int maxTerrainChunkY = floorDiv(maxFilledHeight, kChunkSize);
    const int terrainCenterChunkY = (minTerrainChunkY + maxTerrainChunkY) / 2;

    ChunkCoord streamingCenter{};
    {
        std::lock_guard<std::mutex> lock(chunkQueueMutex);
        requestedTerrainCenterChunk = {centerChunk.x, terrainCenterChunkY, centerChunk.z};
        streamingCenter = requestedTerrainCenterChunk;
    }

    std::vector<ChunkCoord> targetCoords;
    std::vector<ChunkCoord> keepCoords;
    const int activeRenderDistance = renderDistanceChunks.load(std::memory_order_relaxed);
    const int prefetchRadius = activeRenderDistance + kPrefetchRadiusExtra;
    const int keepRadius = activeRenderDistance + kKeepRadiusExtra;

    const int minStreamingChunkY = minTerrainChunkY - 2;
    const int maxStreamingChunkY = maxTerrainChunkY + 2;

    targetCoords.reserve((prefetchRadius * 2 + 1) * (prefetchRadius * 2 + 1) * (prefetchRadius * 2 + 1));
    keepCoords.reserve((keepRadius * 2 + 1) * (keepRadius * 2 + 1) * (keepRadius * 2 + 1));

    const int prefetchRadiusSq = prefetchRadius * prefetchRadius * 2;
    const int keepRadiusSq = keepRadius * keepRadius * 2;

    for (int cz = -prefetchRadius; cz <= prefetchRadius; ++cz) {
        for (int cx = -prefetchRadius; cx <= prefetchRadius; ++cx) {
            const int horizontalDistanceSq = cx * cx + cz * cz;
            if (horizontalDistanceSq > prefetchRadiusSq) {
                continue;
            }
            for (int cy = -prefetchRadius; cy <= prefetchRadius; ++cy) {
                const int chunkY = streamingCenter.y + cy;
                if (chunkY < minStreamingChunkY || chunkY > maxStreamingChunkY) {
                    continue;
                }
                targetCoords.push_back({streamingCenter.x + cx, chunkY, streamingCenter.z + cz});
            }
        }
    }

    for (int cz = -keepRadius; cz <= keepRadius; ++cz) {
        for (int cx = -keepRadius; cx <= keepRadius; ++cx) {
            const int horizontalDistanceSq = cx * cx + cz * cz;
            if (horizontalDistanceSq > keepRadiusSq) {
                continue;
            }
            for (int cy = -keepRadius; cy <= keepRadius; ++cy) {
                const int chunkY = streamingCenter.y + cy;
                if (chunkY < minStreamingChunkY || chunkY > maxStreamingChunkY) {
                    continue;
                }
                keepCoords.push_back({streamingCenter.x + cx, chunkY, streamingCenter.z + cz});
            }
        }
    }

    std::unordered_set<ChunkCoord, ChunkCoordHash> loadedTargetChunkSet;
    loadedTargetChunkSet.reserve(targetCoords.size());
    {
        std::shared_lock<std::shared_mutex> worldLock(worldDataMutex);
        for (const ChunkCoord& coord : targetCoords) {
            if (world.hasChunk(coord)) {
                loadedTargetChunkSet.insert(coord);
            }
        }
    }

    std::unordered_set<ChunkCoord, ChunkCoordHash> targetSet(targetCoords.begin(), targetCoords.end());
    std::unordered_set<ChunkCoord, ChunkCoordHash> keepSet(keepCoords.begin(), keepCoords.end());

    ++requestWindowUpdateCounter;
    {
        std::unique_lock<std::shared_mutex> worldLock(worldDataMutex);
        world.retainChunks(keepSet);
    }

    {
        std::lock_guard<std::mutex> lock(chunkQueueMutex);
        keepChunkSet = keepSet;

        for (auto it = generationJobQueue.begin(); it != generationJobQueue.end();) {
            if (keepSet.find(*it) == keepSet.end()) {
                queuedOrGeneratingChunkSet.erase(*it);
                it = generationJobQueue.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = meshJobQueue.begin(); it != meshJobQueue.end();) {
            if (keepSet.find(*it) == keepSet.end()) {
                queuedOrMeshingChunkSet.erase(*it);
                it = meshJobQueue.erase(it);
            } else {
                ++it;
            }
        }

        size_t remainingGenerationSlots = kMaxGenerationJobs > generationJobQueue.size() ? (kMaxGenerationJobs - generationJobQueue.size()) : 0;
        size_t remainingMeshSlots = kMaxMeshJobs > meshJobQueue.size() ? (kMaxMeshJobs - meshJobQueue.size()) : 0;

        std::vector<std::vector<ChunkCoord>> ringBuckets(static_cast<size_t>(prefetchRadius + 1));
        const size_t averagePerRing = targetCoords.empty() ? 0 : (targetCoords.size() / static_cast<size_t>(prefetchRadius + 1));
        for (auto& bucket : ringBuckets) {
            bucket.reserve(averagePerRing + 1);
        }
        for (const ChunkCoord& coord : targetCoords) {

            if (activeChunkMeshes.find(coord) != activeChunkMeshes.end() ||
                completedEmptyChunkSet.find(coord) != completedEmptyChunkSet.end()) {
                continue;
            }

            if (queuedOrGeneratingChunkSet.find(coord) != queuedOrGeneratingChunkSet.end() ||
                queuedOrMeshingChunkSet.find(coord) != queuedOrMeshingChunkSet.end()) {
                continue;
            }

            const int dx = std::abs(coord.x - streamingCenter.x);
            const int dy = std::abs(coord.y - streamingCenter.y);
            const int dz = std::abs(coord.z - streamingCenter.z);
            const int ringDistance = (std::max)(dx, (std::max)(dy, dz));
            ringBuckets[static_cast<size_t>(ringDistance)].push_back(coord);
        }

        size_t enqueueBudget = static_cast<size_t>(512 + activeRenderDistance * 48);
        if (gSmoothedFrameTimeMs > 22.0) {
            enqueueBudget = (std::max)(static_cast<size_t>(192), enqueueBudget / 2);
        } else if (gSmoothedFrameTimeMs < 14.0) {
            enqueueBudget = enqueueBudget + enqueueBudget / 2;
        }
        const size_t availableSlots = remainingGenerationSlots + remainingMeshSlots;
        if (enqueueBudget > availableSlots) {
            enqueueBudget = availableSlots;
        }

        const auto enqueueCoord = [&](const ChunkCoord& coord) {
            const bool hasChunkReady = loadedTargetChunkSet.find(coord) != loadedTargetChunkSet.end();
            if (hasChunkReady) {
                if (remainingMeshSlots == 0) {
                    return false;
                }
                meshJobQueue.push_back(coord);
                queuedOrMeshingChunkSet.insert(coord);
                --remainingMeshSlots;
                return true;
            }

            if (remainingGenerationSlots == 0) {
                return false;
            }
            generationJobQueue.push_back(coord);
            queuedOrGeneratingChunkSet.insert(coord);
            --remainingGenerationSlots;
            return true;
        };

        for (size_t ringIndex = 0; ringIndex < ringBuckets.size(); ++ringIndex) {
            if ((remainingGenerationSlots == 0 && remainingMeshSlots == 0) || enqueueBudget == 0) {
                break;
            }

            for (const ChunkCoord& coord : ringBuckets[ringIndex]) {
                if ((remainingGenerationSlots == 0 && remainingMeshSlots == 0) || enqueueBudget == 0) {
                    break;
                }

                if (enqueueCoord(coord)) {
                    --enqueueBudget;
                }
            }
        }

        desiredChunkSet = std::move(targetSet);
    }

    std::vector<ChunkCoord> removedActiveCoords;
    removedActiveCoords.reserve(256);
    {
        std::lock_guard<std::mutex> activeLock(chunkQueueMutex);
        for (auto it = activeChunkMeshes.begin(); it != activeChunkMeshes.end();) {
            if (keepSet.find(it->first) == keepSet.end()) {
                removedActiveCoords.push_back(it->first);
                enqueueChunkMeshForDestruction(std::move(it->second));
                it = activeChunkMeshes.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = completedEmptyChunkSet.begin(); it != completedEmptyChunkSet.end();) {
            if (keepSet.find(*it) == keepSet.end()) {
                it = completedEmptyChunkSet.erase(it);
            } else {
                ++it;
            }
        }
    }

    if (!removedActiveCoords.empty()) {
        static constexpr std::array<ChunkCoord, 6> kNeighborOffsets{{
            {-1, 0, 0}, {1, 0, 0},
            {0, -1, 0}, {0, 1, 0},
            {0, 0, -1}, {0, 0, 1}
        }};

        std::lock_guard<std::mutex> lock(chunkQueueMutex);
        for (const ChunkCoord& removed : removedActiveCoords) {
            for (const ChunkCoord& offset : kNeighborOffsets) {
                const ChunkCoord neighbor{
                    removed.x + offset.x,
                    removed.y + offset.y,
                    removed.z + offset.z
                };

                if (keepSet.find(neighbor) == keepSet.end()) {
                    continue;
                }
                if (queuedOrMeshingChunkSet.find(neighbor) != queuedOrMeshingChunkSet.end()) {
                    continue;
                }
                if (meshJobQueue.size() >= kMaxMeshJobs) {
                    break;
                }

                meshJobQueue.push_back(neighbor);
                queuedOrMeshingChunkSet.insert(neighbor);
            }
        }
    }

    updateActiveMeshBounds();
    generationQueueCv.notify_all();
    meshQueueCv.notify_all();
}

void VulkanApp::runChunkGenerationWorker() {
    while (chunkWorkerRunning.load(std::memory_order_relaxed)) {
        ChunkCoord coord{};
        bool hasJob = false;

        {
            std::unique_lock<std::mutex> lock(chunkQueueMutex);
            generationQueueCv.wait(lock, [this]() {
                return !chunkWorkerRunning.load(std::memory_order_relaxed) || !generationJobQueue.empty();
            });

            if (!chunkWorkerRunning.load(std::memory_order_relaxed)) {
                return;
            }

            if (!generationJobQueue.empty()) {
                coord = generationJobQueue.front();
                generationJobQueue.pop_front();
                hasJob = true;
            }
        }

        if (!hasJob) {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(chunkQueueMutex);
            if (keepChunkSet.find(coord) == keepChunkSet.end()) {
                queuedOrGeneratingChunkSet.erase(coord);
                continue;
            }
        }

        {
            std::unique_lock<std::shared_mutex> worldLock(worldDataMutex);
            world.getOrCreateChunk(coord);
        }

        {
            std::lock_guard<std::mutex> lock(chunkQueueMutex);
            queuedOrGeneratingChunkSet.erase(coord);
            if (keepChunkSet.find(coord) != keepChunkSet.end() &&
                activeChunkMeshes.find(coord) == activeChunkMeshes.end() &&
                completedEmptyChunkSet.find(coord) == completedEmptyChunkSet.end() &&
                queuedOrMeshingChunkSet.find(coord) == queuedOrMeshingChunkSet.end() && meshJobQueue.size() < kMaxMeshJobs) {
                meshJobQueue.push_back(coord);
                queuedOrMeshingChunkSet.insert(coord);
            }
        }

        meshQueueCv.notify_one();
    }
}

void VulkanApp::runChunkMeshWorker() {
    while (chunkWorkerRunning.load(std::memory_order_relaxed)) {
        ChunkCoord coord{};
        bool hasJob = false;

        {
            std::unique_lock<std::mutex> lock(chunkQueueMutex);
            meshQueueCv.wait(lock, [this]() {
                return !chunkWorkerRunning.load(std::memory_order_relaxed) || !meshJobQueue.empty();
            });

            if (!chunkWorkerRunning.load(std::memory_order_relaxed)) {
                return;
            }

            if (!meshJobQueue.empty()) {
                coord = meshJobQueue.front();
                meshJobQueue.pop_front();
                hasJob = true;
            }
        }

        if (!hasJob) {
            continue;
        }

        PendingChunkMesh meshData = buildChunkMeshData(coord);

        {
            std::lock_guard<std::mutex> lock(chunkQueueMutex);
            queuedOrMeshingChunkSet.erase(coord);
            if (keepChunkSet.find(coord) == keepChunkSet.end()) {
                continue;
            }
        }

        while (chunkWorkerRunning.load(std::memory_order_relaxed)) {
            bool queued = false;
            {
                std::lock_guard<std::mutex> lock(completedChunkMutex);
                if (completedChunkMeshes.size() < kMaxCompletedChunkMeshes) {
                    completedChunkMeshes.push_back(std::move(meshData));
                    queued = true;
                }
            }

            if (queued) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

void VulkanApp::uploadCompletedChunkMeshes() {
    std::vector<std::pair<ChunkCoord, GpuChunkMesh>> readyGpuMeshes;
    std::vector<UploadStagingBuffers> uploadStaging;
    readyGpuMeshes.reserve(kChunkUploadsPerFrame);
    uploadStaging.reserve(kChunkUploadsPerFrame);

    VkCommandBuffer uploadCommandBuffer = VK_NULL_HANDLE;
    bool uploadCommandStarted = false;

    size_t completedBacklog = 0;
    {
        std::lock_guard<std::mutex> lock(completedChunkMutex);
        completedBacklog = completedChunkMeshes.size();
    }

    int uploadBudget = kChunkUploadsPerFrame;
    if (gSmoothedFrameTimeMs > 22.0) {
        uploadBudget = (std::max)(4, kChunkUploadsPerFrame / 2);
    } else if (gSmoothedFrameTimeMs < 14.0) {
        uploadBudget = kChunkUploadsPerFrame * 2;
    }
    if (completedBacklog > (kMaxCompletedChunkMeshes * 3) / 4) {
        uploadBudget += 8;
    } else if (completedBacklog > kMaxCompletedChunkMeshes / 2) {
        uploadBudget += 4;
    }
    uploadBudget = (std::min)(uploadBudget, 24);

    int uploadedCount = 0;
    while (uploadedCount < uploadBudget) {
        PendingChunkMesh pending{};
        {
            std::lock_guard<std::mutex> lock(completedChunkMutex);
            if (completedChunkMeshes.empty()) {
                break;
            }
            pending = std::move(completedChunkMeshes.front());
            completedChunkMeshes.pop_front();
        }

        {
            std::lock_guard<std::mutex> lock(chunkQueueMutex);
            if (keepChunkSet.find(pending.coord) == keepChunkSet.end()) {
                continue;
            }
        }

        {
            std::lock_guard<std::mutex> lock(chunkQueueMutex);
            completedEmptyChunkSet.erase(pending.coord);
        }

        if (pending.hasGeometry) {
            GpuChunkMesh gpuMesh{};
            gpuMesh.indexCount = static_cast<uint32_t>(pending.indices.size());
            gpuMesh.minCorner = pending.minCorner;
            gpuMesh.maxCorner = pending.maxCorner;

            UploadStagingBuffers buffers{};

            VkDeviceSize vertexBufferSize = sizeof(Vertex) * pending.vertices.size();
            createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffers.stagingVertex, buffers.stagingVertexMemory);
            void* vertexData = nullptr;
            vkMapMemory(device, buffers.stagingVertexMemory, 0, vertexBufferSize, 0, &vertexData);
            std::memcpy(vertexData, pending.vertices.data(), static_cast<size_t>(vertexBufferSize));
            vkUnmapMemory(device, buffers.stagingVertexMemory);

            createBuffer(vertexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gpuMesh.vertexBuffer, gpuMesh.vertexBufferMemory);

            VkDeviceSize indexBufferSize = sizeof(uint32_t) * pending.indices.size();
            createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffers.stagingIndex, buffers.stagingIndexMemory);
            void* indexData = nullptr;
            vkMapMemory(device, buffers.stagingIndexMemory, 0, indexBufferSize, 0, &indexData);
            std::memcpy(indexData, pending.indices.data(), static_cast<size_t>(indexBufferSize));
            vkUnmapMemory(device, buffers.stagingIndexMemory);

            createBuffer(indexBufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, gpuMesh.indexBuffer, gpuMesh.indexBufferMemory);

            if (!uploadCommandStarted) {
                VkCommandBufferAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = commandPool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
                allocInfo.commandBufferCount = 1;
                if (vkAllocateCommandBuffers(device, &allocInfo, &uploadCommandBuffer) != VK_SUCCESS) {
                    throw std::runtime_error("Failed to allocate batched upload command buffer");
                }

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                if (vkBeginCommandBuffer(uploadCommandBuffer, &beginInfo) != VK_SUCCESS) {
                    vkFreeCommandBuffers(device, commandPool, 1, &uploadCommandBuffer);
                    throw std::runtime_error("Failed to begin batched upload command buffer");
                }
                uploadCommandStarted = true;
            }

            VkBufferCopy vertexRegion{};
            vertexRegion.size = vertexBufferSize;
            vkCmdCopyBuffer(uploadCommandBuffer, buffers.stagingVertex, gpuMesh.vertexBuffer, 1, &vertexRegion);

            VkBufferCopy indexRegion{};
            indexRegion.size = indexBufferSize;
            vkCmdCopyBuffer(uploadCommandBuffer, buffers.stagingIndex, gpuMesh.indexBuffer, 1, &indexRegion);

            uploadStaging.push_back(buffers);
            readyGpuMeshes.emplace_back(pending.coord, std::move(gpuMesh));
        } else {
            std::lock_guard<std::mutex> lock(chunkQueueMutex);
            completedEmptyChunkSet.insert(pending.coord);
        }

        ++uploadedCount;
    }

    if (uploadCommandStarted) {
        if (vkEndCommandBuffer(uploadCommandBuffer) != VK_SUCCESS) {
            vkFreeCommandBuffers(device, commandPool, 1, &uploadCommandBuffer);
            throw std::runtime_error("Failed to end batched upload command buffer");
        }

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence uploadFence = VK_NULL_HANDLE;
        if (vkCreateFence(device, &fenceInfo, nullptr, &uploadFence) != VK_SUCCESS) {
            vkFreeCommandBuffers(device, commandPool, 1, &uploadCommandBuffer);
            throw std::runtime_error("Failed to create batched upload fence");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &uploadCommandBuffer;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, uploadFence) != VK_SUCCESS) {
            vkDestroyFence(device, uploadFence, nullptr);
            vkFreeCommandBuffers(device, commandPool, 1, &uploadCommandBuffer);
            throw std::runtime_error("Failed to submit batched upload command buffer");
        }

        PendingUploadBatch batch{};
        batch.commandBuffer = uploadCommandBuffer;
        batch.fence = uploadFence;
        batch.stagingBuffers = std::move(uploadStaging);
        batch.readyMeshes = std::move(readyGpuMeshes);
        pendingUploadBatches.push_back(std::move(batch));
    }

    if (uploadedCount > 0) {
        loadedTerrainCenterChunk = requestedTerrainCenterChunk;
        updateActiveMeshBounds();
    }
}

void RenderLoop(VulkanApp& app, GLFWwindow* window) {
    constexpr double targetFrameTime = 1.0 / 60.0; // basic 60 FPS cap

    while (!glfwWindowShouldClose(window)) {
        glfwWaitEventsTimeout(targetFrameTime);
        auto* appPtr = static_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
        if (appPtr != nullptr) {
            appPtr->drawFrame(*appPtr);
        }
    }

    app.waitIdle();
}
