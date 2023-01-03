/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2022 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * \brief Tests for VK_EXT_external_memory_acquire_unmodified
 *
 * We expect the driver to implement VkExternalMemoryAcquireUnmodifiedEXT::acquireUnmodifiedMemory as a no-op when
 * acquiring ownership from VK_QUEUE_FAMILY_EXTERNAL because of the spec's requirements[1] on the queue.  Therefore, we
 * only test VkExternalMemoryHandleTypeFlagBits that support VK_QUEUE_FAMILY_FOREIGN_EXT, which has no restriction.
 *
 * [1]: The Vulkan 1.3.238 spec says:
 *		The special queue family index VK_QUEUE_FAMILY_EXTERNAL represents any queue external to the resource’s current
 *		Vulkan instance, as long as the queue uses the same underlying device group or physical device, and the same
 *		driver version as the resource’s VkDevice, as indicated by VkPhysicalDeviceIDProperties::deviceUUID and
 *		VkPhysicalDeviceIDProperties::driverUUID.
 *
 * TODO: Allocate and import dma_buf with gbm.
 * TODO: Add test cases for using Vulkan as allocator vs using external allocator.
 *//*--------------------------------------------------------------------*/

#include "vktMemoryExternalMemoryAcquireUnmodifiedTests.hpp"

#include "tcuImageCompare.hpp"
#include "tcuTestCase.hpp"
#include "tcuTextureUtil.hpp"

#include "vktExternalMemoryUtil.hpp"
#include "vktTestCaseUtil.hpp"

#include "vkBufferWithMemory.hpp"
#include "vkBuilderUtil.hpp"
#include "vkCmdUtil.hpp"
#include "vkImageUtil.hpp"
#include "vkObjUtil.hpp"
#include "vkQueryUtil.hpp"
#include "vkRefUtil.hpp"
#include "vkTypeUtil.hpp"

namespace vkt
{
namespace memory
{
namespace
{

using namespace vk;

using tcu::PixelBufferAccess;
using tcu::Vec4;

struct TestParams;
class TestCase;
class TestInstance;
class ImageWithMemory;

const VkImageSubresourceRange imageSubresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

struct TestParams
{
	VkFormat							format;
	VkExternalMemoryHandleTypeFlagBits	externalMemoryType;
	const VkExtent3D					imageExtent			= { 512, 512, 1 };
};

class TestCase: public vkt::TestCase
{
public:
	TestCase(tcu::TestContext&	context,
			 const char*		name,
			 const char*		description,
			 TestParams			params)
		: vkt::TestCase	(context, name, description)
		, m_params		(params)
	{}

	void				checkSupport	(Context&	context) const override;
	vkt::TestInstance*	createInstance	(Context&	context) const override;

	const	TestParams	m_params;
};

class TestInstance : public vkt::TestInstance
{
public:
	TestInstance (Context& context, TestParams params)
		: vkt::TestInstance		(context)
		, m_params				(params)
		, m_textureFormat		(mapVkFormat(params.format))
		, m_allocator			(context.getDefaultAllocator())
	{}

	tcu::TestStatus		iterate							(void) override;
	qpTestResult		testDmaBuf						(void);
	qpTestResult		testDmaBufWithDrmFormatModifier	(uint64_t drmFormatModifier);

private:
	const TestParams									m_params;
	const tcu::TextureFormat							m_textureFormat;
	Allocator&											m_allocator;
	Move<VkCommandPool>									m_cmdPool;
	MovePtr<BufferWithMemory>							m_src1Buffer;
	MovePtr<PixelBufferAccess>							m_src1Access;

};

class ImageWithMemory
{
public:
	ImageWithMemory	(Context&							context,
					 VkFormat							format,
					 VkExtent3D							imageExtent,
					 VkExternalMemoryHandleTypeFlagBits	externalMemoryType);

	const VkImage&					get			(void) const { return *m_image; }
	const VkImage&					operator*	(void) const { return get(); }

private:
	static std::vector<uint64_t>	getDrmFormatModifiersForFormat	(Context&	context,
																	VkFormat	format);

