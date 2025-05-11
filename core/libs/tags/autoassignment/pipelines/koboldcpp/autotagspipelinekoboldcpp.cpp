/* ============================================================
 *
 * This file is a part of digikam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-06
 * Description : Performs autotags using KoboldCpp models
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "autotagspipelinekoboldcpp.h"

// Qt includes

#include <QList>
#include <QElapsedTimer>
#include <QByteArray>
#include <QBuffer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QtConcurrentRun>
#include <QFuture>
#include <QTimer>

// KDE includes

#include <klocalizedstring.h>

// LLM includes

#include "llmprocessmanager.h"
#include "llmprocessmanagerkoboldcpp.h"

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
#include "scancontroller.h"
#include "metadatahub.h"
#include "itemcomments.h"
#include "tagscache.h"
#include "localizeselector.h"
#include "autotagspipelinekoboldcpppackage.h"

namespace Digikam
{

AutotagsPipelineKoboldCpp::AutotagsPipelineKoboldCpp(const AutotagsScanSettings& settings)
    : AutotagsPipelineBase(settings),
      m_llmProcessManager(nullptr)
{
    maxQueueDepth = 1;
}

AutotagsPipelineKoboldCpp::~AutotagsPipelineKoboldCpp()
{
    if (m_llmProcessManager)
    {
        m_llmProcessManager->shutdownEngine();
        m_llmProcessManager->deleteLater();
        m_llmProcessManager = nullptr;
    }
}

bool AutotagsPipelineKoboldCpp::start()
{
    bool started = false;

    // Create the KoboldCpp model

    try
    {
        model = static_cast<DNNModelNet*>(DNNModelManager::instance()->getModel(settings.objectDetectModel,
                                                                                DNNModelUsage::DNNUsageImageClassification));
    
        if (!model)
        {
            qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::start(): Unable to load model:"
                                                   << settings.objectDetectModel;
            return false;
        }

        m_llmSettings.modelPath = model->info.fileName;
        m_llmSettings.mmprojPath = model->info.configName;
        m_llmSettings.chatCompletionsAdapter = model->info.preprocessor;

        // Create the LLM Process Manager
        m_llmProcessManager = new LLMProcessManagerKoboldCpp(m_llmSettings, this);
        
        // Start the KoboldCpp LLM engine
        if (!m_llmProcessManager->isEngineRunning())
        {
            // Connect signals for engine status updates
            
            // Connect to the engineStarted signal to know when the process has started
            connect(m_llmProcessManager, SIGNAL(engineStarted(bool)), 
                    this, SLOT(slotEngineStarted(bool)));
                        
            // Connect to the engineReadyChanged signal to know when the engine is ready for inference
            connect(m_llmProcessManager, SIGNAL(engineReadyChanged(bool)),
                    this, SLOT(slotEngineReadyChanged(bool)));
                        
            // Start the engine
            started = m_llmProcessManager->startEngine();
            
            if (!started) 
            {
                // notify(MLPipelineNotification::,
                //        i18n("Unable to start KoboldCpp engine. Please check the settings and try again."),
                //        QString(),
                //        QString(),
                //        0,
                //        QIcon());

                qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::start(): Unable to initiate KoboldCpp engine start:"
                                                       << m_llmProcessManager->lastErrorMessage();

                delete m_llmProcessManager;
                m_llmProcessManager = nullptr;

                Q_EMIT finished();

                return false;
            }
            
            // Set timeout for waiting
            QTimer::singleShot(llmStartupTimeout * 1000, this, SLOT(checkLLMEngineReady()));
        }            
    }
    catch (const std::exception& e)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::start(): Unable to load model. " << e.what();

        if (model)
        {
            model = nullptr;
        }
        
        return false;
    }
    catch (...)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::start(): Unknown error. Unable to load model.";

        if (model)
        {
            model = nullptr;
        }
        
        return false;
    }

    // Check if the model and classifier were created

    if (nullptr == model)
    {
        return false;
    }

    return started;
}

void AutotagsPipelineKoboldCpp::cancel()
{
    if (!cancelled)
    {
        // QTimer::singleShot(100, m_llmProcessManager, SLOT(abortInference()));

        // Capture 'this' pointer to access class members from the lambda
        // Store the QFuture to resolve the 'nodiscard' warning
        if (m_llmProcessManager)
        {
            QFuture<void> future = QtConcurrent::run([this]()
            {
                QThread::msleep(100);
                m_llmProcessManager->abortInference();
                qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::cancel(): sent abort request to LLM engine";
            });
        }
        
        // Note: We don't need to wait for the future completion here
        // as we only want to start the abort process asynchronously
        
        AutotagsPipelineBase::cancel();
    }
}

bool AutotagsPipelineKoboldCpp::finder()
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
    Q_UNUSED(moreCpu);

    nextQueue->setMaxDepth(1);

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

            // Iterate over the image IDs and add unique IDs to the queue for processing

            for (qlonglong imageId : std::as_const(imageIds))
            {
                ++performanceProfileList[MLPipelineStage::Finder].itemCount;

                // Filter out duplicate image IDs

                if (!filter.contains(imageId))
                {
                    ++totalItemCount;
                    filter << imageId;

                    enqueue(nextQueue, new AutotagsPipelineKoboldCppPackage(imageId));
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

bool AutotagsPipelineKoboldCpp::loader()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Loader, MLPipelineStage::Extractor);
    AutotagsPipelineKoboldCppPackage* package = nullptr;

    int imageSize= 1024;

    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */

    MLPIPELINE_LOOP_START(MLPipelineStage::Loader, thisQueue);
    package = static_cast<AutotagsPipelineKoboldCppPackage*>(mlpackage);

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
            // Load image for KoboldCpp model processing

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

    MLPIPELINE_LOOP_END(MLPipelineStage::Loader, "AutotagsPipelineKoboldCpp::loader");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Loader, MLPipelineStage::Extractor);
}

