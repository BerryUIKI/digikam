/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchnlmodelmanager.h"

// Qt includes

#include <QFileInfo>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "dnnmodelmanager.h"
#include "dnnmodelbase.h"
#include "dnnmodeldefinitions.h"

namespace Digikam
{

namespace
{
    // Matches the [section] name registered in dnnmodels.conf.
    // DNNModelManager::getModel() lower-cases the lookup internally.
    const QLatin1String s_nlModelName("Qwen2.5-1.5B-Instruct");
}

SearchNlModelManager::SearchNlModelManager(QObject* const parent)
    : QObject(parent)
{
}

SearchNlModelManager::~SearchNlModelManager() = default;

QString SearchNlModelManager::defaultModelPath()
{
    // Resolve through digiKam's central model manager rather than a
    // private directory: the file lives in the shared model location and
    // is fetched by FilesDownloader like every other DNN model.

    DNNModelBase* const model = DNNModelManager::instance()->getModel(
        s_nlModelName, DNNModelUsage::DNNUsageNaturalLanguageSearch);

    return (model ? model->getModelPath() : QString());
}

bool SearchNlModelManager::isModelAvailable()
{
    const QString path = defaultModelPath();

    return (!path.isEmpty() && QFileInfo::exists(path));
}

QString SearchNlModelManager::currentModelPath() const
{
    return (m_currentModelPath.isEmpty() ? defaultModelPath()
                                         : m_currentModelPath);
}

QString SearchNlModelManager::currentModelVersion() const
{
    return m_currentVersion;
}

void SearchNlModelManager::ensureModelAvailable()
{
    if (isModelAvailable())
    {
        m_currentModelPath = defaultModelPath();

        Q_EMIT signalModelReady(m_currentModelPath);

        qCInfo(DIGIKAM_NLSEARCH_LOG) << "isModelAvailable:" << isModelAvailable()
                             << "path:" << defaultModelPath();

        return;
    }

    // The model file is registered (dnnmodels.conf) but not yet present.
    // Downloading is handled centrally by FilesDownloader (the setup
    // dialog), not by a parallel mechanism here. Signal that a download
    // is required so the UI can direct the user to the downloader.

    qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search model not present; download required via FilesDownloader:"
                                 << defaultModelPath();

    Q_EMIT signalModelError(i18nc("@info",
                                  "The natural-language search model is not installed. "
                                  "Please download it from the digiKam setup downloader."));
}

void SearchNlModelManager::cleanupObsoleteModels()
{
    // No-op: obsolete model files are cleaned up centrally by
    // FilesDownloader::deleteUnusedFiles(). Kept for interface stability.
}

} // namespace Digikam

#include "moc_searchnlmodelmanager.cpp"