/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-06
 * Description : Performs autotags using KoboldLLM models
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "autotagspipelinekoboldllm.h"

// C++ includes

#include <vector>

// Qt includes

#include <QList>
#include <QSet>
#include <QElapsedTimer>
#include <QRectF>
#include <QBuffer>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QTimer>
#include <QEventLoop>

// KDE includes

#include <klocalizedstring.h>

// Local includes

#include "digikam_debug.h"
#include "digikam_opencv.h"
#include "sharedqueue.h"
#include "collectionmanager.h"
#include "album.h"
#include "iteminfo.h"
#include "coredb.h"
#include "autotagsscansettings.h"
#include "dimg.h"
#include "previewloadthread.h"
#include "dnnmodelconfig.h"
#include "autotagsclassifierbase.h"
#include "autotagsclassifierminmax.h"
#include "scancontroller.h"
#include "metadatahub.h"
#include "itemcomments.h"
#include "tagscache.h"
#include "localizeselector.h"
#include "qtopencvimg.h"
#include "autotagspipelinekoboldllmpackage.h"

namespace Digikam
{

AutotagsPipelineKoboldLLM::AutotagsPipelineKoboldLLM(const AutotagsScanSettings& settings)
    : AutotagsPipelineBase(settings)
{
}

AutotagsPipelineKoboldLLM::~AutotagsPipelineKoboldLLM()
{
    if (autotagsClassifier)
    {
        delete autotagsClassifier;
    }
}

bool AutotagsPipelineKoboldLLM::start()
{
    // Create the KoboldLLM model

    try
    {
        model = static_cast<DNNModelNet*>(DNNModelManager::instance()->getModel(settings.objectDetectModel,
                                                                                DNNModelUsage::DNNUsageImageClassification));

        model->getNet();

        const DNNModelConfig* configModel = static_cast<DNNModelConfig*>(DNNModelManager::instance()->getModel(model->info.classList,
                                                                                                               DNNModelUsage::DNNUsageImageClassification));

        if (configModel)
        {
            // Create appropriate classifier for KoboldLLM model
            autotagsClassifier = static_cast<AutotagsClassifierBase*>(new AutotagsClassifierMinmax(0.5, configModel->getModelPath()));            
        }
        else
        {
            qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldLLM::start(): Failed to find config model.";
            return false;
        }
    }
    catch (const std::exception& e)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldLLM::start(): Unable to load model. " << e.what();

        if (model)
        {
            model = nullptr;
        }
        
        return false;
    }
    catch (...)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldLLM::start(): Unknown error. Unable to load model.";

        if (model)
        {
            model = nullptr;
        }
        
        return false;
    }

    // Check if the model and classifier were created

    if ((nullptr == model) || (nullptr == autotagsClassifier))
    {
        return false;
    }

    {
        // Use the mutex to synchronize the start of the threads

        QMutexLocker lock(&mutex);

        // Add the worker threads for this pipeline

        addWorker(MLPipelineStage::Finder);
        addWorker(MLPipelineStage::Loader);
        addWorker(MLPipelineStage::Extractor);
        addWorker(MLPipelineStage::Classifier);
        addWorker(MLPipelineStage::Writer);
    }

    return AutotagsPipelineBase::start();
}

