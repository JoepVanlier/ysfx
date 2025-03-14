// Copyright 2021 Jean Pierre Cimalando
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
// Modifications by Joep Vanlier, 2024
//
// SPDX-License-Identifier: Apache-2.0
//

#include "graphics_view.h"
#include "utility/functional_timer.h"
#include "utility/async_updater.h"
#include "utility/rt_semaphore.h"
#include <list>
#include <map>
#include <queue>
#include <tuple>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cmath>
#include <cstdio>

struct YsfxGraphicsView::Impl final : public better::AsyncUpdater::Listener {
    static uint32_t translateKeyCode(int code);
    static uint32_t translateModifiers(juce::ModifierKeys mods);
    static void translateKeyPress(const juce::KeyPress &key, uint32_t &ykey, uint32_t &ymods);

    juce::Point<int> getDisplayOffset() const;

    void tickGfx();
    bool updateGfxTarget(int newWidth, int newHeight, int newRetina);
    void updateYsfxKeyModifiers();
    void updateYsfxMousePosition(const juce::MouseEvent &event);
    void updateYsfxMouseButtons(const juce::MouseEvent &event);
    static int showYsfxMenu(void *userdata, const char *desc, int32_t xpos, int32_t ypos);
    static void setYsfxCursor(void *userdata, int32_t cursor);
    static const char *getYsfxDropFile(void *userdata, int32_t index);

    YsfxGraphicsView *m_self = nullptr;
    ysfx_u m_fx;
    std::unique_ptr<juce::Timer> m_gfxTimer;

    //--------------------------------------------------------------------------
    struct GfxTarget : public std::enable_shared_from_this<GfxTarget> {
        int m_gfxWidth = 0;
        int m_gfxHeight = 0;
        bool m_wantRetina = false;
        juce::Image m_renderBitmap{juce::Image::ARGB, 1, 1, false, juce::SoftwareImageType{}};
        double m_bitmapScale = 1;
        using Ptr = std::shared_ptr<GfxTarget>;
    };

    struct GfxInputState : public std::enable_shared_from_this<GfxInputState> {
        uint32_t m_ysfxMouseMods = 0;
        uint32_t m_ysfxMouseButtons = 0;
        int32_t m_ysfxMouseX = 0;
        int32_t m_ysfxMouseY = 0;
        double m_ysfxWheel = 0;
        double m_ysfxHWheel = 0;
        using YsfxKeyEvent = std::tuple<uint32_t, uint32_t, bool>;
        std::queue<YsfxKeyEvent> m_ysfxKeys;
        using Ptr = std::shared_ptr<GfxInputState>;
    };

    GfxTarget::Ptr m_gfxTarget;
    GfxInputState::Ptr m_gfxInputState;

    // whether the next @gfx is required to repaint the screen in full
    bool m_gfxDirty = true;

    // whether the jsfx had a first initialization of the gfx resolution or not
    bool m_gfxInitialized = false;

    //--------------------------------------------------------------------------
    struct KeyPressed {
        int jcode = 0;
        uint32_t ykey = 0;
        uint32_t ymods = 0;
    };

    std::list<KeyPressed> m_keysPressed;

    //--------------------------------------------------------------------------
    // Asynchronous popup menu

    static std::unique_ptr<juce::PopupMenu> createPopupMenu(const char *str);
    void endPopupMenu(int menuResult);

    std::unique_ptr<juce::PopupMenu> m_popupMenu;

    //--------------------------------------------------------------------------
    // Dropped files

    std::mutex m_droppedFilesMutex;
    juce::StringArray m_droppedFiles;
    juce::String m_droppedFileReturned;

    //--------------------------------------------------------------------------
    // The background thread will trigger these async updates.

    // sends a bitmap the component should repaint itself with
    struct AsyncRepainter : public better::AsyncUpdater {
        // whether the bitmap contains changes
        bool m_hasBitmapChanged = false;
        // a double-buffer of the render bitmap, copied after a finished rendering
        juce::Image m_bitmap{juce::Image::ARGB, 1, 1, false, juce::SoftwareImageType{}};
        std::mutex m_mutex;
    };

    // changes the mouse cursor on the component
    struct AsyncMouseCursor : public better::AsyncUpdater {
        std::atomic<juce::MouseCursor::StandardCursorType> m_cursorType;
    };

