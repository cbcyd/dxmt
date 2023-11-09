//-------------------------------------------------------------------------------------------------------------------------------------------------------------
//
// Foundation/NSTypes.hpp
//
// Copyright 2020-2023 Apple Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#pragma once

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

#include "NSDefines.hpp"

// #include <CoreFoundation/CoreFoundation.h>
#include <cstdint>

//-------------------------------------------------------------------------------------------------------------------------------------------------------------

namespace NS {
using TimeInterval = double;

using Integer = std::intptr_t;
using UInteger = std::uintptr_t;

const Integer IntegerMax = INTPTR_MAX;
const Integer IntegerMin = INTPTR_MIN;
const UInteger UIntegerMax = UINTPTR_MAX;

struct OperatingSystemVersion {
  Integer majorVersion;
  Integer minorVersion;
  Integer patchVersion;
} _NS_PACKED;
} // namespace NS

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
