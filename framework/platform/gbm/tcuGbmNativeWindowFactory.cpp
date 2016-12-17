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
#include "tcuGbmNativeWindow.hpp"
#include "tcuGbmNativeWindowFactory.hpp"
#include "tcuGbmPlatform.hpp"

namespace tcu
{
namespace gbm
{

NativeWindowFactory::NativeWindowFactory (void)
	: eglu::NativeWindowFactory("default", "default", NativeWindow::CAPABILITIES)
{}

eglu::NativeWindow* NativeWindowFactory::createWindow (eglu::NativeDisplay* nativeDisplay, const eglu::WindowParams& params) const
{
	return createWindow(nativeDisplay, params.width, params.height, GBM_FORMAT_RGBA8888);
}

eglu::NativeWindow* NativeWindowFactory::createWindow (eglu::NativeDisplay* nativeDisplay, eglw::EGLDisplay display, eglw::EGLConfig config, const eglw::EGLAttrib* attribList, const eglu::WindowParams& params) const
{
	const eglw::Library& egl = nativeDisplay->getLibrary();
	(void) attribList;
	return createWindow(nativeDisplay, params.width, params.height, getGbmFormat(egl, display, config));
}

eglu::NativeWindow* NativeWindowFactory::createWindow (eglu::NativeDisplay* nativeDisplay, int width, int height, uint32_t gbm_format) const
{
	if (width == eglu::WindowParams::SIZE_DONT_CARE)
		width = 512;

	if (height == eglu::WindowParams::SIZE_DONT_CARE)
		height = 512;

	return new NativeWindow(static_cast<NativeDisplay*>(nativeDisplay), width, height, gbm_format);
}

} // gbm
} // tcu