	static bool						isDrmFormatModifierCompatible	(Context&							context,
																	 VkFormat							format,
																	 VkExtent3D							imageExtent,
																	 uint64_t							modifier,
																	 VkExternalMemoryHandleTypeFlagBits	externalMemoryType);

	static constexpr VkImageType				m_imageType				= VK_IMAGE_TYPE_2D;
	static const VkImageUsageFlags				m_usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
																		  VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	static const VkFormatFeatureFlags			m_formatFeatures		= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
																		  VK_FORMAT_FEATURE_TRANSFER_DST_BIT;

	Context&									m_context;
	const VkFormat								m_format;
	const VkExtent3D							m_extent;
	const VkExternalMemoryHandleTypeFlagBits	m_externalMemoryType;

	vk::Move<VkImage>							m_image;
	vk::Move<VkDeviceMemory>					m_memory;

	// "deleted"
												ImageWithMemory	(const ImageWithMemory&);
	ImageWithMemory&							operator=		(const ImageWithMemory&);
};

ptrdiff_t
ptrDiff (const void* x, const void* y)
{
	return static_cast<const char*>(x) - static_cast<const char*>(y);
}

std::vector<uint64_t> ImageWithMemory::getDrmFormatModifiersForFormat (Context& context, VkFormat format)
{
	const InstanceInterface& vki		= context.getInstanceInterface();
	VkPhysicalDevice physicalDevice		= context.getPhysicalDevice();

	VkDrmFormatModifierPropertiesListEXT modifierList	= initVulkanStructure();
	modifierList.drmFormatModifierCount					= 0;
	modifierList.pDrmFormatModifierProperties			= DE_NULL;

	VkFormatProperties2 formatProperties2	= initVulkanStructure();
	formatProperties2.pNext					= &modifierList;

	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);

	std::vector<VkDrmFormatModifierPropertiesEXT> modifierProperties;
	modifierProperties.resize(modifierList.drmFormatModifierCount);
	modifierList.pDrmFormatModifierProperties = modifierProperties.data();

	vki.getPhysicalDeviceFormatProperties2(physicalDevice, format, &formatProperties2);

	std::vector<uint64_t> modifiers;

	for (const auto& props : modifierProperties) {
		if (m_formatFeatures == (m_formatFeatures & props.drmFormatModifierTilingFeatures)) {
			modifiers.push_back(props.drmFormatModifier);
		}
	}

	return modifiers;
}

void TestCase::checkSupport (Context& context) const
{
	// Do not explicitly require extensions that are transitively required.

	context.requireDeviceFunctionality("VK_EXT_external_memory_acquire_unmodified");

    switch (m_params.externalMemoryType) {
	case VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
		context.requireDeviceFunctionality("VK_EXT_external_memory_dma_buf");
		context.requireDeviceFunctionality("VK_EXT_image_drm_format_modifier");
        break;
	case VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID:
		context.requireDeviceFunctionality("VK_ANDROID_external_memory_android_hardware_buffer");
		break;
    default:
		DE_ASSERT(false);
		break;
	}
}

vkt::TestInstance* TestCase::createInstance(Context& context) const
{
	return new TestInstance(context, m_params);
}

struct MemoryTypeFilter {
	uint32_t				allowedIndexes;
	VkMemoryPropertyFlags	requiredProps;
	VkMemoryPropertyFlags	preferredProps;
};

// Return UINT32_MAX on failure.
uint32_t chooseMemoryType (Context& context, const MemoryTypeFilter& filter)
{
	const InstanceInterface& vki	= context.getInstanceInterface();
	VkPhysicalDevice physicalDevice	= context.getPhysicalDevice();

	const VkPhysicalDeviceMemoryProperties memProps = vk::getPhysicalDeviceMemoryProperties(vki, physicalDevice);
	uint32_t score[VK_MAX_MEMORY_TYPES] = { 0 };

	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++)
	{
		if (!(filter.allowedIndexes & (1 << i)))
			continue;

		VkMemoryPropertyFlags curProps = memProps.memoryTypes[i].propertyFlags;

		if (filter.requiredProps != (filter.requiredProps & curProps))
			continue;

		if (!filter.preferredProps) {
			// Choose the first match
			return i;
		}

		score[i] = 1 + dePop32(filter.preferredProps & curProps);
	}

