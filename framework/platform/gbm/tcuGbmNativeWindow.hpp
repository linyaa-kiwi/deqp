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

#include "egluNativeWindow.hpp"

namespace tcu
{
namespace gbm
{

class NativeDisplay;

class NativeWindow final : public eglu::NativeWindow
{
public:
	typedef eglu::NativeWindow::Capability Capability;

	static const Capability CAPABILITIES = Capability(CAPABILITY_CREATE_SURFACE_LEGACY |
													  CAPABILITY_CREATE_SURFACE_PLATFORM |
													  CAPABILITY_GET_SURFACE_SIZE);

	NativeWindow (NativeDisplay *display,
				  uint32_t width,
				  uint32_t height,
				  uint32_t gbm_format);

	~NativeWindow (void) override;

	eglw::EGLNativeWindowType getLegacyNative (void) override
	{
		return m_gbm_surface;
	}

	void* getPlatformNative (void) override
	{
		return m_gbm_surface;
	}

	tcu::IVec2 getSurfaceSize (void) const override
	{
		return tcu::IVec2(m_width, m_height);
	}

	tcu::IVec2 getScreenSize (void) const override
	{
		return getSurfaceSize();
	}

private:
							NativeWindow	(const NativeWindow&) = delete;
	NativeWindow&			operator=		(const NativeWindow&) = delete;

	::gbm_surface *const	m_gbm_surface;
	const uint32_t			m_width;
	const uint32_t			m_height;
};

} // gbm
} // tcu