bool AutotagsPipelineKoboldLLM::finder()
{
    if (settings.bqmMode)
    {
        return true;
    }

    MLPIPELINE_FINDER_START(QThread::LowPriority, MLPipelineStage::Loader);

    /* =========================================================================================
     * Pipeline finder specific initialization code
     *
     * Use the block from here to MLPIPELINE_FINDER_END to find the IDs images to process.
     * The code in this block is run once per stage initialization. The number of instances
     * is always 1.
     */

    // Get the IDs to process

    QSet<qlonglong> filter;

    for (const Album* const album : std::as_const(settings.albums))
    {
        if (cancelled)
        {
            break;
        }

        if (album && !album->isTrashAlbum())
        {
            QList<qlonglong> imageIds;

            // Get the image IDs for the album/tag

            if      (album->type() == Album::PHYSICAL)
            {
                imageIds = CoreDbAccess().db()->getItemIDsInAlbum(album->id());
            }
            else if (album->type() == Album::TAG)
            {
                imageIds = CoreDbAccess().db()->getItemIDsInTag(album->id());
            }

            // Quick check if we should add threads

            if (!moreCpu)
            {
                moreCpu = checkMoreWorkers(totalItemCount, imageIds.size(), settings.useFullCpu);
            }

            // Iterate over the image IDs and add unique IDs to the queue for processing

            for (qlonglong imageId : std::as_const(imageIds))
            {
                ++performanceProfileList[MLPipelineStage::Finder].itemCount;

                // Filter out duplicate image IDs

                if (!filter.contains(imageId))
                {
                    ++totalItemCount;
                    filter << imageId;

                    enqueue(nextQueue, new AutotagsPipelineKoboldLLMPackage(imageId));
                }
            }
        }
    }

    /* =========================================================================================
     * Pipeline finder specific cleanup
     *
     * Use the block from here to MLPIPELINE_FINDER_END to clean up any resources used by the stage.
     */

    MLPIPELINE_FINDER_END(MLPipelineStage::Loader);
}

bool AutotagsPipelineKoboldLLM::loader()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Loader, MLPipelineStage::Extractor);
    AutotagsPipelineKoboldLLMPackage* package = nullptr;

    int imageSize= 1024;

    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */

    MLPIPELINE_LOOP_START(MLPipelineStage::Loader, thisQueue);
    package = static_cast<AutotagsPipelineKoboldLLMPackage*>(mlpackage);

    /* =========================================================================================
     * Start pipeline stage specific loop
     *
     * All code from here to MLPIPELINE_LOOP_END is in a try/catch block and loop.
     * This loop is run once per image.
     */

    {
        // Check if the ID is for an image (not video or other file type)

        bool sendNotification = true;

        if (DatabaseItem::Category::Image == package->info.category())
        {
            // Load image for KoboldLLM model processing

            // package->image = PreviewLoadThread::loadFastSynchronously(package->info.filePath(), model->info.imageSize);
            package->image = PreviewLoadThread::loadFastSynchronously(package->info.filePath(), imageSize);

            // Check for corrupted images that can't be loaded

            if (!package->image.isNull())
            {
                // Create a thumbnail for the notification

                package->thumbnailIcon = QIcon(package->image.smoothScale(48, 48, Qt::KeepAspectRatio).convertToPixmap());

                // Send to the next stage

                enqueue(nextQueue, package);

                package = nullptr;

                sendNotification = false;
            }
        }

        if (sendNotification)
        {
            // Send a notification that the file was skipped

            notify(MLPipelineNotification::notifySkipped,
                   package->info.name(),
                   package->info.relativePath(),
                   QString(),
                   0,
                   package->thumbnailIcon);

            // Delete the package since it is not needed

            delete package;

            package = nullptr;
        }
    }

    /* =========================================================================================
     * End pipeline stage specific loop
     */

    MLPIPELINE_LOOP_END(MLPipelineStage::Loader, "AutotagsPipelineKoboldLLM::loader");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Loader, MLPipelineStage::Extractor);
}

