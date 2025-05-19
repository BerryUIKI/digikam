/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2025-05-07
 * Description : Package class for OpenAI pipeline
 *
 * SPDX-FileCopyrightText: 2024-2025 by Gilles Caulier <caulier dot gilles at gmail dot com>
 * SPDX-FileCopyrightText: 2024-2025 by Michael Miller <michael underscore miller at msn dot com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "annotatepipelineopenaipackage.h"

// Local includes

#include "iteminfo.h"

namespace Digikam
{

AnnotatePipelineOpenAIPackage::AnnotatePipelineOpenAIPackage(qlonglong imageId)
    : AutotagsPipelinePackageBase(imageId)
{
    // Initialize OpenAI specific members with default values
    base64Image = QString();
    imageDescription = QString();
}

} // namespace Digikam