    // triggers a menu to redisplay
    struct AsyncShowMenu : public better::AsyncUpdater {
        std::string m_menuDesc;
        int m_menuX = 0;
        int m_menuY = 0;
        volatile bool m_completionFlag = false;
        volatile int m_completionValue = 0;
        std::condition_variable m_completionVariable;
        std::mutex m_mutex;
    };

    std::unique_ptr<AsyncRepainter> m_asyncRepainter;
    std::unique_ptr<AsyncMouseCursor> m_asyncMouseCursor;
    std::unique_ptr<AsyncShowMenu> m_asyncShowMenu;

    void handleAsyncUpdate(better::AsyncUpdater *updater) override;

    //--------------------------------------------------------------------------
    // This background thread runs @gfx.
    // This is on a separate thread, because it has elements which can block,
    // which otherwise would require modal loops (eg. `gfx_showmenu`).

    class BackgroundWork {
    public:
        void start();
        void stop();

        struct Message : std::enable_shared_from_this<Message> {
            explicit Message(int type) : m_type{type} {}
            int m_type = 0;
        };

        struct GfxMessage : Message {
            GfxMessage() : Message{'@gfx'} {}
            ysfx_u m_fx;
            GfxTarget::Ptr m_target;
            bool m_dirty = false;
            GfxInputState m_input;
            AsyncRepainter *m_asyncRepainter = nullptr;
            void *m_userData = nullptr;
        };

        void postMessage(std::shared_ptr<Message> message);

    private:
        void run();
        std::shared_ptr<Message> popNextMessage();
        void processGfxMessage(GfxMessage &msg);

    private:
        std::thread m_thread;
        RTSemaphore m_sema;
        volatile bool m_running = false;
        std::queue<std::shared_ptr<Message>> m_messages;
        std::mutex m_messagesMutex;
    };

    BackgroundWork m_work;

    uint32_t m_numWaitedRepaints = 0;
};

YsfxGraphicsView::YsfxGraphicsView()
    : m_impl{new YsfxGraphicsView::Impl}
{
    m_impl->m_self = this;

    m_impl->m_gfxTarget.reset(new Impl::GfxTarget);
    m_impl->m_gfxInputState.reset(new Impl::GfxInputState);

    m_impl->m_asyncRepainter.reset(new Impl::AsyncRepainter);
    m_impl->m_asyncMouseCursor.reset(new Impl::AsyncMouseCursor);
    m_impl->m_asyncShowMenu.reset(new Impl::AsyncShowMenu);

    m_impl->m_asyncRepainter->addListener(m_impl.get());
    m_impl->m_asyncMouseCursor->addListener(m_impl.get());
    m_impl->m_asyncShowMenu->addListener(m_impl.get());

    setOpaque(true);

    setWantsKeyboardFocus(true);
}

YsfxGraphicsView::~YsfxGraphicsView()
{
    m_impl->endPopupMenu(0);
    m_impl->m_work.stop();

    m_impl->m_asyncRepainter->removeListener(m_impl.get());
    m_impl->m_asyncMouseCursor->removeListener(m_impl.get());
    m_impl->m_asyncShowMenu->removeListener(m_impl.get());
}

void YsfxGraphicsView::setEffect(ysfx_t *fx)
{
    if (m_impl->m_fx.get() == fx)
        return;

    m_impl->m_fx.reset(fx);
    if (fx)
        ysfx_add_ref(fx);

    m_impl->endPopupMenu(0);
    m_impl->m_work.stop();

    m_impl->m_gfxDirty = true;
    m_impl->m_gfxInitialized = false;

    if (!fx || !ysfx_has_section(fx, ysfx_section_gfx)) {
        m_impl->m_gfxTimer.reset();
        repaint();
    }
    else {
        m_impl->m_work.start();
        m_impl->m_gfxTimer.reset(FunctionalTimer::create([this]() { m_impl->tickGfx(); }));
        m_impl->m_gfxTimer->startTimerHz(ysfx_get_requested_framerate(fx));
    }

    m_impl->m_gfxInputState.reset(new Impl::GfxInputState);

    m_impl->m_asyncRepainter->cancelPendingUpdate();
    m_impl->m_asyncMouseCursor->cancelPendingUpdate();
    m_impl->m_asyncShowMenu->cancelPendingUpdate();

    m_impl->m_popupMenu.reset();

    m_impl->m_numWaitedRepaints = 0;

    setMouseCursor(juce::MouseCursor{juce::MouseCursor::NormalCursor});
}