bool AutotagsPipelineKoboldCpp::extractor()
{
    MLPIPELINE_STAGE_START(QThread::NormalPriority, MLPipelineStage::Extractor, MLPipelineStage::Classifier);
    AutotagsPipelineKoboldCppPackage* package = nullptr;

    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */
    
    MLPIPELINE_LOOP_START(MLPipelineStage::Extractor, thisQueue);
    package = static_cast<AutotagsPipelineKoboldCppPackage*>(mlpackage);

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

    MLPIPELINE_LOOP_END(MLPipelineStage::Extractor, "AutotagsPipelineKoboldCpp::extractor");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Extractor, MLPipelineStage::Classifier);
}

bool AutotagsPipelineKoboldCpp::classifier()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Classifier, MLPipelineStage::Writer);
    AutotagsPipelineKoboldCppPackage* package = nullptr;

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
    package = static_cast<AutotagsPipelineKoboldCppPackage*>(mlpackage);

    /* =========================================================================================
     * Start pipeline stage specific loop
     *
     * All code from here to MLPIPELINE_LOOP_END is in a try/catch block and loop.
     * This loop is run once per image.
     */

    {
        // Prepare payload using the new method
        QJsonObject payload = createPayload(llmPrompt, package->base64Image);
        
        // Log the request being sent
        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Sending request to KoboldCpp API";
        
        // First check if the LLM engine is ready
        if (!m_llmProcessManager->engineReady())
        {
            qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "KoboldCpp is not ready for inference. Cancelling the pipeline...";
            cancelled = true;
            QTimer::singleShot(10, this, SLOT(cancel()));
            delete package;
            package = nullptr;
            break;
        }

        QString albumName = CollectionManager::instance()->albumRootLabel(package->info.albumRootId());

        notify(MLPipelineNotification::notifyProcessed,
               package->info.name(),
               albumName + package->info.relativePath(),
               QString(),
               0,
               package->thumbnailIcon);

        
        // Use LLMProcessManager's submitRequest method to communicate with the LLM
        QByteArray responseData = m_llmProcessManager->submitRequest(QStringLiteral("/v1/chat/completions"), 
                                                                     payload, 
                                                                     LLMProcessManager::POST);

        if (cancelled)
        {
            delete package;
            package = nullptr;
            break;
        }

        // Check if request was successful
        if (!responseData.isEmpty())
        {
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Received successful response from KoboldCpp API";
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Response data:" << responseData;

            // Use LLMResponseHelper to process the result
            QJsonObject resultObj = LLMResponseHelper::processResult(responseData);

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
            // throw an exception for empty response
            QString errorMessage = i18n("KoboldCpp API returned an empty response");
            qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << errorMessage;;
            throw std::runtime_error(errorMessage.toStdString());
        }

        // Clear the base64 image data to free memory

        package->base64Image.clear();

        // Send the package to the next stage

        enqueue(nextQueue, package);

        package = nullptr;
    }

    /* =========================================================================================
     * End pipeline stage specific loop
     */

    MLPIPELINE_LOOP_END(MLPipelineStage::Classifier, "AutotagsPipelineKoboldCpp::classifier");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */
    
    MLPIPELINE_STAGE_END(MLPipelineStage::Classifier, MLPipelineStage::Writer);
}

