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

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

// Local includes

#include "digikam_debug.h"

namespace Digikam
{

SearchNlModelManager::SearchNlModelManager(QObject* const parent)
    : QObject(parent)
{
}

SearchNlModelManager::~SearchNlModelManager()
{
}

QString SearchNlModelManager::assetFileName()
{
    // Primary candidate per proposal. The fallback TinyLlama asset will
    // be registered alongside once benchmarking decides the default.
    return QLatin1String("qwen2.5-1.5b-instruct-q4_k_m.gguf");
}

QString SearchNlModelManager::defaultModelPath()
{
    // TODO: replace with DNNModelManager path resolution 
    // For the boilerplate we use the generic writable data location.

    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    return (base + QLatin1String("/nlsearch/") + assetFileName());
}

bool SearchNlModelManager::isModelAvailable()
{
    return QFileInfo::exists(defaultModelPath());
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

        Q_EMIT modelReady(m_currentModelPath);

        return;
    }

    // TODO: wire FilesDownloader from core/utilities/setup/downloader:

    qCInfo(DIGIKAM_GENERAL_LOG) << "NL search model missing, download required:"
                                << defaultModelPath();

    Q_EMIT modelError(QLatin1String("Model download not implemented yet (boilerplate)."));
}

void SearchNlModelManager::cleanupObsoleteModels()
{
    // TODO: enumerate the nlsearch model dir and remove files
    // whose version tag does not match m_currentVersion.
}

} // namespace Digikam

#include "moc_searchnlmodelmanager.cpp"