void YsfxGraphicsView::setScaling(float new_scaling)
{
    m_outputScalingFactor.store(new_scaling);
    fullPixelScaling = static_cast<bool>(std::abs(std::round(new_scaling) - new_scaling) <= 0.0000001f);
}

float YsfxGraphicsView::getScaling()
{
    return m_outputScalingFactor.load();
}

float YsfxGraphicsView::getTotalScaling()
{
    // We rescale this only when we have active UI rescaling on under the assumption that that mode
    // is used mostly for JSFX that do not have the inherent ability to scale with the UI.
    return m_outputScalingFactor.load() / (m_outputScalingFactor.load() > 1.1f ? m_pixelFactor.load() : 1.0f);
}

void YsfxGraphicsView::paint(juce::Graphics &g)
{
    ///
    Impl::GfxTarget *target = m_impl->m_gfxTarget.get();

    // Get final pixel size (we want to correct for any DPI scaling that's happening by making the
    // graphical render target larger).
    m_pixelFactor = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());

    ///
    if (fullPixelScaling) {
        g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    } else {
        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    }
    std::lock_guard<std::mutex> lock{m_impl->m_asyncRepainter->m_mutex};
    juce::Image &image = m_impl->m_asyncRepainter->m_bitmap;

    g.setOpacity(1.0f);
    auto trafo = juce::AffineTransform::scale(m_outputScalingFactor.load() / m_pixelFactor.load()).translated(0.5f, 0.5f);
    g.drawImageTransformed(image, trafo, false);
}

void YsfxGraphicsView::resized()
{
    Component::resized();
    if (m_impl->updateGfxTarget(-1, -1, -1))
        m_impl->m_gfxDirty = true;
}

bool YsfxGraphicsView::keyPressed(const juce::KeyPress &key)
{
    m_impl->updateYsfxKeyModifiers();

    for (const Impl::KeyPressed &kp : m_impl->m_keysPressed) {
        if (kp.jcode == key.getKeyCode())
            return true;
    }

    Impl::KeyPressed kp;
    kp.jcode = key.getKeyCode();
    Impl::translateKeyPress(key, kp.ykey, kp.ymods);

    m_impl->m_keysPressed.push_back(kp);
    ysfx_t *fx = m_impl->m_fx.get();
    if (fx && ysfx_has_section(fx, ysfx_section_gfx)) {
        Impl::GfxInputState *inputs = m_impl->m_gfxInputState.get();
        inputs->m_ysfxKeys.emplace(kp.ymods, kp.ykey, true);
    }

    // Pass escape through so users can close the plugin
    if (key.getKeyCode() == key.escapeKey) return false;

    // Pass space through so users can change transport state
    if (key.getKeyCode() == key.spaceKey) return false;

    // Pass modifier-based key combos through
    juce::ModifierKeys mods = key.getModifiers();
    if (mods.isCtrlDown() || mods.isCommandDown()) return false;

    return true;
}

bool YsfxGraphicsView::keyStateChanged(bool isKeyDown)
{
    m_impl->updateYsfxKeyModifiers();

    if (!isKeyDown) {
        for (auto it = m_impl->m_keysPressed.begin(); it != m_impl->m_keysPressed.end(); ) {
            Impl::KeyPressed kp = *it;
            if (juce::KeyPress::isKeyCurrentlyDown(kp.jcode))
                ++it;
            else {
                m_impl->m_keysPressed.erase(it++);
                kp.ymods = Impl::translateModifiers(juce::ModifierKeys::getCurrentModifiers());
                ysfx_t *fx = m_impl->m_fx.get();
                if (fx && ysfx_has_section(fx, ysfx_section_gfx)) {
                    Impl::GfxInputState *inputs = m_impl->m_gfxInputState.get();
                    inputs->m_ysfxKeys.emplace(kp.ymods, kp.ykey, false);
                }
            }
        }
    }

    // Make sure modifier-based key combos are not lost
    juce::ModifierKeys mods = juce::ModifierKeys::getCurrentModifiers();
    if (mods.isCtrlDown() || mods.isCommandDown()) return false;

    return true;
}

void YsfxGraphicsView::mouseMove(const juce::MouseEvent &event)
{
    m_impl->updateYsfxKeyModifiers();
    m_impl->updateYsfxMousePosition(event);
}

void YsfxGraphicsView::mouseDown(const juce::MouseEvent &event)
{
    m_impl->updateYsfxKeyModifiers();
    m_impl->updateYsfxMousePosition(event);
    m_impl->updateYsfxMouseButtons(event);
}

