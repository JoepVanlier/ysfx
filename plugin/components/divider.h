#include <juce_gui_extra/juce_gui_extra.h>


class Divider : public juce::Component
{
public:
    Divider(Component* parent): component(parent)
    {
        setRepaintsOnMouseActivity(true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }

    void mouseDown (const juce::MouseEvent& e [[maybe_unused]])
    {
        m_startPosition = m_position;
    }

    void mouseUp(const juce::MouseEvent& e)
    {
        if (e.getNumberOfClicks() > 1) {
            // Reset wasdragged so that this will snap back to default next layout update cycle.
            m_wasDragged = false;
            if (component) {
                component->resized();
            }
        };
    }

    void mouseDrag(const juce::MouseEvent& e){
        m_wasDragged = true;
        m_position = std::min(m_maximumHeight, std::max(m_minimumHeight, m_startPosition + e.getDistanceFromDragStartY()));
        if (component) {
            component->resized();
        }
    }

    void paint (juce::Graphics& g)
    {
        getLookAndFeel().drawStretchableLayoutResizerBar(g, getWidth(), getHeight(), true, isMouseOver(), isMouseButtonDown());
    }

    void setPosition(int position)
    {
        m_wasDragged = true;
        m_position = position;
    }

    void setSizes(int recommendedHeight, int minimumHeight, int maximumHeight) {
        m_minimumHeight = minimumHeight;
        m_maximumHeight = maximumHeight;

        if (!m_wasDragged) {
            m_position = recommendedHeight;
        }

        m_position = std::min(m_maximumHeight, std::max(m_minimumHeight, m_position));
    }

    void resetDragged() {
        m_wasDragged = false;
    }

    bool wasDragged() {
        return m_wasDragged;
    }

    int m_position{200};
    int m_startPosition{200};

private:
    juce::WeakReference<juce::Component> component;

    int m_maximumHeight{4096};
    int m_minimumHeight{200};
    bool m_wasDragged{false};  // Once it has been dragged once, it overrides what the plugin asks for
};
