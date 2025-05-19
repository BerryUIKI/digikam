/* ============================================================
 *
 * This file is a part of digikam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-06
 * Description : Performs autotags using OpenAI models
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "annotatepipelineopenai.h"

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
#include "annotatepipelineopenaipackage.h"
#include "llmprocessmanager.h"
#include "llmprocessmanagerkoboldcpp.h"

namespace Digikam
{

AnnotatePipelineOpenAI::AnnotatePipelineOpenAI(const AutotagsScanSettings& settings)
    : AutotagsPipelineBase(settings),
      m_llmProcessManager(nullptr)
{
    // limit the queues to 1 package to avoid using too much memory

    maxQueueDepth = 1;

    // apply advanced settings if enabled

    if (ApplicationSettings::instance()->getAIAnnotateAdvSetEnabled())
    {
        maxTokens     = ApplicationSettings::instance()->getAIAnnotateMaxTokens();
        temperature   = ApplicationSettings::instance()->getAIAnnotateTemperature();
    }
}

AnnotatePipelineOpenAI::~AnnotatePipelineOpenAI()
{
    if (m_llmProcessManager)
    {
        m_llmProcessManager->shutdownEngine();
        m_llmProcessManager->deleteLater();
        m_llmProcessManager = nullptr;
    }
}

bool AnnotatePipelineOpenAI::start()
{
    bool started = false;

    // Create the OpenAI model

    try
    {
        model = static_cast<DNNModelNet*>(DNNModelManager::instance()->getModel(settings.objectDetectModel,
                                                                                DNNModelUsage::DNNUsageImageClassification));
    
        if (!model)
        {
            qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::start(): Unable to load model:"
                                                   << settings.objectDetectModel;
            return false;
        }

        m_llmSettings.modelPath = model->info.fileName;
        m_llmSettings.mmprojPath = model->info.configName;
        m_llmSettings.chatCompletionsAdapter = model->info.preprocessor;

        if (ApplicationSettings::instance()->getAIAnnotateExternalLLMEnabled())
        {
            m_llmSettings.external      = true;
            m_llmSettings.apiHost       = ApplicationSettings::instance()->getAIAnnotateExternalLLMUrl();
            m_llmSettings.apiPort       = ApplicationSettings::instance()->getAIAnnotateExternalLLMPort();
            m_llmSettings.engineType    = ApplicationSettings::instance()->getAIAnnotateEngine();
        }

        // Create the LLM Process Manager
        m_llmProcessManager = new LLMProcessManagerKoboldCpp(m_llmSettings, this);
        
        // Start the OpenAI LLM engine
        if (!m_llmProcessManager->isEngineRunning() || m_llmSettings.external)
        {
            // Connect signals for engine status updates
            
            // Connect to the engineStarted signal to know when the process has started
            connect(m_llmProcessManager, SIGNAL(signalEngineStarted(bool)), 
                    this, SLOT(slotEngineStarted(bool)));
                        
            // Connect to the engineReadyChanged signal to know when the engine is ready for inference
            connect(m_llmProcessManager, SIGNAL(signalEngineReady()),
                    this, SLOT(slotEngineReady()));

            connect(m_llmProcessManager, &LLMProcessManager::signalEngineStopped,
                    this, &AnnotatePipelineOpenAI::cancel);

            // Start the engine
            started = m_llmProcessManager->startEngine();

            QString startupMessage;
            
            if (!started) 
            {
                startupMessage = i18n("Unable to start OpenAI engine. Please check the settings and try again.");

                qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::start(): Unable to initiate OpenAI engine start:"
                                                       << m_llmProcessManager->lastErrorMessage();

                delete m_llmProcessManager;
                m_llmProcessManager = nullptr;
                return false;
            }
            else
            {
                startupMessage = i18n("OpenAI engine starting. Please wait...");
                Q_EMIT signalStartupMessage(startupMessage);
                qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << startupMessage;
            }
        }            
    }
    catch (const std::exception& e)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::start(): Unable to load model. " << e.what();

        if (model)
        {
            model = nullptr;
        }
        
        return false;
    }
    catch (...)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::start(): Unknown error. Unable to load model.";

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

void AnnotatePipelineOpenAI::cancel()
{
    if (!cancelled)
    {
        // disconnect the signals to avoid multiple calls

        disconnect(m_llmProcessManager, SIGNAL(signalEngineStarted(bool)), 
                   this, SLOT(slotEngineStarted(bool)));

        disconnect(m_llmProcessManager, SIGNAL(signalEngineReady()),
                   this, SLOT(slotEngineReady()));

        disconnect(m_llmProcessManager, &LLMProcessManager::signalEngineStopped,
                   this, &AnnotatePipelineOpenAI::cancel);

        // Capture 'this' pointer to access class members from the lambda
        // Store the QFuture to resolve the 'nodiscard' warning
        if (m_llmProcessManager && m_llmProcessManager->isEngineRunning())
        {
            // Use QtConcurrent to run the abort process in a separate thread
            // This allows the main thread to continue processing other tasks
            // while waiting for the LLM engine to abort the inference
            
            QFuture<void> future = QtConcurrent::run([this]()
            {
                QThread::msleep(100);
                m_llmProcessManager->abortInference();
                qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::cancel(): sent abort request to LLM engine";
            });
        }
        else
        {
            Q_EMIT finished();
        }
        
        // Note: We don't need to wait for the future completion here
        // as we only want to start the abort process asynchronously
        
        AutotagsPipelineBase::cancel();
    }
}

bool AnnotatePipelineOpenAI::finder()
{

    MLPIPELINE_FINDER_START(QThread::LowPriority, MLPipelineStage::Loader);

    /* =========================================================================================
     * Pipeline finder specific initialization code
     *
     * Use the block from here to MLPIPELINE_FINDER_END to find the IDs images to process.
     * The code in this block is run once per stage initialization. The number of instances
     * is always 1.
     */

    Q_UNUSED(moreCpu);

    nextQueue->setMaxDepth(1);

    while (!cancelled)
    {
        // Add images without comments to the queue using CoreDB method

        QList<qlonglong> imageIdsWithoutComments = CoreDbAccess().db()->getNextLLMItemsToScan();

        // Q_EMIT the updated total item count
        qlonglong itemCount = totalItemCount + imageIdsWithoutComments.size() - m_imageIdsInProgress.size();

        Q_EMIT signalUpdateItemCount(itemCount);
        
        // Process the images without comments first
        
        for (qlonglong imageId : std::as_const(imageIdsWithoutComments))
        {
            ++performanceProfileList[MLPipelineStage::Finder].itemCount;
            
            if (!m_imageIdsInProgress.contains(imageId))
            {
                ++totalItemCount;

                {
                    // keep track of the IDs in progress so we don't process them again

                    QMutexLocker locker(&m_progressIdsMutex);
                    m_imageIdsInProgress << imageId;
                }
                
                qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::finder: adding image ID" << imageId;

                enqueue(nextQueue, new AnnotatePipelineOpenAIPackage(imageId));

                // added imageId to the pipeline.  Break to refresh the list
                // this allows recently added images to be processed first

                break;
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

bool AnnotatePipelineOpenAI::loader()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Loader, MLPipelineStage::Extractor);
    AnnotatePipelineOpenAIPackage* package = nullptr;

    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */

    MLPIPELINE_LOOP_START(MLPipelineStage::Loader, thisQueue);
    package = static_cast<AnnotatePipelineOpenAIPackage*>(mlpackage);

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
            // Load image for OpenAI model processing

            package->image = PreviewLoadThread::loadFastSynchronously(package->info.filePath(), model->info.imageSize);

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

    MLPIPELINE_LOOP_END(MLPipelineStage::Loader, "AnnotatePipelineOpenAI::loader");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Loader, MLPipelineStage::Extractor);
}

