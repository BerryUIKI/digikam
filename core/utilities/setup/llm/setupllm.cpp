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
#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
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
#include "digikam_globals.h"
#include "applicationsettings.h"
#include "dnuminput.h"
#include "dnnmodelmanager.h"
#include "llmprocessmanagerkoboldcpp.h"
#include "llmprocesssettings.h"
#include "llmengineinstalldialog.h"

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
    QLabel*             openAIModelLabel                        = nullptr;
    QComboBox*          openAIModelCombo                        = nullptr;
    
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
    QPushButton*        initializeLocalLLMButton                = nullptr;
    
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

    LLMProcessManagerKoboldCpp* localLLMInitManager             = nullptr;
    QString             localLLMInitLog;
    QPointer<QDialog>   localLLMInitLogDialog;
    QPointer<QPlainTextEdit> localLLMInitLogTextEdit;
    bool                localLLMInitInProgress                  = false;
    bool                localLLMInitReady                       = false;

    LLMTab              activeTab                               = Annotate;
};

SetupLLM::SetupLLM(QWidget* const parent)
    : QScrollArea(parent),
      d(new Private)
{
    const int spacing                   = layoutSpacing();

    QWidget* const panel                = new QWidget(viewport());
    QVBoxLayout* const layout           = new QVBoxLayout(panel);

    d->tab                              = new QTabWidget(panel);
    d->annotateTab                      = new QWidget(d->tab);

    // --------------------------------------------------------

    QVBoxLayout* const annotateTabLayout = new QVBoxLayout(d->annotateTab);

    // Add the Enable AI Annotate checkbox and Install LLM button to the top of the tab
    QWidget* const topRowWidget         = new QWidget(d->annotateTab);
    QHBoxLayout* const topRowLayout     = new QHBoxLayout(topRowWidget);
    topRowLayout->setContentsMargins(0, 0, 0, 0);

    // Enable AI Annotate checkbox
    d->enableAIAnnotateCheck            = new QCheckBox(i18n("Enable AI Annotate (requires restart)"), topRowWidget);
    d->enableAIAnnotateCheck->setToolTip(i18n("Enable or disable the AI Annotate feature"));
    topRowLayout->addWidget(d->enableAIAnnotateCheck);

    topRowLayout->addStretch(); // Add stretch before the button to push it to the right
    
    // Add Install llama.cpp button to the same row, right-justified
    QPushButton* const installButton    = new QPushButton(i18n("Install LLM engine"), topRowWidget);
    installButton->setToolTip(i18n("Download and install llama.cpp binaries for local AI inference"));
    installButton->setEnabled(false); // Initially disabled
    topRowLayout->addWidget(installButton);

    d->initializeLocalLLMButton         = new QPushButton(i18n("Initialize Local LLM Engine"), topRowWidget);
    d->initializeLocalLLMButton->setToolTip(i18n("Start llama-server to initialize the local LLM engine, validate it with a health check, and then stop it"));
    d->initializeLocalLLMButton->setEnabled(false);

    const int llmButtonWidth = qMax(installButton->sizeHint().width(), d->initializeLocalLLMButton->sizeHint().width());
    installButton->setFixedWidth(llmButtonWidth);
    d->initializeLocalLLMButton->setFixedWidth(llmButtonWidth);

    annotateTabLayout->addWidget(topRowWidget);

    QHBoxLayout* const openAIModelLayout = new QHBoxLayout();
    d->openAIModelLabel                  = new QLabel(i18n("OpenAI Model for Tagging:"), d->annotateTab);
    d->openAIModelLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->openAIModelCombo                  = new QComboBox(d->annotateTab);
    d->openAIModelCombo->setToolTip(i18n("Select the OpenAI-compatible model used for AI tag generation"));

    DNNModelManager* const dnnModelManager = DNNModelManager::instance();
    const QList<QPair<QString, QStringList> > modelList = dnnModelManager->getModelList(DNNModelUsage::DNNUsageObjectDetection);

    for (const auto& model : modelList)
    {
        DNNModelBase* const modelPtr = dnnModelManager->getModel(model.first, DNNModelUsage::DNNUsageObjectDetection);

        if (!modelPtr || (modelPtr->info.classifier.compare(QLatin1String("openai"), Qt::CaseInsensitive) != 0))
        {
            continue;
        }

        const QString displayName = (model.second.isEmpty() ? model.first : model.second.first());
        d->openAIModelCombo->addItem(displayName, model.first);
    }

    if (d->openAIModelCombo->count() == 0)
    {
        d->openAIModelCombo->addItem(i18n("No OpenAI models available"), QString());
        d->openAIModelCombo->setEnabled(false);
    }

    openAIModelLayout->addWidget(d->openAIModelLabel);
    openAIModelLayout->addWidget(d->openAIModelCombo);
    openAIModelLayout->addStretch();
    openAIModelLayout->addWidget(d->initializeLocalLLMButton);
    annotateTabLayout->addLayout(openAIModelLayout);
    annotateTabLayout->addSpacing(spacing);

    // Description label to explain the feature
    QLabel* const descriptionLabel      = new QLabel(d->annotateTab);
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
    QGroupBox* const tagGenBox          = new QGroupBox(i18n("Generation Settings"), d->annotateTab);
    QGridLayout* const tagGenLayout     = new QGridLayout(tagGenBox);

    // Generation components checkboxes

    // Generation components checkboxes
    d->enableAIAnnotateDescriptionCheck = new QCheckBox(i18n("Enable Description Generation"), tagGenBox);
    d->enableAIAnnotateDescriptionCheck->setToolTip(i18n("Generate detailed descriptions of image contents"));
    
    d->enableAIAnnotateTagsCheck        = new QCheckBox(i18n("Enable Tag Generation"), tagGenBox);
    d->enableAIAnnotateTagsCheck->setToolTip(i18n("Generate tags/keywords for image categorization"));
    
    // Max Tags settings
    d->maxTagsLabel                     = new QLabel(i18n("Maximum tags:"), tagGenBox);
    d->maxTagsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    d->maxTagsSpinBox                   = new DIntNumInput(tagGenBox);
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
    d->temperatureInput->setValue(AIAnnotate::DEFAULT_TEMPERATURE);
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
    d->maxTokensSpinBox->setRange(256, 16384, 256);
    d->maxTokensSpinBox->setValue(AIAnnotate::DEFAULT_MAX_TOKENS);
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
                // Default prompt text: matches the output of createPrompt() with default settings
                // (Neutral voice, no perspective, no tone, 15 max tags, English/United States)
                QString defaultPrompt = QLatin1String("You will look at an image and produce one Description and one list of no more than 15 objective Keywords that best describe the image.\n\n"
                                                      "Write the Description in active, neutral voice. Use a neutral tone.\n\n"
                                                      "Each keyword must be one or two words, use a consistent and objective vocabulary, and use lower case unless the term is a proper noun.\n\n"
                                                      "The Description and the Keywords must cover:\n"
                                                      "themes and concepts\n"
                                                      "items, animals, and objects\n"
                                                      "structures, landmarks, and setting\n"
                                                      "foreground and background elements\n"
                                                      "notable colors, textures, and styles\n"
                                                      "actions and activities\n\n"
                                                      "If humans appear, describe:\n"
                                                      "physical appearance\n"
                                                      "gender\n"
                                                      "clothing\n"
                                                      "age range\n"
                                                      "visibly apparent ancestry\n"
                                                      "occupation or role\n"
                                                      "relationships between people\n"
                                                      "emotions, expressions, and body language\n\n"
                                                      "Use English as spoken in United States.\n\n"
                                                      "Output only a JSON object in this exact structure:\n"
                                                      "{\"Description\": \"...\", \"Keywords\": [\"...\", \"...\"]}\n");

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

    // --- Connect Enable AI Annotate checkbox to Install llama.cpp button ---
    connect(d->enableAIAnnotateCheck, &QCheckBox::toggled,
            installButton, &QPushButton::setEnabled);
            
    connect(installButton, &QPushButton::clicked,
            this, &SetupLLM::slotShowInstallDialog);

        connect(d->initializeLocalLLMButton, &QPushButton::clicked,
            this, &SetupLLM::slotInitializeLocalLLMService);

    // Initialize settings

    readSettings();
    refreshInitializeButtonState();

    // call the slot to set the initial state of the settings

    slotEnableAIAnnotateSettings(d->enableAIAnnotateCheck->isChecked());
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

    const QString selectedOpenAIModel = settings->getAIAnnotateOpenAIModel();

    if (!selectedOpenAIModel.isEmpty())
    {
        const int selectedModelIndex = d->openAIModelCombo->findData(selectedOpenAIModel);

        if (selectedModelIndex >= 0)
        {
            d->openAIModelCombo->setCurrentIndex(selectedModelIndex);
        }
    }
    
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

    refreshInitializeButtonState();
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
    settings->setAIAnnotateOpenAIModel(d->openAIModelCombo->currentData().toString());
    
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

void SetupLLM::refreshInitializeButtonState()
{
    if (!d->initializeLocalLLMButton)
    {
        return;
    }

#ifdef Q_OS_WIN
    const QString executableName = QStringLiteral("llama-server.exe");
#else
    const QString executableName = QStringLiteral("llama-server");
#endif

    const QString executablePath = QDir::toNativeSeparators(LLMProcessManager::getBaseEnginePath() +
                                                             QStringLiteral("/llamacpp/") + executableName);
    const QFileInfo executableInfo(executablePath);
    const bool llamaCppDownloaded = executableInfo.exists() && executableInfo.isFile() && executableInfo.isExecutable();

    d->initializeLocalLLMButton->setEnabled(!d->localLLMInitInProgress && llamaCppDownloaded);

    if (d->localLLMInitInProgress)
    {
        d->initializeLocalLLMButton->setText(i18n("Initializing..."));
        d->initializeLocalLLMButton->setToolTip(i18n("Initialization is currently running"));
    }
    else
    {
        d->initializeLocalLLMButton->setText(i18n("Initialize Local LLM Engine"));

        if (llamaCppDownloaded)
        {
            d->initializeLocalLLMButton->setToolTip(i18n("Start llama-server to initialize the local LLM engine, validate it with a health check, and then stop it"));
        }
        else
        {
            d->initializeLocalLLMButton->setToolTip(i18n("llama.cpp is not installed. Install the LLM engine first."));
        }
    }
}

LLMProcessSettings SetupLLM::createLocalLLMInitSettings(QString* errorMessage) const
{
    LLMProcessSettings llmSettings;

    if (errorMessage)
    {
        errorMessage->clear();
    }

    const QString modelName = d->openAIModelCombo->currentData().toString();

    if (modelName.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = i18n("No OpenAI-compatible model is selected for AI Annotate.");
        }

        return llmSettings;
    }

    DNNModelBase* const model = DNNModelManager::instance()->getModel(modelName,
                                                                       DNNModelUsage::DNNUsageImageClassification);

    if (!model)
    {
        if (errorMessage)
        {
            *errorMessage = i18n("Unable to load the selected AI Annotate model for initialization.");
        }

        return llmSettings;
    }

    llmSettings.modelPath               = model->info.fileName;
    llmSettings.mmprojPath              = model->info.configName;
    llmSettings.chatCompletionsAdapter  = model->info.preprocessor;
    llmSettings.engineType              = LLMProcessSettings::LlamaCpp;
    llmSettings.external                = false;

    return llmSettings;
}