void YsfxGraphicsView::mouseDrag(const juce::MouseEvent &event)
{
    m_impl->updateYsfxKeyModifiers();
    m_impl->updateYsfxMousePosition(event);
}

void YsfxGraphicsView::mouseUp(const juce::MouseEvent &event)
{
    m_impl->updateYsfxKeyModifiers();
    m_impl->updateYsfxMousePosition(event);

    Impl::GfxInputState *gfxInputState = m_impl->m_gfxInputState.get();
    gfxInputState->m_ysfxMouseButtons = 0;
}

void YsfxGraphicsView::mouseWheelMove(const juce::MouseEvent &event, const juce::MouseWheelDetails &wheel)
{
    m_impl->updateYsfxKeyModifiers();
    m_impl->updateYsfxMousePosition(event);

    Impl::GfxInputState *gfxInputState = m_impl->m_gfxInputState.get();
    gfxInputState->m_ysfxWheel += wheel.deltaY / m_pixelFactor.load();
    gfxInputState->m_ysfxHWheel += wheel.deltaX / m_pixelFactor.load();
}

//------------------------------------------------------------------------------
bool YsfxGraphicsView::isInterestedInFileDrag(const juce::StringArray &files)
{
    (void)files;
    return true;
}

void YsfxGraphicsView::filesDropped(const juce::StringArray &files, int x, int y)
{
    (void)x;
    (void)y;
    std::lock_guard<std::mutex> lock{m_impl->m_droppedFilesMutex};
    m_impl->m_droppedFiles = files;
}

//------------------------------------------------------------------------------
uint32_t YsfxGraphicsView::Impl::translateKeyCode(int code)
{
    using Map = std::map<int, uint32_t>;

    static const Map keyCodeMap = []() -> Map
    {
        Map map {
            {juce::KeyPress::F1Key, ysfx_key_f1},
            {juce::KeyPress::F2Key, ysfx_key_f2},
            {juce::KeyPress::F3Key, ysfx_key_f3},
            {juce::KeyPress::F4Key, ysfx_key_f4},
            {juce::KeyPress::F5Key, ysfx_key_f5},
            {juce::KeyPress::F6Key, ysfx_key_f6},
            {juce::KeyPress::F7Key, ysfx_key_f7},
            {juce::KeyPress::F8Key, ysfx_key_f8},
            {juce::KeyPress::F9Key, ysfx_key_f9},
            {juce::KeyPress::F10Key, ysfx_key_f10},
            {juce::KeyPress::F11Key, ysfx_key_f11},
            {juce::KeyPress::F12Key, ysfx_key_f12},
            {juce::KeyPress::leftKey, ysfx_key_left},
            {juce::KeyPress::upKey, ysfx_key_up},
            {juce::KeyPress::rightKey, ysfx_key_right},
            {juce::KeyPress::downKey, ysfx_key_down},
            {juce::KeyPress::pageUpKey, ysfx_key_page_up},
            {juce::KeyPress::pageDownKey, ysfx_key_page_down},
            {juce::KeyPress::homeKey, ysfx_key_home},
            {juce::KeyPress::endKey, ysfx_key_end},
            {juce::KeyPress::insertKey, ysfx_key_insert},
        };
        return map;
    }();

    Map::const_iterator it = keyCodeMap.find(code);
    if (it == keyCodeMap.end())
        return 0;

    return it->second;
}

uint32_t YsfxGraphicsView::Impl::translateModifiers(juce::ModifierKeys mods)
{
    uint32_t ymods = 0;
    if (mods.isShiftDown())
        ymods |= ysfx_mod_shift;
    if (mods.isCtrlDown())
        ymods |= ysfx_mod_ctrl;
    if (mods.isAltDown())
        ymods |= ysfx_mod_alt;
    if (mods.isCommandDown())
        ymods |= ysfx_mod_super;
    return ymods;
}

void YsfxGraphicsView::Impl::translateKeyPress(const juce::KeyPress &key, uint32_t &ykey, uint32_t &ymods)
{
    int code = key.getKeyCode();
    juce::juce_wchar character = key.getTextCharacter();
    juce::ModifierKeys mods = key.getModifiers();

    ykey = translateKeyCode(code);
    if (ykey == 0) {
        ykey = (uint32_t)character;
        if (mods.isCtrlDown() && ykey >= 1 && ykey <= 26)
            ykey = ykey - 1 + 'a';
    }

    ymods = translateModifiers(mods);
}

