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

#include "tcuGbmNativeDisplay.hpp"

namespace tcu
{
namespace gbm
{

NativeDisplay::NativeDisplay (void)
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

NativeDisplay::~NativeDisplay (void)
{
	if (m_gbm_device != nullptr)
		gbm_device_destroy(m_gbm_device);

	if (m_fd != -1)
		close(m_fd);
}

} // gbm
} // tcu