void SetupLLM::showLocalLLMInitResult(bool success, const QString& message)
{
    QMessageBox msgBox(this);
    msgBox.setIcon(success ? QMessageBox::Information : QMessageBox::Critical);
    msgBox.setWindowTitle(success ? i18n("LLM Initialization Successful")
                                  : i18n("LLM Initialization Failed"));
    msgBox.setText(message);

    QPushButton* const viewLogButton = msgBox.addButton(i18n("View Log"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Ok);
    msgBox.exec();

    if (msgBox.clickedButton() == viewLogButton)
    {
        QDialog logDialog(this);
        logDialog.setWindowTitle(i18n("llama-server Log"));
        logDialog.resize(820, 480);

        QVBoxLayout* const logLayout = new QVBoxLayout(&logDialog);
        QTextEdit* const logTextEdit = new QTextEdit(&logDialog);
        logTextEdit->setReadOnly(true);
        logTextEdit->setPlainText(d->localLLMInitLog.isEmpty() ? i18n("No log output was captured.")
                                                                : d->localLLMInitLog);
        logLayout->addWidget(logTextEdit);

        QDialogButtonBox* const buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, &logDialog);
        connect(buttonBox, &QDialogButtonBox::rejected,
                &logDialog, &QDialog::reject);
        logLayout->addWidget(buttonBox);

        logDialog.exec();
    }
}

