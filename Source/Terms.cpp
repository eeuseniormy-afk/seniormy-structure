#include "Terms.h"
#include "LookAndFeel.h"
#include "AGBinaryData.h"

using namespace juce;

namespace
{
    std::unique_ptr<PropertiesFile> openSettings()
    {
        PropertiesFile::Options o;
        o.applicationName = "SeniorMy Acoustic Guitars";
        o.folderName = "SeniorMy";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        o.commonToAllUsers = false;
        o.storageFormat = PropertiesFile::storeAsXML;
        return std::make_unique<PropertiesFile> (o);
    }

    const char* uiStrings[2][6] = {
        { "Termos de Uso", "Leia e aceite para usar o SeniorMy Acoustic Guitars. Até lá, o plugin fica bloqueado e em silêncio.",
          "Li e aceito os Termos de Uso", "Aceitar e continuar", "Fechar", "Idioma" },
        { "Terms of Use", "Read and accept to use SeniorMy Acoustic Guitars. Until then, the plug-in stays locked and silent.",
          "I have read and accept the Terms of Use", "Accept and continue", "Close", "Language" },
    };
}

StringArray Terms::languageNames() { return { String::fromUTF8 ("Português (Brasil)"), "English" }; }

int Terms::defaultLanguage()
{
    const auto l = SystemStats::getDisplayLanguage().toLowerCase();
    if (l.startsWith ("pt")) return 0;
    if (l.isEmpty()) return 0;
    return 1;
}

String Terms::text (int language)
{
    if (language == 1) return String::fromUTF8 (AGBinaryData::termosen_txt, AGBinaryData::termosen_txtSize);
    return String::fromUTF8 (AGBinaryData::termosptBR_txt, AGBinaryData::termosptBR_txtSize);
}

bool Terms::isAccepted()
{
    auto s = openSettings();
    return s->getValue ("termsVersion") == version;
}

void Terms::recordAcceptance()
{
    auto s = openSettings();
    s->setValue ("termsVersion", version);
    s->setValue ("termsAcceptedAt", Time::getCurrentTime().toISO8601 (true));
    s->saveIfNeeded();
}

//==============================================================================
TermsOverlay::TermsOverlay (std::function<void()> cb, bool ro) : onAccepted (std::move (cb)), readOnly (ro)
{
    setInterceptsMouseClicks (true, true);
    title.setFont (WoodLookAndFeel::titleFont (28.0f));
    title.setColour (Label::textColourId, Wood::ivory);
    subtitle.setFont (WoodLookAndFeel::uiFont (13.5f));
    subtitle.setColour (Label::textColourId, Wood::ivorySoft);
    addAndMakeVisible (title);
    addAndMakeVisible (subtitle);

    language.addItemList (Terms::languageNames(), 1);
    language.onChange = [this] { setLanguage (language.getSelectedItemIndex()); };
    addAndMakeVisible (language);

    body.setMultiLine (true);
    body.setReadOnly (true);
    body.setScrollbarsShown (true);
    body.setCaretVisible (false);
    body.setFont (WoodLookAndFeel::uiFont (14.0f));
    body.setColour (TextEditor::textColourId, Wood::ivory);
    body.setColour (TextEditor::backgroundColourId, Wood::well);
    body.setColour (TextEditor::outlineColourId, Wood::goldDark);
    addAndMakeVisible (body);

    agree.onClick = [this] { refreshButtons(); };
    accept.onClick = [this]
    {
        if (! agree.getToggleState()) return;
        Terms::recordAcceptance();
        if (onAccepted) onAccepted();
    };
    close.onClick = [this] { if (onAccepted) onAccepted(); };
    if (readOnly) addAndMakeVisible (close);
    else { addAndMakeVisible (agree); addAndMakeVisible (accept); }

    language.setSelectedItemIndex (Terms::defaultLanguage(), sendNotificationSync);
    refreshButtons();
}

void TermsOverlay::setLanguage (int lang)
{
    lang = jlimit (0, 1, lang);
    title.setText (String::fromUTF8 (uiStrings[lang][0]), dontSendNotification);
    subtitle.setText (readOnly ? String() : String::fromUTF8 (uiStrings[lang][1]), dontSendNotification);
    agree.setButtonText (String::fromUTF8 (uiStrings[lang][2]));
    accept.setButtonText (String::fromUTF8 (uiStrings[lang][3]));
    close.setButtonText (String::fromUTF8 (uiStrings[lang][4]));
    body.setText (Terms::text (lang), false);
    body.applyColourToAllText (Wood::ivory);
    body.moveCaretToTop (false);
}

void TermsOverlay::refreshButtons() { accept.setEnabled (agree.getToggleState()); }

void TermsOverlay::paint (Graphics& g)
{
    WoodLookAndFeel::paintWood (g, getLocalBounds().toFloat());
    g.fillAll (Colours::black.withAlpha (0.35f));
    auto card = getLocalBounds().reduced (60, 34).toFloat();
    g.setColour (Wood::black.withAlpha (0.96f));
    g.fillRoundedRectangle (card, 12.0f);
    g.setColour (Wood::ivory);
    g.drawRoundedRectangle (card.reduced (0.5f), 12.0f, 1.4f);
    g.setColour (Wood::gold);
    g.drawRoundedRectangle (card.reduced (4.0f), 9.0f, 0.9f);
    g.setColour (Wood::ivorySoft);
    g.setFont (WoodLookAndFeel::uiFont (11.5f));
    g.drawText (String::fromUTF8 ("© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77"),
                card.reduced (28, 10).removeFromBottom (14).toNearestInt(), Justification::centredLeft);
}

void TermsOverlay::resized()
{
    auto r = getLocalBounds().reduced (60, 34).reduced (28, 20);
    r.removeFromBottom (14);
    auto top = r.removeFromTop (38);
    language.setBounds (top.removeFromRight (200).reduced (0, 5));
    title.setBounds (top);
    subtitle.setBounds (r.removeFromTop (22));
    r.removeFromTop (8);
    auto bottom = r.removeFromBottom (40);
    r.removeFromBottom (10);
    body.setBounds (r);
    if (readOnly) close.setBounds (bottom.removeFromRight (150).reduced (0, 4));
    else
    {
        accept.setBounds (bottom.removeFromRight (210).reduced (0, 4));
        agree.setBounds (bottom.reduced (0, 6));
    }
}
