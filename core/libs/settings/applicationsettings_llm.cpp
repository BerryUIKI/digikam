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

void ApplicationSettings::setEnableAIAnnotate(bool enable)
{
    d->aiAnnotateEnable = enable;
}

bool ApplicationSettings::getAIAnnotateEnabled() const
{
    return d->aiAnnotateEnable;
}

void ApplicationSettings::setEnableAIAnnotateAdvSet(bool enable)
{
    d->aiAnnotateEnableAdvSet = enable;
}

bool ApplicationSettings::getAIAnnotateAdvSetEnabled() const
{
    return d->aiAnnotateEnableAdvSet;
}

void ApplicationSettings::setEnableAIAnnotateDescription(bool enable)
{
    d->aiAnnotateEnableDescription = enable;
}

bool ApplicationSettings::getAIAnnotateDescriptionEnabled() const
{
    return d->aiAnnotateEnableDescription;
}

void ApplicationSettings::setEnableAIAnnotateTags(bool enable)
{
    d->aiAnnotateEnableTags = enable;
}

bool ApplicationSettings::getAIAnnotateTagsEnabled() const
{
    return d->aiAnnotateEnableTags;
}

void ApplicationSettings::setEnableAIAnnotateAesthetic(bool enable)
{
    d->aiAnnotateEnableAesthetic = enable;
}

bool ApplicationSettings::getAIAnnotateAestheticEnabled() const
{
    return d->aiAnnotateEnableAesthetic;
}

void ApplicationSettings::setEnableAIAnnotateCustomPrompt(bool enable)
{
    d->aiAnnotateEnableCustomPrompt = enable;
}

bool ApplicationSettings::getAIAnnotateCustomPromptEnabled() const
{
    return d->aiAnnotateEnableCustomPrompt;
}

void ApplicationSettings::setAIAnnotateEngine(const LLMProcessManagerKoboldCpp::LLMEngineType engine)
{
    d->aiAnnotateLlmEngine = engine;
}

LLMProcessManagerKoboldCpp::LLMEngineType ApplicationSettings::getAIAnnotateEngine() const
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

void ApplicationSettings::setAIAnnotatePrompt(const QString& prompt)
{
    d->aiAnnotateLlmPrompt = prompt;
}

QString ApplicationSettings::getAIAnnotatePrompt() const
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

void ApplicationSettings::setAIAnnotateTextTones(LLMProcessSettings::AITextTones tones)
{
    d->aiAnnotateTextTones = tones;
}

LLMProcessSettings::AITextTones ApplicationSettings::getAIAnnotateTextTones() const
{
    return d->aiAnnotateTextTones;
}

}