bool AutotagsPipelineKoboldLLM::extractor()
{
    MLPIPELINE_STAGE_START(QThread::NormalPriority, MLPipelineStage::Extractor, MLPipelineStage::Classifier);
    AutotagsPipelineKoboldLLMPackage* package = nullptr;

    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */
    
    MLPIPELINE_LOOP_START(MLPipelineStage::Extractor, thisQueue);
    package = static_cast<AutotagsPipelineKoboldLLMPackage*>(mlpackage);

    /* =========================================================================================
     * Start pipeline stage specific loop
     *
     * All code from here to MLPIPELINE_LOOP_END is in a try/catch block and loop.
     * This loop is run once per image.
     */

    {
        // Convert DImg to QImage
        QImage qImage = package->image.copyQImage();
        
        // Check if the longest dimension is at least minImageSize
        int longestDimension = qMax(qImage.width(), qImage.height());
        
        // Resize the image if it's smaller than minImageSize
        if (longestDimension < minImageSize)
        {
            // Calculate scale factor to reach minImageSize for longest dimension
            double scaleFactor = static_cast<double>(minImageSize) / longestDimension;
            
            // Calculate new dimensions
            int newWidth = static_cast<int>(qImage.width() * scaleFactor);
            int newHeight = static_cast<int>(qImage.height() * scaleFactor);
            
            // Resize with smooth transformation for better quality
            qImage = qImage.scaled(newWidth, newHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
        
        // Convert QImage to base64 string
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        
        // Save as JPEG format with good quality for balance between size and quality
        qImage.save(&buffer, base64ImageType, jpegQuality);
        
        // Convert to base64
        QString base64Image = QString::fromLatin1(byteArray.toBase64());
        
        // Store the base64 image string in the package
        
        package->base64Image = base64Image;

        // Clear the image data to free memory

        package->image = DImg();

        // Send the package to the next stage
        
        enqueue(nextQueue, package);

        package = nullptr;
    }

    /* =========================================================================================
     * End pipeline stage specific loop
     */

    MLPIPELINE_LOOP_END(MLPipelineStage::Extractor, "AutotagsPipelineKoboldLLM::extractor");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Extractor, MLPipelineStage::Classifier);
}

bool AutotagsPipelineKoboldLLM::classifier()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Classifier, MLPipelineStage::Writer);
    AutotagsPipelineKoboldLLMPackage* package = nullptr;
    QString endpointURL = apiURL + QLatin1String("/v1/chat/completions");
    QNetworkAccessManager* manager = new QNetworkAccessManager();

    llmPrompt = QLatin1String("The tasks are to describe the image and to come up with a large set of keyword tags for it.\n\n"
                              "Write the Description using the active voice.\n\n"
                              "The Keywords must be one or two words each. Generate as many Keywords as possible using a controlled and consistent vocabulary.\n\n"
                              "For both Description and Keywords, make sure to include:\n\n"
                              " - Themes, concepts\n"
                              " - Items, animals, objects\n"
                              " - Structures, landmarks, setting\n"
                              " - Foreground and background elements\n"
                              " - Notable colors, textures, styles\n"
                              " - Actions, activities\n\n"
                              "If humans are present, include: \n"
                              " - Physical appearance\n"
                              " - Gender\n"
                              " - Clothing\n"
                              " - Age range\n"
                              " - Visibly apparent ancestry\n"
                              " - Occupation/role\n"
                              " - Relationships between individuals\n"
                              " - Emotions, expressions, body language\n\n"
                              "Use ENGLISH only. Generate ONLY a JSON object with the keys Description and Keywords as follows {\"Description\": str, \"Keywords\": []}\n"
                              "<EXAMPLE>\n"
                              "The example input would be a stock photo of two apples, one red and one green, against a white backdrop and is a hypothetical Description and Keyword for a non-existent image.\n"
                              "OUTPUT=```json{\"Description\": \"Two apples next to each other, one green and one red, placed side by side against a white background. There is even and diffuse studio lighting. The fruit is glossy and covered with dropplets of water indicating they are fresh and recently washed. The image emphasizes the cleanliness and appetizing nature of the food\", \"Keywords\": [\"studio shot\",\"green\",\"fruit\",\"red\",\"apple\",\"stock image\",\"health food\",\"appetizing\",\"empty background\",\"grocery\",\"food\",\"snack\"]}```\n"
                              "</EXAMPLE> ");