bool AnnotatePipelineOpenAI::extractor()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Extractor, MLPipelineStage::Classifier);
    AnnotatePipelineOpenAIPackage* package = nullptr;

    /* =========================================================================================
     * Pipeline stage specific initialization code
     *
     * Use the block from here to MLPIPELINE_LOOP_START to initialize the stage.
     * The code in this block is run once per stage initialization. The number of instances
     * is at least 1. More instances are created by addMoreWorkers if needed.
     */
    
    MLPIPELINE_LOOP_START(MLPipelineStage::Extractor, thisQueue);
    package = static_cast<AnnotatePipelineOpenAIPackage*>(mlpackage);

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

    MLPIPELINE_LOOP_END(MLPipelineStage::Extractor, "AnnotatePipelineOpenAI::extractor");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Extractor, MLPipelineStage::Classifier);
}

bool AnnotatePipelineOpenAI::classifier()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Classifier, MLPipelineStage::Writer);
    AnnotatePipelineOpenAIPackage* package = nullptr;

    llmPrompt = createPrompt();

    // llmPrompt = QLatin1String("The tasks are to describe the image and to come up with a large set of keyword tags for it.\n\n"
    //                           "Write the Description using the active voice.\n\n"
    //                           "The Keywords must be one or two words each. Generate as many Keywords as possible using a controlled and consistent vocabulary.\n\n"
    //                           "For both Description and Keywords, make sure to include:\n\n"
    //                           " - Themes, concepts\n"
    //                           " - Items, animals, objects\n"
    //                           " - Structures, landmarks, setting\n"
    //                           " - Foreground and background elements\n"
    //                           " - Notable colors, textures, styles\n"
    //                           " - Actions, activities\n\n"
    //                           "If humans are present, include: \n"
    //                           " - Physical appearance\n"
    //                           " - Gender\n"
    //                           " - Clothing\n"
    //                           " - Age range\n"
    //                           " - Visibly apparent ancestry\n"
    //                           " - Occupation/role\n"
    //                           " - Relationships between individuals\n"
    //                           " - Emotions, expressions, body language\n\n"
    //                           "Use ENGLISH only. Generate ONLY a JSON object with the keys Description and Keywords as follows {\"Description\": str, \"Keywords\": []}\n"
    //                           "<EXAMPLE>\n"
    //                           "The example input would be a stock photo of two apples, one red and one green, against a white backdrop and is a hypothetical Description and Keyword for a non-existent image.\n"
    //                           "OUTPUT=```json{\"Description\": \"Two apples next to each other, one green and one red, placed side by side against a white background. There is even and diffuse studio lighting. The fruit is glossy and covered with dropplets of water indicating they are fresh and recently washed. The image emphasizes the cleanliness and appetizing nature of the food\", \"Keywords\": [\"studio shot\",\"green\",\"fruit\",\"red\",\"apple\",\"stock image\",\"health food\",\"appetizing\",\"empty background\",\"grocery\",\"food\",\"snack\"]}```\n"
    //                           "</EXAMPLE> ");

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
    package = static_cast<AnnotatePipelineOpenAIPackage*>(mlpackage);

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
        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Sending request to OpenAI API";
        
        // First check if the LLM engine is ready
        if (!m_llmProcessManager->engineReady())
        {
            qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "OpenAI is not ready for inference. Cancelling the pipeline...";
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

        QElapsedTimer timer;
        timer.start();

        // Use LLMProcessManager's submitRequest method to communicate with the LLM

        QByteArray responseData = m_llmProcessManager->submitRequest(QStringLiteral("/v1/chat/completions"), 
                                                                     payload, 
                                                                     LLMProcessManager::POST);

        qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "OpenAI API response time:" << timer.elapsed() << "ms";
        
        if (cancelled)
        {
            delete package;
            package = nullptr;
            break;
        }

        // Check if request was successful
        if (!responseData.isEmpty())
        {
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Received successful response from OpenAI API";
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "Response data:" << responseData;

            // Use LLMResponseHelper to process the result
            QJsonObject resultObj = LLMResponseHelper::processResult(responseData);

            if (!resultObj.isEmpty())
            {
                // Extract description and keywords
                if ((llmResponseType & LLMResponses::LLMDescription) && resultObj.contains(QStringLiteral("Description")))
                {
                    // Extract description
                    package->imageDescription = resultObj[QStringLiteral("Description")].toString();
                }
                
                // Get keywords - clear any existing tags first
                package->tagList.clear();
                
                if ((llmResponseType & LLMResponses::LLMKeywords) && resultObj.contains(QStringLiteral("Keywords")))
                {
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
            }
            else
            {
                // TODO: add error handling for empty response
                
            }
        }
        else
        {
            // throw an exception for empty response
            
            QString errorMessage = i18n("OpenAI API returned an empty response");
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

    MLPIPELINE_LOOP_END(MLPipelineStage::Classifier, "AnnotatePipelineOpenAI::classifier");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */
    
    MLPIPELINE_STAGE_END(MLPipelineStage::Classifier, MLPipelineStage::Writer);
}

bool AnnotatePipelineOpenAI::writer()
{
    MLPIPELINE_STAGE_START(QThread::LowPriority, MLPipelineStage::Writer, MLPipelineStage::None);
    AnnotatePipelineOpenAIPackage* package = nullptr;

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
    package = static_cast<AnnotatePipelineOpenAIPackage*>(mlpackage);

    /* =========================================================================================
     * Start pipeline stage specific loop
     *
     * All code from here to MLPIPELINE_LOOP_END is in a try/catch block and loop.
     * This loop is run once per image.
     */

    {
        QStringList tagsPath;
        QStringList displayTags;
        QSet<int> newIds;
        MetadataHub::WriteComponent changedMetadata;

        // Add the description to the image comments

        if ((llmResponseType & LLMResponses::LLMDescription) && !package->imageDescription.isEmpty())
        {
            package->info.imageComments(CoreDbAccess()).addComment(package->imageDescription);
            changedMetadata |= MetadataHub::WRITE_COMMENTS;
        }

        // Add the tags to the image

        if ((llmResponseType & LLMResponses::LLMKeywords) && !package->tagList.isEmpty())
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
                        changedMetadata |= MetadataHub::WRITE_TAGS;
                    }
                    else
                    {
                        // Add existing tags to the new tags list
                        
                        newIds << tid;
                    }
                }
            }

            // Iterate over the new list of found tags

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

                if ((tagId != -1) && !newIds.contains(tagId))
                {
                    // If new tag, add tag to image

                    package->info.setTag(tagId);
                    newIds << tagId;
                    changedMetadata |= MetadataHub::WRITE_TAGS;
                }
            }        
        }

        // Save the metadata to the image
        if (changedMetadata)
        {
            MetadataHub hub;
            hub.load(package->info);

            ScanController::FileMetadataWrite writeScope(package->info);
            writeScope.changed(hub.writeToMetadata(package->info, changedMetadata));
        }

        // Send a notification that the image was processed

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
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::writer: adding "
                                                << displayTags.size() << " OpenAI tags to the image.";
            qCDebug(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::writer: "
                                                << albumName + package->info.relativePath();
        }
        
        {
            // remove the ID from the in-progress list so the image may be reprocessed if modified by the user

            QMutexLocker locker(&m_progressIdsMutex);
            m_imageIdsInProgress.removeAll(package->info.id());
        }

        // Delete the package

        delete package;
        package = nullptr;
    }

    /* =========================================================================================
     * End pipeline stage specific loop
     */

    MLPIPELINE_LOOP_END(MLPipelineStage::Writer, "AnnotatePipelineOpenAI::writer");

    /* =========================================================================================
     * Pipeline stage specific cleanup
     *
     * Use the block from here to MLPIPELINE_STAGE_END to clean up any resources used by the stage.
     */

    MLPIPELINE_STAGE_END(MLPipelineStage::Writer, MLPipelineStage::None);
}

