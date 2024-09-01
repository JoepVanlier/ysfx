#pragma once
#include <set>
#include <map>
#include <vector>
#include <juce_gui_basics/juce_gui_basics.h>

/*
  An almost drop-in replacement for PopupMenu::showMenuAsync, that adds a "quick search" interface
  to the PopupMenu: when the PopupMenu is shown, any character typed on the keyboard will switch to
  the "quick search" view and start filtering the PopupMenu entries. Sub-menus are handled
  (everything is flattened into a giant list). The <tab> key switches back and forth between
  PopupMenu and QuickSearch view.

  The search is case-insensitive, and looks for multiple partial matches: for example "nelan"
  matches "Ned Flanders".

  Mouse and arrow keys can also be used while in quicksearch.

  Options are provided by the PopupMenuQuickSearchOptions, which is a subclass of
  PopupMenu::Options.

  If some entries are not relevant when in quick-search mode, or if some of them must be relabeled,
  the PopupMenuQuickSearchOptions::itemsToIgnoreOrRenameInQuickSearch option provides a way to
  achieve that.

  If some popup menu items are duplicated, an option is also available to merge them. Otherwise, it
  will try to disambiguate them by adding (if possible) the name of the parent sub-menu to the
  label.

  Limitations:

  - a target component has to be provided (via the PopupMenu::Options). It is used to avoid leaking
    memory (the PopupMenuQuickSearch structure is automatically destroyed if the targetComponent is
    deleted). A better solution would be to detect when PopupMenu::dismissAllActiveMenus is called
    but that is not possible.

    If you need to call PopupMenu::Options::withTargetScreenArea, call it *after*
    PopupMenu::Options::withTargetComponent (that way getTargetComponent return non-null, but
    getTargetScreenArea is the correct one).

  - the QuickSearchComponent is a desktop window, it does not have a juce parent (should be
    relativily easy to fix it to handle that case when PopupMenu::Options.getParentComponent is
    non-null).

  Hacks:

  - The way key press are intercepted from PopupMenu is not very elegant (registering a KeyListener
    to the currentlyModalWindow in a timer callback). But it seems to work very well.

  - Giving focus to the texteditor of QuickSearchComponent (grabKeyboardFocus in a timerCallback). I
    always have trouble with JUCE for this kind of stuff


  License: CC0 ( https://creativecommons.org/publicdomain/zero/1.0/ )

*/

    /* --------------------- interface --------------------- */

    struct PopupMenuQuickSearchOptions : public juce::PopupMenu::Options
    {
        bool startInQuickSearchMode = false;
        bool mergeEntriesWithSameLabel = false;
        int maxNumberOfMatchesDisplayed = 60;
        std::map<int, juce::String> itemsToIgnoreOrRenameInQuickSearch;

        PopupMenuQuickSearchOptions() {}
        PopupMenuQuickSearchOptions (juce::PopupMenu::Options opt) : juce::PopupMenu::Options (opt) {}
    };

    void showPopupMenuWithQuickSearch (const juce::PopupMenu& m, const PopupMenuQuickSearchOptions& options, std::function<void (int)> user_callback);

    /* --------------------- implementation --------------------- */

    namespace
    {
        /* this struct is automatically destroyed when the PopupMenu or QuickSearchComponent completes, or
 when the target component is destroyed */
      struct PopupMenuQuickSearch : public juce::KeyListener, public juce::Timer, public juce::ComponentListener,
                                    public juce::DeletedAtShutdown
        {
            class QuickSearchComponent : public juce::Component,
                                         public juce::Timer,
                                         public juce::TextEditor::Listener,
                                         public juce::KeyListener
            {
                juce::Label search_label;
                juce::TextEditor editor;
                PopupMenuQuickSearch* owner;

                // the hierarchy of submenus in the PopupMenu (used when disambiguating duplicated label)
                struct MenuTree
                {
                    juce::String label;
                    MenuTree* parent = nullptr;
                    std::list<MenuTree> childs;
                };
                MenuTree menu_tree;

                struct QuickSearchItem
                {
                    int id;
                    juce::String label;
                    juce::PopupMenu::Item* popup_menu_item = nullptr;
                    MenuTree* menu;
                };

                std::vector<QuickSearchItem> quick_search_items;
                std::vector<size_t> matches;
                int first_displayed_match = 0, highlighted_match = 0;

                int item_width = 0, item_height = 0;
                int displayed_over_or_under = 0; // -1 over, 1 under, 0 undecided

                juce::Colour text_colour;
                juce::Time creation_time;

                struct MenuItemComponent : public juce::Component
                {
                    QuickSearchItem e;
                    bool m_highlighted = false;
                    PopupMenuQuickSearch* m_owner;
                    MenuItemComponent (PopupMenuQuickSearch* owner) : m_owner (owner) {}
                    void paint (juce::Graphics& g) override
                    {
                        getLookAndFeel().drawPopupMenuItem (
                            g, getLocalBounds(), false /* isSeparator */, e.popup_menu_item->isEnabled /* isActive */, m_highlighted /* isHighlighted */, e.popup_menu_item->isTicked, false /* hasSubMenu */, e.label, e.popup_menu_item->shortcutKeyDescription, e.popup_menu_item->image.get(), e.popup_menu_item->colour.isTransparent() ? nullptr : &e.popup_menu_item->colour);
                    }
                    void updateWith (QuickSearchItem& new_e, bool new_highlighted)
                    {
                        if (new_e.popup_menu_item != e.popup_menu_item || m_highlighted != new_highlighted)
                        {
                            this->e = new_e;
                            this->m_highlighted = new_highlighted;
                            repaint();
                        }
                    }
                    void mouseUp (const juce::MouseEvent& event) override
                    {
                        if (! event.mouseWasDraggedSinceMouseDown())
                        {
                            if (e.popup_menu_item->isEnabled)
                                m_owner->quickSearchFinished (e.id);
                        }
                    }
                    void mouseExit (const juce::MouseEvent&) override
                    {
                        m_highlighted = false;
                        repaint();
                    }
                    void mouseEnter (const juce::MouseEvent&) override
                    {
                        m_highlighted = true;
                        repaint();
                    }
                };
                std::vector<std::unique_ptr<MenuItemComponent>> best_items;

            float m_scaleFactor{1.0f};

            public:
                QuickSearchComponent (PopupMenuQuickSearch* owner, juce::String& initial_string, float scale_factor) : owner (owner), m_scaleFactor(scale_factor)
                {
                    jassert(owner->target_component_weak_ref.get());
                    auto target_screen_area = getTargetScreenArea();
                    
                    setOpaque (true);

                    setWantsKeyboardFocus (false);
                    setMouseClickGrabsKeyboardFocus (false);
                    setAlwaysOnTop (true);

                    creation_time = juce::Time::getCurrentTime();
                    readPopupMenuItems (menu_tree, owner->menu);
                    handleDuplicatedLabels();

                    /* compute the width and item height */
                    juce::String longest_string;
                    for (auto& q : quick_search_items)
                    {
                        if (q.label.length() > longest_string.length())
                            longest_string = q.label;
                    }
                    getLookAndFeel().getIdealPopupMenuItemSize (longest_string, false /* isSeparator */, owner->options.getStandardItemHeight(), item_width, item_height);

                    if (item_width < target_screen_area.getWidth() && target_screen_area.getWidth() < 300)
                    {
                        // it is always nice to have a windows aligned with the target button
                        item_width = target_screen_area.getWidth();
                    }

                    // the actual height will be adjusted later
                    setBounds (target_screen_area.getX(), target_screen_area.getY(), item_width, item_height);

                    juce::Font font = getLookAndFeel().getPopupMenuFont();

                    // this is what does LookAndFeel::drawPopupMenuItem
                    if (font.getHeight() > (item_height - 2) / 1.3f)
                        font.setHeight ((item_height - 2) / 1.3f);


                    text_colour = getLookAndFeel().findColour (juce::PopupMenu::textColourId);

                    search_label.setText (TRANS ("Search:"), juce::dontSendNotification);
                    search_label.setColour (juce::Label::textColourId, text_colour.withAlpha (0.5f));
                    search_label.setFont (font);
                    search_label.setJustificationType (juce::Justification::bottomLeft);
                    search_label.setSize (search_label.getBorderSize().getLeftAndRight() + int (font.getStringWidth (search_label.getText())),
                                          item_height);
                    addAndMakeVisible (search_label);

                    editor.setBounds (search_label.getRight(), 0, item_width - search_label.getRight(), item_height);
                    editor.setFont (font);
                    editor.addListener (this);
                    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
                    editor.setColour (juce::TextEditor::textColourId, text_colour); // kTransparentBlack);
                    editor.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
                    editor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
                    editor.setColour (juce::CaretComponent::caretColourId, text_colour);
                    addAndMakeVisible (editor);

                    editor.setText (initial_string, juce::dontSendNotification);

                    editor.addKeyListener (this);
                    editor.setAccessible (false); // no need to hear voiceover spelling each letter..

                    // startTimer (100); // will grab the keyboard focus for the texteditor.

                    updateContent();
                }
                
                juce::Rectangle<int> getTargetScreenArea() {
                    auto target_screen_area = owner->options.getTargetScreenArea();
                    target_screen_area.setX((int) (target_screen_area.getX() / m_scaleFactor));
                    target_screen_area.setY((int) (target_screen_area.getY() / m_scaleFactor));
                    return target_screen_area;
                }

                float getDesktopScaleFactor() const override { return m_scaleFactor * juce::Desktop::getInstance().getGlobalScaleFactor(); }

                /* recursively parse the PopupMenu items */
                void readPopupMenuItems (MenuTree& tree, const juce::PopupMenu& menu)
                {
                    juce::PopupMenu::MenuItemIterator iter (menu);

                    while (iter.next())
                    {
                        auto& item = iter.getItem();
                        if (item.subMenu)
                        {
                            MenuTree child;
                            child.label = item.text;
                            child.parent = &tree;
                            tree.childs.push_back (child);
                            readPopupMenuItems (tree.childs.back(), *item.subMenu);
                        }
                        else if (item.itemID > 0)
                        {
                            QuickSearchItem q;
                            q.id = item.itemID;
                            q.label = item.text;
                            q.menu = &tree;
                            q.popup_menu_item = &item;
                            auto it = owner->options.itemsToIgnoreOrRenameInQuickSearch.find (q.id);
                            if (it != owner->options.itemsToIgnoreOrRenameInQuickSearch.end())
                            {
                                q.label = it->second; // the label is renamed, or just set to empty string if we want to
                                    // ignore this entry.
                            }
                            if (q.label.isEmpty())
                                continue; // ignore entries with no label
                            quick_search_items.push_back (q);
                        }
                    }
                }

                void handleDuplicatedLabels()
                {
                    if (! owner->options.mergeEntriesWithSameLabel)
                    {
                        // use name of parent menu to disambiguate duplicates
                        std::vector<MenuTree*> parents (quick_search_items.size());
                        for (size_t idx = 0; idx < quick_search_items.size(); ++idx)
                        {
                            parents[idx] = quick_search_items[idx].menu;
                        }

                        while (true)
                        {
                            bool done = true;
                            std::map<juce::int64, std::list<size_t>> hashes;
                            for (size_t idx = 0; idx < quick_search_items.size(); ++idx)
                            {
                                auto& q = quick_search_items[idx];
                                auto h = q.label.hashCode64();
                                hashes[h].push_back (idx);
                                if (hashes[h].size() > 1 && parents[idx] != nullptr)
                                {
                                    done = false;
                                }
                            }
                            if (done)
                                break;
                            else
                            {
                                bool changed_something = false;
                                for (auto hh : hashes)
                                {
                                    if (hh.second.size() > 1)
                                    {
                                        for (auto idx : hh.second)
                                        {
                                            auto& q = quick_search_items.at (idx);
                                            if (parents[idx] && parents[idx]->label.isNotEmpty())
                                            {
                                                q.label = parents[idx]->label + " / " + q.label;
                                                parents[idx] = parents[idx]->parent;
                                                changed_something = true;
                                            }
                                        }
                                    }
                                }
                                if (! changed_something)
                                    break;
                            }
                        }
                    }
                    else
                    {
                        // remove all duplicates
                        std::set<juce::int64> hashes;
                        std::vector<QuickSearchItem> v;
                        for (auto& q : quick_search_items)
                        {
                            auto h = q.label.hashCode64();
                            if (hashes.count (h) == 0)
                            {
                                hashes.insert (h);
                                v.push_back (q);
                            }
                        }
                        quick_search_items.swap (v);
                    }
                }

                // decide the orientation and dimensions of the QuickSearchComponent
                juce::Rectangle<int> getBestBounds (int total_h)
                {
                    auto tr = getTargetScreenArea();

                    auto screenArea =
                        juce::Desktop::getInstance().getDisplays().getDisplayForPoint (tr.getCentre())->userArea;

                    auto spaceUnder = screenArea.getBottom() - tr.getBottom();
                    auto spaceOver = tr.getY() - screenArea.getY();

                    if (spaceUnder >= 0.8 * spaceOver)
                    {
                        displayed_over_or_under = 1;
                    }
                    else
                    {
                        displayed_over_or_under = -1;
                    }

                    if (displayed_over_or_under == -1)
                    {
                        total_h = std::min (total_h, spaceOver);
                        return { tr.getX(), tr.getY() - total_h, getWidth(), total_h };
                    }
                    else
                    {
                        total_h = std::min (total_h, spaceUnder);
                        return { tr.getX(), tr.getBottom(), getWidth(), total_h };
                    }
                }

                void updateContent()
                {
                    updateMatches();

                    int nb_visible_matches =
                        std::min<int> (owner->options.maxNumberOfMatchesDisplayed, (int) matches.size());

                    int h = item_height;
                    jassert (h);

                    int separator_height = item_height / 2;

                    int max_total_h = h + std::max (1, nb_visible_matches) * h + separator_height;

                    auto rect = getBestBounds (max_total_h);

                    nb_visible_matches = std::min (nb_visible_matches,
                                                   std::max (1, ((rect.getHeight() - h - separator_height) / h)));

                    int total_h = h + separator_height + std::max (1, nb_visible_matches) * h;
                    setBounds (getBestBounds (total_h));
                    total_h = getHeight();

                    if (displayed_over_or_under == 1)
                    {
                        search_label.setTopLeftPosition (0, 0);
                        editor.setBounds (editor.getX(), 0, editor.getWidth(), h);
                    }
                    else
                    {
                        search_label.setTopLeftPosition (0, total_h - h);
                        editor.setBounds (editor.getX(), total_h - h, editor.getWidth(), h);
                    }

                    best_items.resize (nb_visible_matches);
                    for (size_t i = 0; i < nb_visible_matches; ++i)
                    {
                        if (best_items.at (i) == nullptr)
                        {
                            best_items[i] = std::make_unique<MenuItemComponent> (owner);
                            addAndMakeVisible (*best_items[i]);
                        }
                        size_t ii = first_displayed_match + i;
                        best_items[i]->updateWith (quick_search_items.at (matches.at (ii)), ii == highlighted_match);
                        if (displayed_over_or_under == 1)
                        {
                            best_items[i]->setBounds (0, (h + separator_height) + (int) i * h, item_width, h);
                        }
                        else
                        {
                            best_items[i]->setBounds (0, total_h - (h + separator_height) - (int) (i + 1) * h, item_width, h);
                        }
                    }

                    if (highlighted_match < nb_visible_matches)
                    {
#if JUCE_ACCESSIBILITY_FEATURES
#ifndef TARGET_WIN32 // ca marche meme qd narrator n'est pas active sous win. J'ai reporte le bug a JUCE
                        juce::AccessibilityHandler::postAnnouncement(quick_search_items.at (matches.at (highlighted_match)).label,
                                                                     juce::AccessibilityHandler::AnnouncementPriority::medium);
#endif
#endif
                    }
                }

                const int no_match_score = -1000000;

                // give a score for the match between searched string 'needle' and 'str'
                int evalMatchScore (juce::String str, const juce::String& needle)
                {
                    if (needle.isEmpty())
                        return 0;

                    int score = 0;
                    int old_best_j = -1;
                    for (int i = 0; i < needle.length();)
                    {
                        int best_j = -1, best_len = 0;
                        for (int j = 0; j < str.length(); ++j)
                        {
                            int len = 0;
                            while (i + len < needle.length() && j + len < str.length() && juce::CharacterFunctions::compareIgnoreCase (str[j + len], needle[i + len]) == 0)
                            {
                                ++len;
                            }
                            if (len > best_len)
                            {
                                best_len = len;
                                best_j = j;
                            }
                        }
                        if (best_len == 0)
                            return no_match_score; // char not found ! no need to continue
                        score +=
                            (best_len == 1 ? 1 : (best_len * best_len + 1)); // boost for matches of more that one char
                        if ((best_len == 1 && str[best_j] != ' ') || best_j < old_best_j)
                        {
                            score -= 100;
                        }
                        str = str.replaceSection (
                            best_j, best_len,
                            "\t"); // mark these characters are 'done' , so that 'xxxx' does not match 'x'
                        old_best_j = best_j;
                        i += std::max (1, best_len);
                    }
                    return score;
                }

                // get the the list of best matches, sorted by decreasing score
                void updateMatches()
                {
                    juce::String needle = editor.getText();
                    auto old_matches = matches;
                    matches.resize (0);

                    std::vector<int> scores (quick_search_items.size(), 0);

                    for (size_t idx = 0; idx < quick_search_items.size(); ++idx)
                    {
                        auto& q = quick_search_items[idx];

                        scores[idx] = evalMatchScore (q.label, needle);
                        if (! q.popup_menu_item->isEnabled)
                            scores[idx] -= 10000;
                        matches.push_back (idx);
                    }
                    std::stable_sort (matches.begin(), matches.end(), [&scores] (size_t a, size_t b) { return scores[a] > scores[b]; });
                    int threshold = (! matches.empty() && scores[matches.front()] > 0 ? 0 : no_match_score);
                    while (! matches.empty() && scores[matches.back()] <= threshold)
                        matches.pop_back();

                    if (matches != old_matches)
                    {
                        first_displayed_match = highlighted_match = 0;
                    }
                }

                void paint (juce::Graphics& g) override
                {
                    getLookAndFeel().drawPopupMenuBackground (g, getWidth(), getHeight());
                    g.setColour (text_colour.withAlpha (0.4f));

                    int y_separator = item_height + item_height / 4;
                    if (displayed_over_or_under == -1)
                        y_separator = getHeight() - y_separator;
                    g.drawHorizontalLine (y_separator, (float) item_height / 2, getWidth() - (float) item_height / 2);
                    if (matches.empty())
                    {
                        g.setFont (search_label.getFont());
                        g.setColour (text_colour.withAlpha (0.5f));
                        int y0 = (displayed_over_or_under == 1 ? (getHeight() - item_height) : 0);
                        g.drawText (TRANS ("(no match)"), juce::Rectangle<int> (0, y0, item_width, item_height), juce::Justification::centred);
                    }
                }

                void textEditorReturnKeyPressed (juce::TextEditor&) override
                {
                    if (! matches.empty())
                    {
                        auto& q = quick_search_items.at (matches.at (highlighted_match));
                        if (q.popup_menu_item->isEnabled)
                        {
                            owner->quickSearchFinished (q.id);
                        }
                    }
                }

                void textEditorEscapeKeyPressed (juce::TextEditor&) override { owner->quickSearchFinished (0); }

                void textEditorTextChanged (juce::TextEditor&) override { updateContent(); }

                // just to silence a clang warning about the other overriden keyPressed member function
                bool keyPressed (const juce::KeyPress& key) override { return juce::Component::keyPressed (key); }

                // capture KeyPress events of the TextEditor
                bool keyPressed (const juce::KeyPress& key, juce::Component*) override
                {
                    if (key == '\t')
                    {
                        // async because it will destroy the QuickSearchComponent and this is causing issues if done in the keyPressed callback
                        juce::MessageManager::getInstance()->callAsync([this, ref=juce::WeakReference<Component>(this)]() { if (ref) { owner->showPopupMenu(); } });
                    }

                    bool up = (key == juce::KeyPress::upKey), down = (key == juce::KeyPress::downKey);
                    if (displayed_over_or_under == -1)
                        std::swap (up, down);
                    if (up)
                    {
                        if (highlighted_match > 0)
                        {
                            --highlighted_match;
                            if (first_displayed_match > highlighted_match)
                                first_displayed_match = highlighted_match;
                            updateContent();
                        }
                        return true;
                    }
                    if (down)
                    {
                        if (highlighted_match + 1 < (int) matches.size())
                        {
                            ++highlighted_match;
                            if (highlighted_match - first_displayed_match >= (int) best_items.size())
                            {
                                first_displayed_match = highlighted_match - (int) best_items.size() + 1;
                                jassert (first_displayed_match >= 0);
                            }
                            auto& q = quick_search_items.at (matches.at (highlighted_match));
                            if (! q.popup_menu_item->isEnabled)
                                highlighted_match = 0;
                            updateContent();
                        }
                        return true;
                    }
                    return false;
                }

                // I always have some trouble to manage keyboard focus with JUCE.
                void timerCallback() override
                {
                    editor.grabKeyboardFocus();
                }

                void inputAttemptWhenModal() override
                {
                    // I get sometimes spurious calls to inputAttemptWhenModal when switching from PopupMenu to
                    // QuickSearchComponent, just after the creation of QuickSearchComponent, so that's why there
                    // is a delay here.
                    double dt = (juce::Time::getCurrentTime() - creation_time).inSeconds();
                    if (dt > 0.2)
                    {
                        owner->quickSearchFinished (0);
                    }
                }

            }; // end of QuickSearchComponent

            juce::PopupMenu menu;
            PopupMenuQuickSearchOptions options;

            std::unique_ptr<QuickSearchComponent> quick_search;
            juce::WeakReference<juce::Component> target_component_weak_ref;

            std::function<void (int)> user_callback;
            juce::String key_pressed_while_menu;
            bool is_finishing = false;

            // just for security, keep a list all the component that we have added key listeners
            std::list<juce::WeakReference<juce::Component>> listened_components;

            void showPopupMenu()
            {
                if (quick_search)
                {
                    quick_search = nullptr; // destroy the quick_search component
                    key_pressed_while_menu = "";
                }
                menu.showMenuAsync (options, [this] (int result) { popupMenuFinished (result); });
                startTimer (20); // follow the current modal component and intercepts keyPressed events
            }

            void showQuickSearch()
            {
                if (quick_search == nullptr && target_component_weak_ref)
                {
                    float scale_factor = juce::Component::getApproximateScaleFactorForComponent(options.getTargetComponent());
                    quick_search = std::make_unique<QuickSearchComponent> (this, key_pressed_while_menu, scale_factor);

                    juce::PopupMenu::dismissAllActiveMenus(); // user_callback won't run, since quick_search != 0

                    quick_search->setAlwaysOnTop (true);
                    quick_search->setVisible (true);
                    quick_search->addToDesktop(juce::ComponentPeer::windowIsTemporary);

                    // quick_search->toFront(true /* shouldAlsoGainFocus */);
                    quick_search->enterModalState (true);
                }
            }

            // called by PopupMenu::showMenuAsync it has completed
            void popupMenuFinished (int result)
            {
                if (! quick_search)
                {
                    is_finishing = true; // stop stealing keypress when running user_callback
                    if (target_component_weak_ref.get())
                    {
                        user_callback (result);
                    }
                    delete this;
                }
                else
                {
                    // ignore it, showing quick search just destroyed the menu
                }
            }

            // called by QuickSearchComponent when it has completed
            void quickSearchFinished (int result)
            {
                if (quick_search)
                {
                    is_finishing = true; // stop stealing keypress when running user_callback
                    quick_search = nullptr;
                    if (target_component_weak_ref.get())
                    {
                        user_callback (result);
                    }
                    delete this; // delete only when quick_search is active, otherwise this is deleted by the
                        // popupmenu callback
                }
            }

            // avoid leaks as quick search does not have many ways to know when to close
            void componentBeingDeleted (juce::Component&) override { quickSearchFinished (true); }

            // hijack the KeyPress received by the PopupMenu
            bool keyPressed (const juce::KeyPress& key, juce::Component* /*originatingComponent*/) override
            {
                if (is_finishing) return false;
                if (menu.getNumItems() < 2) return false;
                
                auto c = key.getTextCharacter();
                if (c > ' ' || c == '\t')
                {
                    if (c != '\t')
                        key_pressed_while_menu += c;
                    showQuickSearch();
                    if (quick_search)
                        return true;
                }
                return false;
            }

            void timerCallback() override
            {
                if (! quick_search)
                {
                    auto c = juce::Component::getCurrentlyModalComponent();

                    if (! c)
                        return;

                    // check if already listened to, and remove deleted components
                    for (auto it = listened_components.begin(); it != listened_components.end();)
                    {
                        auto itn = it;
                        ++itn;
                        if (it->get() == nullptr)
                            listened_components.erase (it);
                        else if (it->get() == c)
                            return; // this comp is alreadly listened to
                        it = itn;
                    }

                    listened_components.push_back (c);
                    c->addKeyListener (this);
                }
            }

            PopupMenuQuickSearch (const juce::PopupMenu& menu_, const PopupMenuQuickSearchOptions& options_, std::function<void (int)> user_callback_)
            {
                menu = menu_;
                options = options_;
                user_callback = user_callback_;
                target_component_weak_ref = options.getTargetComponent();

                // for managing the lifetime of PopupMenuQuickSearch we require that a target_component is provided
                jassert (target_component_weak_ref.get());
                target_component_weak_ref->addComponentListener (this);

                if (! options.startInQuickSearchMode)
                {
                    showPopupMenu();
                }
                else
                {
                    showQuickSearch();
                }
            }

            ~PopupMenuQuickSearch()
            {
                if (target_component_weak_ref)
                {
                    target_component_weak_ref->removeComponentListener (this);
                }

                for (auto c : listened_components)
                {
                    if (c.get())
                    {
                        c->removeKeyListener (this);
                    }
                }
            }
            JUCE_LEAK_DETECTOR (PopupMenuQuickSearch)
        };

    } // namespace

    inline void showPopupMenuWithQuickSearch (const juce::PopupMenu& m,
                                              const PopupMenuQuickSearchOptions& options,
                                              std::function<void (int)> user_callback)
    {
        new PopupMenuQuickSearch (m, options, user_callback);
    }
