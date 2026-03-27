/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-01-06
 * Description : Widget to choose options for autotags scanning
 *
 * SPDX-FileCopyrightText: 2024-2026 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "autotagsscanwidget_p.h"

// Local includes

#include "dnnmodelmanager.h"

// Qt includes

#include <QSignalBlocker>

namespace Digikam
{

AutotagsScanWidget::AutotagsScanWidget(SettingsDisplayMode displayMode, QWidget* const parent)
    : QTabWidget       (parent),
      StateSavingObject(this),
      d                (new Private)
{
    d->displayMode = displayMode;
    setObjectName(d->configName);
    setupUi();
}

AutotagsScanWidget::~AutotagsScanWidget()
{
    delete d;
}

void AutotagsScanWidget::doLoadState()
{
    KConfigGroup group = getConfigGroup();
    d->albumSelectors->loadState();

    AutotagsScanSettings prm;
    prm.readFromConfig(group);

    if (d->showExternalLLMServiceOnly && (d->externalLLMModelIndex >= 0))
    {
        if (modelUsesOpenAI(prm.objectDetectModel))
        {
            d->objectDetectModel->setItemData(d->externalLLMModelIndex, prm.objectDetectModel);
        }
        else if (!d->fallbackOpenAIModelId.isEmpty())
        {
            d->objectDetectModel->setItemData(d->externalLLMModelIndex, d->fallbackOpenAIModelId);
        }
    }

    int objectDetectModel       = d->objectDetectModel->findData(prm.objectDetectModel);

    if (objectDetectModel < 0)
    {
        objectDetectModel = 0;

        updateScanModeOptions();
        updateTagModeOptions();

        int tagScanMode             = d->scanMode->findData(prm.scanMode);

        if (tagScanMode < 0)
        {
            tagScanMode = 0;
        }

        d->scanMode->setCurrentIndex(tagScanMode);
    }

    d->objectDetectModel->setCurrentIndex(objectDetectModel);

    updateTagModeOptions();

    int tagTagMode              = d->tagMode->findData(prm.tagMode);

    if (tagTagMode < 0)
    {
        tagTagMode = 0;
    }

    d->tagMode->setCurrentIndex(tagTagMode);

    d->accuracyInput->setValue(prm.uiConfidenceThreshold);

    d->useFullCpuButton->setChecked(prm.useFullCpu);

    d->trSelectorList->clearLanguages();

    for (const QString& lg : std::as_const(prm.languages))
    {
        d->trSelectorList->addLanguage(lg);
    }
}

void AutotagsScanWidget::doSaveState()
{
    KConfigGroup group       = getConfigGroup();
    d->albumSelectors->saveState();

    AutotagsScanSettings prm = settings();
    prm.writeToConfig(group);
}

void AutotagsScanWidget::setupUi()
{
    // --- Album tab --------------------------------------------------------------------------------------

    d->albumSelectors       = new AlbumSelectors(QString(), d->configName,
                                                 this, AlbumSelectors::AlbumType::All, true);
    addTab(d->albumSelectors, i18nc("@title:tab", "Search in"));

    // --- Settings tab -----------------------------------------------------------------------------------

    d->settingsTab          = new QWidget(this);
    QGridLayout* const grid = new QGridLayout(d->settingsTab);

    QLabel* const title     = new QLabel(d->settingsTab);
    title->setText(i18nc("@label",
                         "<p><b>This tool automatically assigns tags to images by analyzing the image using "
                         "a deep-learning neural network AI model.</b></p>"
                         "<p>The settings below configure the deep-learning AI model used for analyzing image "
                         "contents to identify objects. The AI neural network will automatically generate "
                         "descriptive tags for the image contents and store the results in the database.</p>"));
    title->setWordWrap(true);

    QLabel* const scanLbl   = new QLabel(i18nc("@label with a short translated text about autotags scanning mode",
                                               "Scan Mode:"), d->settingsTab);

    d->scanMode             = new SqueezedComboBox(d->settingsTab);

    QLabel* const tmodeLbl  = new QLabel(i18nc("@label with a short translated text ",
                                               "Write Mode:"), d->settingsTab);

    d->tagMode              = new SqueezedComboBox(d->settingsTab);

    QLabel* const modelLbl  = new QLabel(i18nc("@label with a short translated text about deep learning model used to detect objects to auto-tag",
                                               "Model:"), d->settingsTab);

    DNNModelManager* const dnnModelManager        = DNNModelManager::instance();
    QList<QPair<QString, QStringList> > modelList = dnnModelManager->getModelList(DNNModelUsage::DNNUsageObjectDetection);
    d->objectDetectModel                          = new SqueezedComboBox(d->settingsTab);

    if (modelList.isEmpty())
    {
        d->objectDetectModel->addSqueezedItem(i18n("No models available"), QStringLiteral("none"));
        this->parentWidget()->setEnabled(false);
    }
    else
    {
        const bool aiAnnotateEnabled = ApplicationSettings::instance()->getAIAnnotateEnabled();
        const bool externalLLMEnabled = ApplicationSettings::instance()->getAIAnnotateExternalLLMEnabled();
        d->showExternalLLMServiceOnly = (aiAnnotateEnabled && externalLLMEnabled);

        for (const auto& model : modelList)
        {
            DNNModelBase* const modelPtr = dnnModelManager->getModel(model.first, DNNModelUsage::DNNUsageObjectDetection);
            const bool isOpenAIModel = (modelPtr && (modelPtr->info.classifier.compare(QLatin1String("openai"), Qt::CaseInsensitive) == 0));

            if (isOpenAIModel && d->fallbackOpenAIModelId.isEmpty())
            {
                d->fallbackOpenAIModelId = model.first;
            }

            // Replace the OpenAI model list with one synthetic external service entry.
            if (d->showExternalLLMServiceOnly && isOpenAIModel)
            {
                continue;
            }

            // Hide OpenAI-backed models unless AI Annotate is enabled in settings.
            if (!aiAnnotateEnabled && isOpenAIModel)
            {
                continue;
            }

            d->objectDetectModel->addSqueezedItem(model.second[0], model.first);
            d->objectDetectModel->setToolTip(d->objectDetectModel->toolTip() + model.second[1]);
        }

        if (d->showExternalLLMServiceOnly && !d->fallbackOpenAIModelId.isEmpty())
        {
            d->objectDetectModel->addSqueezedItem(i18n("External LLM Service"), d->fallbackOpenAIModelId);
            d->externalLLMModelIndex = d->objectDetectModel->count() - 1;
            d->objectDetectModel->setToolTip(d->objectDetectModel->toolTip() + i18n("Use the configured external LLM service with OpenAI-compatible behavior."));
        }

        if (d->objectDetectModel->count() == 0)
        {
            d->objectDetectModel->addSqueezedItem(i18n("No models available"), QStringLiteral("none"));
            this->parentWidget()->setEnabled(false);
        }
    }

    updateScanModeOptions();
    updateTagModeOptions();

    d->accuracyLabel        = new QLabel(i18nc("@label with a short translated text",
                                               "Accuracy:"), d->settingsTab);
    d->accuracyInput        = new DIntNumInput(d->settingsTab);
    d->accuracyInput->setDefaultValue(7);
    d->accuracyInput->setRange(1, 10, 1);
    d->accuracyInput->setToolTip(i18nc("@info:tooltip",
                                       "Adjust sensitivity versus specificity: the higher the value, the more accurately objects will<br>"
                                       "be recognized, but fewer objects will be recognized.<br>"));

    d->useFullCpuButton     = new QCheckBox(d->settingsTab);
    d->useFullCpuButton->setText(i18nc("@option:check with a short translated text",
                                       "Use all processors"));
    d->useFullCpuButton->setToolTip(i18nc("@info:tooltip",
                                          "Object detection and auto-tagging are time-consuming tasks.<br>"
                                          "You can choose if you wish to employ all processor cores<br>"
                                          "on your system, or work in the background only on one core."));

    grid->addWidget(title,                0, 0, 1, 2);
    grid->addWidget(scanLbl,              1, 0, 1, 1);
    grid->addWidget(d->scanMode,          1, 1, 1, 1);
    grid->addWidget(tmodeLbl,             2, 0, 1, 1);
    grid->addWidget(d->tagMode,           2, 1, 1, 1);
    grid->addWidget(modelLbl,             3, 0, 1, 1);
    grid->addWidget(d->objectDetectModel, 3, 1, 1, 1);
    grid->addWidget(d->accuracyLabel,     4, 0, 1, 1);
    grid->addWidget(d->accuracyInput,     4, 1, 1, 1);
    grid->addWidget(d->useFullCpuButton,  5, 0, 1, 2);
    grid->setColumnStretch(1, 10);
    grid->setRowStretch(6, 10);

    addTab(d->settingsTab, i18nc("@title:tab", "Settings"));

    // --- Translate tab -----------------------------------------------------------------------------------

    d->trSelectorList       = new LocalizeSelectorList(d->settingsTab);
    d->trSelectorList->setTitle(i18nc("@label with a short translated text",
                                      "Translate Tags to:"));

    addTab(d->trSelectorList, i18nc("@title:tab", "Translate"));

    // --- Configure UI -----------------------------------------------------------------------------------

    if (SettingsDisplayMode::BQM == d->displayMode)
    {
        d->useFullCpuButton->hide();
        scanLbl->hide();
        d->scanMode->hide();
        setTabVisible(0, false);
        setCurrentIndex(1);
    }

    if (SettingsDisplayMode::Maintenance == d->displayMode)
    {
        d->useFullCpuButton->hide();
        setTabVisible(0, false);
        setCurrentIndex(1);
    }

    if (SettingsDisplayMode::Normal == d->displayMode)
    {
        title->hide();
    }

    // --- Signals -----------------------------------------------------------------------------------

    connect(d->albumSelectors, SIGNAL(signalSelectionChanged()),
            this, SLOT(slotSettingsChanged()));

    connect(d->objectDetectModel, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotObjectDetectModelChanged()));