	uint32_t bestIndex = UINT32_MAX;
	uint32_t bestScore = 0;

	for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
		if (score[i] > bestScore) {
			bestIndex = i;
			bestScore = score[i];
		}
	}

	return bestIndex;
}

tcu::TestStatus TestInstance::iterate (void)
{
	// Only 2D images are supported
	DE_ASSERT(m_params.imageExtent.depth == 1);

	const DeviceInterface&		vkd					= m_context.getDeviceInterface();
	VkDevice					device				= m_context.getDevice();

	VkQueue						queue				= m_context.getUniversalQueue();
	deUint32					queueFamilyIndex	= m_context.getUniversalQueueFamilyIndex();
	m_cmdPool		= createCommandPool(vkd, device, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, queueFamilyIndex);

	const VkFormat				format				(m_params.format);
	const VkExtent3D			imageExtent			(m_params.imageExtent);

	const VkDeviceSize			bufferSize			= m_textureFormat.getPixelSize() * imageExtent.width * imageExtent.height;

	m_src1Buffer	= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, m_allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::HostVisible));
	m_src1Access	= MovePtr<PixelBufferAccess>(new PixelBufferAccess(m_textureFormat, imageExtent.width, imageExtent.height, 1, src1Buffer.getAllocation().getHostPtr()));

	m_src2Buffer	= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, m_allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT), MemoryRequirement::HostVisible));
	m_src2TotalAccess	= MovePtr<PixelBufferAccess>(new PixelBufferAccess(m_textureFormat, imageExtent.width, imageExtent.height, 1, src2Buffer.getAllocation().getHostPtr()));
	m_src2UpdateRect.offset.x = imageExtent.width / 4;
	m_src2UpdateRect.offset.y = imageExtent.height / 4;
							{ imageExtent.width / 4, imageExtent.height / 4 },
							{ imageExtent.width / 2, imageExtent.height / 2 },
	};

	m_resultBuffer	= MovePtr<BufferWithMemory>(new BufferWithMemory(vkd, device, m_allocator, makeBufferCreateInfo(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT), MemoryRequirement::HostVisible));
	m_resultAccess	= MovePtr<PixelBufferAccess>(new PixelBufferAccess(m_textureFormat, imageExtent.width, imageExtent.height, 1, resultBuffer.getAllocation().getHostPtr());

	// Fill the first source buffer with gradient.
	{
		const Vec4 minColor(0.1f, 0.0f, 0.8f, 1.0f);
		const Vec4 maxColor(0.9f, 0.7f, 0.2f, 1.0f);
		tcu::fillWithComponentGradients2(src1Access, minColor, maxColor);
		flushAlloc(vkd, device, src1Buffer.getAllocation());
	}

	// Fill the second source buffer. Its content is a copy of the first source buffer, with a subrect filled with
	// a different gradient.
	std::memcpy(src2Buffer.getAllocation().getHostPtr(),
				src1Buffer.getAllocation().getHostPtr(),
				bufferSize);

	const auto updateX			= imageExtent.width / 4;
	const auto updateY			= imageExtent.height / 4;
	const auto updateWidth		= imageExtent.width / 2;
	const auto updateHeight		= imageExtent.height / 2;
	const auto updateAccess		= tcu::getSubregion(src2Access, updateX, updateY, updateWidth, updateHeight);
	{
		const Vec4 minColor(0.9f, 0.2f, 0.1f, 1.0f);
		const Vec4 maxColor(0.3f, 0.4f, 0.5f, 1.0f);
		tcu::fillWithComponentGradients2(updateAccess, minColor, maxColor);
		flushAlloc(vkd, device, src2Buffer.getAllocation());
	}

	switch (m_externalHandleTypes) {
	case VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID:
		// TODO
		DE_ASSERT(false);
		break;
	case VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT:
		return this->testDmaBuf();
	default:
		DE_ASSERT(false);
		break;
	}
}

