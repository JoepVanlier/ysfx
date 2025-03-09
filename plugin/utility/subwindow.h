#include <map>
#include <juce_gui_extra/juce_gui_extra.h>
#include "utility/functional_timer.h"


class SubWindow : public juce::DocumentWindow {
    public:
        using juce::DocumentWindow::DocumentWindow;
        SubWindow(const juce::String& name, juce::Colour backgroundColour, int requiredButtons, bool addToDesktop=true, std::function<void()> frontCallback=[](){}): DocumentWindow (name, backgroundColour, requiredButtons, addToDesktop), m_frontCallback(frontCallback)
        {
            #ifdef JUCE_MAC
                juce::Timer *timer = FunctionalTimer::create(
                    [this]() {
                        if (juce::Process::isForegroundProcess()) {
                            if (isVisible() && !isAlwaysOnTop()) {
                                setAlwaysOnTop(true);
                            }
                        } else {
                            if (isAlwaysOnTop()) {
                                setAlwaysOnTop(false);
                            }
                        }
                    }
                );
                m_stayOnTopTimer.reset(timer);
                m_stayOnTopTimer->startTimer(50);
            #endif
        }

    private:
        void broughtToFront() override {
            m_frontCallback();
        }
        std::function<void()> m_frontCallback;
        #ifdef JUCE_MAC
            std::unique_ptr<juce::Timer> m_stayOnTopTimer;
        #endif

    protected:
        void closeButtonPressed() override { setVisible(false); }
    };


class SubWindowMgr : public juce::FocusChangeListener {
    public:
        SubWindowMgr(juce::Component* parentWindow): m_parentWindow(parentWindow) {
            juce::Desktop::getInstance().addFocusChangeListener(this);
        }

        ~SubWindowMgr(void) {
            juce::Desktop::getInstance().removeFocusChangeListener(this);
        }

        void globalFocusChanged(juce::Component* _) override {
            (void) _;
            updateOrder();
        }

        void createOrBringForward(juce::String windowName, juce::Component* content, juce::Colour backgroundColour) {
            auto it = windows.find(windowName);
            if (it == windows.end()) {
                auto bringToFront = [this, windowName]() { toFront(windowName); };
                auto window = std::make_unique<SubWindow>(windowName, backgroundColour, juce::DocumentWindow::allButtons, true, bringToFront);
                window->setResizable(true, false);
                window->setContentNonOwned(content, true);
            } else {
                toFront(windowName);
            }
        
            windows[windowName]->setVisible(true);
            windows[windowName]->toFront(true);
        }

        void toFront(juce::String windowName) {
            if (!m_blockUpdates) {
                std::unique_lock<std::mutex> lock(m_mutex);
                auto it = find(order.begin(), order.end(), windowName);

                if(it != order.end()) order.erase(it);
                order.push_back(windowName);
            }
        }

        void updateOrder() {
            if (order.size() < 2) return;  // No re-ordering required
            std::unique_lock<std::mutex> lock(m_mutex);

            m_blockUpdates = true;
            for (auto it : order) {
                if (it != "_main") {
                    windows[it]->toFront(false);
                } else {
                    m_parentWindow->toFront(false);
                }
            }
            m_blockUpdates = false;
        }

    private:
        juce::Component* m_parentWindow;
        bool m_blockUpdates{false};

    protected:
        std::map<juce::String, std::unique_ptr<SubWindow>> windows;
        std::deque<juce::String> order = {"_main"};
        std::mutex m_mutex;
};