    connect(d->scanMode, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotSettingsChanged()));

    connect(d->tagMode, SIGNAL(currentIndexChanged(int)),
            this, SLOT(slotSettingsChanged()));

    connect(d->trSelectorList, SIGNAL(signalSettingsChanged()),
            this, SLOT(slotSettingsChanged()));

    connect(d->accuracyInput, SIGNAL(valueChanged(int)),
            this, SLOT(slotSettingsChanged()));

    updateAccuracyAndCpuOptions();
}

AutotagsScanSettings AutotagsScanWidget::settings() const
{
    AutotagsScanSettings result;

    if (SettingsDisplayMode::Normal == d->displayMode)
    {
        result.albums             = d->albumSelectors->selectedAlbumsAndTags();
        result.wholeAlbums        = d->albumSelectors->wholeAlbumsChecked();
    }

    if (SettingsDisplayMode::BQM != d->displayMode)
    {
        result.scanMode           = (AutotagsScanSettings::ScanMode)d->scanMode->itemData(d->scanMode->currentIndex()).toInt();
    }

    result.useFullCpu             = d->useFullCpuButton->isChecked();
    result.tagMode                = (AutotagsScanSettings::TagMode)d->tagMode->itemData(d->tagMode->currentIndex()).toInt();
    result.objectDetectModel      = d->objectDetectModel->itemData(d->objectDetectModel->currentIndex()).toString();
    result.uiConfidenceThreshold  = d->accuracyInput->value();
    result.languages              = d->trSelectorList->languagesList();

    return result;
}