void AnnotatePipelineOpenAI::addMoreWorkers()
{
}

// The extractJsonFromContent method has been replaced by LLMResponseHelper::processResult and LLMResponseHelper::ensureValidJsonFormat

QJsonObject AnnotatePipelineOpenAI::createPayload(const QString& prompt, const QString& base64Image) const
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
    payload[QStringLiteral("top_p")] = qRound(topP * 100.0) / 100.0;
    payload[QStringLiteral("top_k")] = topK;
    payload[QStringLiteral("min_p")] = qRound(minP * 100.0) / 100.0;
    
    return payload;
}

void AnnotatePipelineOpenAI::slotEngineStarted(bool success)
{
    if (!success)
    {
        qCCritical(DIGIKAM_AUTOTAGSENGINE_LOG) << "AnnotatePipelineOpenAI::slotEngineStarted: Failed to start OpenAI engine:"
                                         << m_llmProcessManager->lastErrorMessage();

        Q_EMIT finished();
    }
    else
    {
        qCInfo(DIGIKAM_AUTOTAGSENGINE_LOG) << "OpenAI process started, waiting for engine to be ready...";

        // Set timeout for waiting
        // 
        // QTimer::singleShot(llmStartupTimeout * 1000, this, SLOT(checkLLMEngineReady()));
    }
}

