/*-------------------------------------------------------------------------
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
 *
 *//*!
 * \file
 * \brief GBM Platform
 *//*--------------------------------------------------------------------*/

#pragma once

#include "egluPlatform.hpp"
#include "gluPlatform.hpp"
#include "tcuPlatform.hpp"

namespace tcu
{
namespace gbm
{

class Platform final : public tcu::Platform, public eglu::Platform, public glu::Platform
{
public:
	Platform (void);
	~Platform (void) override;

	const eglu::Platform& getEGLPlatform (void) const override
	{
		return *this;
	}

	const glu::Platform& getGLPlatform (void) const override
	{
		return *this;
	}
};

} // gbm
} // tcu