qpTestResult TestInstance::testDmaBuf(void)
{
	qpTestResult result = QP_TEST_RESULT_PASS;

	// Get all DRM format modifiers that are compatible with the image.
	std::vector<uint64_t> modifiers;
	for (const uint64_t& modifier : ImageWithMemory::getDrmFormatModifiersForFormat(m_context, m_format)) {
		if (ImageWithMemory::isDrmFormatModifierCompatible(m_context, m_format, m_extent, modifier, m_externalMemoryType)) {
			modifiers.push_back(modifier);
		}
	}

	if (modifiers.empty())
		TCU_THROW(NotSupportedError, "failed to find compatible DRM format modifier");

	// Test each compatible modifier.
	for (const uint64_t& modifier : modifiers) {
		switch (this->testDmaBufWithDrmFormatModifier(modifier)) {
			case QP_TEST_RESULT_PASS:
				break;
			case QP_TEST_RESULT_FAIL:
				result = QP_TEST_RESULT_FAIL;
				break;
			default:
				DE_ASSERT(false);
				break;
		}
	}

	return result;
}

qpTestResult TestInstance::testDmaBufWithDrmFormatModifier (uint64_t drmFormatModifier)
{
	// TODO: pprint drmFormatModifier
	TestLog& log = m_context.getTestContext().getLog();
	log << tcu::TestLog::Section("Check single DRM format modifier", "")
		<< tcu::TestLog::Message << "drmFormatModifier: " << drmFormatModifier
		<< tcu::TestLog::EndMessage;
	de::MovePtr<ImageWithMemory>	image	(new ImageWithMemory(m_context, format, imageExtent, m_params.externalMemoryType));
	qpTestResult result = this->testImage(image);
	log << tcu::TestLog::EndSection;
	return result;
}

