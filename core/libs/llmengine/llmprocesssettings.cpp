/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-09
 * Description : Settings for LLM process operations
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

// Qt includes

#include <QThread>

// Local includes

#include "digikam_debug.h"
#include "llmprocesssettings.h"

namespace Digikam
{

LLMProcessSettings::LLMProcessSettings(const LLMProcessSettings& other)
    : modelPath(other.modelPath),
      mmprojPath(other.mmprojPath),
      apiHost(other.apiHost),
      apiPort(other.apiPort),
      engineType(other.engineType),
      external(other.external),
      contextSize(other.contextSize),
      threads(other.threads),
      chatCompletionsAdapter(other.chatCompletionsAdapter),
      additionalArgs(other.additionalArgs),
      voiceType(other.voiceType),
      textTones(other.textTones)
{
}

LLMProcessSettings& LLMProcessSettings::operator=(const LLMProcessSettings& other)
{
    if (this != &other)
    {
        modelPath = other.modelPath;
        mmprojPath = other.mmprojPath;
        apiHost = other.apiHost;
        apiPort = other.apiPort;
        engineType = other.engineType;
        external = other.external;
        contextSize = other.contextSize;
        threads = other.threads;
        chatCompletionsAdapter = other.chatCompletionsAdapter;
        additionalArgs = other.additionalArgs;
        voiceType = other.voiceType;
        textTones = other.textTones;
    }
    
    return *this;
}

bool LLMProcessSettings::isValid() const
{
    // At a minimum, we need a model path to be valid
    if (external)
    {
        return (!apiHost.isEmpty());
    }
    else
    {
        return (!modelPath.isEmpty() &&
                !apiHost.isEmpty() &&
                apiPort > 0 &&
                contextSize > 0 &&
                threads > 0);
    }

}

QString LLMProcessSettings::validationError() const
{
    if (modelPath.isEmpty())
    {
        return QStringLiteral("Model path not set");
    }
    
    return QString();
}

} // namespace Digikam
