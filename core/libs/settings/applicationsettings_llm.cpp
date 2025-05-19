/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2003-16-10
 * Description : application settings interface
 *
 * SPDX-FileCopyrightText: 2003-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2025      by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "applicationsettings_p.h"

namespace Digikam
{

// LLM Settings

void ApplicationSettings::setAIAnnotateEnabled(bool enable)
{
    d->aiAnnotateEnable = enable;
}

bool ApplicationSettings::getAIAnnotateEnabled() const
{
    return d->aiAnnotateEnable;
}

void ApplicationSettings::setAIAnnotateEnableAdvSet(bool enable)
{
    d->aiAnnotateEnableAdvSet = enable;
}

bool ApplicationSettings::getAIAnnotateAdvSetEnabled() const
{
    return d->aiAnnotateEnableAdvSet;
}

void ApplicationSettings::setAIAnnotateEnableDescription(bool enable)
{
    d->aiAnnotateEnableDescription = enable;
}

bool ApplicationSettings::getAIAnnotateDescriptionEnabled() const
{
    return d->aiAnnotateEnableDescription;
}

void ApplicationSettings::setAIAnnotateEnableTags(bool enable)
{
    d->aiAnnotateEnableTags = enable;
}

bool ApplicationSettings::getAIAnnotateTagsEnabled() const
{
    return d->aiAnnotateEnableTags;
}

// Aesthetic rating feature has been removed

void ApplicationSettings::setAIAnnotateEnableCustomPrompt(bool enable)
{
    d->aiAnnotateEnableCustomPrompt = enable;
}

bool ApplicationSettings::getAIAnnotateCustomPromptEnabled() const
{
    return d->aiAnnotateEnableCustomPrompt;
}

void ApplicationSettings::setAIAnnotateEnableExternalLLM(bool enable)
{
    d->aiAnnotateEnableExternalLLM = enable;
}

bool ApplicationSettings::getAIAnnotateExternalLLMEnabled() const
{
    return d->aiAnnotateEnableExternalLLM;
}

void ApplicationSettings::setAIAnnotateExternalLLMUrl(const QString& url)
{
    d->aiAnnotateExternalLLMUrl = url;
}

QString ApplicationSettings::getAIAnnotateExternalLLMUrl() const
{
    return d->aiAnnotateExternalLLMUrl;
}

void ApplicationSettings::setAIAnnotateExternalLLMPort(int port)
{
    d->aiAnnotateExternalLLMPort = port;
}

int ApplicationSettings::getAIAnnotateExternalLLMPort() const
{
    return d->aiAnnotateExternalLLMPort;
}

void ApplicationSettings::setAIAnnotateEngine(const LLMProcessSettings::LLMEngineType engine)
{
    d->aiAnnotateLlmEngine = engine;
}

LLMProcessSettings::LLMEngineType ApplicationSettings::getAIAnnotateEngine() const
{
    return d->aiAnnotateLlmEngine;
}

void ApplicationSettings::setAIAnnotateMaxTags(int maxTags)
{
    d->aiAnnotateMaxTags = maxTags;
}

int ApplicationSettings::getAIAnnotateMaxTags() const
{
    return d->aiAnnotateMaxTags;
}

void ApplicationSettings::setAIAnnotateMaxTokens(int maxTokens)
{
    d->aiAnnotateMaxTokens = maxTokens;
}

int ApplicationSettings::getAIAnnotateMaxTokens() const
{
    return d->aiAnnotateMaxTokens;
}

void ApplicationSettings::setAIAnnotatePrompt(const QString& prompt)
{
    d->aiAnnotateLlmPrompt = prompt;
}

QString ApplicationSettings::getAIAnnotateCustomPrompt() const
{
    return d->aiAnnotateLlmPrompt;
}

void ApplicationSettings::setAIAnnotateTemperature(double temperature)
{
    d->aiAnnotateLlmTemperature = temperature;
}

double ApplicationSettings::getAIAnnotateTemperature() const
{
    return d->aiAnnotateLlmTemperature;
}

void ApplicationSettings::setAIAnnotateVoice(LLMProcessSettings::AIVoiceType voiceType)
{
    d->aiAnnotateVoiceType = voiceType;
}

LLMProcessSettings::AIVoiceType ApplicationSettings::getAIAnnotateVoice() const
{
    return d->aiAnnotateVoiceType;
}

void ApplicationSettings::setAIAnnotateVoicePerspective(LLMProcessSettings::AIVoicePerspective perspective)
{
    d->aiAnnotateVoicePerspective = perspective;
}

LLMProcessSettings::AIVoicePerspective ApplicationSettings::getAIAnnotateVoicePerspective() const
{
    return d->aiAnnotateVoicePerspective;
}

void ApplicationSettings::setAIAnnotateTextTones(LLMProcessSettings::AITextTones tones)
{
    d->aiAnnotateTextTones = tones;
}

LLMProcessSettings::AITextTones ApplicationSettings::getAIAnnotateTextTones() const
{
    return d->aiAnnotateTextTones;
}

} // namespace Digikam