//------------------------------------------------------------------------------
void YsfxGraphicsView::Impl::tickGfx()
{
    // don't overload the background with @gfx requests
    // (remember that @gfx can block)
    if (m_numWaitedRepaints > 1)
        return;

    ysfx_t *fx = m_fx.get();
    jassert(fx);

    ///
    uint32_t gfxDim[2] = {};
    ysfx_get_gfx_dim(fx, gfxDim);

    bool gfxWantRetina = ysfx_gfx_wants_retina(fx);
    
    if (m_gfxInitialized ? updateGfxTarget(-1, -1, -1) : updateGfxTarget((int)gfxDim[0], (int)gfxDim[1], gfxWantRetina)) {
        m_gfxDirty = true;
        m_gfxInitialized = true;
    }

    ///
    std::shared_ptr<BackgroundWork::GfxMessage> msg{new BackgroundWork::GfxMessage};
    msg->m_fx.reset(fx);
    ysfx_add_ref(fx);
    msg->m_target = m_gfxTarget;
    msg->m_dirty = m_gfxDirty;
    msg->m_input.m_ysfxMouseMods = m_gfxInputState->m_ysfxMouseMods;
    msg->m_input.m_ysfxMouseButtons = m_gfxInputState->m_ysfxMouseButtons;
    msg->m_input.m_ysfxMouseX = m_gfxInputState->m_ysfxMouseX;
    msg->m_input.m_ysfxMouseY = m_gfxInputState->m_ysfxMouseY;
    msg->m_input.m_ysfxWheel = m_gfxInputState->m_ysfxWheel;
    msg->m_input.m_ysfxHWheel = m_gfxInputState->m_ysfxHWheel;
    msg->m_input.m_ysfxKeys = std::move(m_gfxInputState->m_ysfxKeys);
    msg->m_asyncRepainter = m_asyncRepainter.get();
    msg->m_userData = m_self;

    m_gfxInputState->m_ysfxWheel = 0;
    m_gfxInputState->m_ysfxHWheel = 0;

    ///
    m_work.postMessage(msg);
    m_numWaitedRepaints += 1;
}

bool YsfxGraphicsView::Impl::updateGfxTarget(int newWidth, int newHeight, int newRetina)
{
    GfxTarget *target = m_gfxTarget.get();

    float output_scaling = m_self->m_outputScalingFactor.load();
    float pixel_factor = m_self->m_pixelFactor.load() / output_scaling;

    // newWidth is set when the JSFX initializes
    float scaling_factor = 1.0f / (output_scaling > 1.1f ? pixel_factor : 1.0f);
    newWidth = (newWidth == -1) ? m_self->getWidth() : (int) (newWidth * scaling_factor);
    newHeight = (newHeight == -1) ? m_self->getHeight() : (int) (newHeight * scaling_factor);
    newRetina = (newRetina == -1) ? target->m_wantRetina : newRetina;

    // Set internal JSFX texture size
    int internal_width = static_cast<int>(newWidth * pixel_factor);
    int internal_height = static_cast<int>(newHeight * pixel_factor);

    bool needsUpdate = (
        (target->m_gfxWidth != internal_width)
        || (target->m_gfxHeight != internal_height)
        || (target->m_wantRetina != static_cast<bool>(newRetina))
        || (std::abs(target->m_bitmapScale - pixel_factor) > 1e-4)
    );

    if (needsUpdate) {
        target = new GfxTarget;
        m_gfxTarget.reset(target);
        target->m_gfxWidth = internal_width;
        target->m_gfxHeight = internal_height;
        target->m_wantRetina = (bool)newRetina;
        target->m_renderBitmap = juce::Image(juce::Image::ARGB, juce::jmax(1, internal_width - 2), juce::jmax(1, internal_height - 2), true, juce::SoftwareImageType{});
        target->m_bitmapScale = pixel_factor;
    }

    return needsUpdate;
}

void YsfxGraphicsView::Impl::updateYsfxKeyModifiers()
{
    juce::ModifierKeys mods = juce::ModifierKeys::getCurrentModifiers();
    m_gfxInputState->m_ysfxMouseMods = translateModifiers(mods);
}

