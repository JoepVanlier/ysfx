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
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>

class YsfxGraphicsView : public juce::Component, public juce::FileDragAndDropTarget {
public:
    YsfxGraphicsView();
    ~YsfxGraphicsView() override;
    void setEffect(ysfx_t *fx);
    void setScaling(float new_scaling);
    float getScaling();
    float getTotalScaling();
    bool m_hasPainted{false};

protected:
    void paint(juce::Graphics &g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress &key) override;
    bool keyStateChanged(bool isKeyDown) override;
    void mouseMove(const juce::MouseEvent &event) override;
    void mouseDown(const juce::MouseEvent &event) override;
    void mouseDrag(const juce::MouseEvent &event) override;
    void mouseUp(const juce::MouseEvent &event) override;
    void mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel) override;

    // FileDragAndDropTarget implementation
    bool isInterestedInFileDrag(const juce::StringArray &files) override;
    void filesDropped(const juce::StringArray &files, int x, int y) override;

private:
    float m_pixelFactor{1.0f};
    float m_outputScalingFactor{1.0f};
    bool m_clearPaint{true};

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