void AutotagsScanWidget::setSettings(const AutotagsScanSettings& newSettings)
{
    if (d->showExternalLLMServiceOnly && (d->externalLLMModelIndex >= 0))
    {
        if (modelUsesOpenAI(newSettings.objectDetectModel))
        {
            d->objectDetectModel->setItemData(d->externalLLMModelIndex, newSettings.objectDetectModel);
        }
        else if (!d->fallbackOpenAIModelId.isEmpty())
        {
            d->objectDetectModel->setItemData(d->externalLLMModelIndex, d->fallbackOpenAIModelId);
        }
    }

    int objectDetectModelIndex = d->objectDetectModel->findData(newSettings.objectDetectModel);

    if (objectDetectModelIndex < 0)
    {
        objectDetectModelIndex = 0;
    }

    d->objectDetectModel->setCurrentIndex(objectDetectModelIndex);

    updateScanModeOptions();
    updateTagModeOptions();

    int scanModeIndex = d->scanMode->findData(newSettings.scanMode);

    if (scanModeIndex < 0)
    {
        scanModeIndex = 0;
    }

    d->scanMode->setCurrentIndex(scanModeIndex);

    int tagModeIndex = d->tagMode->findData(newSettings.tagMode);

    if (tagModeIndex < 0)
    {
        tagModeIndex = 0;
    }

    d->tagMode->setCurrentIndex(tagModeIndex);
    d->useFullCpuButton->setChecked(newSettings.useFullCpu);

    if (d->accuracyInput->value() != newSettings.uiConfidenceThreshold)
    {
        d->accuracyInput->setValue(newSettings.uiConfidenceThreshold);
    }

    d->trSelectorList->clearLanguages();

    for (const QString& lg : newSettings.languages)
    {
        d->trSelectorList->addLanguage(lg);
    }
}