void YsfxGraphicsView::Impl::updateYsfxMousePosition(const juce::MouseEvent &event)
{
    juce::Point<int> off = getDisplayOffset();
    double bitmapScale = m_gfxTarget->m_bitmapScale;
    m_gfxInputState->m_ysfxMouseX = juce::roundToInt((event.x - off.x) * bitmapScale);
    m_gfxInputState->m_ysfxMouseY = juce::roundToInt((event.y - off.y) * bitmapScale);
}

void YsfxGraphicsView::Impl::updateYsfxMouseButtons(const juce::MouseEvent &event)
{
    uint32_t buttons = 0;
    if (event.mods.isLeftButtonDown())
        buttons |= ysfx_button_left;
    if (event.mods.isMiddleButtonDown())
        buttons |= ysfx_button_middle;
    if (event.mods.isRightButtonDown())
        buttons |= ysfx_button_right;
    m_gfxInputState->m_ysfxMouseButtons = buttons;
}

juce::Point<int> YsfxGraphicsView::Impl::getDisplayOffset() const
{
    // Let JSFX handle the UX offsetting themselves
    return juce::Point<int>{0, 0};
}

int YsfxGraphicsView::Impl::showYsfxMenu(void *userdata, const char *desc, int32_t xpos, int32_t ypos)
{
    YsfxGraphicsView *self = (YsfxGraphicsView *)userdata;
    AsyncShowMenu *updater = self->m_impl->m_asyncShowMenu.get();

    std::unique_lock<std::mutex> lock{updater->m_mutex};
    updater->m_menuDesc.assign(desc);
    updater->m_menuX = xpos;
    updater->m_menuY = ypos;
    updater->m_completionFlag = false;
    updater->m_completionValue = 0;
    updater->triggerAsyncUpdate();

    do updater->m_completionVariable.wait(lock);
    while (!updater->m_completionFlag);

    return updater->m_completionValue;
}

void YsfxGraphicsView::Impl::setYsfxCursor(void *userdata, int32_t cursor)
{
    YsfxGraphicsView *self = (YsfxGraphicsView *)userdata;

    enum {
        ocr_normal = 32512,
        ocr_ibeam = 32513,
        ocr_wait = 32514,
        ocr_cross = 32515,
        ocr_up = 32516,
        ocr_size = 32640,
        ocr_icon = 32641,
        ocr_sizenwse = 32642,
        ocr_sizenesw = 32643,
        ocr_sizewe = 32644,
        ocr_sizens = 32645,
        ocr_sizeall = 32646,
        ocr_icocur = 32647,
        ocr_no = 32648,
        ocr_hand = 32649,
        ocr_appstarting = 32650,
    };

    using CursorType = juce::MouseCursor::StandardCursorType;
    CursorType type;

    switch (cursor) {
    default:
    case ocr_normal:
    case ocr_icon:
        type = CursorType::NormalCursor;
        break;
    case ocr_ibeam:
        type = CursorType::IBeamCursor;
        break;
    case ocr_wait:
        type = CursorType::WaitCursor;
        break;
    case ocr_cross:
        type = CursorType::CrosshairCursor;
        break;
    case ocr_size:
    case ocr_sizeall:
        type = CursorType::UpDownLeftRightResizeCursor;
        break;
    case ocr_sizenwse:
        type = CursorType::TopLeftCornerResizeCursor;
        break;
    case ocr_sizenesw:
        type = CursorType::TopRightCornerResizeCursor;
        break;
    case ocr_sizewe:
        type = CursorType::LeftRightResizeCursor;
        break;
    case ocr_sizens:
        type = CursorType::UpDownResizeCursor;
        break;
    case ocr_hand:
        type = CursorType::PointingHandCursor;
        break;
    }

    AsyncMouseCursor *async = self->m_impl->m_asyncMouseCursor.get();
    async->m_cursorType.store(type, std::memory_order_relaxed);
    async->triggerAsyncUpdate();
}

const char *YsfxGraphicsView::Impl::getYsfxDropFile(void *userdata, int32_t index)
{
    YsfxGraphicsView *self = (YsfxGraphicsView *)userdata;

    std::lock_guard<std::mutex> lock{self->m_impl->m_droppedFilesMutex};

    if (index == -1) {
        self->m_impl->m_droppedFiles.clearQuick();
        return nullptr;
    }

    juce::StringArray &list = self->m_impl->m_droppedFiles;
    if (index < 0 || index >= list.size())
        return nullptr;

    juce::String &buffer = self->m_impl->m_droppedFileReturned;
    buffer = list[index];
    return buffer.toRawUTF8();
}

