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

#include <fcntl.h>
#include <gbm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "tcuGbmDefs.hpp"
#include "egluNativeDisplay.hpp"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

namespace tcu
{
namespace gbm
{

class NativeDisplay final : public eglu::NativeDisplay
{
public:
	typedef eglu::NativeDisplay::Capability Capability;

	static const Capability CAPABILITIES = Capability(CAPABILITY_GET_DISPLAY_LEGACY |
													  CAPABILITY_GET_DISPLAY_PLATFORM);
	NativeDisplay (void)
		: eglu::NativeDisplay(CAPABILITIES,
							  EGL_PLATFORM_GBM_KHR,
							  "EGL_KHR_platform_gbm"),
		  m_library("libEGL.so"),
		  m_gbm_device(nullptr),
		  m_fd(-1)
	{
		for (int i = 128; i < 192; ++i) {
			char* path;
			if (asprintf(&path, "/dev/dri/renderD%d", i) < 0)
				continue;

			m_fd = open(path, O_RDWR | O_CLOEXEC);
			if (m_fd == -1)
				continue;

			m_gbm_device = gbm_create_device(m_fd);
			if (m_gbm_device == nullptr) {
				close(m_fd);
				m_fd = -1;
				continue;
			} else {
				break;
			}
		}

		if (m_gbm_device == nullptr)
			TCU_FAIL("failed to open GBM device");
	}

	~NativeDisplay (void) override
	{
		if (m_gbm_device != nullptr)
			gbm_device_destroy(m_gbm_device);

		if (m_fd != -1)
			close(m_fd);
	}

	const eglw::Library& getLibrary	(void) const override
	{
		return m_library;
	}

	eglw::EGLNativeDisplayType getLegacyNative (void) override
	{
		return m_gbm_device;
	}

	void* getPlatformNative	(void) override
	{
		return m_gbm_device;
	}

	gbm_device* getGbmDevice (void)
	{
		return m_gbm_device;
	}

private:
							NativeDisplay	(const NativeDisplay&) = delete;
	NativeDisplay&			operator=		(const NativeDisplay&) = delete;

	eglw::DefaultLibrary	m_library;
	::gbm_device*			m_gbm_device;
	int						m_fd;
};

} // gbm
} // tcu