// "The description should be a detailed and accurate representation of the image content.\n"
// "The keywords should be relevant to the image and cover various aspects of it.\n"
// "Please provide the output in JSON format with two fields: \"Description\" and \"Keywords\".\n"
// "The \"Description\" field should contain a detailed description of the image, and the \"Keywords\" field should contain a list of keywords.\n"
// "Here is an example:\n"
// "{\n"
// "  \"Description\": \"A beautiful sunset over a mountain range, with vibrant colors in the sky and a calm lake reflecting the scenery.\",\n"
// "  \"Keywords\": [\"sunset\", \"mountains\", \"lake\", \"nature\", \"scenery\"]\n"
// "}\n\n"
// "Now, please provide the output for the following image:\n");



    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */

    MLPIPELINE_LOOP_START(MLPipelineStage::Classifier, thisQueue);
    package = static_cast<AutotagsPipelineKoboldLLMPackage*>(mlpackage);

    /* =========================================================================================
     * Start pipeline stage specific loop
     *
     * All code from here to MLPIPELINE_LOOP_END is in a try/catch block and loop.
     * This loop is run once per image.
     */

    {
        // Prepare payload using the new method
        QJsonObject payload = createPayload(llmPrompt, package->base64Image);
        
        // Create HTTP headers for the request
        QNetworkRequest request;
        request.setUrl(QUrl(endpointURL));
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("appQStringLiteral(lication/json"));
        QString authHeader = QString::fromLatin1("Bearer %1").arg(apiPassword);
        request.setRawHeader("Authorization", authHeader.toLatin1());

        // Convert JSON payload to QByteArray
        QJsonDocument doc(payload);
        QByteArray jsonData = doc.toJson();
        
        // Log the request being sent
        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Sending request to KoboldLLM API at:" << endpointURL;
        
        // Make the API call
        QNetworkReply* reply = manager->post(request, jsonData);
        
        // Set up a timeout for the request
        QTimer* timer = new QTimer();
        timer->setSingleShot(true);
        timer->setInterval(600000); // 10 minutes timeout (600,000 milliseconds)
        
        // Connect timer timeout signal to cancel the request
        QObject::connect(timer, &QTimer::timeout, [reply, timer]() {
            if (reply->isRunning())
            {
                qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "KoboldLLM API request timed out after 10 minutes";
                reply->abort();
            }
            
            timer->deleteLater();
        });
        
        // Wait for the response synchronously (this is a blocking operation)
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timer->start();
        loop.exec();
        
        // Check if request was successful
        if (reply->error() == QNetworkReply::NoError)
        {
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Received successful response from KoboldLLM API";
            
            // Read the response data
            QByteArray responseData = reply->readAll();
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Response data:" << responseData;
            
            // Process the result using the new method
            QJsonObject resultObj = processResult(responseData);
            
            if (!resultObj.isEmpty())
            {
                // Extract description and keywords
                package->imageDescription = resultObj[QStringLiteral("Description")].toString();
                
                // Get keywords - clear any existing tags first
                package->tagList.clear();
                
                QJsonArray keywordsArray = resultObj[QStringLiteral("Keywords")].toArray();
                int dupeCount = 0;
                for (const QJsonValue &keyword : keywordsArray)
                {
                    if (package->tagList.contains(keyword.toString()))
                    {
                        if (++dupeCount > 3)
                        {
                            // stop processing if too many duplicates

                            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Duplicate keyword found:" << keyword.toString();
                            break;
                        }
                    }
                    if (keyword.isString() && !package->tagList.contains(keyword.toString()))
                    {
                        // Add unique keywords to the tag list
                        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Adding keyword:" << keyword.toString();
                        package->tagList.append(keyword.toString());
                    }
                }
                
                qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Extracted description:" << package->imageDescription;
                qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Extracted" << package->tagList.size() << "keywords";
            }
            else
            {
                package->imageDescription = QStringLiteral("Error: API returned invalid JSON format");
            }
        }
        else
        {
            qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "Error in KoboldLLM API request:" 
                                                << reply->errorString() 
                                                << "Status code:" 
                                                << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                                                
            package->imageDescription = QStringLiteral("Error: API request failed");
        }
        
        // Clean up resources
        timer->stop();
        reply->deleteLater();

        // Clear the base64 image data to free memory

        package->base64Image.clear();

        // Send the package to the next stage

        enqueue(nextQueue, package);

        package = nullptr;
    }

    /* =========================================================================================
     * End pipeline stage specific loop
     */

    MLPIPELINE_LOOP_END(MLPipelineStage::Classifier, "AutotagsPipelineKoboldLLM::classifier");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */
    
    // Clean up network manager
    manager->deleteLater();

    MLPIPELINE_STAGE_END(MLPipelineStage::Classifier, MLPipelineStage::Writer);
}

