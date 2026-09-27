#include "ui/views/ConfigManagerView.h"
#include "services/FolderConfig.h"
#include "storage/FileIO.h"
#include "ui/ShellIcons.h"

namespace pc::ui
{

using namespace theme;

namespace
{
// Colours of the Config Manager (Windows list view and group headers).
const juce::Colour groupText { 0xff1e3287 };
const juce::Colour groupLine { 0xffe2e5ea };
const juce::Colour itemSelected { 0xffcce8ff };
const juce::Colour itemSelectedBorder { 0xff99d1ff };
const juce::Colour itemHover { 0xffe5f3ff };

void closeDialog (juce::Component* inside)
{
    if (auto* w = inside->findParentComponentOfClass<juce::DialogWindow>())
        w->exitModalState (0);
}

void openDialog (juce::Component* content, const juce::String& title, juce::Component* around)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (content);
    o.dialogTitle = title;
    o.dialogBackgroundColour = colours::panel;
    o.componentToCentreAround = around;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.escapeKeyTriggersCloseButton = true;
    o.launchAsync();
}

// "Nova pasta": type of the folder to add, with its description.
struct NewFolderDialog : public juce::Component
{
    explicit NewFolderDialog (std::function<void (FolderKind)> chosen) : onChosen (std::move (chosen))
    {
        addAndMakeVisible (question);
        question.setText (L"Qual o tipo de pasta você deseja adicionar?", juce::dontSendNotification);
        styleLabel (question, 14.0f);
        addAndMakeVisible (types);
        int id = 1;
        for (auto& t : folderTypes())
            types.addItem (juce::String::fromUTF8 (t.name), id++);
        types.onChange = [this] { describe(); };
        addAndMakeVisible (ok);
        makePrimary (ok);
        ok.onClick = [this] {
            auto index = types.getSelectedId() - 1;
            auto callback = onChosen;
            closeDialog (this);
            if (index >= 0 && callback)
                callback (folderTypes()[(size_t) index].kind);
        };
        addAndMakeVisible (description);
        styleReadOnlyText (description);
        types.setSelectedId (1, juce::dontSendNotification);
        describe();
        setSize (360, 300);
    }

    void describe()
    {
        auto index = types.getSelectedId() - 1;
        description.setText (index >= 0 ? juce::String::fromUTF8 (folderTypes()[(size_t) index].description) : juce::String());
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        question.setBounds (r.removeFromTop (22));
        r.removeFromTop (4);
        types.setBounds (r.removeFromTop (30));
        r.removeFromTop (10);
        ok.setBounds (r.removeFromTop (32).removeFromRight (100));
        r.removeFromTop (10);
        description.setBounds (r);
    }

    std::function<void (FolderKind)> onChosen;
    juce::Label question;
    juce::ComboBox types;
    juce::TextButton ok { "Ok" };
    juce::TextEditor description;
};

// "Selecione o ícone": icon libraries of pgm\Icones and their icons.
struct IconPickerDialog : public juce::Component
{
    struct Icons : public juce::Component
    {
        explicit Icons (IconPickerDialog& o) : owner (o) {}

        static constexpr int cell = 64;

        int columns() const { return juce::jmax (1, getWidth() / cell); }

        void layout (int width)
        {
            auto n = ShellIcons::instance().count (owner.file);
            auto cols = juce::jmax (1, width / cell);
            setSize (width, juce::jmax (cell, ((n + cols - 1) / cols) * cell));
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::white);
            auto n = ShellIcons::instance().count (owner.file);
            auto clip = g.getClipBounds();
            for (int i = 0; i < n; ++i)
            {
                auto r = juce::Rectangle<int> ((i % columns()) * cell, (i / columns()) * cell, cell, cell);
                if (! r.intersects (clip))
                    continue;
                if (i == owner.index)
                {
                    g.setColour (itemSelected);
                    g.fillRect (r.reduced (3));
                    g.setColour (itemSelectedBorder);
                    g.drawRect (r.reduced (3));
                }
                auto image = ShellIcons::instance().get (owner.file, i, 48);
                g.drawImageWithin (image, r.getX() + 8, r.getY() + 8, 48, 48, juce::RectanglePlacement::centred);
            }
        }

        int indexAt (juce::Point<int> p) const
        {
            auto i = (p.y / cell) * columns() + p.x / cell;
            return p.x < columns() * cell && i < ShellIcons::instance().count (owner.file) ? i : -1;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto i = indexAt (e.getPosition());
            if (i >= 0)
            {
                owner.index = i;
                repaint();
            }
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            if (indexAt (e.getPosition()) >= 0)
                owner.ok.triggerClick();
        }

        IconPickerDialog& owner;
    };

