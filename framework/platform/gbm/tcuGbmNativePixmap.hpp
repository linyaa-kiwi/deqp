/*-----------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
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
 */

#pragma once

#include <gbm.h>

#include "egluNativePixmap.hpp"

namespace tcu
{
namespace gbm
{

class NativePixmap final : public eglu::NativePixmap
{
public:
	typedef eglu::NativePixmap::Capability Capability;

	static const Capability CAPABILITIES = Capability(CAPABILITY_CREATE_SURFACE_LEGACY |
													  CAPABILITY_CREATE_SURFACE_PLATFORM);

	NativePixmap (NativeDisplay *display,
				  uint32_t width,
				  uint32_t height,
				  uint32_t gbm_format)
		: eglu::NativePixmap(CAPABILITIES),
		  m_gbm_surface(gbm_surface_create(display->getGbmDevice(),
										   width, height, gbm_format,
										   GBM_BO_USE_RENDERING))
	{
		TCU_CHECK(m_gbm_surface != nullptr);
	}

	~NativePixmap (void) override
	{
		if (m_gbm_surface != nullptr)
			gbm_surface_destroy(m_gbm_surface);
	}

	eglw::EGLNativePixmapType getLegacyNative (void) override
	{
		return m_gbm_surface;
	}

	void* getPlatformNative (void) override
	{
		return m_gbm_surface;
	}

private:
							NativePixmap	(const NativePixmap&) = delete;
	NativePixmap&			operator=		(const NativePixmap&) = delete;

	::gbm_surface *const	m_gbm_surface;
};

} // gbm
} // tcu
