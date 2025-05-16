/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-15
 * Description : AI configuration setup tab
 *
 * SPDX-FileCopyrightText: 2025 by Michael Miller <michael underscore miller at msn dot com>
 * SPDX-FileCopyrightText: 2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "setupllm.h"

// Qt includes

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QTextEdit>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_config.h"
#include "digikam_debug.h"
#include "applicationsettings.h"
#include "dfontselect.h"
#include "dnuminput.h"
#include "dimg.h"
#include "dnnmodelmanager.h"
#include "dfiledialog.h"
#include "llmprocessmanagerkoboldcpp.h"
#include "llmprocesssettings.h"

namespace Digikam
{

class Q_DECL_HIDDEN SetupLLM::Private
{
public:

    explicit Private()
      : tab(nullptr),
        annotateTab(nullptr),
        llmEngineCombo(nullptr),
        maxTagsSpinBox(nullptr),
        maxTagsLabel(nullptr),
        promptTextEdit(nullptr),
        resetPromptButton(nullptr),
        temperatureInput(nullptr),
        temperatureLabel(nullptr),
        enableAIAnnotateCheck(nullptr),
        enableAIAnnotateAdvSetCheck(nullptr),
        enableAIAnnotateDescriptionCheck(nullptr),
        enableAIAnnotateTagsCheck(nullptr),
        enableAIAnnotateAestheticCheck(nullptr),
        enableAIAnnotateCustomPromptCheck(nullptr),
        aiVoiceTypeCombo(nullptr),
        aiVoiceTypeLabel(nullptr),
        activeTab(Annotate)
    {
    }

    QTabWidget*    tab;
    QWidget*       annotateTab;
    
    // Engine tab
    QComboBox*     llmEngineCombo;
    DIntNumInput*  maxTagsSpinBox;
    QLabel*        maxTagsLabel;

    // Custom prompt
    QTextEdit*     promptTextEdit;
    QPushButton*   resetPromptButton;

    // Generation settings
    DDoubleNumInput* temperatureInput;
    QLabel*        temperatureLabel;

    // Enable AI Annotate
    QCheckBox*     enableAIAnnotateCheck;
    QCheckBox*     enableAIAnnotateAdvSetCheck;
    
    // Generation components checkboxes
    QCheckBox*     enableAIAnnotateDescriptionCheck;
    QCheckBox*     enableAIAnnotateTagsCheck;
    QCheckBox*     enableAIAnnotateAestheticCheck;
    QCheckBox*     enableAIAnnotateCustomPromptCheck;
    
    // Voice style
    QComboBox*     aiVoiceTypeCombo;
    QLabel*        aiVoiceTypeLabel;
    
    // Text tones
    QCheckBox*     textToneFriendlyCheck;
    QCheckBox*     textToneSeriousCheck;
    QCheckBox*     textToneHumorousCheck;
    QCheckBox*     textToneEmpatheticCheck;
    QCheckBox*     textToneAuthoritativeCheck;
    QCheckBox*     textToneEncouragingCheck;
    QCheckBox*     textToneUrgentCheck;
    QCheckBox*     textToneInspirationalCheck;
    QCheckBox*     textToneRespectfulCheck;
    QCheckBox*     textToneConciseCheck;
    QCheckBox*     textToneDescriptiveCheck;
    QCheckBox*     textTonePersuasiveCheck;
    QCheckBox*     textToneDramaticCheck;
    QCheckBox*     textTonePoeticCheck;
    QCheckBox*     textToneTechnicalCheck;

