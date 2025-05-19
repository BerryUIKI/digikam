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
#include <QLineEdit>
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
#include "dnuminput.h"
#include "dnnmodelmanager.h"
#include "llmprocessmanagerkoboldcpp.h"
#include "llmprocesssettings.h"

namespace Digikam
{

class Q_DECL_HIDDEN SetupLLM::Private
{
public:

    explicit Private()
    {
    }

    QTabWidget*         tab                                     = nullptr;
    QWidget*            annotateTab                             = nullptr;
    
    DIntNumInput*       maxTagsSpinBox                          = nullptr;
    QLabel*             maxTagsLabel                            = nullptr;
    
    // External LLM
    QLabel*             engineLabel                             = nullptr;
    QComboBox*          llmEngineCombo                          = nullptr;
    QLineEdit*          externalLLMUrlEdit                      = nullptr;
    QLabel*             externalLLMUrlLabel                     = nullptr;
    QLineEdit*          externalLLMPortEdit                     = nullptr;
    QLabel*             externalLLMPortLabel                    = nullptr;

    // Custom prompt
    QTextEdit*          promptTextEdit                          = nullptr;
    QPushButton*        resetPromptButton                       = nullptr;

    // Generation settings
    DDoubleNumInput*    temperatureInput                        = nullptr;
    QLabel*             temperatureLabel                        = nullptr;
    DIntNumInput*       maxTokensSpinBox                        = nullptr;
    QLabel*             maxTokensLabel                          = nullptr;

    // Enable AI Annotate
    QCheckBox*          enableAIAnnotateCheck                   = nullptr;
    QCheckBox*          enableAIAnnotateAdvSetCheck             = nullptr;
    
    // Generation components checkboxes
    QCheckBox*          enableAIAnnotateDescriptionCheck        = nullptr;
    QCheckBox*          enableAIAnnotateTagsCheck               = nullptr;
    QCheckBox*          enableAIAnnotateCustomPromptCheck       = nullptr;
    QCheckBox*          enableAIAnnotateExternalLLMCheck        = nullptr;
    
    // Voice style
    QComboBox*          aiVoiceTypeCombo                        = nullptr;
    QLabel*             aiVoiceTypeLabel                        = nullptr;
    QComboBox*          aiVoicePerspectiveCombo                 = nullptr;
    QLabel*             aiVoicePerspectiveLabel                 = nullptr;
    
    // Text tones
    QLabel*             textTonesLabel                          = nullptr;
    QCheckBox*          textToneFriendlyCheck                   = nullptr;
    QCheckBox*          textToneSeriousCheck                    = nullptr;
    QCheckBox*          textToneHumorousCheck                   = nullptr;
    QCheckBox*          textToneEmpatheticCheck                 = nullptr;
    QCheckBox*          textToneAuthoritativeCheck              = nullptr;
    QCheckBox*          textToneEncouragingCheck                = nullptr;
    QCheckBox*          textToneUrgentCheck                     = nullptr;
    QCheckBox*          textToneInspirationalCheck              = nullptr;
    QCheckBox*          textToneRespectfulCheck                 = nullptr;
    QCheckBox*          textToneConciseCheck                    = nullptr;
    QCheckBox*          textToneDescriptiveCheck                = nullptr;
    QCheckBox*          textTonePersuasiveCheck                 = nullptr;
    QCheckBox*          textToneDramaticCheck                   = nullptr;
    QCheckBox*          textTonePoeticCheck                     = nullptr;
    QCheckBox*          textToneTechnicalCheck                  = nullptr;