bool AutotagsPipelineKoboldLLM::writer()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Writer, MLPipelineStage::None);
    AutotagsPipelineKoboldLLMPackage* package = nullptr;

    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */

    TagsCache* const tagsCache = TagsCache::instance();
    const QString rootTag = QLatin1String("auto/");
    const int rootTagId = tagsCache->getOrCreateTag(rootTag);

    MLPIPELINE_LOOP_START(MLPipelineStage::Writer, thisQueue);
    package = static_cast<AutotagsPipelineKoboldLLMPackage*>(mlpackage);

    /* =========================================================================================
     * Start pipeline stage specific loop
     *
     * All code from here to MLPIPELINE_LOOP_END is in a try/catch block and loop.
     * This loop is run once per image.
     */

    {
        bool tagsChanged = false;
        QStringList tagsPath;
        QStringList displayTags;
        QSet<int> newIds;
        
        // In BQM mode we don't want to touch the source image tags
        if (!settings.bqmMode)
        {
            const auto ids = package->info.tagIds();

            for (int tid : ids)
            {
                if (tagsCache->parentTags(tid).contains(rootTagId))
                {
                    if (AutotagsScanSettings::TagMode::Replace == settings.tagMode)
                    {
                        // Clear existing tags when TagMode is replace
                        
                        package->info.removeTag(tid);
                        tagsChanged = true;
                    }
                    else
                    {
                        // Add existing tags to the tags list
                        
                        newIds << tid;
                    }
                }
            }
        }

        // Iterate over the list of found tags
        for (const auto& tag : std::as_const(package->tagList))
        {
            int tagId = -1;

            // Translate tag if requested
            if (!settings.languages.isEmpty())
            {
                for (const QString& trLang : std::as_const(settings.languages))
                {
                    QString trOut;
                    QString error;
                    bool trRet = s_inlineTranslateString(tag, trLang, trOut, error);

                    if (trRet)
                    {
                        // Translation found
                        QString newTag = rootTag + trLang + QLatin1Char('/') + trOut;
                        tagsPath << newTag;
                        displayTags << trOut;
                        tagId = tagsCache->getOrCreateTag(newTag);
                    }
                    else
                    {
                        // Tag could not be translated
                        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Auto-Tags online translation error:"
                                                            << error;
                        QString newTag = rootTag + trLang + QLatin1Char('/') + tag;
                        tagsPath << newTag;
                        displayTags << tag;
                        tagId = tagsCache->getOrCreateTag(newTag);
                    }
                }
            }
            else
            {
                // Translation not requested, use default tag name
                QString newTag = rootTag + tag;
                tagsPath << newTag;
                displayTags << tag;
                tagId = tagsCache->getOrCreateTag(newTag);
            }

            if (!settings.bqmMode && (tagId != -1) && !newIds.contains(tagId))
            {
                // If new tag, add tag to image
                package->info.setTag(tagId);
                newIds << tagId;
                tagsChanged = true;
            }
        }

        // Write tags to the metadata
        if (!settings.bqmMode)
        {
            // Normal mode
            if (tagsChanged || !package->imageDescription.isEmpty())
            {
                MetadataHub::WriteComponent changedMetadata;

                if (!package->imageDescription.isEmpty())
                {
                    package->info.imageComments(CoreDbAccess()).addComment(package->imageDescription);
                    changedMetadata |= MetadataHub::WRITE_COMMENTS;
                }

                if (tagsChanged)
                {
                    changedMetadata |= MetadataHub::WRITE_TAGS;
                }

                MetadataHub hub;
                hub.load(package->info);

                ScanController::FileMetadataWrite writeScope(package->info);
                writeScope.changed(hub.writeToMetadata(package->info, changedMetadata));    
            }
        }
        else
        {
            // BQM mode
            if (tagsPath.size() > 0)
            {
                bqmMeta->setItemTagsPath(tagsPath);
                bqmMeta->save(bqmOutputUrl.toLocalFile());
            }
        }

        QString albumName = CollectionManager::instance()->albumRootLabel(package->info.albumRootId());

        // Send a notification that the image was processed
        notify(MLPipelineNotification::notifyProcessed,
              package->info.name(),
              albumName + package->info.relativePath(),
              displayTags.join(QLatin1String(", ")),
              displayTags.size(),
              package->thumbnailIcon);

        // Log debug information
        if (displayTags.size() > 0)
        {
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldLLM::writer: adding "
                                              << displayTags.size() << " KoboldLLM tags to the image.";
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldLLM::writer: "
                                              << package->info.relativePath() << "/"
                                              << albumName + package->info.relativePath();
        }

        // Delete the package
        delete package;
        package = nullptr;
    }

    /* =========================================================================================
     * End pipeline stage specific loop
     */

    MLPIPELINE_LOOP_END(MLPipelineStage::Writer, "AutotagsPipelineKoboldLLM::writer");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Writer, MLPipelineStage::None);
}