    LLMTab         activeTab;
};

SetupLLM::SetupLLM(QWidget* const parent)
    : QScrollArea(parent),
      d(new Private)
{
    // const int spacing         = QApplication::style()->pixelMetric(QStyle::PM_DefaultLayoutSpacing);
    const int spacing         = layoutSpacing();


    QWidget* const panel      = new QWidget(viewport());
    QVBoxLayout* const layout = new QVBoxLayout(panel);

    d->tab                    = new QTabWidget(panel);
    d->annotateTab            = new QWidget(d->tab);

    // --------------------------------------------------------

    QVBoxLayout* const annotateTabLayout = new QVBoxLayout(d->annotateTab);
    
    // Enable AI Annotate checkbox
    d->enableAIAnnotateCheck = new QCheckBox(i18n("Enable AI Annotate"), d->annotateTab);
    d->enableAIAnnotateCheck->setToolTip(i18n("Enable or disable the AI Annotate feature"));
    annotateTabLayout->addWidget(d->enableAIAnnotateCheck);
    annotateTabLayout->addSpacing(spacing);

    // Description label to explain the feature
    QLabel* const descriptionLabel = new QLabel(d->annotateTab);
    descriptionLabel->setText(i18n("<b>AI Annotate</b> in <b>digiKam</b> brings intelligent automation to your image organization. "
                                  "Leveraging advanced AI, this feature <b>analyzes photos</b>, generates <b>detailed descriptions</b>, "
                                  "and assigns <b>accurate keyword tags</b>, making searching and categorization effortless. "
                                  "Additionally, AI Annotate performs <b>aesthetic analysis</b>, rating each image with a "
                                  "<b>red, yellow, or green flag</b> to quickly indicate visual appeal or quality."));
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    descriptionLabel->setLineWidth(1);
    descriptionLabel->setMargin(10);
    
    annotateTabLayout->addWidget(descriptionLabel);
    annotateTabLayout->addSpacing(spacing);

    // LLM Engine selection
    QHBoxLayout* const engineLayout = new QHBoxLayout();
    
    QLabel* const engineLabel = new QLabel(i18n("LLM Engine:"), d->annotateTab);
    engineLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    d->llmEngineCombo = new QComboBox(d->annotateTab);
    d->llmEngineCombo->addItem(i18n("KoboldCPP"), static_cast<int>(LLMProcessManagerKoboldCpp::KoboldCpp));
    d->llmEngineCombo->addItem(i18n("LlamaCPP"), static_cast<int>(LLMProcessManagerKoboldCpp::LlamaCpp));
    d->llmEngineCombo->setToolTip(i18n("Select the LLM engine to use for text generation"));
    
    engineLayout->addWidget(engineLabel);
    engineLayout->addWidget(d->llmEngineCombo);
    engineLayout->addStretch();
    
    annotateTabLayout->addLayout(engineLayout);

    // Generation settings
    QGroupBox* const tagGenBox     = new QGroupBox(i18n("Generation Settings"), d->annotateTab);
    QGridLayout* const tagGenLayout = new QGridLayout(tagGenBox);
    
    // Generation components checkboxes
    d->enableAIAnnotateDescriptionCheck = new QCheckBox(i18n("Enable Description Generation"), tagGenBox);
    d->enableAIAnnotateDescriptionCheck->setToolTip(i18n("Generate detailed descriptions of image contents"));
    
    d->enableAIAnnotateTagsCheck = new QCheckBox(i18n("Enable Tag Generation"), tagGenBox);
    d->enableAIAnnotateTagsCheck->setToolTip(i18n("Generate tags/keywords for image categorization"));
    
    // Max Tags settings
    d->maxTagsLabel = new QLabel(i18n("Maximum tags:"), tagGenBox);
    d->maxTagsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->maxTagsSpinBox = new DIntNumInput(tagGenBox);
    d->maxTagsSpinBox->setRange(0, 30, 1);
    d->maxTagsSpinBox->setValue(15);
    d->maxTagsSpinBox->setToolTip(i18n("Set the maximum number of tags to generate per image"));
    
    d->enableAIAnnotateAestheticCheck = new QCheckBox(i18n("Enable Aesthetic Rating"), tagGenBox);
    d->enableAIAnnotateAestheticCheck->setToolTip(i18n("Rate image aesthetic quality (red, yellow, green flag)"));

    tagGenLayout->addWidget(d->enableAIAnnotateDescriptionCheck, 0, 0);
    tagGenLayout->addWidget(d->enableAIAnnotateTagsCheck, 1, 0);
    tagGenLayout->addWidget(d->maxTagsLabel, 1, 1);
    tagGenLayout->addWidget(d->maxTagsSpinBox, 1, 2);
    tagGenLayout->addWidget(d->enableAIAnnotateAestheticCheck, 2, 0);
    
    // Text tones and voice style
    QGroupBox* const textTonesBox = new QGroupBox(i18n("Text Tones and Voice Style"), tagGenBox);
    QGridLayout* const textTonesLayout = new QGridLayout(textTonesBox);
    
    // Voice style dropdown
    d->aiVoiceTypeLabel = new QLabel(i18n("Voice Style:"), textTonesBox);
    d->aiVoiceTypeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->aiVoiceTypeCombo = new QComboBox(textTonesBox);
    d->aiVoiceTypeCombo->addItem(i18n("Casual"), static_cast<int>(LLMProcessSettings::Casual));
    d->aiVoiceTypeCombo->addItem(i18n("Neutral"), static_cast<int>(LLMProcessSettings::Neutral));
    d->aiVoiceTypeCombo->addItem(i18n("Formal"), static_cast<int>(LLMProcessSettings::Formal));
    d->aiVoiceTypeCombo->setToolTip(i18n("Select the tone of voice used in AI-generated descriptions"));
    
    QHBoxLayout* const voiceLayout = new QHBoxLayout();
    voiceLayout->addWidget(d->aiVoiceTypeLabel);
    voiceLayout->addWidget(d->aiVoiceTypeCombo);
    voiceLayout->addStretch();
    textTonesLayout->addLayout(voiceLayout, 0, 0, 1, 3);
    
    QLabel* const textTonesLabel = new QLabel(i18n("Select one or more tones to apply to AI-generated text:"), textTonesBox);
    textTonesLabel->setWordWrap(true);
    textTonesLayout->addWidget(textTonesLabel, 1, 0, 1, 3);
    
    // Create and add the tone checkboxes in a grid layout
    d->textToneFriendlyCheck = new QCheckBox(i18n("Friendly"), textTonesBox);
    d->textToneSeriousCheck = new QCheckBox(i18n("Serious"), textTonesBox);
    d->textToneHumorousCheck = new QCheckBox(i18n("Humorous"), textTonesBox);
    d->textToneEmpatheticCheck = new QCheckBox(i18n("Empathetic"), textTonesBox);
    d->textToneAuthoritativeCheck = new QCheckBox(i18n("Authoritative"), textTonesBox);
    d->textToneEncouragingCheck = new QCheckBox(i18n("Encouraging"), textTonesBox);
    d->textToneUrgentCheck = new QCheckBox(i18n("Urgent"), textTonesBox);
    d->textToneInspirationalCheck = new QCheckBox(i18n("Inspirational"), textTonesBox);
    d->textToneRespectfulCheck = new QCheckBox(i18n("Respectful"), textTonesBox);
    d->textToneConciseCheck = new QCheckBox(i18n("Concise"), textTonesBox);
    d->textToneDescriptiveCheck = new QCheckBox(i18n("Descriptive"), textTonesBox);
    d->textTonePersuasiveCheck = new QCheckBox(i18n("Persuasive"), textTonesBox);
    d->textToneDramaticCheck = new QCheckBox(i18n("Dramatic"), textTonesBox);
    d->textTonePoeticCheck = new QCheckBox(i18n("Poetic"), textTonesBox);
    d->textToneTechnicalCheck = new QCheckBox(i18n("Technical"), textTonesBox);
    
    // Arrange checkboxes in a 5x3 grid
    textTonesLayout->addWidget(d->textToneFriendlyCheck, 2, 0);
    textTonesLayout->addWidget(d->textToneSeriousCheck, 2, 1);
    textTonesLayout->addWidget(d->textToneHumorousCheck, 2, 2);
    textTonesLayout->addWidget(d->textToneEmpatheticCheck, 3, 0);
    textTonesLayout->addWidget(d->textToneAuthoritativeCheck, 3, 1);
    textTonesLayout->addWidget(d->textToneEncouragingCheck, 3, 2);
    textTonesLayout->addWidget(d->textToneUrgentCheck, 4, 0);
    textTonesLayout->addWidget(d->textToneInspirationalCheck, 4, 1);
    textTonesLayout->addWidget(d->textToneRespectfulCheck, 4, 2);
    textTonesLayout->addWidget(d->textToneConciseCheck, 5, 0);
    textTonesLayout->addWidget(d->textToneDescriptiveCheck, 5, 1);
    textTonesLayout->addWidget(d->textTonePersuasiveCheck, 5, 2);
    textTonesLayout->addWidget(d->textToneDramaticCheck, 6, 0);
    textTonesLayout->addWidget(d->textTonePoeticCheck, 6, 1);
    textTonesLayout->addWidget(d->textToneTechnicalCheck, 6, 2);
    
    tagGenLayout->addWidget(textTonesBox, 5, 0, 1, 3);
    
    annotateTabLayout->addWidget(tagGenBox);

    // Advanced Settings section
    QGroupBox* const advancedBox = new QGroupBox(i18n("Advanced Settings"), d->annotateTab);
    QVBoxLayout* const advancedLayout = new QVBoxLayout(advancedBox);
    
    // Checkbox to enable advanced settings
    d->enableAIAnnotateAdvSetCheck = new QCheckBox(i18n("Enable Advanced Settings"), advancedBox);
    d->enableAIAnnotateAdvSetCheck->setToolTip(i18n("Enable advanced options for fine-tuning AI generation"));
    advancedLayout->addWidget(d->enableAIAnnotateAdvSetCheck);
    
    // Container for advanced settings
    QWidget* const advancedSettingsContainer = new QWidget(advancedBox);
    QVBoxLayout* const advContainerLayout = new QVBoxLayout(advancedSettingsContainer);
    advContainerLayout->setContentsMargins(20, 10, 0, 0);  // Add left margin for indentation
    
    // Temperature settings (moved to advanced settings)
    QHBoxLayout* const tempLayout = new QHBoxLayout();
    d->temperatureLabel = new QLabel(i18n("Temperature:"), advancedSettingsContainer);
    d->temperatureLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->temperatureInput = new DDoubleNumInput(advancedSettingsContainer);
    d->temperatureInput->setRange(0.0, 1.0, 0.1);
    d->temperatureInput->setValue(0.1);  // Default is 0.1 for more consistent results
    d->temperatureInput->setToolTip(i18n("Controls randomness in generation (higher = more creative, lower = more deterministic)"));
    tempLayout->addWidget(d->temperatureLabel);
    tempLayout->addWidget(d->temperatureInput);
    tempLayout->addStretch();
    advContainerLayout->addLayout(tempLayout);
    
    // Voice type and text tones have been moved to Generation Settings section
    
    // Custom prompt area (moved to advanced settings)
    QGroupBox* const promptBox = new QGroupBox(i18n("Custom Prompt"), advancedSettingsContainer);
    QVBoxLayout* const promptLayout = new QVBoxLayout(promptBox);

    d->enableAIAnnotateCustomPromptCheck = new QCheckBox(i18n("Enable Custom Prompt"), promptBox);
    d->enableAIAnnotateCustomPromptCheck->setToolTip(i18n("Use the custom prompt instead of the auto-generated one"));
    
    d->promptTextEdit = new QTextEdit(promptBox);
    d->promptTextEdit->setMinimumHeight(150);
    d->promptTextEdit->setToolTip(i18n("Enter a custom prompt to send to the LLM"));

    d->resetPromptButton = new QPushButton(i18n("Reset to Default"), promptBox);
    d->resetPromptButton->setToolTip(i18n("Reset the prompt to the default template"));

    promptLayout->addWidget(d->enableAIAnnotateCustomPromptCheck);
    promptLayout->addWidget(d->promptTextEdit);
    promptLayout->addWidget(d->resetPromptButton);
    advContainerLayout->addWidget(promptBox);
    
    advancedLayout->addWidget(advancedSettingsContainer);
    annotateTabLayout->addWidget(advancedBox);
    annotateTabLayout->addStretch();

    // --------------------------------------------------------

    d->tab->addTab(d->annotateTab, i18n("AI Annotate"));
    // Future tabs will be added here

    layout->addWidget(d->tab);
    layout->addStretch();

    setWidget(panel);
    setWidgetResizable(true);

    // --------------------------------------------------------

    connect(d->resetPromptButton, &QPushButton::clicked,
            this, [this]()
            {
                // Default prompt text
                QString defaultPrompt = QLatin1String("The tasks are to describe the image and to come up with a large set of keyword tags for it.\n\n"
                                                     "Write the Description using the active voice.\n\n"
                                                     "The Keywords must be one or two words each. Generate as many Keywords as possible using a controlled and consistent vocabulary.\n\n"
                                                     "For both Description and Keywords, make sure to include:\n\n"
                                                     " - Themes, concepts\n"
                                                     " - Items, animals, objects\n"
                                                     " - Structures, landmarks, setting\n"
                                                     " - Foreground and background elements\n"
                                                     " - Notable colors, textures, styles\n"
                                                     " - Actions, activities\n\n"
                                                     "Use ENGLISH only. Generate ONLY a JSON object with the keys Description and Keywords as follows {\"Description\": str, \"Keywords\": []}");

                d->promptTextEdit->setPlainText(defaultPrompt);
            });
            
    connect(d->enableAIAnnotateCheck, &QCheckBox::toggled,
            this, &SetupLLM::slotEnableAIAnnotateSettings);
            
    connect(d->enableAIAnnotateAdvSetCheck, &QCheckBox::toggled,
            this, &SetupLLM::slotEnableAIAnnotateAdvancedSettings);
            
    connect(d->enableAIAnnotateCustomPromptCheck, &QCheckBox::toggled,
            this, &SetupLLM::slotEnableAIAnnotateCustomPrompt);
            
    connect(d->enableAIAnnotateTagsCheck, &QCheckBox::toggled,
            this, &SetupLLM::slotEnableAIAnnotateMaxTags);

    // Initialize settings
    readSettings();    
}

SetupLLM::~SetupLLM()
{
    delete d;
}

void SetupLLM::setActiveTab(LLMTab tab)
{
    d->tab->setCurrentIndex((int)tab);
    d->activeTab = tab;
}

SetupLLM::LLMTab SetupLLM::activeTab() const
{
    return d->activeTab;
}

void SetupLLM::readSettings()
{
    ApplicationSettings* const settings = ApplicationSettings::instance();
    
    if (!settings)
    {
        return;
    }
    
    // Enable/disable checkboxes
    d->enableAIAnnotateCheck->setChecked(settings->getAIAnnotateEnabled());
    d->enableAIAnnotateAdvSetCheck->setChecked(settings->getAIAnnotateAdvSetEnabled());
    
    // Component checkboxes
    d->enableAIAnnotateDescriptionCheck->setChecked(settings->getAIAnnotateDescriptionEnabled());
    d->enableAIAnnotateTagsCheck->setChecked(settings->getAIAnnotateTagsEnabled());
    d->enableAIAnnotateAestheticCheck->setChecked(settings->getAIAnnotateAestheticEnabled());
    d->enableAIAnnotateCustomPromptCheck->setChecked(settings->getAIAnnotateCustomPromptEnabled());
    
    // Engine tab
    d->llmEngineCombo->setCurrentIndex(d->llmEngineCombo->findData(static_cast<int>(settings->getAIAnnotateEngine())));
    d->maxTagsSpinBox->setValue(settings->getAIAnnotateMaxTags());

    // Custom prompt
    d->promptTextEdit->setPlainText(settings->getAIAnnotatePrompt());
    
    // Generation settings
    d->temperatureInput->setValue(settings->getAIAnnotateTemperature());
    
    // Voice type
    d->aiVoiceTypeCombo->setCurrentIndex(d->aiVoiceTypeCombo->findData(static_cast<int>(settings->getAIAnnotateVoice())));
    
    // Text tones
    LLMProcessSettings::AITextTones tones = settings->getAIAnnotateTextTones();
    d->textToneFriendlyCheck->setChecked(tones & LLMProcessSettings::Friendly);
    d->textToneSeriousCheck->setChecked(tones & LLMProcessSettings::Serious);
    d->textToneHumorousCheck->setChecked(tones & LLMProcessSettings::Humorous);
    d->textToneEmpatheticCheck->setChecked(tones & LLMProcessSettings::Empathetic);
    d->textToneAuthoritativeCheck->setChecked(tones & LLMProcessSettings::Authoritative);
    d->textToneEncouragingCheck->setChecked(tones & LLMProcessSettings::Encouraging);
    d->textToneUrgentCheck->setChecked(tones & LLMProcessSettings::Urgent);
    d->textToneInspirationalCheck->setChecked(tones & LLMProcessSettings::Inspirational);
    d->textToneRespectfulCheck->setChecked(tones & LLMProcessSettings::Respectful);
    d->textToneConciseCheck->setChecked(tones & LLMProcessSettings::Concise);
    d->textToneDescriptiveCheck->setChecked(tones & LLMProcessSettings::Descriptive);
    d->textTonePersuasiveCheck->setChecked(tones & LLMProcessSettings::Persuasive);
    d->textToneDramaticCheck->setChecked(tones & LLMProcessSettings::Dramatic);
    d->textTonePoeticCheck->setChecked(tones & LLMProcessSettings::Poetic);
    d->textToneTechnicalCheck->setChecked(tones & LLMProcessSettings::Technical);
}

bool SetupLLM::checkSettings()
{
    bool validated = true;

    // No validation needed for tone checkboxes anymore

    return validated;
}

void SetupLLM::applySettings()
{
    ApplicationSettings* const settings = ApplicationSettings::instance();
    
    if (!settings)
    {
        return;
    }
    
    // Enable/disable toggles
    settings->setEnableAIAnnotate(d->enableAIAnnotateCheck->isChecked());
    settings->setEnableAIAnnotateAdvSet(d->enableAIAnnotateAdvSetCheck->isChecked());
    
    // Component toggles
    settings->setEnableAIAnnotateDescription(d->enableAIAnnotateDescriptionCheck->isChecked());
    settings->setEnableAIAnnotateTags(d->enableAIAnnotateTagsCheck->isChecked());
    settings->setEnableAIAnnotateAesthetic(d->enableAIAnnotateAestheticCheck->isChecked());
    settings->setEnableAIAnnotateCustomPrompt(d->enableAIAnnotateCustomPromptCheck->isChecked());
    
    // Engine tab
    settings->setAIAnnotateEngine(static_cast<LLMProcessManagerKoboldCpp::LLMEngineType>(d->llmEngineCombo->currentData().toInt()));
    settings->setAIAnnotateMaxTags(d->maxTagsSpinBox->value());

    // Custom prompt
    settings->setAIAnnotatePrompt(d->promptTextEdit->toPlainText());

    // Generation settings
    settings->setAIAnnotateTemperature(d->temperatureInput->value());
    
    // Voice style
    settings->setAIAnnotateVoice(static_cast<LLMProcessSettings::AIVoiceType>(d->aiVoiceTypeCombo->currentData().toInt()));
    
    // Text tones
    LLMProcessSettings::AITextTones tones = LLMProcessSettings::NoTone;
    if (d->textToneFriendlyCheck->isChecked())      tones |= LLMProcessSettings::Friendly;
    if (d->textToneSeriousCheck->isChecked())       tones |= LLMProcessSettings::Serious;
    if (d->textToneHumorousCheck->isChecked())      tones |= LLMProcessSettings::Humorous;
    if (d->textToneEmpatheticCheck->isChecked())    tones |= LLMProcessSettings::Empathetic;
    if (d->textToneAuthoritativeCheck->isChecked()) tones |= LLMProcessSettings::Authoritative;
    if (d->textToneEncouragingCheck->isChecked())   tones |= LLMProcessSettings::Encouraging;
    if (d->textToneUrgentCheck->isChecked())        tones |= LLMProcessSettings::Urgent;
    if (d->textToneInspirationalCheck->isChecked()) tones |= LLMProcessSettings::Inspirational;
    if (d->textToneRespectfulCheck->isChecked())    tones |= LLMProcessSettings::Respectful;
    if (d->textToneConciseCheck->isChecked())       tones |= LLMProcessSettings::Concise;
    if (d->textToneDescriptiveCheck->isChecked())   tones |= LLMProcessSettings::Descriptive;
    if (d->textTonePersuasiveCheck->isChecked())    tones |= LLMProcessSettings::Persuasive;
    if (d->textToneDramaticCheck->isChecked())      tones |= LLMProcessSettings::Dramatic;
    if (d->textTonePoeticCheck->isChecked())        tones |= LLMProcessSettings::Poetic;
    if (d->textToneTechnicalCheck->isChecked())     tones |= LLMProcessSettings::Technical;
    settings->setAIAnnotateTextTones(tones);

    settings->saveSettings();
}

void SetupLLM::slotEnableAIAnnotateSettings(bool enable)
{
    d->llmEngineCombo->setEnabled(enable);
    d->maxTagsSpinBox->setEnabled(enable);
    d->maxTagsLabel->setEnabled(enable);
    d->enableAIAnnotateAdvSetCheck->setEnabled(enable);
    d->enableAIAnnotateDescriptionCheck->setEnabled(enable);
    d->enableAIAnnotateTagsCheck->setEnabled(enable);
    d->enableAIAnnotateAestheticCheck->setEnabled(enable);
    d->aiVoiceTypeCombo->setEnabled(enable);
    d->aiVoiceTypeLabel->setEnabled(enable);

    // Voice style and text tones now depend directly on main checkbox
    d->aiVoiceTypeCombo->setEnabled(enable);
    d->aiVoiceTypeLabel->setEnabled(enable);
    
    d->textToneFriendlyCheck->setEnabled(enable);
    d->textToneSeriousCheck->setEnabled(enable);
    d->textToneHumorousCheck->setEnabled(enable);
    d->textToneEmpatheticCheck->setEnabled(enable);
    d->textToneAuthoritativeCheck->setEnabled(enable);
    d->textToneEncouragingCheck->setEnabled(enable);
    d->textToneUrgentCheck->setEnabled(enable);
    d->textToneInspirationalCheck->setEnabled(enable);
    d->textToneRespectfulCheck->setEnabled(enable);
    d->textToneConciseCheck->setEnabled(enable);
    d->textToneDescriptiveCheck->setEnabled(enable);
    d->textTonePersuasiveCheck->setEnabled(enable);
    d->textToneDramaticCheck->setEnabled(enable);
    d->textTonePoeticCheck->setEnabled(enable);
    d->textToneTechnicalCheck->setEnabled(enable);

    slotEnableAIAnnotateAdvancedSettings(d->enableAIAnnotateAdvSetCheck->isChecked());
    slotEnableAIAnnotateCustomPrompt(d->enableAIAnnotateCustomPromptCheck->isChecked());
    slotEnableAIAnnotateMaxTags(d->enableAIAnnotateTagsCheck->isChecked());
}

void SetupLLM::slotEnableAIAnnotateMaxTags(bool enable)
{
    d->maxTagsSpinBox->setEnabled(enable);
    d->maxTagsLabel->setEnabled(enable);
}

void SetupLLM::slotEnableAIAnnotateAdvancedSettings(bool enable)
{
    d->enableAIAnnotateCustomPromptCheck->setEnabled(enable && d->enableAIAnnotateAdvSetCheck->isEnabled());
    d->promptTextEdit->setEnabled(enable && d->enableAIAnnotateCustomPromptCheck->isChecked() && d->enableAIAnnotateCustomPromptCheck->isEnabled());
    d->resetPromptButton->setEnabled(enable && d->enableAIAnnotateCustomPromptCheck->isChecked() && d->enableAIAnnotateCustomPromptCheck->isEnabled());
    d->temperatureInput->setEnabled(enable && d->enableAIAnnotateAdvSetCheck->isEnabled());
    d->temperatureLabel->setEnabled(enable && d->enableAIAnnotateAdvSetCheck->isEnabled());
}

void SetupLLM::slotEnableAIAnnotateCustomPrompt(bool enable)
{
    d->promptTextEdit->setEnabled(enable && d->enableAIAnnotateCustomPromptCheck->isEnabled());
    d->resetPromptButton->setEnabled(enable && d->enableAIAnnotateCustomPromptCheck->isEnabled());
}

} // namespace Digikam
