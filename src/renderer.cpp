#include "renderer.h"

#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace {

struct SceneUBO {
    glm::mat4 viewProj;
    glm::vec4 cameraPosFogDist;
};

struct PushConstants {
    glm::mat4 model;
    glm::vec4 params; // x = point size
};

std::vector<char> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open shader file: " + path);
    size_t size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

} // namespace

VkShaderModule Renderer::loadShader(VkCore& core, const char* filename) const {
    std::string path = std::string(SHADER_DIR) + filename;
    auto code = readFile(path);
    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module;
    if (vkCreateShaderModule(core.device(), &createInfo, nullptr, &module) != VK_SUCCESS) {
        throw std::runtime_error(std::string("failed to create shader module: ") + filename);
    }
    return module;
}

void Renderer::init(VkCore& core) {
    createDescriptorSetLayout(core);
    createUniformBuffers(core);
    createDescriptorPool(core);
    createDescriptorSets(core);
    createScenePipelines(core);
    createOverlayPipeline(core);
}

void Renderer::cleanup(VkCore& core) {
    vkDestroyPipeline(core.device(), linePipeline_, nullptr);
    vkDestroyPipeline(core.device(), pointPipeline_, nullptr);
    vkDestroyPipelineLayout(core.device(), sceneLayout_, nullptr);
    vkDestroyPipeline(core.device(), overlayPipeline_, nullptr);
    vkDestroyPipelineLayout(core.device(), overlayLayout_, nullptr);

    for (auto& buf : uniformBuffers_) core.destroyBuffer(buf);
    vkDestroyDescriptorPool(core.device(), descriptorPool_, nullptr);
    vkDestroyDescriptorSetLayout(core.device(), descriptorSetLayout_, nullptr);
}

