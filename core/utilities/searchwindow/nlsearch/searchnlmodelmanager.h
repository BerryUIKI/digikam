/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2026-06-10
 * Description : Lifecycle management of the NL search model file.
 *               Thin bridge to digiKam's existing infrastructure:
 *               DNNModelManager (core/libs/dnnmodelmanager) for
 *               registration/path lookup and FilesDownloader
 *               (core/utilities/setup/downloader) for KDE-hosted
 *               on-demand retrieval. No parallel download mechanism.
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#pragma once

// Qt includes

#include <QObject>
#include <QString>

namespace Digikam
{

class SearchNlModelManager : public QObject
{
    Q_OBJECT

public:

    explicit SearchNlModelManager(QObject* const parent = nullptr);
    ~SearchNlModelManager() override;

    static QString defaultModelPath();
    static bool    isModelAvailable();

    QString currentModelPath()    const;
    QString currentModelVersion() const;

    /**
     * Ensure the model file exists locally. If missing, triggers a
     * download through FilesDownloader (KDE-hosted asset). Emits
     * modelReady() or modelError().
     */
    void ensureModelAvailable();

    // Remove superseded model versions from the storage directory.
    void cleanupObsoleteModels();

Q_SIGNALS:

    void downloadProgress(qint64 received, qint64 total);
    void modelReady(const QString& localPath);
    void modelError(const QString& message);

private:

    static QString assetFileName();   // e.g. "qwen2.5-1.5b-instruct-q4_k_m.gguf"

private:

    QString m_currentModelPath;
    QString m_currentVersion;
};

} // namespace Digikam
