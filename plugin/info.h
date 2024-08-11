// Copyright 2021 Jean Pierre Cimalando
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
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "ysfx.h"
#include <juce_core/juce_core.h>
#include <memory>

struct YsfxCurrentPresetInfo : public std::enable_shared_from_this<YsfxCurrentPresetInfo> {
    juce::String m_lastChosenPreset{""};
    using Ptr = std::shared_ptr<YsfxCurrentPresetInfo>;
};

struct YsfxInfo : public std::enable_shared_from_this<YsfxInfo> {
    ysfx_u effect;
    ysfx_bank_u bank;
    juce::Time timeStamp;
    juce::StringArray errors;
    juce::StringArray warnings;
    juce::String m_name;

    using Ptr = std::shared_ptr<YsfxInfo>;
};