void AutotagsPipelineKoboldLLM::addMoreWorkers()
{
    /* =========================================================================================
     * Use the performanceProfile metrics to find the slowest stages
     * and add more workers to those stages.
     *
     * For the KoboldLLM pipeline, add more workers to handle the potentially large models
     */

    // addWorker(Loader);
    // addWorker(Loader);
    // addWorker(Extractor);
    // addWorker(Extractor);
    // addWorker(Classifier);  // Add extra classifier workers for KoboldLLM models which may be compute-intensive
}

QJsonObject AutotagsPipelineKoboldLLM::createPayload(const QString& prompt, const QString& base64Image)
{
    // Create a JSON object with the LLM prompt and base64 image data
    QJsonArray messagesArray;
    
    // Add system message
    QJsonObject systemMessage;
    systemMessage[QStringLiteral("role")] = QStringLiteral("system");
    systemMessage[QStringLiteral("content")] = QStringLiteral("You describe the image and generate keywords.");
    messagesArray.append(systemMessage);
    
    // Add user message with text and image
    QJsonObject userMessage;
    userMessage[QStringLiteral("role")] = QStringLiteral("user");
    
    QJsonArray contentArray;
    
    // Add text content
    QJsonObject textContent;
    textContent[QStringLiteral("type")] = QStringLiteral("text");
    textContent[QStringLiteral("text")] = prompt;
    contentArray.append(textContent);
    
    // Add image content
    QJsonObject imageContent;
    imageContent[QStringLiteral("type")] = QStringLiteral("image_url");
    
    QJsonObject imageUrl;
    QString urlString = QStringLiteral("data:image/jpeg;base64,") + base64Image;
    imageUrl[QStringLiteral("url")] = urlString;
    
    imageContent[QStringLiteral("image_url")] = imageUrl;
    contentArray.append(imageContent);
    
    userMessage[QStringLiteral("content")] = contentArray;
    messagesArray.append(userMessage);
    
    // Create the payload object
    QJsonObject payload;
    payload[QStringLiteral("messages")] = messagesArray;
    payload[QStringLiteral("max_tokens")] = maxTokens;
    payload[QStringLiteral("temperature")] = qRound(temperature * 100.0) / 100.0;
    payload[QStringLiteral("top_p")] = topP;
    payload[QStringLiteral("top_k")] = topK;
    payload[QStringLiteral("min_p")] = qRound(minP * 100.0) / 100.0;
    
    return payload;
}