    LLMTab              activeTab                               = Annotate;
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
                                  "and assigns <b>accurate keyword tags</b>, making searching and categorization effortless."));
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    descriptionLabel->setLineWidth(1);
    descriptionLabel->setMargin(10);
    
    annotateTabLayout->addWidget(descriptionLabel);
    annotateTabLayout->addSpacing(spacing);

    // Generation settings
    QGroupBox* const tagGenBox     = new QGroupBox(i18n("Generation Settings"), d->annotateTab);
    QGridLayout* const tagGenLayout = new QGridLayout(tagGenBox);

    // Generation components checkboxes

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
    
    tagGenLayout->addWidget(d->enableAIAnnotateDescriptionCheck, 3, 0);
    tagGenLayout->addWidget(d->enableAIAnnotateTagsCheck, 4, 0);
    tagGenLayout->addWidget(d->maxTagsLabel, 4, 1);
    tagGenLayout->addWidget(d->maxTagsSpinBox, 4, 2);
    
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
    
    // Voice perspective dropdown
    d->aiVoicePerspectiveLabel = new QLabel(i18n("Voice Perspective:"), textTonesBox);
    d->aiVoicePerspectiveLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->aiVoicePerspectiveCombo = new QComboBox(textTonesBox);
    d->aiVoicePerspectiveCombo->addItem(i18n("None"), static_cast<int>(LLMProcessSettings::NoPerspective));
    d->aiVoicePerspectiveCombo->addItem(i18n("First Person"), static_cast<int>(LLMProcessSettings::FirstPerson));
    d->aiVoicePerspectiveCombo->addItem(i18n("Second Person"), static_cast<int>(LLMProcessSettings::SecondPerson));
    d->aiVoicePerspectiveCombo->addItem(i18n("Third Person"), static_cast<int>(LLMProcessSettings::ThirdPerson));
    d->aiVoicePerspectiveCombo->setToolTip(i18n("Select the narrative perspective used in AI-generated descriptions"));

    QHBoxLayout* const voiceLayout = new QHBoxLayout();
    voiceLayout->addWidget(d->aiVoiceTypeLabel);
    voiceLayout->addWidget(d->aiVoiceTypeCombo);
    voiceLayout->addStretch();
    textTonesLayout->addLayout(voiceLayout, 0, 0, 1, 3);

    QHBoxLayout* const perspectiveLayout = new QHBoxLayout();
    perspectiveLayout->addWidget(d->aiVoicePerspectiveLabel);
    perspectiveLayout->addWidget(d->aiVoicePerspectiveCombo);
    perspectiveLayout->addStretch();
    textTonesLayout->addLayout(perspectiveLayout, 1, 0, 1, 3);

    d->textTonesLabel = new QLabel(i18n("Select one or more tones to apply to AI-generated text:"), textTonesBox);
    d->textTonesLabel->setWordWrap(true);
    textTonesLayout->addWidget(d->textTonesLabel, 2, 0, 1, 3);

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
    textTonesLayout->addWidget(d->textToneFriendlyCheck, 3, 0);
    textTonesLayout->addWidget(d->textToneSeriousCheck, 3, 1);
    textTonesLayout->addWidget(d->textToneHumorousCheck, 3, 2);
    textTonesLayout->addWidget(d->textToneEmpatheticCheck, 4, 0);
    textTonesLayout->addWidget(d->textToneAuthoritativeCheck, 4, 1);
    textTonesLayout->addWidget(d->textToneEncouragingCheck, 4, 2);
    textTonesLayout->addWidget(d->textToneUrgentCheck, 5, 0);
    textTonesLayout->addWidget(d->textToneInspirationalCheck, 5, 1);
    textTonesLayout->addWidget(d->textToneRespectfulCheck, 5, 2);
    textTonesLayout->addWidget(d->textToneConciseCheck, 6, 0);
    textTonesLayout->addWidget(d->textToneDescriptiveCheck, 6, 1);
    textTonesLayout->addWidget(d->textTonePersuasiveCheck, 6, 2);
    textTonesLayout->addWidget(d->textToneDramaticCheck, 7, 0);
    textTonesLayout->addWidget(d->textTonePoeticCheck, 7, 1);
    textTonesLayout->addWidget(d->textToneTechnicalCheck, 7, 2);

    tagGenLayout->addWidget(textTonesBox, 6, 0, 1, 3);

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
    d->temperatureInput->setRange(0.1, 1.9, 0.2);
    d->temperatureInput->setValue(0.5);  // Default is 0.5 for a balance of consistency and creativity
    d->temperatureInput->setToolTip(i18n("Controls randomness in generation (higher = more creative, lower = more deterministic)"));
    tempLayout->addWidget(d->temperatureLabel);
    tempLayout->addWidget(d->temperatureInput);
    tempLayout->addStretch();
    advContainerLayout->addLayout(tempLayout);
    
    // Max tokens settings 
    QHBoxLayout* const maxTokensLayout = new QHBoxLayout();
    d->maxTokensLabel = new QLabel(i18n("Max Tokens:"), advancedSettingsContainer);
    d->maxTokensLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->maxTokensSpinBox = new DIntNumInput(advancedSettingsContainer);
    d->maxTokensSpinBox->setRange(250, 500, 50);
    d->maxTokensSpinBox->setValue(300);  // Default is 300
    d->maxTokensSpinBox->setToolTip(i18n("Maximum number of tokens (words) to generate as a response"));
    maxTokensLayout->addWidget(d->maxTokensLabel);
    maxTokensLayout->addWidget(d->maxTokensSpinBox);
    maxTokensLayout->addStretch();
    advContainerLayout->addLayout(maxTokensLayout);

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

    // External LLM Service section
    QGroupBox* const externalLLMBox = new QGroupBox(i18n("External LLM Service"), d->annotateTab);
    QVBoxLayout* const externalLLMLayout = new QVBoxLayout(externalLLMBox);

    // Checkbox to enable external LLM service
    d->enableAIAnnotateExternalLLMCheck = new QCheckBox(i18n("Enable External LLM Service"), externalLLMBox);
    d->enableAIAnnotateExternalLLMCheck->setToolTip(i18n("Enable or disable the external LLM service"));
    externalLLMLayout->addWidget(d->enableAIAnnotateExternalLLMCheck);

    // Container for external LLM settings
    QWidget* const externalLLMSettingsContainer = new QWidget(externalLLMBox);
    QVBoxLayout* const externalLLMContainerLayout = new QVBoxLayout(externalLLMSettingsContainer);
    externalLLMContainerLayout->setContentsMargins(20, 10, 0, 0);  // Add left margin for indentation

    // LLM Engine selection
    QHBoxLayout* const engineLayout = new QHBoxLayout();

    d->engineLabel = new QLabel(i18n("External LLM Service API Style:"), externalLLMSettingsContainer);
    d->engineLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    d->llmEngineCombo = new QComboBox(externalLLMSettingsContainer);
    d->llmEngineCombo->addItem(i18n("KoboldCPP"), static_cast<int>(LLMProcessSettings::KoboldCpp));
    d->llmEngineCombo->addItem(i18n("LlamaCPP"), static_cast<int>(LLMProcessSettings::LlamaCpp));
    d->llmEngineCombo->setToolTip(i18n("Select the LLM engine to use for text generation"));

    engineLayout->addWidget(d->engineLabel);
    engineLayout->addWidget(d->llmEngineCombo);
    engineLayout->addStretch();

    // URL for external LLM service
    QHBoxLayout* const urlLayout = new QHBoxLayout();

    d->externalLLMUrlLabel = new QLabel(i18n("Host URL:"), externalLLMSettingsContainer);
    d->externalLLMUrlLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    d->externalLLMUrlEdit = new QLineEdit(externalLLMSettingsContainer);
    d->externalLLMUrlEdit->setPlaceholderText(i18n("http://localhost"));
    d->externalLLMUrlEdit->setToolTip(i18n("Enter the URL for the external LLM service without the port number (port is specified separately)"));

    urlLayout->addWidget(d->externalLLMUrlLabel);
    urlLayout->addWidget(d->externalLLMUrlEdit);

    // Port number for external LLM service
    QHBoxLayout* const portLayout = new QHBoxLayout();

    d->externalLLMPortLabel = new QLabel(i18n("Port:"), externalLLMSettingsContainer);
    d->externalLLMPortLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    d->externalLLMPortEdit = new QLineEdit(externalLLMSettingsContainer);
    d->externalLLMPortEdit->setPlaceholderText(i18n("5000"));
    d->externalLLMPortEdit->setToolTip(i18n("Enter the port number for the external LLM service (1-65535)"));
    d->externalLLMPortEdit->setValidator(new QIntValidator(1, 65535, d->externalLLMPortEdit));
    d->externalLLMPortEdit->setMaximumWidth(100);

    portLayout->addWidget(d->externalLLMPortLabel);
    portLayout->addWidget(d->externalLLMPortEdit);
    portLayout->addStretch();

    externalLLMContainerLayout->addLayout(engineLayout);
    externalLLMContainerLayout->addLayout(urlLayout);
    externalLLMContainerLayout->addLayout(portLayout);
    externalLLMLayout->addWidget(externalLLMSettingsContainer);

    annotateTabLayout->addWidget(externalLLMBox);
    annotateTabLayout->addSpacing(spacing);

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
                QString defaultPrompt = QLatin1String("Write the Description using the active voice.\n\n"
                                                      "The Keywords must be one or two words each. Generate no more than 15 Keywords using a controlled and consistent vocabulary.\n\n"
                                                      "For both Description and Keywords, make sure to include:\n\n"
                                                      " - Themes, concepts\n"
                                                      " - Items, animals, objects\n"
                                                      " - Structures, landmarks, setting\n"
                                                      " - Foreground and background elements\n"
                                                      " - Notable colors, textures, styles\n"
                                                      " - Actions, activities\n\n"
                                                      "If humans are present, include:\n"
                                                      " - Physical appearance\n"
                                                      " - Gender\n"
                                                      " - Clothing\n"
                                                      " - Age range\n"
                                                      " - Visibly apparent ancestry\n"
                                                      " - Occupation/role\n"
                                                      " - Relationships between individuals\n"
                                                      " - Emotions, expressions, body language\n\n"
                                                      "Use ENGLISH only as spoken in Great Britian.\n");

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
            
    connect(d->enableAIAnnotateExternalLLMCheck, &QCheckBox::toggled,
            this, &SetupLLM::slotEnableAIAnnotateExternalLLM);

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
    d->enableAIAnnotateCustomPromptCheck->setChecked(settings->getAIAnnotateCustomPromptEnabled());
    d->enableAIAnnotateExternalLLMCheck->setChecked(settings->getAIAnnotateExternalLLMEnabled());
    
    // Engine tab
    d->llmEngineCombo->setCurrentIndex(d->llmEngineCombo->findData(static_cast<int>(settings->getAIAnnotateEngine())));
    d->maxTagsSpinBox->setValue(settings->getAIAnnotateMaxTags());
    
    // External LLM
    d->externalLLMUrlEdit->setText(settings->getAIAnnotateExternalLLMUrl());
    d->externalLLMPortEdit->setText(QString::number(settings->getAIAnnotateExternalLLMPort()));

    // Custom prompt
    d->promptTextEdit->setPlainText(settings->getAIAnnotateCustomPrompt());
    
    // Generation settings
    d->temperatureInput->setValue(settings->getAIAnnotateTemperature());
    d->maxTokensSpinBox->setValue(settings->getAIAnnotateMaxTokens());
    
    // Voice type and perspective
    d->aiVoiceTypeCombo->setCurrentIndex(d->aiVoiceTypeCombo->findData(static_cast<int>(settings->getAIAnnotateVoice())));
    d->aiVoicePerspectiveCombo->setCurrentIndex(d->aiVoicePerspectiveCombo->findData(static_cast<int>(settings->getAIAnnotateVoicePerspective())));
    
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
    settings->setAIAnnotateEnabled(d->enableAIAnnotateCheck->isChecked());
    settings->setAIAnnotateEnableAdvSet(d->enableAIAnnotateAdvSetCheck->isChecked());
    
    // Component toggles
    settings->setAIAnnotateEnableDescription(d->enableAIAnnotateDescriptionCheck->isChecked());
    settings->setAIAnnotateEnableTags(d->enableAIAnnotateTagsCheck->isChecked());
    settings->setAIAnnotateEnableCustomPrompt(d->enableAIAnnotateCustomPromptCheck->isChecked());
    settings->setAIAnnotateEnableExternalLLM(d->enableAIAnnotateExternalLLMCheck->isChecked());
    
    // Engine tab
    settings->setAIAnnotateEngine(static_cast<LLMProcessSettings::LLMEngineType>(d->llmEngineCombo->currentData().toInt()));
    settings->setAIAnnotateMaxTags(d->maxTagsSpinBox->value());
    
    // External LLM
    settings->setAIAnnotateExternalLLMUrl(d->externalLLMUrlEdit->text());
    
    // Convert port text to integer, with fallback to 5000 if conversion fails
    bool ok = false;
    int port = d->externalLLMPortEdit->text().toInt(&ok);
    settings->setAIAnnotateExternalLLMPort(ok ? port : 5000);

    // Custom prompt
    settings->setAIAnnotatePrompt(d->promptTextEdit->toPlainText());

    // Generation settings
    settings->setAIAnnotateTemperature(d->temperatureInput->value());
    settings->setAIAnnotateMaxTokens(d->maxTokensSpinBox->value());
    
    // Voice style and perspective
    settings->setAIAnnotateVoice(static_cast<LLMProcessSettings::AIVoiceType>(d->aiVoiceTypeCombo->currentData().toInt()));
    settings->setAIAnnotateVoicePerspective(static_cast<LLMProcessSettings::AIVoicePerspective>(d->aiVoicePerspectiveCombo->currentData().toInt()));
    
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
    d->enableAIAnnotateExternalLLMCheck->setEnabled(enable);
    d->maxTagsSpinBox->setEnabled(enable);
    d->maxTagsLabel->setEnabled(enable);
    d->enableAIAnnotateAdvSetCheck->setEnabled(enable);
    d->enableAIAnnotateDescriptionCheck->setEnabled(enable);
    d->enableAIAnnotateTagsCheck->setEnabled(enable);
    d->aiVoiceTypeCombo->setEnabled(enable);
    d->aiVoiceTypeLabel->setEnabled(enable);
    d->aiVoicePerspectiveCombo->setEnabled(enable);
    d->aiVoicePerspectiveLabel->setEnabled(enable);

    slotEnableAIAnnotateAdvancedSettings(d->enableAIAnnotateAdvSetCheck->isChecked());
    slotEnableAIAnnotateCustomPrompt(d->enableAIAnnotateCustomPromptCheck->isChecked());
    slotEnableAIAnnotateMaxTags(d->enableAIAnnotateTagsCheck->isChecked());
    slotEnableAIAnnotateExternalLLM(d->enableAIAnnotateExternalLLMCheck->isChecked());
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
    d->maxTokensSpinBox->setEnabled(enable && d->enableAIAnnotateAdvSetCheck->isEnabled());
    d->maxTokensLabel->setEnabled(enable && d->enableAIAnnotateAdvSetCheck->isEnabled());
}