bool AutotagsPipelineKoboldCpp::writer()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Writer, MLPipelineStage::None);
    AutotagsPipelineKoboldCppPackage* package = nullptr;

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
    package = static_cast<AutotagsPipelineKoboldCppPackage*>(mlpackage);

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
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::writer: adding "
                                                << displayTags.size() << " KoboldCpp tags to the image.";
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::writer: "
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

    MLPIPELINE_LOOP_END(MLPipelineStage::Writer, "AutotagsPipelineKoboldCpp::writer");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Writer, MLPipelineStage::None);
}

void AutotagsPipelineKoboldCpp::addMoreWorkers()
{
}

// The extractJsonFromContent method has been replaced by LLMResponseHelper::processResult and LLMResponseHelper::ensureValidJsonFormat

QJsonObject AutotagsPipelineKoboldCpp::createPayload(const QString& prompt, const QString& base64Image)
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

void AutotagsPipelineKoboldCpp::slotEngineStarted(bool success)
{
    if (!success)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AutotagsPipelineKoboldCpp::slotEngineStarted: Failed to start KoboldCpp engine:"
                                         << m_llmProcessManager->lastErrorMessage();
    }
    else
    {
        qCInfo(DIGIKAM_AUTOTAGSENGINE_LOG) << "KoboldCpp process started, waiting for engine to be ready...";
    }
}

void AutotagsPipelineKoboldCpp::slotEngineReadyChanged(bool ready)
{
    if (ready && !cancelled && !running)
    {
        running = true;

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

        AutotagsPipelineBase::start();
        qCInfo(DIGIKAM_AUTOTAGSENGINE_LOG) << "KoboldCpp LLM is ready for inference";
    }
    else if (!cancelled)
    {
        qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "KoboldCpp LLM is no longer ready for inference";
        QTimer::singleShot(10, this, SLOT(cancel()));
    }
}

void AutotagsPipelineKoboldCpp::checkLLMEngineReady()
{
    /*
     * Check if the LLM engine is ready for inference.
     * This method is connected to a timer to check engine readiness at startup
     * to kill the process if startup takes too long.
     * 
     * Note: This method is called from a separate thread, so be careful with UI updates.
    */ 

    if (!running && !cancelled)
    {
        if (m_llmProcessManager)
        {
            m_llmProcessManager->shutdownEngine();
            delete m_llmProcessManager;
            m_llmProcessManager = nullptr;
        }

        Q_EMIT finished();
    }
}

} // namespace Digikam

#include "moc_autotagspipelinekoboldcpp.cpp"