void SetupLLM::ensureLocalLLMInitLogWindow()
{
    if (!d->localLLMInitLogDialog)
    {
        QDialog* const logDialog = new QDialog(this);
        logDialog->setAttribute(Qt::WA_DeleteOnClose);
        logDialog->setWindowTitle(i18n("Initializing Local LLM Engine"));
        logDialog->resize(900, 520);

        QVBoxLayout* const logLayout = new QVBoxLayout(logDialog);

        QLabel* const guidanceLabel = new QLabel(i18n("Initializing the LLM engine can take a while. Do not navigate away from this screen, and please be patient."), logDialog);
        guidanceLabel->setWordWrap(true);
        logLayout->addWidget(guidanceLabel);

        QLabel* const infoLabel = new QLabel(i18n("llama-server output (live):"), logDialog);
        logLayout->addWidget(infoLabel);

        QPlainTextEdit* const logTextEdit = new QPlainTextEdit(logDialog);
        logTextEdit->setReadOnly(true);
        logTextEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
        logLayout->addWidget(logTextEdit);

        QDialogButtonBox* const buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, logDialog);
        connect(buttonBox, &QDialogButtonBox::rejected,
                logDialog, &QDialog::reject);
        logLayout->addWidget(buttonBox);

        connect(logDialog, &QObject::destroyed,
                this, [this]()
                {
                    d->localLLMInitLogDialog   = nullptr;
                    d->localLLMInitLogTextEdit = nullptr;
                });

        d->localLLMInitLogDialog   = logDialog;
        d->localLLMInitLogTextEdit = logTextEdit;
    }

    if (d->localLLMInitLogTextEdit)
    {
        d->localLLMInitLogTextEdit->setPlainText(d->localLLMInitLog);
        QTextCursor cursor = d->localLLMInitLogTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        d->localLLMInitLogTextEdit->setTextCursor(cursor);
    }

    if (d->localLLMInitLogDialog)
    {
        d->localLLMInitLogDialog->show();
        d->localLLMInitLogDialog->raise();
        d->localLLMInitLogDialog->activateWindow();
    }
}

