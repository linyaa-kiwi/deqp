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
#include "tcuGbmNativeWindow.hpp"

namespace tcu
{
namespace gbm
{

class NativeWindowFactory final : public eglu::NativeWindowFactory
{
public:
	NativeWindowFactory (void)
		: eglu::NativeWindowFactory("default", "default",
									 NativeWindow::CAPABILITIES)
	{}

	~NativeWindowFactory (void) override {}

	eglu::NativeWindow* createWindow (eglu::NativeDisplay* nativeDisplay,
									  const eglu::WindowParams& params) const override
	{
		return new NativeWindow(static_cast<NativeDisplay*>(nativeDisplay),
								params.width, params.height, GBM_FORMAT_RGBA8888);
	}

	eglu::NativeWindow* createWindow (eglu::NativeDisplay* nativeDisplay,
									  eglw::EGLDisplay display,
									  eglw::EGLConfig config,
									  const eglw::EGLAttrib* attribList,
									  const eglu::WindowParams& params) const override
	{
		const eglw::Library& egl = nativeDisplay->getLibrary();
		(void) attribList;

		return new NativeWindow(static_cast<NativeDisplay*>(nativeDisplay),
								params.width, params.height,
								getGbmFormat(egl, display, config));
	}

private:
	NativeWindowFactory	(const NativeWindowFactory&) = delete;
	NativeWindowFactory& operator=(const NativeWindowFactory&) = delete;

	static uint32_t getGbmFormat (const eglw::Library& egl, eglw::EGLDisplay display, eglw::EGLConfig config)
	{
		eglw::EGLint gbm_format = 0;
		TCU_CHECK(egl.getConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &gbm_format));
		return gbm_format;
	}
};

} // gbm
} // tcu
