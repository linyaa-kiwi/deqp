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

#include "tcuGbmNativePixmap.hpp"

namespace tcu
{
namespace gbm
{

class NativePixmapFactory final : public eglu::NativePixmapFactory
{
public:
	NativePixmapFactory (void)
		: eglu::NativePixmapFactory("default", "default",
									 NativePixmap::CAPABILITIES)
	{}

	~NativePixmapFactory (void) override {}

	eglu::NativePixmap* createPixmap (eglu::NativeDisplay* nativeDisplay,
									  int width, int height) const override
	{
		return new NativePixmap(static_cast<NativeDisplay*>(nativeDisplay),
								width, height, GBM_FORMAT_RGBA8888);
	}

	eglu::NativePixmap* createPixmap (eglu::NativeDisplay* nativeDisplay,
									  eglw::EGLDisplay display,
									  eglw::EGLConfig config,
									  const eglw::EGLAttrib* attribList,
									  int width, int height) const override
	{
		const eglw::Library& egl = nativeDisplay->getLibrary();
		(void) attribList;

		return new NativePixmap(static_cast<NativeDisplay*>(nativeDisplay),
								width, height,
								Platform::getGbmFormat(egl, display, config));
	}

private:
	NativePixmapFactory	(const NativePixmapFactory&) = delete;
	NativePixmapFactory& operator=(const NativePixmapFactory&) = delete;
};

} // gbm
} // tcu