void SetupLLM::slotInitializeLocalLLMService()
{
    if (d->localLLMInitInProgress)
    {
        return;
    }

    refreshInitializeButtonState();

    if (!d->initializeLocalLLMButton->isEnabled())
    {
        return;
    }

    QString errorMessage;
    const LLMProcessSettings llmSettings = createLocalLLMInitSettings(&errorMessage);

    if (!errorMessage.isEmpty())
    {
        showLocalLLMInitResult(false, errorMessage);
        return;
    }

    d->localLLMInitLog.clear();
    d->localLLMInitReady       = false;
    d->localLLMInitInProgress  = true;

    ensureLocalLLMInitLogWindow();

    d->localLLMInitManager = new LLMProcessManagerKoboldCpp(llmSettings, this);

    connect(d->localLLMInitManager, &LLMProcessManager::signalEngineReady,
            this, &SetupLLM::slotLocalLLMInitEngineReady);

    connect(d->localLLMInitManager, &LLMProcessManager::signalEngineStopped,
            this, &SetupLLM::slotLocalLLMInitEngineStopped);

    connect(d->localLLMInitManager, &LLMProcessManager::signalEngineOutput,
            this, &SetupLLM::slotLocalLLMInitEngineOutput);

        connect(d->localLLMInitManager, &LLMProcessManager::signalEngineError,
            this, &SetupLLM::slotLocalLLMInitEngineError);

    refreshInitializeButtonState();

    if (!d->localLLMInitManager->startEngine())
    {
        d->localLLMInitLog.append(d->localLLMInitManager->lastErrorMessage() + QLatin1Char('\n'));

        const QString startupError = i18n("Unable to start llama-server for LLM engine initialization: %1",
                                          d->localLLMInitManager->lastErrorMessage());

        d->localLLMInitManager->deleteLater();
        d->localLLMInitManager    = nullptr;
        d->localLLMInitInProgress = false;
        d->localLLMInitReady      = false;

        refreshInitializeButtonState();
        showLocalLLMInitResult(false, startupError);
    }
}

void SetupLLM::slotLocalLLMInitEngineReady()
{
    if (!d->localLLMInitManager)
    {
        return;
    }

    d->localLLMInitReady = true;
    d->localLLMInitManager->shutdownEngine();
}