void AutotagsScanWidget::slotSettingsChanged()
{
    Q_EMIT signalSettingsChanged();
}

void AutotagsScanWidget::slotObjectDetectModelChanged()
{
    updateScanModeOptions();
    updateTagModeOptions();
    updateAccuracyAndCpuOptions();
    slotSettingsChanged();
}

void AutotagsScanWidget::updateScanModeOptions()
{
    const QVariant currentScanMode = d->scanMode->currentData();
    const bool showAIAnnotateModes = selectedModelUsesOpenAI();

    QSignalBlocker blocker(d->scanMode);

    d->scanMode->clear();

    if (showAIAnnotateModes)
    {
        d->scanMode->addSqueezedItem(i18nc("@item: all items to scan with autotags", "All"),
                                     AutotagsScanSettings::ScanMode::AllItems);
        d->scanMode->addSqueezedItem(i18nc("@item: autotags scan skipping items with assigned comments", "Skip Assigned Comments"),
                                     AutotagsScanSettings::ScanMode::SkipAssignedComments);
        d->scanMode->addSqueezedItem(i18nc("@item: autotags scan skipping items with assigned auto-tags", "Skip Assigned Tags"),
                                     AutotagsScanSettings::ScanMode::SkipAssignedTags);
        d->scanMode->addSqueezedItem(i18nc("@item: autotags scan skipping items with assigned comments or auto-tags", "Skip Assigned Comments Or Tags"),
                                     AutotagsScanSettings::ScanMode::SkipAssignedCommentsOrTags);
        d->scanMode->setToolTip(i18nc("@info:tooltip",
                                      "<p><b>All</b>: re-scan all items for tags.</p>"
                                      "<p><b>Skip Assigned Comments</b>: scan only items without assigned comments.</p>"
                                      "<p><b>Skip Assigned Tags</b>: scan only items without assigned auto-tags.</p>"
                                      "<p><b>Skip Assigned Comments Or Tags</b>: scan only items without assigned comments and without assigned auto-tags.</p>"));
    }
    else
    {
        d->scanMode->addSqueezedItem(i18nc("@item: all items to scan with autotags", "All"),
                                     AutotagsScanSettings::ScanMode::AllItems);
        d->scanMode->addSqueezedItem(i18nc("@item: autotags scan skipping items with assigned auto-tags", "Skip Assigned Tags"),
                                     AutotagsScanSettings::ScanMode::SkipAssignedTags);
        d->scanMode->setToolTip(i18nc("@info:tooltip",
                                      "<p><b>All</b>: re-scan all items for tags.</p>"
                                      "<p><b>Skip Assigned Tags</b>: scan only items without assigned auto-tags.</p>"));
    }

    int scanModeIndex = d->scanMode->findData(currentScanMode);

    if (scanModeIndex < 0)
    {
        scanModeIndex = 0;
    }

    d->scanMode->setCurrentIndex(scanModeIndex);
}