void AnnotatePipelineOpenAI::slotEngineReady()
{
    if (!cancelled && !running)
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
        qCInfo(DIGIKAM_AUTOTAGSENGINE_LOG) << "OpenAI LLM is ready for inference";
    }
    else if (!cancelled)
    {
        qCWarning(DIGIKAM_AUTOTAGSENGINE_LOG) << "OpenAI LLM is no longer ready for inference";
        QTimer::singleShot(10, this, SLOT(cancel()));
    }
}

void AnnotatePipelineOpenAI::checkLLMEngineReady()
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

QString AnnotatePipelineOpenAI::createPrompt() const
{
    ApplicationSettings* const appSettings = ApplicationSettings::instance();

    /**
     * @note: All strings are English.  Do not translate them. 
     * */

    // Add the tasks to the prompt

    QString llmPrompt = QLatin1String("The tasks are to describe the image and to come up with a set of keyword tags for it.\n");

    if (appSettings->getAIAnnotateAdvSetEnabled() && appSettings->getAIAnnotateCustomPromptEnabled())
    {
        llmPrompt += appSettings->getAIAnnotateCustomPrompt();
        llmPrompt += QLatin1String("\n");
    }
    else
    {
        llmPrompt += QLatin1String("Write the Description using the active");

        switch(appSettings->getAIAnnotateVoice())
        {
            case LLMProcessSettings::AIVoiceType::Casual:
                llmPrompt += QLatin1String(", casual");
                break;
            case LLMProcessSettings::AIVoiceType::Formal:
                llmPrompt += QLatin1String(", formal");
                break;
            case LLMProcessSettings::AIVoiceType::Neutral:
                llmPrompt += QLatin1String(", neutral");
                break;
        }

        llmPrompt += QLatin1String(" voice");

        switch (appSettings->getAIAnnotateVoicePerspective())
        {
            case LLMProcessSettings::FirstPerson:
                llmPrompt += QLatin1String(" in the first person");
                break;
            case LLMProcessSettings::SecondPerson:
                llmPrompt += QLatin1String(" in the second person");
                break;
            case LLMProcessSettings::ThirdPerson:
                llmPrompt += QLatin1String(" in the third person");
                break;
            default:
                break;
        }

        llmPrompt += QLatin1String(".\n");

        if (LLMProcessSettings::NoTone != appSettings->getAIAnnotateTextTones())
        {
            // iterate over the tones and add them to the prompt

            llmPrompt += QLatin1String("The tone of the description is ");
            LLMProcessSettings::AITextTones tone = appSettings->getAIAnnotateTextTones();
            if (tone & LLMProcessSettings::AITextTone::Authoritative)
            {
                llmPrompt += QLatin1String("authoritative, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Concise)
            {
                llmPrompt += QLatin1String("concise, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Descriptive)
            {
                llmPrompt += QLatin1String("descriptive, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Dramatic)
            {
                llmPrompt += QLatin1String("dramatic, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Empathetic)
            {
                llmPrompt += QLatin1String("empathetic, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Encouraging)
            {
                llmPrompt += QLatin1String("encouraging, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Friendly)
            {
                llmPrompt += QLatin1String("friendly, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Humorous)
            {
                llmPrompt += QLatin1String("humorous, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Inspirational)
            {
                llmPrompt += QLatin1String("inspirational, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Persuasive)
            {
                llmPrompt += QLatin1String("persuasive, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Poetic)
            {
                llmPrompt += QLatin1String("poetic, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Respectful)
            {
                llmPrompt += QLatin1String("respectful, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Serious)
            {
                llmPrompt += QLatin1String("serious, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Technical)
            {
                llmPrompt += QLatin1String("technical, ");
            }
            if (tone & LLMProcessSettings::AITextTone::Urgent)
            {
                llmPrompt += QLatin1String("urgent, ");
            }

            // trim the last comma and space from the string
            llmPrompt.chop(2);
            llmPrompt += QLatin1String(".\n");
        }

        llmPrompt += QLatin1String("The Keywords must be one or two words each. Generate no more than %1 Keywords using a controlled and consistent vocabulary.\n\n").arg(QString::number(appSettings->getAIAnnotateMaxTags()));

        llmPrompt += QLatin1String("For both Description and Keywords, make sure to include:\n"
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
                                " - Emotions, expressions, body language\n\n");
    }

    // get the current language from the system settings
    QString currentLanguage = QLocale::system().languageToString(QLocale::system().language());
    QString currentCountry  = QLocale::system().countryToString(QLocale::system().country());

    // define the result format

    llmPrompt += QLatin1String("Use %1 only as spoken in %2."
                               " Generate ONLY a JSON object with the keys Description, Keywords as follows"
                               " {\"Description\": str, \"Keywords\": []}\n")
                            .arg(currentLanguage)
                            .arg(currentCountry);
                            //    "<EXAMPLE>\n"
                            //    "The example input would be a stock photo of two apples, one red and one green, against a white backdrop and is a hypothetical Description and Keyword for a non-existent image.\n"
                            //    "OUTPUT=```json{\"Description\": \"Two apples next to each other, one green and one red, placed side by side against a white background. There is even and diffuse studio lighting. The fruit is glossy and covered with dropplets of water indicating they are fresh and recently washed. The image emphasizes the cleanliness and appetizing nature of the food\", \"Keywords\": [\"studio shot\",\"green\",\"fruit\",\"red\",\"apple\",\"stock image\",\"health food\",\"appetizing\",\"empty background\",\"grocery\",\"food\",\"snack\"]}```\n"
                            //    "</EXAMPLE> ");

    return llmPrompt;

}

} // namespace Digikam

#include "moc_annotatepipelineopenai.cpp"