//------------------------------------------------------------------------------
void YsfxGraphicsView::Impl::BackgroundWork::start()
{
    if (m_running)
        return;

    m_running = true;
    m_thread = std::thread([this]() { run(); });
}

void YsfxGraphicsView::Impl::BackgroundWork::stop()
{
    if (!m_running)
        return;

    m_running = false;
    m_sema.post();

    m_thread.join();

    std::lock_guard<std::mutex> lock{m_messagesMutex};
    while (!m_messages.empty())
        m_messages.pop();

    m_sema.clear();
}

void YsfxGraphicsView::Impl::BackgroundWork::postMessage(std::shared_ptr<Message> message)
{
    if (!m_running)
        return;

    {
        std::lock_guard<std::mutex> lock{m_messagesMutex};
        m_messages.emplace(message);
    }
    m_sema.post();
}

void YsfxGraphicsView::Impl::BackgroundWork::run()
{
    while (m_sema.wait(), m_running) {
        std::shared_ptr<Message> msg = popNextMessage();
        jassert(msg);

        if (msg) {
            switch (msg->m_type) {
            case '@gfx':
                processGfxMessage(static_cast<GfxMessage &>(*msg));
                break;
            }
        }
    }
}

std::shared_ptr<YsfxGraphicsView::Impl::BackgroundWork::Message> YsfxGraphicsView::Impl::BackgroundWork::popNextMessage()
{
    std::lock_guard<std::mutex> lock{m_messagesMutex};
    if (m_messages.empty())
        return nullptr;

    std::shared_ptr<Message> msg = m_messages.front();
    m_messages.pop();
    return msg;
}

void YsfxGraphicsView::Impl::BackgroundWork::processGfxMessage(GfxMessage &msg)
{
    ysfx_t *fx = msg.m_fx.get();
    GfxInputState &input = msg.m_input;

    while (!input.m_ysfxKeys.empty()) {
        GfxInputState::YsfxKeyEvent event = input.m_ysfxKeys.front();
        input.m_ysfxKeys.pop();
        ysfx_gfx_add_key(fx, std::get<0>(event), std::get<1>(event), std::get<2>(event));
    }

    ysfx_gfx_update_mouse(fx, input.m_ysfxMouseMods, input.m_ysfxMouseX, input.m_ysfxMouseY, input.m_ysfxMouseButtons, input.m_ysfxWheel, input.m_ysfxHWheel);

    ///
    GfxTarget *target = msg.m_target.get();
    bool mustRepaint;

    {
        juce::Image::BitmapData bdata{target->m_renderBitmap, juce::Image::BitmapData::readWrite};

        ysfx_gfx_config_t gc{};
        gc.user_data = msg.m_userData;
        gc.pixel_width = (uint32_t)bdata.width;
        gc.pixel_height = (uint32_t)bdata.height;
        gc.pixel_stride = (uint32_t)bdata.lineStride;
        gc.pixels = bdata.data;
        gc.scale_factor = 1.0;  // This is handled by setting the UI size in the plugin ourselves
        gc.show_menu = &showYsfxMenu;
        gc.set_cursor = &setYsfxCursor;
        gc.get_drop_file = &getYsfxDropFile;
        ysfx_gfx_setup(fx, &gc);

        // multiple @gfx cannot run concurrently on different threads (issue 44)
        // FIXME: this workaround affects performance, actually fix this properly
        static std::mutex globalGfxRunMutex;
        std::lock_guard<std::mutex> gfxRunLock{globalGfxRunMutex};

        mustRepaint = ysfx_gfx_run(fx) || msg.m_dirty;
    }

    ///
    std::lock_guard<std::mutex> lock{msg.m_asyncRepainter->m_mutex};

    if (!mustRepaint)
        msg.m_asyncRepainter->m_hasBitmapChanged = false;
    else
    {
        juce::Image &imgsrc = target->m_renderBitmap;
        juce::Image &imgdst = msg.m_asyncRepainter->m_bitmap;

        int w = imgsrc.getWidth();
        int h = imgsrc.getHeight();

        if (w != imgdst.getWidth() || h != imgdst.getHeight())
            imgdst = juce::Image{juce::Image::ARGB, w, h, false, juce::SoftwareImageType{}};

        juce::Image::BitmapData src{imgsrc, juce::Image::BitmapData::readOnly};
        juce::Image::BitmapData dst{imgdst, juce::Image::BitmapData::writeOnly};

        // Set alpha channel to 255 explicitly.
        for (int row = 0; row < h; ++row)
        {
            juce::uint8* from = src.getLinePointer(row);
            juce::uint8* to = dst.getLinePointer(row);
            for (int pix = 0; pix < w; ++pix)
            {
                juce::uint32 pixel = *reinterpret_cast<const juce::uint32*>(from);
                *reinterpret_cast<juce::uint32*>(to) = pixel | 0xFF000000;

                from += src.pixelStride;
                to += src.pixelStride;
            }
        }

        msg.m_asyncRepainter->m_hasBitmapChanged = true;
    }

    msg.m_asyncRepainter->triggerAsyncUpdate();
}