void SetupLLM::slotLocalLLMInitEngineStopped(int exitCode)
{
    if (!d->localLLMInitManager)
    {
        return;
    }

    const bool initializedSuccessfully = d->localLLMInitReady;
    QString resultMessage;

    if (initializedSuccessfully)
    {
        resultMessage = i18n("The local LLM engine completed initialization successfully.");
    }
    else
    {
        QString reason = d->localLLMInitManager->lastErrorMessage().trimmed();

        if (reason.isEmpty())
        {
            reason = i18n("llama-server exited before passing the health check (exit code: %1).", exitCode);
        }
        else
        {
            reason = i18n("llama-server exited before passing the health check (exit code: %1).\n%2", exitCode, reason);
        }

        resultMessage = reason;
    }

    d->localLLMInitManager->deleteLater();
    d->localLLMInitManager    = nullptr;
    d->localLLMInitInProgress = false;
    d->localLLMInitReady      = false;

    refreshInitializeButtonState();
    showLocalLLMInitResult(initializedSuccessfully, resultMessage);
}

void SetupLLM::slotLocalLLMInitEngineOutput(const QString& output)
{
    d->localLLMInitLog.append(output);

    const int maxLogSize = (256 * 1024);

    if (d->localLLMInitLog.size() > maxLogSize)
    {
        d->localLLMInitLog = d->localLLMInitLog.right(maxLogSize);
    }

    if (d->localLLMInitLogTextEdit)
    {
        d->localLLMInitLogTextEdit->setPlainText(d->localLLMInitLog);
        QTextCursor cursor = d->localLLMInitLogTextEdit->textCursor();
        cursor.movePosition(QTextCursor::End);
        d->localLLMInitLogTextEdit->setTextCursor(cursor);
    }
}

void SetupLLM::slotLocalLLMInitEngineError(const QString& error)
{
    // Transient startup and health-check related errors are expected while llama-server initializes.
    // They are captured in the log and surfaced only if the process exits before reporting ready.
    slotLocalLLMInitEngineOutput(error + QLatin1Char('\n'));
}

void SetupLLM::slotEnableAIAnnotateSettings(bool enable)
{
    d->enableAIAnnotateExternalLLMCheck->setEnabled(enable);
    d->maxTagsSpinBox->setEnabled(enable);
    d->maxTagsLabel->setEnabled(enable);
    d->enableAIAnnotateAdvSetCheck->setEnabled(enable);
    d->enableAIAnnotateDescriptionCheck->setEnabled(enable);
    d->enableAIAnnotateTagsCheck->setEnabled(enable);
    d->openAIModelLabel->setEnabled(enable);
    d->openAIModelCombo->setEnabled(enable && (d->openAIModelCombo->count() > 0) && !d->openAIModelCombo->currentData().toString().isEmpty());
    
    // Voice style controls
    d->aiVoiceTypeCombo->setEnabled(enable);
    d->aiVoiceTypeLabel->setEnabled(enable);
    d->aiVoicePerspectiveCombo->setEnabled(enable);
    d->aiVoicePerspectiveLabel->setEnabled(enable);
    
    // Text tones controls
    d->textTonesLabel->setEnabled(enable);
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

    slotEnableAIAnnotateAdvancedSettings(d->enableAIAnnotateAdvSetCheck->isChecked() && d->enableAIAnnotateAdvSetCheck->isEnabled());
    slotEnableAIAnnotateCustomPrompt(d->enableAIAnnotateCustomPromptCheck->isChecked() && d->enableAIAnnotateCustomPromptCheck->isEnabled());
    slotEnableAIAnnotateMaxTags(d->enableAIAnnotateTagsCheck->isChecked() && d->enableAIAnnotateTagsCheck->isEnabled());
    slotEnableAIAnnotateExternalLLM(d->enableAIAnnotateExternalLLMCheck->isChecked() && d->enableAIAnnotateExternalLLMCheck->isEnabled());
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

    // Only enable voice style and text tones if:
    // 1. The main AI Annotate feature is enabled
    // 2. Either custom prompt is not checked or custom prompt is disabled
    bool mainFeatureEnabled = d->enableAIAnnotateCheck->isChecked();
    bool enableVoiceAndTones = mainFeatureEnabled && (!enable || !d->enableAIAnnotateDescriptionCheck->isEnabled());
    
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

void SetupLLM::slotShowInstallDialog()
{
    // Create and show the install dialog
    LLMEngineInstallDialog* const dialog = new LLMEngineInstallDialog(this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(dialog, &QObject::destroyed,
            this, &SetupLLM::refreshInitializeButtonState);
    dialog->show();
}

} // namespace Digikam