void AutotagsScanWidget::updateTagModeOptions()
{
    const QVariant currentTagMode = d->tagMode->currentData();
    const bool showAIAnnotateModes = selectedModelUsesOpenAI();

    QSignalBlocker blocker(d->tagMode);

    d->tagMode->clear();

    if (showAIAnnotateModes)
    {
        d->tagMode->addSqueezedItem(i18nc("@item: replace existing autotags comments and tags in database", "Replace All"),
                                    AutotagsScanSettings::TagMode::ReplaceAll);
        d->tagMode->addSqueezedItem(i18nc("@item: replace existing autotags comments in database", "Replace Comment"),
                                    AutotagsScanSettings::TagMode::ReplaceComment);
        d->tagMode->addSqueezedItem(i18nc("@item: replace existing autotags tags in database", "Replace Tags"),
                                    AutotagsScanSettings::TagMode::ReplaceTags);
        d->tagMode->addSqueezedItem(i18nc("@item: update autotags tags by adding new ones in database", "Update Tags (add new)"),
                                    AutotagsScanSettings::TagMode::UpdateTags);
        d->tagMode->setToolTip(i18nc("@info:tooltip",
                                     "<p><b>Replace All</b>: replace existing image comments and auto-tags with the results of the scan.</p>"
                                     "<p><b>Replace Comment</b>: replace existing image comments, but keep existing auto-tags and add any new ones found.</p>"
                                     "<p><b>Replace Tags</b>: replace existing auto-tags, but leave existing image comments unchanged.</p>"
                                     "<p><b>Update Tags (add new)</b>: leave existing image comments unchanged and add only new auto-tags.</p>"));
    }
    else
    {
        d->tagMode->addSqueezedItem(i18nc("@item: replace existing autotags tags in database", "Replace Tags"),
                                    AutotagsScanSettings::TagMode::ReplaceTags);
        d->tagMode->addSqueezedItem(i18nc("@item: update autotags tags by adding new ones in database", "Update Tags (add new)"),
                                    AutotagsScanSettings::TagMode::UpdateTags);
        d->tagMode->setToolTip(i18nc("@info:tooltip",
                                     "<p><b>Replace Tags</b>: replace existing auto-tags, but leave existing image comments unchanged.</p>"
                                     "<p><b>Update Tags (add new)</b>: leave existing image comments unchanged and add only new auto-tags.</p>"));
    }

    int tagModeIndex = d->tagMode->findData(currentTagMode);

    if (tagModeIndex < 0)
    {
        tagModeIndex = 0;
    }

    d->tagMode->setCurrentIndex(tagModeIndex);
}

bool AutotagsScanWidget::selectedModelUsesOpenAI() const
{
    if (!ApplicationSettings::instance()->getAIAnnotateEnabled())
    {
        return false;
    }

    const QString modelName = d->objectDetectModel->currentData().toString();

    return modelUsesOpenAI(modelName);
}

void AutotagsScanWidget::updateAccuracyAndCpuOptions()
{
    const bool isOpenAI = selectedModelUsesOpenAI();

    d->accuracyLabel->setEnabled(!isOpenAI);
    d->accuracyInput->setEnabled(!isOpenAI);
    d->useFullCpuButton->setEnabled(!isOpenAI);
}

bool AutotagsScanWidget::modelUsesOpenAI(const QString& modelName) const
{
    if (modelName.isEmpty() || (modelName == QLatin1String("none")))
    {
        return false;
    }

    DNNModelBase* const model = DNNModelManager::instance()->getModel(modelName, DNNModelUsage::DNNUsageObjectDetection);

    return model && (model->info.classifier.compare(QLatin1String("openai"), Qt::CaseInsensitive) == 0);
}

} // namespace Digikam

#include "moc_autotagsscanwidget.cpp"