    IconPickerDialog (const juce::File& iconsFolder, const juce::String& currentFile, int currentIndex,
                      std::function<void (const juce::String&, int)> chosen)
        : onChosen (std::move (chosen)), grid (*this)
    {
        // As in the Config Manager: icon libraries first, then .ico files.
        auto dlls = iconsFolder.findChildFiles (juce::File::findFiles, false, "*.dll");
        auto icos = iconsFolder.findChildFiles (juce::File::findFiles, false, "*.ico");
        auto byName = [] (const juce::File& a, const juce::File& b) { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; };
        std::sort (dlls.begin(), dlls.end(), byName);
        std::sort (icos.begin(), icos.end(), byName);
        for (auto& f : dlls)
            files.add (f.getFullPathName());
        for (auto& f : icos)
            files.add (f.getFullPathName());
        if (currentFile.isNotEmpty() && ! files.contains (currentFile, true))
            files.add (currentFile);

        addAndMakeVisible (fileBox);
        for (int i = 0; i < files.size(); ++i)
            fileBox.addItem (files[i], i + 1);
        fileBox.onChange = [this] {
            file = files[fileBox.getSelectedId() - 1];
            index = -1;
            resized();
            grid.repaint();
        };
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&grid, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (ok);
        makePrimary (ok);
        ok.onClick = [this] {
            auto f = file;
            auto i = index;
            auto callback = onChosen;
            closeDialog (this);
            if (i >= 0 && callback)
                callback (f, i);
        };
        auto current = juce::jmax (0, files.indexOf (currentFile, true));
        file = files[current];
        index = files[current].equalsIgnoreCase (currentFile) ? currentIndex : -1;
        fileBox.setSelectedId (current + 1, juce::dontSendNotification);
        setSize (440, 380);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        fileBox.setBounds (r.removeFromTop (28));
        r.removeFromTop (8);
        ok.setBounds (r.removeFromBottom (32).removeFromRight (100));
        r.removeFromBottom (10);
        viewport.setBounds (r);
        grid.layout (viewport.getMaximumVisibleWidth());
        if (index >= 0)
            viewport.setViewPosition (0, juce::jmax (0, (index / grid.columns()) * Icons::cell - Icons::cell));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
        g.setColour (colours::border);
        g.drawRect (viewport.getBounds().expanded (1));
    }

    std::function<void (const juce::String&, int)> onChosen;
    juce::StringArray files;
    juce::String file;
    int index = -1;
    juce::ComboBox fileBox;
    juce::Viewport viewport;
    Icons grid;
    juce::TextButton ok { "Ok" };
};
} // namespace

// Toolbar button with the pictures of the Config Manager toolbar.
struct ConfigManagerView::ToolButton : public juce::Button
{
    enum class Kind
    {
        save,
        add,
        remove
    };