QJsonObject AutotagsPipelineKoboldLLM::processResult(const QByteArray& responseData)
{
    QJsonObject resultObj;
    
    // Check if the response contains valid JSON
    QJsonParseError parseError;
    QJsonDocument jsonResponse = QJsonDocument::fromJson(responseData, &parseError);
    
    if (parseError.error == QJsonParseError::NoError)
    {
        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Response contains valid JSON data";
        
        // Extract content from the response
        QJsonObject responseObj = jsonResponse.object();
        
        // Try to get the content from various possible paths in the response JSON structure
        QString contentText;
        
        // Check for content in choices[0].message.content path (typical for OpenAI API format)
        if (responseObj.contains(QStringLiteral("choices")) && 
            responseObj[QStringLiteral("choices")].isArray() && 
            !responseObj[QStringLiteral("choices")].toArray().isEmpty())
        {
            QJsonObject messageObj = responseObj[QStringLiteral("choices")].toArray()[0].toObject()
                                    [QStringLiteral("message")].toObject();
            
            if (messageObj.contains(QStringLiteral("content")))
            {
                contentText = messageObj[QStringLiteral("content")].toString();
            }
        }
        // Also try direct content field for other possible API formats
        else if (responseObj.contains(QStringLiteral("content")))
        {
            contentText = responseObj[QStringLiteral("content")].toString();
        }
        
        // If we found content text, process it
        if (!contentText.isEmpty())
        {
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Found content in response:" << contentText;
            
            // Ensure the JSON string is properly formatted
            QString jsonStr = ensureValidJsonFormat(contentText);
            
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Cleaned JSON string:" << jsonStr;
            
            // Parse the extracted JSON
            QJsonParseError contentParseError;
            QJsonDocument contentJson = QJsonDocument::fromJson(jsonStr.toUtf8(), &contentParseError);
            
            if (contentParseError.error == QJsonParseError::NoError)
            {
                qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Successfully parsed JSON from content";
                
                // Extract description and keywords
                QJsonObject contentObj = contentJson.object();
                if (contentObj.contains(QStringLiteral("Description")) && 
                    contentObj.contains(QStringLiteral("Keywords")))
                {
                    // Return the content object with Description and Keywords
                    return contentObj;
                }
                else
                {
                    qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "JSON missing Description or Keywords fields";
                }
            }
            else
            {
                qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "Failed to parse JSON content:" 
                                                    << contentParseError.errorString();
            }
        }
        else
        {
            qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "No content found in the JSON response";
        }
    }
    else
    {
        qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "Response does not contain valid JSON:"
                                            << parseError.errorString()
                                            << "at offset:" << parseError.offset;
        
        // Log a sample of the response for debugging
        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Response sample:" << responseData.left(200) << "...";
    }
    
    // Return empty object if processing failed
    return resultObj;
}

QString AutotagsPipelineKoboldLLM::ensureValidJsonFormat(const QString& jsonStr)
{
    QString result = jsonStr.trimmed();
    
    // Check and fix beginning of JSON
    if (!result.startsWith(QLatin1Char('{')))
    {
        // Find the first opening brace
        int startPos = result.indexOf(QLatin1Char('{'));
        if (startPos >= 0)
        {
            // Remove everything before the opening brace
            result = result.mid(startPos);
        }
        else
        {
            // No opening brace found, add one
            result.prepend(QLatin1String("{\"Description\": \"Unable to parse response\", \"Keywords\": ["));
        }
    }
    
    // Check and fix end of JSON
    if (!result.endsWith(QLatin1Char('}')) && result.contains(QLatin1Char('}')))
    {
        // Find the last closing brace
        int endPos = result.lastIndexOf(QLatin1Char('}'));
        if (endPos >= 0)
        {
            // Remove everything after the closing brace
            result.truncate(endPos + 1);
        }
    }
    else if (!result.endsWith(QLatin1Char('}')))
    {
        // No proper JSON closing - check for the last quote preceded by alphanumeric character
        // which likely indicates the end of the last keyword in the array
        for (int i = result.length() - 1; i >= 0; --i)
        {
            if (result.at(i) == QLatin1Char('"') && i > 0)
            {
                QChar precedingChar = result.at(i - 1);
                if (precedingChar.isLetterOrNumber())
                {
                    // Found the last quote preceded by an alphanumeric character
                    // Truncate and close the JSON properly
                    result.truncate(i + 1);
                    result.append(QLatin1String("]}"));
                    break;
                }
            }
        }
        
        // If we still don't have a proper JSON closing, add it
        if (!result.endsWith(QLatin1Char('}')))
        {
            result.append(QLatin1String("]}"));
        }
    }
    
    qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "JSON formatted string:" << result;
    return result;
}

} // namespace Digikam

#include "moc_autotagspipelinekoboldllm.cpp"