//------------------------------------------------------------------------------
std::unique_ptr<juce::PopupMenu> YsfxGraphicsView::Impl::createPopupMenu(const char *str)
{
    std::vector<std::unique_ptr<juce::PopupMenu>> chain;
    chain.reserve(8);
    chain.emplace_back(new juce::PopupMenu);

    ysfx_menu_u desc{ysfx_parse_menu(str)};
    if (!desc)
        return std::move(chain.front());

    for (uint32_t i = 0; i < desc->insn_count; ++i) {
        ysfx_menu_insn_t insn = desc->insns[i];
        switch (insn.opcode) {
        case ysfx_menu_item:
            chain.back()->addItem(
                (int)insn.id, juce::CharPointer_UTF8(insn.name),
                (insn.item_flags & ysfx_menu_item_disabled) == 0,
                (insn.item_flags & ysfx_menu_item_checked) != 0);
            break;
        case ysfx_menu_separator:
            chain.back()->addSeparator();
            break;
        case ysfx_menu_sub: {
            chain.emplace_back(new juce::PopupMenu);
            break;
        }
        case ysfx_menu_endsub:
            if (chain.size() <= 1)
                jassertfalse;
            else {
                std::unique_ptr<juce::PopupMenu> subm = std::move(chain.back());
                chain.pop_back();
                chain.back()->addSubMenu(
                    juce::CharPointer_UTF8(insn.name), std::move(*subm),
                    (insn.item_flags & ysfx_menu_item_disabled) == 0, nullptr,
                    (insn.item_flags & ysfx_menu_item_checked) != 0, 0);
            }
            break;
        }
    }

    return std::move(chain.front());
}

void YsfxGraphicsView::Impl::endPopupMenu(int menuResult)
{
    AsyncShowMenu *updater = m_asyncShowMenu.get();
    if (!updater)
        return;

    std::lock_guard<std::mutex> lock{updater->m_mutex};
    updater->m_completionFlag = true;
    updater->m_completionValue = menuResult;
    updater->m_completionVariable.notify_one();
}

//------------------------------------------------------------------------------
void YsfxGraphicsView::Impl::handleAsyncUpdate(better::AsyncUpdater *updater)
{
    if (updater == m_asyncRepainter.get()) {
        if (m_asyncRepainter->m_hasBitmapChanged)
            m_self->repaint();
        m_numWaitedRepaints -= 1;
    }
    else if (updater == m_asyncMouseCursor.get()) {
        AsyncMouseCursor &cursorUpdater = static_cast<AsyncMouseCursor &>(*updater);
        m_self->setMouseCursor(juce::MouseCursor{cursorUpdater.m_cursorType.load(std::memory_order_relaxed)});
    }
    else if (updater == m_asyncShowMenu.get()) {
        AsyncShowMenu &menuShower = static_cast<AsyncShowMenu &>(*updater);
        std::lock_guard<std::mutex> lock{menuShower.m_mutex};
        const char *menuDesc = menuShower.m_menuDesc.c_str();
        m_popupMenu = createPopupMenu(menuDesc);

        juce::Point<int> off = getDisplayOffset();
        juce::Point<int> position;
        position.x = juce::roundToInt(menuShower.m_menuX / m_gfxTarget->m_bitmapScale + off.x);
        position.y = juce::roundToInt(menuShower.m_menuY / m_gfxTarget->m_bitmapScale + off.y);
        juce::Point<int> screenPosition = m_self->localPointToGlobal(position);

        m_popupMenu->showMenuAsync(juce::PopupMenu::Options{}
            .withParentComponent(m_self)
            .withTargetScreenArea(juce::Rectangle<int>{screenPosition.x, screenPosition.y, 0, 0}),
            [this](int result) { endPopupMenu(result); });
    }
}