void Renderer::createDescriptorSetLayout(VkCore& core) {
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboBinding;
    if (vkCreateDescriptorSetLayout(core.device(), &layoutInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }
}

void Renderer::createUniformBuffers(VkCore& core) {
    uniformBuffers_.resize(VkCore::kMaxFramesInFlight);
    for (auto& buf : uniformBuffers_) {
        buf = core.createBuffer(sizeof(SceneUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
}

void Renderer::createDescriptorPool(VkCore& core) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = VkCore::kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = VkCore::kMaxFramesInFlight;
    if (vkCreateDescriptorPool(core.device(), &poolInfo, nullptr, &descriptorPool_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool");
    }
}

void Renderer::createDescriptorSets(VkCore& core) {
    std::vector<VkDescriptorSetLayout> layouts(VkCore::kMaxFramesInFlight, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = descriptorPool_;
    allocInfo.descriptorSetCount = VkCore::kMaxFramesInFlight;
    allocInfo.pSetLayouts = layouts.data();

    descriptorSets_.resize(VkCore::kMaxFramesInFlight);
    if (vkAllocateDescriptorSets(core.device(), &allocInfo, descriptorSets_.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets");
    }

    for (size_t i = 0; i < descriptorSets_.size(); ++i) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = uniformBuffers_[i].buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SceneUBO);

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = descriptorSets_[i];
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(core.device(), 1, &write, 0, nullptr);
    }
}

namespace {

VkPipelineShaderStageCreateInfo stageInfo(VkShaderStageFlagBits stage, VkShaderModule module) {
    VkPipelineShaderStageCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    info.stage = stage;
    info.module = module;
    info.pName = "main";
    return info;
}

} // namespace

void Renderer::createScenePipelines(VkCore& core) {
    VkShaderModule vert = loadShader(core, "scene.vert.spv");
    VkShaderModule frag = loadShader(core, "scene.frag.spv");
    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {
        stageInfo(VK_SHADER_STAGE_VERTEX_BIT, vert),
        stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, frag),
    };

    auto binding = Vertex::bindingDescription();
    auto attrs = Vertex::attributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = core.supportsWideLines() ? 1.4f : 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorSetLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(core.device(), &layoutInfo, nullptr, &sceneLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create scene pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = sceneLayout_;
    pipelineInfo.renderPass = core.renderPass();
    pipelineInfo.subpass = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyLines{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyLines.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    pipelineInfo.pInputAssemblyState = &inputAssemblyLines;
    if (vkCreateGraphicsPipelines(core.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &linePipeline_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create line pipeline");
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyPoints{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssemblyPoints.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    pipelineInfo.pInputAssemblyState = &inputAssemblyPoints;
    if (vkCreateGraphicsPipelines(core.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pointPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create point pipeline");
    }

    vkDestroyShaderModule(core.device(), vert, nullptr);
    vkDestroyShaderModule(core.device(), frag, nullptr);
}

void Renderer::createOverlayPipeline(VkCore& core) {
    VkShaderModule vert = loadShader(core, "overlay.vert.spv");
    VkShaderModule frag = loadShader(core, "overlay.frag.spv");
    std::array<VkPipelineShaderStageCreateInfo, 2> stages = {
        stageInfo(VK_SHADER_STAGE_VERTEX_BIT, vert),
        stageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, frag),
    };

    auto binding = Vertex::bindingDescription();
    auto attrs = Vertex::attributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vertexInput.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.lineWidth = core.supportsWideLines() ? 1.8f : 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // No depth test: overlay always draws on top, in its own final pass over the scene.
    VkPipelineDepthStencilStateCreateInfo depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (vkCreatePipelineLayout(core.device(), &layoutInfo, nullptr, &overlayLayout_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create overlay pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = overlayLayout_;
    pipelineInfo.renderPass = core.renderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(core.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &overlayPipeline_) != VK_SUCCESS) {
        throw std::runtime_error("failed to create overlay pipeline");
    }

    vkDestroyShaderModule(core.device(), vert, nullptr);
    vkDestroyShaderModule(core.device(), frag, nullptr);
}

GpuMesh Renderer::uploadMesh(VkCore& core, const std::vector<Vertex>& vertices) const {
    GpuMesh mesh;
    if (vertices.empty()) return mesh;
    VkDeviceSize size = sizeof(Vertex) * vertices.size();
    mesh.vertexBuffer = core.createDeviceLocalBufferWithData(vertices.data(), size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    mesh.vertexCount = static_cast<uint32_t>(vertices.size());
    return mesh;
}

GpuMesh Renderer::uploadDynamicMesh(VkCore& core, VkDeviceSize capacityBytes, VkBufferUsageFlags extraUsage) const {
    GpuMesh mesh;
    mesh.vertexBuffer = core.createBuffer(capacityBytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | extraUsage,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    mesh.vertexCount = 0;
    return mesh;
}

void Renderer::updateDynamicMesh(VkCore& core, GpuMesh& mesh, const std::vector<Vertex>& vertices) const {
    if (vertices.empty()) {
        mesh.vertexCount = 0;
        return;
    }
    VkDeviceSize size = sizeof(Vertex) * vertices.size();
    if (size > mesh.vertexBuffer.size) {
        throw std::runtime_error("dynamic mesh exceeded its preallocated capacity");
    }
    core.uploadToHostVisible(mesh.vertexBuffer, vertices.data(), size);
    mesh.vertexCount = static_cast<uint32_t>(vertices.size());
}

void Renderer::destroyMesh(VkCore& core, GpuMesh& mesh) const {
    core.destroyBuffer(mesh.vertexBuffer);
    mesh.vertexCount = 0;
}

void Renderer::beginSceneFrame(VkCore& core, const glm::mat4& viewProj, const glm::vec3& cameraPos, float fogDistance) {
    currentFrame_ = (currentFrame_ + 1) % VkCore::kMaxFramesInFlight;
    SceneUBO ubo{viewProj, glm::vec4(cameraPos, fogDistance)};
    core.uploadToHostVisible(uniformBuffers_[currentFrame_], &ubo, sizeof(ubo));
}

void Renderer::drawMesh(VkCommandBuffer cmd, const GpuMesh& mesh, Topology topology, const glm::mat4& model, float pointSize) {
    if (mesh.vertexCount == 0) return;
    VkPipeline pipeline = topology == Topology::Lines ? linePipeline_ : pointPipeline_;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sceneLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);

    PushConstants pc{model, glm::vec4(pointSize, 0.0f, 0.0f, 0.0f)};
    vkCmdPushConstants(cmd, sceneLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

    VkBuffer buffers[] = {mesh.vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdDraw(cmd, mesh.vertexCount, 1, 0, 0);
}

void Renderer::drawOverlay(VkCommandBuffer cmd, const GpuMesh& mesh) {
    if (mesh.vertexCount == 0) return;
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipeline_);
    VkBuffer buffers[] = {mesh.vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    vkCmdDraw(cmd, mesh.vertexCount, 1, 0, 0);
}
