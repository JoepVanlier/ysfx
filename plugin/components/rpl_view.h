// Copyright 2024 Joep Vanlier
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
#include <juce_gui_basics/juce_gui_basics.h>

class YsfxRPLView : public juce::Component {
public:
    YsfxRPLView();
    ~YsfxRPLView() override;
    void setEffect(ysfx_t *fx);
    void setBankUpdateCallback(std::function<void(void)> callback);

    void focusOnPresetViewer();

protected:
    void resized() override;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
