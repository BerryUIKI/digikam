/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * SPDX-FileCopyrightText: 2026 by Srirupa Datta <srirupa dot sps at gmail dot com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchlanguagebackend.h"

namespace Digikam
{

SearchLanguageBackend::SearchLanguageBackend(QObject* const parent)
    : QObject(parent)
{
}

SearchLanguageBackend::~SearchLanguageBackend()
{
}

void SearchLanguageBackend::cancel()
{
    // Default: no-op. Backends that support cancellation override this.
}

} // namespace Digikam