void SetupLLM::slotEnableAIAnnotateCustomPrompt(bool enable)
{
    d->promptTextEdit->setEnabled(enable && d->enableAIAnnotateCustomPromptCheck->isEnabled());
    d->resetPromptButton->setEnabled(enable && d->enableAIAnnotateCustomPromptCheck->isEnabled());

    // Enable voice style and text tones if custom prompt is not checked or custom prompt is disabled
    bool enableVoiceAndTones = (!enable || !d->enableAIAnnotateDescriptionCheck->isEnabled());
    d->aiVoiceTypeCombo->setEnabled(enableVoiceAndTones);
    d->aiVoiceTypeLabel->setEnabled(enableVoiceAndTones);
    d->aiVoicePerspectiveCombo->setEnabled(enableVoiceAndTones);
    d->aiVoicePerspectiveLabel->setEnabled(enableVoiceAndTones);
    
    d->textTonesLabel->setEnabled(enableVoiceAndTones);
    d->textToneFriendlyCheck->setEnabled(enableVoiceAndTones);
    d->textToneSeriousCheck->setEnabled(enableVoiceAndTones);
    d->textToneHumorousCheck->setEnabled(enableVoiceAndTones);
    d->textToneEmpatheticCheck->setEnabled(enableVoiceAndTones);
    d->textToneAuthoritativeCheck->setEnabled(enableVoiceAndTones);
    d->textToneEncouragingCheck->setEnabled(enableVoiceAndTones);
    d->textToneUrgentCheck->setEnabled(enableVoiceAndTones);
    d->textToneInspirationalCheck->setEnabled(enableVoiceAndTones);
    d->textToneRespectfulCheck->setEnabled(enableVoiceAndTones);
    d->textToneConciseCheck->setEnabled(enableVoiceAndTones);
    d->textToneDescriptiveCheck->setEnabled(enableVoiceAndTones);
    d->textTonePersuasiveCheck->setEnabled(enableVoiceAndTones);
    d->textToneDramaticCheck->setEnabled(enableVoiceAndTones);
    d->textTonePoeticCheck->setEnabled(enableVoiceAndTones);
    d->textToneTechnicalCheck->setEnabled(enableVoiceAndTones);
}

void SetupLLM::slotEnableAIAnnotateExternalLLM(bool enable)
{
    bool isEnabled = enable && d->enableAIAnnotateExternalLLMCheck->isEnabled();
    d->engineLabel->setEnabled(isEnabled);
    d->llmEngineCombo->setEnabled(isEnabled);
    d->externalLLMUrlEdit->setEnabled(isEnabled);
    d->externalLLMUrlLabel->setEnabled(isEnabled);
    d->externalLLMPortEdit->setEnabled(isEnabled);
    d->externalLLMPortLabel->setEnabled(isEnabled);
}

} // namespace Digikam
