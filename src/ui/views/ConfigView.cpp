#include "ui/views/ConfigView.h"

namespace pc::ui
{

using namespace theme;

class ConfigView::FieldRow : public juce::Component
{
public:
    FieldRow (ConfigView& owner, const ConfigEntry& entry, const juce::String& value, bool changed, bool editable)
        : owner_ (owner), entry_ (entry), changed_ (changed)
    {
        auto type = entry.field != nullptr ? entry.field->type : ConfigType::readOnly;
        bool canEdit = editable && entry.editable();
        if (type == ConfigType::flag && (value == "0" || value == "1"))
        {
            addAndMakeVisible (toggle_);
            toggle_.setToggleState (value == "1", juce::dontSendNotification);
            toggle_.setEnabled (canEdit);
            toggle_.onClick = [this] { owner_.valueChanged (entry_, toggle_.getToggleState() ? "1" : "0"); };
        }
        else
        {
            addAndMakeVisible (editor_);
            editor_.setFont (type == ConfigType::readOnly ? monoFont (13.0f) : font (14.0f));
            editor_.setIndents (6, 6);
            editor_.setText (value, false);
            editor_.setReadOnly (! canEdit);
            if (type == ConfigType::secret)
            {
                editor_.setPasswordCharacter ((juce::juce_wchar) 0x2022);
                addAndMakeVisible (reveal_);
                reveal_.setButtonText ("Mostrar");
                reveal_.setClickingTogglesState (true);
                reveal_.onClick = [this] {
                    editor_.setPasswordCharacter (reveal_.getToggleState() ? 0 : (juce::juce_wchar) 0x2022);
                    reveal_.setButtonText (reveal_.getToggleState() ? "Ocultar" : "Mostrar");
                };
            }
            if (type == ConfigType::integer)
                editor_.setInputRestrictions (12, "-0123456789");
            auto commit = [this] {
                if (! editor_.isReadOnly())
                    owner_.valueChanged (entry_, editor_.getText());
            };
            editor_.onReturnKey = commit;
            editor_.onFocusLost = commit;
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        g.setColour (changed_ ? colours::warningBack : colours::panel);
        g.fillRect (r);
        g.setColour (colours::border);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
        auto text = r.reduced (14, 8).withTrimmedRight (editorWidth + 20);
        g.setColour (colours::text);
        g.setFont (font (14.0f, true));
        auto label = entry_.label();
        g.drawText (label, text.removeFromTop (22), juce::Justification::centredLeft, true);
        auto labelWidth = (int) juce::GlyphArrangement::getStringWidth (font (14.0f, true), label);
        auto badges = juce::Rectangle<float> ((float) (14 + labelWidth + 10), 11.0f, 400.0f, 18.0f);
        if (entry_.field != nullptr && ! entry_.field->confirmed)
            drawBadge (g, badges.removeFromLeft (badgeWidth (L"CORRESPONDÊNCIA PROVÁVEL")), L"CORRESPONDÊNCIA PROVÁVEL",
                       colours::warningBack, colours::warning, 10.5f);
        if (! entry_.editable())
            drawBadge (g, badges.removeFromLeft (badgeWidth ("SOMENTE LEITURA")).translated (4, 0), "SOMENTE LEITURA",
                       colours::panelAlt, colours::textMuted, 10.5f);
        g.setColour (colours::textMuted);
        g.setFont (font (12.5f));
        juce::String help = entry_.field != nullptr ? juce::String::fromUTF8 (entry_.field->help) : juce::String (L"Chave não documentada no manual; valor preservado.");
        if (entry_.field != nullptr && juce::String (entry_.field->unit).isNotEmpty())
            help << L"  Unidade: " << entry_.field->unit << ".";
        g.drawFittedText (help, text.removeFromTop (34), juce::Justification::topLeft, 2);
        g.setFont (monoFont (11.5f));
        g.drawText ("<" + entry_.path + ">", text, juce::Justification::topLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 12);
        auto area = r.removeFromRight (editorWidth);
        if (toggle_.isVisible())
            toggle_.setBounds (area.removeFromTop (28));
        else
        {
            auto line = area.removeFromTop (30);
            if (reveal_.isVisible())
            {
                reveal_.setBounds (line.removeFromRight (78));
                line.removeFromRight (6);
            }
            editor_.setBounds (line);
        }
    }

    static constexpr int editorWidth = 330;

private:
    ConfigView& owner_;
    ConfigEntry entry_;
    bool changed_ = false;
    juce::ToggleButton toggle_ { L"Ativado" };
    juce::TextEditor editor_;
    juce::TextButton reveal_;
};

ConfigView::ConfigView (AppContext& context) : View (context)
{
    addAndMakeVisible (groupList_);
    groupList_.setModel (this);
    groupList_.setRowHeight (34);
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (&rowsHolder_, false);
    viewport_.setScrollBarsShown (true, false);
    addAndMakeVisible (fileTitle_);
    styleLabel (fileTitle_, 16.0f, true);
    addAndMakeVisible (fileInfo_);
    styleLabel (fileInfo_, 12.5f, false, colours::textMuted);
    addAndMakeVisible (saveButton_);
    addAndMakeVisible (discardButton_);
    makePrimary (saveButton_);
    saveButton_.onClick = [this] { save(); };
    discardButton_.onClick = [this] { discard(); };
    addChildComponent (banner_);
    load();
}

ConfigView::~ConfigView()
{
    groupList_.setModel (nullptr);
}

bool ConfigView::editable() const
{
    return config_.has_value() && ! ctx.workspace.readOnly() && ! ctx.workspace.runtime().playlistRunning;
}

void ConfigView::load()
{
    session_ = FileSession (ctx.workspace.pgm().getChildFile ("CONFIG.XML"));
    config_.reset();
    entries_.clear();
    groups_.clear();
    changes_.clear();
    juce::String err;
    if (ctx.workspace.isOpen() && session_.load (err))
    {
        config_ = ConfigXml::parse (session_.bytes(), err);
        if (config_.has_value())
            entries_ = config_->entries();
    }
    for (auto& g : configGroups())
        for (auto& e : entries_)
            if (e.group() == g)
            {
                groups_.add (g);
                break;
            }
    groupList_.updateContent();
    if (! groups_.isEmpty())
    {
        auto index = juce::jmax (0, groups_.indexOf (currentGroup_));
        groupList_.selectRow (index);
        showGroup (groups_[index]);
    }
    updateHeader();
}

int ConfigView::getNumRows()
{
    return groups_.size();
}

void ConfigView::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    g.fillAll (selected ? colours::infoBack : colours::panel);
    if (selected)
    {
        g.setColour (colours::brandLight);
        g.fillRect (0, 4, 3, height - 8);
    }
    int n = 0, changed = 0;
    for (auto& e : entries_)
        if (e.group() == groups_[row])
        {
            ++n;
            changed += changes_.count (e.path) > 0 ? 1 : 0;
        }
    g.setColour (colours::text);
    g.setFont (font (13.5f, selected));
    g.drawText (groups_[row], 14, 0, width - 60, height, juce::Justification::centredLeft, true);
    auto badge = changed > 0 ? juce::String (changed) + "*" : juce::String (n);
    drawBadge (g, juce::Rectangle<float> ((float) width - 46, (float) height / 2 - 9, 36, 18), badge,
               changed > 0 ? colours::warningBack : colours::panelAlt, changed > 0 ? colours::warning : colours::textMuted);
}

void ConfigView::selectedRowsChanged (int row)
{
    if (row >= 0 && row < groups_.size())
        showGroup (groups_[row]);
}

void ConfigView::showGroup (const juce::String& group)
{
    currentGroup_ = group;
    rows_.clear();
    int y = 0;
    auto width = viewport_.getMaximumVisibleWidth();
    for (auto& e : entries_)
    {
        if (e.group() != group)
            continue;
        auto changed = changes_.find (e.path);
        auto value = changed != changes_.end() ? changed->second : e.value;
        auto* row = rows_.add (new FieldRow (*this, e, value, changed != changes_.end(), editable()));
        rowsHolder_.addAndMakeVisible (row);
        row->setBounds (0, y, width, 92);
        y += 92;
    }
    rowsHolder_.setSize (width, y);
}

void ConfigView::valueChanged (const ConfigEntry& entry, const juce::String& value)
{
    auto v = value.trim();
    if (auto problem = validateConfigValue (entry, v, session_.file()))
    {
        inform (L"Valor inválido", problem->message + "\n\n" + problem->reason + "\n\n" + problem->fix, true);
        juce::Component::SafePointer<ConfigView> self (this);
        juce::MessageManager::callAsync ([self] { if (self != nullptr) self->showGroup (self->currentGroup_); });
        return;
    }
    if (v == entry.value)
        changes_.erase (entry.path);
    else
        changes_[entry.path] = v;
    // Passwords are never echoed to the status bar or logs.
    bool secret = entry.field != nullptr && entry.field->type == ConfigType::secret;
    ctx.status (entry.label() + (secret ? L" alterado" : L" = " + v));
    groupList_.repaint();
    // The row that raised this event is rebuilt after its callback returns.
    juce::Component::SafePointer<ConfigView> self (this);
    juce::MessageManager::callAsync ([self] {
        if (self != nullptr)
        {
            self->showGroup (self->currentGroup_);
            self->updateHeader();
        }
    });
}

void ConfigView::updateHeader()
{
    fileTitle_.setText (config_.has_value() ? juce::String ("CONFIG.XML") + (changes_.empty() ? "" : "  *") : juce::String (L"CONFIG.XML não encontrado"),
                        juce::dontSendNotification);
    int documented = 0;
    for (auto& e : entries_)
        documented += e.field != nullptr ? 1 : 0;
    fileInfo_.setText (config_.has_value() ? juce::String (documented) + L" de " + juce::String ((int) entries_.size())
                                                 + L" opções identificadas pelo manual  ·  " + juce::String ((int) changes_.size()) + L" alteração(ões) pendente(s)"
                                           : juce::String(),
                       juce::dontSendNotification);
    saveButton_.setEnabled (! changes_.empty() && editable());
    discardButton_.setEnabled (! changes_.empty());
    if (ctx.workspace.runtime().playlistRunning)
        banner_.show (Severity::warning, L"O Playlist Digital está aberto e regrava o CONFIG.XML. Feche-o para alterar as opções por aqui.");
    else if (config_.has_value())
        banner_.show (Severity::info, L"As alterações valem na próxima vez que o Playlist Digital for aberto.");
    else
        banner_.hideBanner();
    resized();
}

void ConfigView::save()
{
    if (changes_.empty() || ! config_.has_value())
        return;
    if (ctx.workspace.readOnly())
    {
        inform (L"Modo somente leitura", L"Ative \"Permitir alterações\" no topo da janela para gravar.");
        return;
    }
    // Checked again right before writing: the Playlist may have been opened.
    ctx.workspace.refreshRuntime();
    if (ctx.workspace.runtime().playlistRunning)
    {
        inform (L"Playlist Digital aberto", L"Feche o Playlist Digital antes de salvar: ele regrava o CONFIG.XML ao fechar e desfaria a alteração.", true);
        updateHeader();
        return;
    }

    auto edited = *config_;
    juce::StringArray summary;
    for (auto& [path, value] : changes_)
    {
        if (! edited.set (path, value))
        {
            inform (L"Não foi possível alterar", L"O elemento <" + path + L"> não pôde ser alterado.", true);
            return;
        }
        auto* field = findConfigField (path);
        bool secret = field != nullptr && field->type == ConfigType::secret;
        summary.add (path + (secret ? L" (senha)" : " = " + value));
    }
    juce::MemoryBlock bytes;
    edited.toBytes (bytes);
    auto expected = changes_;
    auto r = session_.save (ctx.workspace.writer(), bytes, L"Editar opções", summary.joinIntoString ("; "),
                            [expected] (const juce::MemoryBlock& b) {
                                juce::String err;
                                auto check = ConfigXml::parse (b, err);
                                if (! check.has_value())
                                    return L"O CONFIG.XML gerado não pôde ser lido: " + err;
                                for (auto& [path, value] : expected)
                                    if (check->get (path).value_or ("\x01") != value)
                                        return L"A releitura de <" + path + L"> não confere.";
                                return juce::String();
                            });
    if (r.succeeded())
    {
        ctx.workspace.addActivity (L"Opções do Playlist salvas: " + juce::String ((int) changes_.size()) + L" alteração(ões)", session_.file());
        ctx.status (L"CONFIG.XML salvo com cópia de segurança. As opções valem ao abrir o Playlist Digital.");
        ctx.workspace.reload();
        load();
    }
    else
        inform (L"Não foi possível salvar", r.message, true);
}

void ConfigView::discard()
{
    changes_.clear();
    load();
}

void ConfigView::refresh()
{
    if (changes_.empty())
        load();
    else
        updateHeader();
}

void ConfigView::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void ConfigView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (64).reduced (16, 6);
    auto top = header.removeFromTop (32);
    saveButton_.setBounds (top.removeFromRight (96));
    top.removeFromRight (6);
    discardButton_.setBounds (top.removeFromRight (96));
    fileTitle_.setBounds (top);
    fileInfo_.setBounds (header);
    banner_.setBounds (r.removeFromTop (banner_.preferredHeight()));
    groupList_.setBounds (r.removeFromLeft (260));
    viewport_.setBounds (r);
    if (currentGroup_.isNotEmpty() && viewport_.getMaximumVisibleWidth() != rowsHolder_.getWidth())
        showGroup (currentGroup_);
}

} // namespace pc::ui