qpTestResult TestInstance::testImage (const ImageWithMemory &image)
{
	// Copy the first source buffer to the image, filling the whole image. Then release ownership of image to foreign queue.
	{
		Move<VkCommandBuffer> cmdBuffer = vk::allocateCommandBuffer(vkd, device, *m_cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		vk::beginCommandBuffer(vkd, *cmdBuffer, 0);

		{
			// Prepare buffer as copy source.
			VkBufferMemoryBarrier bufferBarrier	= initVulkanStructure();
			bufferBarrier.srcAccessMask			= VK_ACCESS_HOST_WRITE_BIT;
			bufferBarrier.dstAccessMask			= VK_ACCESS_TRANSFER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer				= *m_src1Buffer;
			bufferBarrier.offset				= 0;
			bufferBarrier.size					= VK_WHOLE_SIZE;

			// Prepare image as copy dest.
			VkImageMemoryBarrier imageBarrier	= initVulkanStructure();
			imageBarrier.srcAccessMask			= 0;
			imageBarrier.dstAccessMask			= VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.oldLayout				= VK_IMAGE_LAYOUT_UNDEFINED;
			imageBarrier.newLayout				= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image					= *image;
			imageBarrier.subresourceRange		= imageSubresourceRange;

			vkd.cmdPipelineBarrier(*cmdBuffer,
								   VK_PIPELINE_STAGE_HOST_BIT,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,
								   (VkDependencyFlags) 0,
								   0, (VkMemoryBarrier*) DE_NULL,
								   1, &bufferBarrier,
								   1, &imageBarrier);
		}

		{
			// Copy the gradient to the whole image.
			VkBufferImageCopy copy;
			copy.bufferOffset						= 0;
			copy.bufferRowLength					= src1Access.getWidth();
			copy.bufferImageHeight					= src1Access.getHeight();
			copy.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
			copy.imageSubresource.mipLevel			= 0;
			copy.imageSubresource.baseArrayLayer	= 0;
			copy.imageSubresource.layerCount		= 1;
			copy.imageOffset.x						= 0;
			copy.imageOffset.y						= 0;
			copy.imageOffset.z						= 0;
			copy.imageExtent						= imageExtent;

			vkd.cmdCopyBufferToImage(*cmdBuffer,
									 *src1Buffer,
									 *image,
									 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									 1, &copy);
		}

		{
			// Release ownership of image to foreign queue.
			VkImageMemoryBarrier imageBarrier	= initVulkanStructure();
			imageBarrier.srcAccessMask			= VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask			= VK_ACCESS_NONE;
			imageBarrier.oldLayout				= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout				= VK_IMAGE_LAYOUT_GENERAL;
			imageBarrier.srcQueueFamilyIndex	= queueFamilyIndex;
			imageBarrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_FOREIGN_EXT;
			imageBarrier.image					= *image;
			imageBarrier.subresourceRange		= imageSubresourceRange;

			vkd.cmdPipelineBarrier(*cmdBuffer,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,
								   VK_PIPELINE_STAGE_NONE,
								   (VkDependencyFlags) 0,
								   0, (VkMemoryBarrier*) DE_NULL,
								   0, (VkBufferMemoryBarrier*) DE_NULL,
								   1, &imageBarrier);
		}

		vk::endCommandBuffer(vkd, *cmdBuffer);
		vk::submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
	}

	// Acquire ownership of the image from the foreign queue. Then copy the new gradient in the updated region of the
	// buffer to the corresponding region of the image. We do not overwrite the full image because we wish to test the
	// interaction of partial updates with VK_EXT_external_memory_acquire_unmodified.
	{
		Move<VkCommandBuffer> cmdBuffer = vk::allocateCommandBuffer(vkd, device, *cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

		vk::beginCommandBuffer(vkd, *cmdBuffer, 0);

		{
			// Prepare buffer as copy source.
			VkBufferMemoryBarrier bufferBarrier	= initVulkanStructure();
			bufferBarrier.srcAccessMask			= VK_ACCESS_HOST_WRITE_BIT;
			bufferBarrier.dstAccessMask			= VK_ACCESS_TRANSFER_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer				= *referenceBuffer;
			bufferBarrier.offset				= 0;
			bufferBarrier.size					= VK_WHOLE_SIZE;

			// Image is unmodified since the most recent release
			VkExternalMemoryAcquireUnmodifiedEXT acquireUnmodified	= initVulkanStructure();
			acquireUnmodified.acquireUnmodifiedMemory				= VK_TRUE;

			// Acquire ownership of image and prepare as copy dest.
			VkImageMemoryBarrier imageBarrier	= initVulkanStructure();
			imageBarrier.pNext					= &acquireUnmodified;
			imageBarrier.srcAccessMask			= VK_ACCESS_NONE;
			imageBarrier.dstAccessMask			= VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.oldLayout				= VK_IMAGE_LAYOUT_GENERAL;
			imageBarrier.newLayout				= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_FOREIGN_EXT;
			imageBarrier.dstQueueFamilyIndex	= queueFamilyIndex;
			imageBarrier.image					= *image;
			imageBarrier.subresourceRange		= imageSubresourceRange;

			vkd.cmdPipelineBarrier(*cmdBuffer,
								   VK_PIPELINE_STAGE_HOST_BIT,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,
								   (VkDependencyFlags) 0,
								   0, (VkMemoryBarrier*) DE_NULL,
								   1, &bufferBarrier,
								   1, &imageBarrier);
		}

		{
			// Copy the updated region of the reference buffer to the image. This is a partial copy.
			VkBufferImageCopy copy;
			copy.bufferOffset						= ptrDiff(updateAccess.getDataPtr(), referenceBuffer.getAllocation().getHostPtr());
			copy.bufferRowLength					= referenceAccess.getWidth();
			copy.bufferImageHeight					= referenceAccess.getHeight();
			copy.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
			copy.imageSubresource.mipLevel			= 0;
			copy.imageSubresource.baseArrayLayer	= 0;
			copy.imageSubresource.layerCount		= 1;
			copy.imageOffset.x						= updateX;
			copy.imageOffset.y						= updateY;
			copy.imageOffset.z						= 0;
			copy.imageExtent.width					= updateWidth;
			copy.imageExtent.height					= updateHeight;
			copy.imageExtent.depth					= 1;

			vkd.cmdCopyBufferToImage(*cmdBuffer,
									 *referenceBuffer,
									 *image,
									 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									 1, &copy);
		}

		{
			// Prepare image as copy source.
			VkImageMemoryBarrier imageBarrier	= initVulkanStructure();
			imageBarrier.srcAccessMask			= VK_ACCESS_TRANSFER_WRITE_BIT;
			imageBarrier.dstAccessMask			= VK_ACCESS_TRANSFER_READ_BIT;
			imageBarrier.oldLayout				= VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			imageBarrier.newLayout				= VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			imageBarrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			imageBarrier.image					= *image;
			imageBarrier.subresourceRange		= imageSubresourceRange;

			vkd.cmdPipelineBarrier(*cmdBuffer,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,
								   (VkDependencyFlags) 0,
								   0, (VkMemoryBarrier*) DE_NULL,
								   0, (VkBufferMemoryBarrier*) DE_NULL,
								   1, &imageBarrier);
		}

		{
			// Copy image to results buffer.
			VkBufferImageCopy copy;
			copy.bufferOffset						= 0;
			copy.bufferRowLength					= 0;
			copy.bufferImageHeight					= 0;
			copy.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
			copy.imageSubresource.mipLevel			= 0;
			copy.imageSubresource.baseArrayLayer	= 0;
			copy.imageSubresource.layerCount		= 1;
			copy.imageOffset.x						= 0;
			copy.imageOffset.y						= 0;
			copy.imageOffset.z						= 0;
			copy.imageExtent						= imageExtent;

			vkd.cmdCopyImageToBuffer(*cmdBuffer,
									 *image,
									 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
									 *resultBuffer,
									 1, &copy);
		}

		{
			// Prepare results buffer for host read.
			VkBufferMemoryBarrier bufferBarrier	= initVulkanStructure();
			bufferBarrier.srcAccessMask			= VK_ACCESS_TRANSFER_WRITE_BIT;
			bufferBarrier.dstAccessMask			= VK_ACCESS_HOST_READ_BIT;
			bufferBarrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
			bufferBarrier.buffer				= *resultBuffer;
			bufferBarrier.offset				= 0;
			bufferBarrier.size					= VK_WHOLE_SIZE;

			vkd.cmdPipelineBarrier(*cmdBuffer,
								   VK_PIPELINE_STAGE_TRANSFER_BIT,
								   VK_PIPELINE_STAGE_HOST_BIT,
								   (VkDependencyFlags) 0,
								   0, (VkMemoryBarrier*) DE_NULL,
								   1, &bufferBarrier,
								   0, (VkImageMemoryBarrier*) DE_NULL);
		}

		vk::endCommandBuffer(vkd, *cmdBuffer);
		vk::submitCommandsAndWait(vkd, device, queue, *cmdBuffer);
	}

	// Compare reference buffer and results buffer.
	if (vk::isFloatFormat(format)) {
		const Vec4 threshold (0.0f);
		if (!tcu::floatThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceAccess, resultAccess, threshold, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Image comparison failed");
	} else if (vk::isUnormFormat(format)) {
		const tcu::UVec4 threshold (0u);
		if (!tcu::intThresholdCompare(m_context.getTestContext().getLog(), "Compare", "Result comparison", referenceAccess, resultAccess, threshold, tcu::COMPARE_LOG_RESULT))
			return tcu::TestStatus::fail("Image comparison failed");
	} else {
		DE_ASSERT(0);
	}

	return tcu::TestStatus::pass("Pass");
}

ImageWithMemory::ImageWithMemory (Context&								context,
								  VkFormat								format,
								  VkExtent3D							imageExtent,
								  VkExternalMemoryHandleTypeFlagBits	externalMemoryType)
	: m_context				(context)
	, m_format				(format)
	, m_extent				(imageExtent)
	, m_externalMemoryType	(externalMemoryType)
{
	const DeviceInterface& vkd	= m_context.getDeviceInterface();
	VkDevice device				= m_context.getDevice();

	// Only 2D images are supported
	DE_ASSERT(imageExtent.depth == 1);

	// FINISHME: Android
	DE_ASSERT(externalMemoryType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT);

	std::vector<uint64_t> modifiers;
	for (const uint64_t& modifier : getDrmFormatModifiersForFormat(m_context, m_format)) {
		if (isDrmFormatModifierCompatible(m_context, m_format, m_extent, modifier, m_externalMemoryType)) {
			modifiers.push_back(modifier);
		}
	}

	if (modifiers.empty())
		TCU_THROW(NotSupportedError, "failed to find compatible DRM format modifier");

	// Create VkImage
	{
		VkImageDrmFormatModifierListCreateInfoEXT modifierInfo	= initVulkanStructure();
		modifierInfo.drmFormatModifierCount						= static_cast<uint32_t>(modifiers.size());
		modifierInfo.pDrmFormatModifiers						= modifiers.data();

		VkExternalMemoryImageCreateInfo externalInfo	= initVulkanStructure();
		externalInfo.pNext								= &modifierInfo;
		externalInfo.handleTypes						= m_externalMemoryType;

		VkImageCreateInfo imageInfo		= initVulkanStructure();
		imageInfo.pNext					= &externalInfo;
		imageInfo.flags					= 0;
		imageInfo.imageType				= m_imageType;
		imageInfo.format				= m_format;
		imageInfo.extent				= m_extent;
		imageInfo.mipLevels				= 1u;
		imageInfo.arrayLayers			= 1u;
		imageInfo.samples				= VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling				= VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
		imageInfo.usage					= m_usage;
		imageInfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.queueFamilyIndexCount	= 0;
		imageInfo.pQueueFamilyIndices	= DE_NULL;
		imageInfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;

		m_image = vk::createImage(vkd, device, &imageInfo);
	}

	// Allocate VkDeviceMemory
	{
		VkImageMemoryRequirementsInfo2 memReqsInfo2	= initVulkanStructure();
		memReqsInfo2.image							= *m_image;

		VkMemoryDedicatedRequirements dedicatedReqs	= initVulkanStructure();

		VkMemoryRequirements2 memReqs2	= initVulkanStructure();
		memReqs2.pNext					= &dedicatedReqs;

		vkd.getImageMemoryRequirements2(device, &memReqsInfo2, &memReqs2);

		MemoryTypeFilter filter;
		filter.allowedIndexes	= memReqs2.memoryRequirements.memoryTypeBits;
		filter.requiredProps	= 0;
		filter.preferredProps	= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		uint32_t memTypeIndex = chooseMemoryType(m_context, filter);
		DE_ASSERT(memTypeIndex != UINT32_MAX);

		VkMemoryDedicatedAllocateInfo dedicatedAllocInfo	= initVulkanStructure();
		dedicatedAllocInfo.image							= *m_image;

		VkMemoryAllocateInfo allocInfo	= initVulkanStructure();
		if (dedicatedReqs.requiresDedicatedAllocation)
			allocInfo.pNext				= &dedicatedAllocInfo;
		allocInfo.allocationSize		= memReqs2.memoryRequirements.size;
		allocInfo.memoryTypeIndex		= memTypeIndex;

		m_memory = vk::allocateMemory(vkd, device, &allocInfo);
	}

	VK_CHECK(vkd.bindImageMemory(device, *m_image, *m_memory, 0));
}

bool ImageWithMemory::isDrmFormatModifierCompatible (Context&							context,
													 VkFormat							format,
													 VkExtent3D							imageExtent,
													 uint64_t							modifier,
													 VkExternalMemoryHandleTypeFlagBits	externalMemoryType)
{
	const InstanceInterface& vki	= context.getInstanceInterface();
	VkPhysicalDevice physicalDevice	= context.getPhysicalDevice();

	VkPhysicalDeviceImageDrmFormatModifierInfoEXT modifierInfo	= initVulkanStructure();
	modifierInfo.drmFormatModifier								= modifier;
	modifierInfo.sharingMode									= VK_SHARING_MODE_EXCLUSIVE;
	modifierInfo.queueFamilyIndexCount							= 0;
	modifierInfo.pQueueFamilyIndices							= NULL;

	VkPhysicalDeviceExternalImageFormatInfo externalImageInfo	= initVulkanStructure();
	externalImageInfo.pNext										= &modifierInfo;
	externalImageInfo.handleType								= externalMemoryType;

	VkPhysicalDeviceImageFormatInfo2 imageInfo2	= initVulkanStructure();
	imageInfo2.pNext							= &externalImageInfo;
	imageInfo2.format							= format;
	imageInfo2.type								= m_imageType;
	imageInfo2.tiling							= VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
	imageInfo2.usage							= m_usage;
	imageInfo2.flags							= 0;

	VkExternalImageFormatProperties externalImageProperties = initVulkanStructure();

	VkImageFormatProperties2 imageProperties2	= initVulkanStructure();
	imageProperties2.pNext						= &externalImageProperties;

	if (vki.getPhysicalDeviceImageFormatProperties2(physicalDevice, &imageInfo2, &imageProperties2) == VK_ERROR_FORMAT_NOT_SUPPORTED)
		return false;

	if (!(externalImageProperties.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT))
		return false;

	if (imageExtent.width > imageProperties2.imageFormatProperties.maxExtent.width ||
		imageExtent.height > imageProperties2.imageFormatProperties.maxExtent.height ||
		imageExtent.depth > imageProperties2.imageFormatProperties.maxExtent.depth)
		return false;

	return true;
}

std::string formatToName (VkFormat format)
{
	const std::string	formatStr	= de::toString(format);
	const std::string	prefix		= "VK_FORMAT_";

	DE_ASSERT(formatStr.substr(0, prefix.length()) == prefix);

	return de::toLower(formatStr.substr(prefix.length()));
}

} // anonymous

tcu::TestCaseGroup* createExternalMemoryAcquireUnmodifiedTests (tcu::TestContext& testCtx)
{
	typedef de::MovePtr<tcu::TestCaseGroup>	Group;
	typedef de::MovePtr<tcu::TestCase>		Case;

	const VkExternalMemoryHandleTypeFlagBits extMemTypes[] =
	{
		VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,

		// TODO: Add tests for AHB.
		// VK_EXTERNAL_MEMORY_HANDLE_TYPE_ANDROID_HARDWARE_BUFFER_BIT_ANDROID,
	};

    const VkFormat formats[] =
    {
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R32G32B32A32_SFLOAT,
    };

	Group rootGroup(new tcu::TestCaseGroup(testCtx, "external_memory_acquire_unmodified", "Tests for VK_EXT_external_memory_acquire_unmodified"));

	for (auto extMemType : extMemTypes) {
		auto extMemName		= vkt::ExternalMemoryUtil::externalMemoryTypeToName(extMemType);
		auto extMemStr		= de::toString(vk::getExternalMemoryHandleTypeFlagsStr(extMemType));
		Group extMemGroup	(new tcu::TestCaseGroup(testCtx, extMemName, extMemStr.c_str()));

		for (auto format : formats) {
			TestParams params;
			params.format				= format;
			params.externalMemoryType	= extMemType;

			auto formatName	= formatToName(format);
			auto formatStr	= de::toString(vk::getFormatStr(format));
			Case formatCase	(new TestCase(testCtx, formatName.c_str(), formatStr.c_str(), params));

			extMemGroup->addChild(formatCase.release());
		}

		rootGroup->addChild(extMemGroup.release());
    }

    return rootGroup.release();
}

} // memory
} // vkt