    ToolButton (Kind k, const juce::String& tip) : juce::Button (tip), kind (k)
    {
        setTooltip (tip);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        if ((over || down) && isEnabled())
        {
            g.setColour (down ? itemSelected : itemHover);
            g.fillRoundedRectangle (r, 5.0f);
            g.setColour (itemSelectedBorder);
            g.drawRoundedRectangle (r, 5.0f, 1.0f);
        }
        auto a = r.withSizeKeepingCentre (24.0f, 24.0f);
        g.setOpacity (isEnabled() ? 1.0f : 0.35f);
        switch (kind)
        {
            case Kind::save:
            {
                juce::Path body;
                body.startNewSubPath (a.getX() + 1, a.getY() + 1);
                body.lineTo (a.getRight() - 5, a.getY() + 1);
                body.lineTo (a.getRight() - 1, a.getY() + 5);
                body.lineTo (a.getRight() - 1, a.getBottom() - 1);
                body.lineTo (a.getX() + 1, a.getBottom() - 1);
                body.closeSubPath();
                g.setColour (juce::Colour (0xff2c64c8).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.fillPath (body);
                g.setColour (juce::Colour (0xff17428f).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.strokePath (body, juce::PathStrokeType (1.0f));
                g.setColour (juce::Colours::white.withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.fillRect (juce::Rectangle<float> (a.getX() + 5, a.getY() + 12, a.getWidth() - 10, a.getHeight() - 14));
                g.setColour (juce::Colour (0xffc9d6ec).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.fillRect (juce::Rectangle<float> (a.getX() + 6, a.getY() + 2, a.getWidth() - 14, 7));
                g.setColour (juce::Colour (0xff17428f).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.fillRect (juce::Rectangle<float> (a.getRight() - 12, a.getY() + 3, 3, 5));
                break;
            }
            case Kind::add:
            {
                juce::Path folder;
                folder.startNewSubPath (a.getX() + 1, a.getY() + 5);
                folder.lineTo (a.getX() + 9, a.getY() + 5);
                folder.lineTo (a.getX() + 11, a.getY() + 8);
                folder.lineTo (a.getRight() - 2, a.getY() + 8);
                folder.lineTo (a.getRight() - 2, a.getBottom() - 3);
                folder.lineTo (a.getX() + 1, a.getBottom() - 3);
                folder.closeSubPath();
                g.setColour (juce::Colour (0xfff0c24b).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.fillPath (folder);
                g.setColour (juce::Colour (0xffc28f1a).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.strokePath (folder, juce::PathStrokeType (1.0f));
                auto plus = juce::Rectangle<float> (a.getRight() - 12, a.getBottom() - 12, 12, 12);
                g.setColour (juce::Colour (0xff35a845).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.fillEllipse (plus);
                g.setColour (juce::Colours::white.withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.fillRect (plus.withSizeKeepingCentre (7.0f, 2.0f));
                g.fillRect (plus.withSizeKeepingCentre (2.0f, 7.0f));
                break;
            }
            case Kind::remove:
            {
                juce::Path x;
                x.startNewSubPath (a.getX() + 3, a.getY() + 3);
                x.lineTo (a.getRight() - 3, a.getBottom() - 3);
                x.startNewSubPath (a.getRight() - 3, a.getY() + 3);
                x.lineTo (a.getX() + 3, a.getBottom() - 3);
                g.setColour (juce::Colour (0xffe03131).withMultipliedAlpha (isEnabled() ? 1.0f : 0.35f));
                g.strokePath (x, juce::PathStrokeType (4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                break;
            }
        }
    }

    Kind kind;
};

// The 64 x 64 button with the icon of the selected folder.
struct ConfigManagerView::IconButton : public juce::Button
{
    IconButton() : juce::Button (L"Ícone")
    {
        setTooltip (L"Clique para escolher o ícone da pasta");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (over && isEnabled() ? itemHover : juce::Colour (0xfff7f8fa));
        g.fillRect (r);
        g.setColour (over && isEnabled() ? itemSelectedBorder : colours::border);
        g.drawRect (r);
        if (image.isValid())
            g.drawImageWithin (image, 8, 8, getWidth() - 16, getHeight() - 16, juce::RectanglePlacement::centred);
    }

    juce::Image image;
};

// Folders as big icons in groups, like the list of the Config Manager.
struct ConfigManagerView::Grid : public juce::Component
{
    explicit Grid (ConfigManagerView& o) : owner (o) { setWantsKeyboardFocus (true); }

    static constexpr int cellW = 82, cellH = 70, headerH = 28;

    struct Item
    {
        int folder;
        juce::Rectangle<int> area;
    };
    struct Group
    {
        juce::String name;
        juce::Rectangle<int> header;
    };

    void layout (int width, int minHeight)
    {
        items.clear();
        groups.clear();
        auto cols = juce::jmax (1, (width - 12) / cellW);
        int y = 4;
        for (int group = 0; group < 4; ++group)
        {
            std::vector<int> members;
            for (int i = 0; i < (int) owner.folders_.size(); ++i)
                if (folderTypeInfo (owner.folders_[(size_t) i].kind).group == group)
                    members.push_back (i);
            if (members.empty())
                continue;
            groups.push_back ({ folderGroupName (group), { 8, y, width - 16, headerH } });
            y += headerH;
            for (size_t k = 0; k < members.size(); ++k)
                items.push_back ({ members[k], { 8 + (int) (k % (size_t) cols) * cellW, y + (int) (k / (size_t) cols) * cellH, cellW, cellH } });
            y += (int) ((members.size() + (size_t) cols - 1) / (size_t) cols) * cellH + 6;
        }
        setSize (width, juce::jmax (y + 4, minHeight));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::white);
        for (auto& gr : groups)
        {
            g.setColour (groupText);
            g.setFont (font (13.5f));
            auto w = (int) juce::GlyphArrangement::getStringWidth (font (13.5f), gr.name) + 6;
            g.drawText (gr.name, gr.header.withWidth (w).translated (4, 0), juce::Justification::centredLeft, false);
            g.setColour (groupLine);
            g.drawHorizontalLine (gr.header.getCentreY(), (float) gr.header.getX() + (float) w + 10.0f, (float) gr.header.getRight());
        }
        for (auto& item : items)
        {
            auto& f = owner.folders_[(size_t) item.folder];
            auto cell = item.area.reduced (3);
            if (item.folder == owner.selected_)
            {
                g.setColour (itemSelected);
                g.fillRect (cell);
                g.setColour (itemSelectedBorder);
                g.drawRect (cell);
            }
            else if (item.folder == hover)
            {
                g.setColour (itemHover);
                g.fillRect (cell);
            }
            auto image = ShellIcons::instance().get (f.iconLocation, f.iconIndex, 32);
            auto iconArea = juce::Rectangle<int> (cell.getCentreX() - 16, cell.getY() + 4, 32, 32);
            if (image.isValid())
                g.drawImageWithin (image, iconArea.getX(), iconArea.getY(), 32, 32, juce::RectanglePlacement::centred);
            else
            {
                g.setColour (colours::border);
                g.drawRect (iconArea.reduced (4));
            }
            g.setColour (item.folder == owner.selected_ ? juce::Colour (0xff1552b8) : colours::text);
            g.setFont (font (12.5f));
            g.drawFittedText (f.title, cell.withTrimmedTop (38).reduced (2, 0), juce::Justification::centredTop, 2, 1.0f);
        }
        if (items.empty())
        {
            g.setColour (colours::textMuted);
            g.setFont (font (13.0f));
            g.drawText (L"Nenhuma pasta configurada. Use o botão Adicionar.", getLocalBounds().reduced (16), juce::Justification::centredTop, true);
        }
    }

    int folderAt (juce::Point<int> p) const
    {
        for (auto& item : items)
            if (item.area.reduced (3).contains (p))
                return item.folder;
        return -1;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto h = folderAt (e.getPosition());
        if (h != hover)
        {
            hover = h;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = -1;
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        auto f = folderAt (e.getPosition());
        if (f >= 0)
            owner.select (f);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() == juce::KeyPress::deleteKey)
        {
            owner.removeFolder();
            return true;
        }
        return false;
    }

    ConfigManagerView& owner;
    std::vector<Item> items;
    std::vector<Group> groups;
    int hover = -1;
};

// Steps of the save, over the view while it runs.
struct ConfigManagerView::Progress : public juce::Component
{
    Progress()
    {
        addAndMakeVisible (close);
        makePrimary (close);
        close.onClick = [this] { setVisible (false); };
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black.withAlpha (0.25f));
        auto card = box().toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (card, 10.0f);
        g.setColour (colours::border);
        g.drawRoundedRectangle (card, 10.0f, 1.0f);
        auto r = box().reduced (22, 18);
        g.setColour (colours::text);
        g.setFont (font (16.0f, true));
        g.drawText (L"Gravando as pastas", r.removeFromTop (26), juce::Justification::centredLeft, false);
        r.removeFromTop (8);
        for (auto& s : steps)
        {
            auto line = r.removeFromTop (24);
            auto icon = line.removeFromLeft (26).toFloat().withSizeKeepingCentre (16, 16);
            switch (s.state)
            {
                case JobStep::State::done:    drawStatusIcon (g, icon, Severity::info, true); break;
                case JobStep::State::failed:  drawStatusIcon (g, icon, Severity::error, false); break;
                case JobStep::State::running:
                    g.setColour (colours::brandLight);
                    g.fillEllipse (icon.reduced (3.0f));
                    break;
                default:
                    g.setColour (colours::border.darker (0.1f));
                    g.drawEllipse (icon.reduced (2.0f), 1.5f);
                    break;
            }
            bool dim = s.state == JobStep::State::pending || s.state == JobStep::State::skipped;
            g.setColour (dim ? colours::textMuted : colours::text);
            g.setFont (font (13.5f, s.state == JobStep::State::running));
            g.drawText (s.title, line, juce::Justification::centredLeft, true);
            if (s.detail.isNotEmpty())
            {
                g.setColour (s.state == JobStep::State::failed ? colours::error : colours::textMuted);
                g.setFont (font (12.0f));
                g.drawFittedText (s.detail, r.removeFromTop (34).withTrimmedLeft (26), juce::Justification::topLeft, 2, 1.0f);
            }
        }
        if (finished)
        {
            r.removeFromTop (6);
            g.setColour (succeeded ? colours::ok : colours::error);
            g.setFont (font (14.0f, true));
            g.drawText (outcome, r.removeFromTop (24), juce::Justification::centredLeft, true);
        }
    }

    juce::Rectangle<int> box() const { return getLocalBounds().withSizeKeepingCentre (juce::jmin (620, getWidth() - 40), 360); }

    void resized() override
    {
        close.setBounds (box().reduced (22, 18).removeFromBottom (32).removeFromRight (110));
    }

    void update (const std::vector<JobStep>& s, bool done, bool ok, const juce::String& text)
    {
        steps = s;
        finished = done;
        succeeded = ok;
        outcome = text;
        close.setVisible (done);
        repaint();
    }

    std::vector<JobStep> steps;
    bool finished = false, succeeded = false;
    juce::String outcome;
    juce::TextButton close { "Fechar" };
};

void ConfigManagerView::captureDialogs (const juce::File& pgm, const juce::String& iconFile, int iconIndex, const juce::File& folder)
{
    auto save = [&folder] (juce::Component& c, const juce::String& name) {
        auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
        auto file = folder.getChildFile (name);
        file.deleteFile();
        juce::FileOutputStream stream (file);
        juce::PNGImageFormat().writeImageToStream (image, stream);
    };
    NewFolderDialog added ([] (FolderKind) {});
    added.types.setSelectedId (7, juce::dontSendNotification);
    added.describe();
    save (added, "07c-nova-pasta.png");
    IconPickerDialog picker (pgm.getChildFile ("Icones"), iconFile, iconIndex, [] (const juce::String&, int) {});
    picker.resized();
    save (picker, "07d-icone.png");
}

ConfigManagerView::ConfigManagerView (AppContext& context) : View (context)
{
    saveButton_ = std::make_unique<ToolButton> (ToolButton::Kind::save, "Salvar");
    addButton_ = std::make_unique<ToolButton> (ToolButton::Kind::add, "Adicionar");
    removeButton_ = std::make_unique<ToolButton> (ToolButton::Kind::remove, "Excluir");
    for (auto* b : { saveButton_.get(), addButton_.get(), removeButton_.get() })
        addAndMakeVisible (*b);
    saveButton_->onClick = [this] { save(); };
    addButton_->onClick = [this] { addFolder(); };
    removeButton_->onClick = [this] { removeFolder(); };

    grid_ = std::make_unique<Grid> (*this);
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (grid_.get(), false);
    viewport_.setScrollBarsShown (true, false);

    addAndMakeVisible (propertiesTitle_);
    propertiesTitle_.setText (L"Propriedades da pasta selecionada", juce::dontSendNotification);
    styleLabel (propertiesTitle_, 14.0f, true);
    auto label = [this] (juce::Label& l, const juce::String& text) {
        addAndMakeVisible (l);
        l.setText (text, juce::dontSendNotification);
        styleLabel (l, 13.0f);
    };
    label (titleLabel_, L"Título");
    label (typeLabel_, "Tipo");
    label (dirLabel_, L"Diretório");
    label (codeLabel_, "Registrar");
    label (commandLabel_, "Linha de comando");
    styleLabel (commandLabel_, 13.0f, true);
    addAndMakeVisible (status_);
    styleLabel (status_, 12.5f, false, colours::textMuted);

    for (auto* e : { &title_, &directory_, &code_, &command_ })
    {
        addAndMakeVisible (*e);
        e->setFont (font (14.0f, true));
        e->setIndents (6, 5);
    }
    directory_.setReadOnly (true);
    directory_.setFont (font (14.0f));
    command_.setFont (font (14.0f));
    code_.setInputRestrictions (12);
    title_.onTextChange = [this] {
        if (updating_ || selected_ < 0)
            return;
        auto& f = folders_[(size_t) selected_];
        f.title = title_.getText();
        normalizeFolder (f, ctx.workspace.pgm());
        edited();
    };
    code_.onTextChange = [this] {
        if (updating_ || selected_ < 0)
            return;
        auto& f = folders_[(size_t) selected_];
        f.code = code_.getText();
        normalizeFolder (f, ctx.workspace.pgm());
        edited();
    };
    code_.onFocusLost = [this] {
        if (selected_ >= 0)
        {
            juce::ScopedValueSetter<bool> s (updating_, true);
            code_.setText (folders_[(size_t) selected_].code, false);
        }
    };
    command_.onTextChange = [this] {
        if (updating_ || selected_ < 0)
            return;
        auto& f = folders_[(size_t) selected_];
        f.shortcutArguments = command_.getText();
        normalizeFolder (f, ctx.workspace.pgm());
        edited();
    };

    addAndMakeVisible (type_);
    int id = 1;
    for (auto& t : folderTypes())
        type_.addItem (juce::String::fromUTF8 (t.name), id++);
    type_.onChange = [this] {
        if (updating_ || selected_ < 0)
            return;
        auto index = type_.getSelectedId() - 1;
        if (index < 0)
            return;
        auto& f = folders_[(size_t) selected_];
        f.kind = folderTypes()[(size_t) index].kind;
        normalizeFolder (f, ctx.workspace.pgm());
        edited();
    };

    addAndMakeVisible (browse_);
    browse_.setTooltip (L"Escolher o diretório da pasta");
    browse_.onClick = [this] { chooseDirectory(); };

    icon_ = std::make_unique<IconButton>();
    addAndMakeVisible (*icon_);
    icon_->onClick = [this] { chooseIcon(); };

    addChildComponent (banner_);
    progress_ = std::make_unique<Progress>();
    addChildComponent (*progress_);

    load();
    attachJob();
    startTimer (2000);
}

ConfigManagerView::~ConfigManagerView()
{
    if (auto* job = ctx.workspace.folderSave())
        job->onChange = nullptr;
}

int ConfigManagerView::nextId() const
{
    int top = original_.has_value() ? original_->nextId() - 1 : 0;
    for (auto& f : folders_)
        top = juce::jmax (top, f.id);
    return top + 1;
}

void ConfigManagerView::load()
{
    original_.reset();
    originalBytes_.reset();
    folders_.clear();
    selected_ = -1;
    dirty_ = false;
    if (ctx.workspace.isOpen())
    {
        juce::String error;
        auto file = ctx.workspace.pgm().getChildFile ("Folders.xml");
        if (readFileShared (file, originalBytes_, error))
            original_ = FoldersXml::parse (originalBytes_, error);
        if (original_.has_value())
            folders_ = original_->folders();
        else
            banner_.show (Severity::error, L"Não foi possível ler o Folders.xml: " + error);
    }
    if (! folders_.empty())
        selected_ = 0;
    grid_->layout (viewport_.getMaximumVisibleWidth(), viewport_.getMaximumVisibleHeight());
    showSelection();
    updateEnabled();
    updateStatus();
    grid_->repaint();
}

void ConfigManagerView::select (int index)
{
    selected_ = index;
    showSelection();
    updateStatus();
    grid_->repaint();
}

void ConfigManagerView::showSelection()
{
    juce::ScopedValueSetter<bool> s (updating_, true);
    bool has = selected_ >= 0 && selected_ < (int) folders_.size();
    const FolderEntry* f = has ? &folders_[(size_t) selected_] : nullptr;
    title_.setText (has ? f->title : juce::String(), false);
    code_.setText (has ? f->code : juce::String(), false);
    auto& info = folderTypeInfo (has ? f->kind : FolderKind::unknown);
    directory_.setText (has && info.needsDirectory ? f->target : juce::String(), false);
    int typeIndex = 0;
    for (size_t i = 0; has && i < folderTypes().size(); ++i)
        if (folderTypes()[i].kind == f->kind)
            typeIndex = (int) i + 1;
    type_.setSelectedId (typeIndex, juce::dontSendNotification);
    bool command = has && f->kind == FolderKind::command;
    commandLabel_.setVisible (command);
    command_.setVisible (command);
    command_.setText (command ? f->shortcutArguments : juce::String(), false);
    icon_->image = has ? ShellIcons::instance().get (f->iconLocation, f->iconIndex, 48) : juce::Image();
    icon_->repaint();
    updateEnabled();
}

void ConfigManagerView::updateEnabled()
{
    auto* job = ctx.workspace.folderSave();
    bool busy = ctx.workspace.busy() || (job != nullptr && job->running());
    bool editable = original_.has_value() && ! ctx.workspace.readOnly() && ! busy;
    bool has = selected_ >= 0 && selected_ < (int) folders_.size();
    auto& info = folderTypeInfo (has ? folders_[(size_t) selected_].kind : FolderKind::unknown);
    saveButton_->setEnabled (editable && dirty_);
    addButton_->setEnabled (editable);
    removeButton_->setEnabled (editable && has);
    for (auto* c : std::initializer_list<juce::Component*> { &title_, &code_, &command_, icon_.get() })
        c->setEnabled (editable && has);
    // Pause and command folders keep their type and have no directory, as in the Config Manager.
    type_.setEnabled (editable && has && info.needsDirectory);
    for (size_t i = 0; i < folderTypes().size(); ++i)
        type_.setItemEnabled ((int) i + 1, folderTypes()[i].needsDirectory);
    browse_.setEnabled (editable && has && info.needsDirectory);
    directory_.setEnabled (has && info.needsDirectory);

    if (! ctx.workspace.isOpen())
        banner_.show (Severity::warning, L"Nenhuma instalação carregada.");
    else if (! original_.has_value())
        banner_.show (Severity::error, L"Folders.xml não encontrado ou ilegível.");
    else if (dirty_)
        banner_.show (Severity::warning, L"Alterações não salvas. Ao salvar, o Playlist Digital é reiniciado para aplicá-las.",
                      "Salvar", [this] { save(); });
    else
        banner_.hideBanner();
}

void ConfigManagerView::updateStatus()
{
    if (selected_ < 0 || selected_ >= (int) folders_.size())
    {
        status_.setText (juce::String ((int) folders_.size()) + L" pastas configuradas.", juce::dontSendNotification);
        return;
    }
    auto& f = folders_[(size_t) selected_];
    juce::String text = "Pasta " + f.title + " selecionada.";
    auto& info = folderTypeInfo (f.kind);
    if (info.needsDirectory && ! juce::File (f.target).isDirectory())
        text << L"  O diretório não existe.";
    if (f.iconLocation.isNotEmpty() && ! icon_->image.isValid())
        text << L"  O ícone não pôde ser carregado.";
    status_.setText (text, juce::dontSendNotification);
}

void ConfigManagerView::edited (const juce::String& statusText)
{
    dirty_ = true;
    grid_->layout (viewport_.getMaximumVisibleWidth(), viewport_.getMaximumVisibleHeight());
    grid_->repaint();
    if (selected_ >= 0)
    {
        auto& f = folders_[(size_t) selected_];
        icon_->image = ShellIcons::instance().get (f.iconLocation, f.iconIndex, 48);
        icon_->repaint();
    }
    updateEnabled();
    updateStatus();
    if (statusText.isNotEmpty())
        ctx.status (statusText);
    resized();
}

void ConfigManagerView::addFolder()
{
    juce::Component::SafePointer<ConfigManagerView> self (this);
    openDialog (new NewFolderDialog ([self] (FolderKind kind) {
                    if (self != nullptr)
                        self->createFolder (kind);
                }),
                "Nova pasta", this);
}

void ConfigManagerView::createFolder (FolderKind kind)
{
    auto pgm = ctx.workspace.pgm();
    auto add = [this, kind, pgm] (const juce::File& dir) {
        auto& lig = ctx.workspace.ligacao();
        folders_.push_back (makeNewFolder (kind, dir, pgm, folders_, lig.has_value() ? &*lig : nullptr, nextId()));
        selected_ = (int) folders_.size() - 1;
        edited (L"Pasta " + folders_.back().title + L" adicionada. Salve para gravar.");
        showSelection();
        grid_->layout (viewport_.getMaximumVisibleWidth(), viewport_.getMaximumVisibleHeight());
        for (auto& item : grid_->items)
            if (item.folder == selected_)
                viewport_.setViewPosition (0, juce::jmax (0, item.area.getY() - 40));
    };
    if (! folderTypeInfo (kind).needsDirectory)
    {
        add ({});
        return;
    }
    chooser_ = std::make_unique<juce::FileChooser> (L"Procurar Pasta", pgm.getParentDirectory());
    chooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                           [add] (const juce::FileChooser& fc) {
                               auto dir = fc.getResult();
                               if (dir != juce::File())
                                   add (dir);
                           });
}

void ConfigManagerView::removeFolder()
{
    if (selected_ < 0 || selected_ >= (int) folders_.size() || ! removeButton_->isEnabled())
        return;
    auto name = folders_[(size_t) selected_].title;
    juce::Component::SafePointer<ConfigManagerView> self (this);
    confirm (folderGroupName (folderTypeInfo (folders_[(size_t) selected_].kind).group) + ": " + name,
             L"Deseja remover " + name + L", das configurações de pastas?", "Remover", [self, name] {
                 if (self == nullptr || self->selected_ < 0)
                     return;
                 self->folders_.erase (self->folders_.begin() + self->selected_);
                 self->selected_ = self->folders_.empty() ? -1 : juce::jmin (self->selected_, (int) self->folders_.size() - 1);
                 self->edited (L"Pasta " + name + L" removida. Salve para gravar.");
                 self->showSelection();
             });
}

void ConfigManagerView::chooseDirectory()
{
    if (selected_ < 0)
        return;
    juce::File start (folders_[(size_t) selected_].target);
    chooser_ = std::make_unique<juce::FileChooser> (L"Procurar Pasta", start.isDirectory() ? start : ctx.workspace.pgm().getParentDirectory());
    juce::Component::SafePointer<ConfigManagerView> self (this);
    chooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                           [self] (const juce::FileChooser& fc) {
                               auto dir = fc.getResult();
                               if (self == nullptr || dir == juce::File() || self->selected_ < 0)
                                   return;
                               auto& f = self->folders_[(size_t) self->selected_];
                               f.target = dir.getFullPathName();
                               normalizeFolder (f, self->ctx.workspace.pgm());
                               self->showSelection();
                               self->edited();
                           });
}

void ConfigManagerView::chooseIcon()
{
    if (selected_ < 0)
        return;
    auto& f = folders_[(size_t) selected_];
    juce::Component::SafePointer<ConfigManagerView> self (this);
    openDialog (new IconPickerDialog (ctx.workspace.pgm().getChildFile ("Icones"), f.iconLocation, f.iconIndex,
                                      [self] (const juce::String& file, int index) {
                                          if (self == nullptr || self->selected_ < 0)
                                              return;
                                          auto& folder = self->folders_[(size_t) self->selected_];
                                          folder.iconLocation = file;
                                          folder.iconIndex = index;
                                          self->edited();
                                      }),
                L"Selecione o ícone", this);
}

void ConfigManagerView::save()
{
    if (! dirty_ || ! original_.has_value())
        return;
    if (ctx.workspace.readOnly())
    {
        inform (L"Modo somente leitura", L"Ative \"Permitir alterações\" no topo da janela para gravar.");
        return;
    }
    if (ctx.workspace.busy())
    {
        inform (L"Aguarde", L"Outra operação está em andamento.");
        return;
    }
    auto pgm = ctx.workspace.pgm();
    // The folders were edited over the file as it was loaded.
    if (! FileSnapshot::take (pgm.getChildFile ("Folders.xml")).sameContent (FileSnapshot::fromBytes (pgm.getChildFile ("Folders.xml"), originalBytes_)))
    {
        inform (L"Folders.xml alterado por outro programa",
                L"O Folders.xml mudou desde que foi aberto aqui (o Config Manager foi usado?). "
                L"Use Descartar no aviso ou recarregue (F5) e refaça as alterações.", true);
        return;
    }
    juce::MemoryBlock dbf;
    juce::String error;
    bool hasDbf = readFileShared (pgm.getChildFile ("Dados/LIGACAO.DBF"), dbf, error);
    auto plan = planFolderChanges (pgm, *original_, folders_, hasDbf ? &dbf : nullptr, juce::Time::getCurrentTime());
    if (! plan.hasChanges())
    {
        dirty_ = false;
        updateEnabled();
        ctx.status (L"Nenhuma alteração a gravar.");
        return;
    }
    if (plan.problems.hasErrors())
    {
        juce::StringArray lines;
        for (auto& p : plan.problems.items())
            if (p.severity == Severity::error)
                lines.add (L"• " + p.message + (p.fix.isNotEmpty() ? "  " + p.fix : juce::String()));
        inform (L"Não é possível salvar", lines.joinIntoString ("\n"), true);
        return;
    }
    auto check = checkPlaylistPrograms (pgm);
    if (! check.blockers.isEmpty())
    {
        inform (L"Não é possível salvar agora", check.blockers.joinIntoString ("\n"), true);
        return;
    }

    juce::String message;
    message << L"Alterações:\n";
    for (auto& c : plan.changes)
        message << L"• " << c << "\n";
    juce::StringArray warnings;
    for (auto& p : plan.problems.items())
        if (p.severity == Severity::warning)
            warnings.add (L"• " + p.message);
    if (! warnings.isEmpty())
        message << L"\nAvisos:\n" << warnings.joinIntoString ("\n") << "\n";
    message << "\n";
    if (! check.playlist.empty())
        message << L"O Playlist Digital será FECHADO, as pastas gravadas e ele será aberto de novo "
                   L"(o Config Manager pede para reiniciar o Playlist). A programação fica fora do ar nesse intervalo.";
    else
        message << L"O Playlist Digital está fechado: as pastas passam a valer quando ele for aberto.";

    juce::Component::SafePointer<ConfigManagerView> self (this);
    auto shared = std::make_shared<FolderPlan> (std::move (plan));
    confirm (L"Salvar configurações de pastas", message, "Salvar", [self, shared] {
        if (self == nullptr)
            return;
        if (self->ctx.workspace.startFolderSave (std::move (*shared), false) == nullptr)
        {
            inform (L"Não foi possível salvar", L"Outra operação está em andamento ou a instalação está em modo somente leitura.", true);
            return;
        }
        self->attachJob();
        self->updateEnabled();
    });
}

void ConfigManagerView::attachJob()
{
    auto* job = ctx.workspace.folderSave();
    if (job == nullptr)
        return;
    juce::Component::SafePointer<ConfigManagerView> self (this);
    job->onChange = [self] {
        if (self == nullptr)
            return;
        auto* j = self->ctx.workspace.folderSave();
        if (j == nullptr)
            return;
        self->progress_->update (j->steps(), j->finished(), j->succeeded(), j->outcome());
        if (j->finished() && j->succeeded())
        {
            self->ctx.workspace.addActivity (L"Pastas gravadas pelo Config Manager do Playlist Control", self->ctx.workspace.pgm().getChildFile ("Folders.xml"));
            self->ctx.workspace.reload();
            self->load();
        }
        self->updateEnabled();
    };
    if (job->running() || ! job->finished())
    {
        progress_->update (job->steps(), job->finished(), job->succeeded(), job->outcome());
        progress_->setVisible (true);
        progress_->toFront (false);
    }
}

void ConfigManagerView::discard()
{
    load();
    ctx.status (L"Alterações descartadas.");
}

void ConfigManagerView::refresh()
{
    if (! dirty_)
        load();
    else
        updateEnabled();
}

void ConfigManagerView::timerCallback()
{
    updateEnabled();
}

void ConfigManagerView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
    // Toolbar strip, list frame and properties box, as in the Config Manager.
    g.setColour (colours::panelAlt);
    g.fillRect (toolbarArea_);
    g.setColour (colours::border);
    g.drawHorizontalLine (toolbarArea_.getBottom(), (float) toolbarArea_.getX(), (float) toolbarArea_.getRight());
    g.drawRect (listArea_.expanded (1));
    g.setColour (colours::panel);
    g.fillRoundedRectangle (propertiesArea_.toFloat(), 6.0f);
    g.setColour (colours::border);
    g.drawRoundedRectangle (propertiesArea_.toFloat().reduced (0.5f), 6.0f, 1.0f);
    g.setColour (colours::border);
    g.drawHorizontalLine (statusArea_.getY(), (float) statusArea_.getX(), (float) statusArea_.getRight());
}

void ConfigManagerView::resized()
{
    auto r = getLocalBounds();
    progress_->setBounds (r);
    banner_.setBounds (r.removeFromTop (banner_.preferredHeight()));
    toolbarArea_ = r.removeFromTop (44);
    auto tb = toolbarArea_.reduced (12, 6);
    for (auto* b : { saveButton_.get(), addButton_.get(), removeButton_.get() })
    {
        b->setBounds (tb.removeFromLeft (34));
        tb.removeFromLeft (4);
    }
    statusArea_ = r.removeFromBottom (26);
    status_.setBounds (statusArea_.reduced (12, 0));
    r = r.reduced (16, 12);
    propertiesArea_ = r.removeFromRight (juce::jlimit (300, 380, r.getWidth() / 3));
    r.removeFromRight (14);
    listArea_ = r;
    viewport_.setBounds (listArea_);
    grid_->layout (viewport_.getMaximumVisibleWidth(), viewport_.getMaximumVisibleHeight());

    auto p = propertiesArea_.reduced (14, 12);
    propertiesTitle_.setBounds (p.removeFromTop (24));
    p.removeFromTop (6);
    auto field = [&] (juce::Label& l, juce::Component& c, int h) {
        l.setBounds (p.removeFromTop (20));
        c.setBounds (p.removeFromTop (h));
        p.removeFromTop (8);
    };
    field (titleLabel_, title_, 28);
    field (typeLabel_, type_, 28);
    dirLabel_.setBounds (p.removeFromTop (20));
    auto dirRow = p.removeFromTop (28);
    browse_.setBounds (dirRow.removeFromRight (34));
    dirRow.removeFromRight (6);
    directory_.setBounds (dirRow);
    p.removeFromTop (8);
    field (codeLabel_, code_, 28);
    icon_->setBounds (p.removeFromTop (64).withWidth (64));
    p.removeFromTop (14);
    commandLabel_.setBounds (p.removeFromTop (20));
    command_.setBounds (p.removeFromTop (28));
}

} // namespace pc::ui
