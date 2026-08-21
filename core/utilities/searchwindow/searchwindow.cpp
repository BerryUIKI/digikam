/* ============================================================
 *
 * This file is a part of digiKam project
 * https://www.digikam.org
 *
 * Date        : 2008-01-20
 * Description : User interface for searches
 *
 * SPDX-FileCopyrightText: 2008-2012 by Marcel Wiesweg <marcel dot wiesweg at gmx dot de>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ============================================================ */

#include "searchwindow.h"

// Qt includes

#include <QApplication>
#include <QScrollArea>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScreen>
#include <QWindow>

#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QPoint>
#include <QCheckBox>
#include <QPushButton>
#include <QPointer>
#include <QTimer>


// KDE includes

#include <klocalizedstring.h>
#include <ksharedconfig.h>
#include <kconfiggroup.h>

// Local includes

#include "searchview.h"
#include "coredbsearchxml.h"
#include "thememanager.h"
#include "digikam_debug.h"


#include "searchqueryengine.h"
#include "searchlanguagebackend.h"
#include "searchmockbackend.h"
#include "searchllamabackend.h"
#include "searchnlmodelmanager.h"
#include "digikam_config.h"
#include "searchpromptbuilder.h"
#include "searchintentparser.h"
#include "searchcapabilitydictionary.h"
#include "searchintentresolver.h"
#include "searchquerycache.h"
#include "donlinetranslator.h"      

#include "tagscache.h"
#include "colorlabelwidget.h"
#include "filesdownloader.h"

#include "coredbaccess.h"
#include "coredb.h"
#include "dworkingpixmap.h"

namespace Digikam
{

class Q_DECL_HIDDEN SearchWindow::Private
{
public:

    Private() = default;

    QScrollArea*         scrollArea     = nullptr;
    SearchView*          searchView     = nullptr;
    SearchViewBottomBar* bottomBar      = nullptr;
    int                  currentId      = -1;
    bool                 hasTouchedXml  = false;
    QString              oldXml;

    QLineEdit*                  describeEdit    = nullptr;
    QLabel*                     nlStatusLabel   = nullptr;
    QLabel*                     nlSpinnerLabel  = nullptr;
    DWorkingPixmap*             nlSpinnerPix    = nullptr;
    QTimer*                     nlSpinnerTimer  = nullptr;
    int                         nlSpinnerIndex  = 0;
    QCheckBox*                  translateCheck  = nullptr;
    QWidget*  nlReadyPanel                      = nullptr;
    QWidget*  nlDownloadPanel                   = nullptr;

    SearchQueryEngine*          nlEngine        = nullptr;
    SearchLanguageBackend*      nlBackend       = nullptr;
    SearchPromptBuilder         nlPromptBuilder;
    SearchIntentParser          nlParser;
    SearchCapabilityDictionary  nlDictionary;
    SearchIntentResolver*       nlResolver      = nullptr;
    SearchQueryCache            nlCache;
    DOnlineTranslator*          nlTranslator    = nullptr;

    QString                     pendingNlText;
    bool                        nlTranslate     = false;
    bool                        nlFromDialog    = false;
    bool pendingOkAfterInterpret = false;

    SearchQueryIntent nlLastIntent;
    QString nlCommittedXml;
};

SearchWindow::SearchWindow()
    : QWidget(nullptr),
      d      (new Private)
{
    QVBoxLayout* const layout = new QVBoxLayout;

    QWidget* const nlPanel      = new QWidget(this);
    QVBoxLayout* const nlLayout = new QVBoxLayout(nlPanel);

#ifdef HAVE_LLAMACPP

    // Ready state: the describe box, shown when the model is present.
    d->nlReadyPanel             = new QWidget(nlPanel);
    QVBoxLayout* const readyLay = new QVBoxLayout(d->nlReadyPanel);
    readyLay->setContentsMargins(0, 0, 0, 0);

    QLabel* const nlTitle       = new QLabel(i18n("Describe what you want to find"), d->nlReadyPanel);

    d->describeEdit             = new QLineEdit(d->nlReadyPanel);
    d->describeEdit->setPlaceholderText(i18n("e.g. landscape photos with red labels taken near Paris"));
    d->describeEdit->setClearButtonEnabled(true);

    d->nlStatusLabel            = new QLabel(d->nlReadyPanel);
    d->nlStatusLabel->setWordWrap(true);
    d->nlStatusLabel->hide();

    d->nlSpinnerLabel           = new QLabel(d->nlReadyPanel);
    d->nlSpinnerLabel->hide();
    d->nlSpinnerPix             = new DWorkingPixmap(this);
    d->nlSpinnerTimer           = new QTimer(this);

    connect(d->nlSpinnerTimer, &QTimer::timeout, this, [this]()
        {
            if (d->nlSpinnerPix->frameCount() > 0)
            {
                d->nlSpinnerIndex = (d->nlSpinnerIndex + 1) % d->nlSpinnerPix->frameCount();
                d->nlSpinnerLabel->setPixmap(d->nlSpinnerPix->frameAt(d->nlSpinnerIndex));
            }
        });

    d->translateCheck           = new QCheckBox(i18n("Translate my query to English first"), d->nlReadyPanel);
    d->translateCheck->setChecked(d->nlTranslate);
    d->translateCheck->setToolTip(i18n("When enabled, your query is translated to English before "
                                       "being interpreted, so you can type in your own language. "
                                       "Requires an internet connection."));

    readyLay->addWidget(nlTitle);
    readyLay->addWidget(d->describeEdit);
    QHBoxLayout* const nlStatusRow = new QHBoxLayout();
    nlStatusRow->setContentsMargins(0, 0, 0, 0);
    nlStatusRow->addWidget(d->nlSpinnerLabel);
    nlStatusRow->addWidget(d->nlStatusLabel, 1);
    readyLay->addLayout(nlStatusRow);
    readyLay->addWidget(d->translateCheck);

    // Download state: shown when llama.cpp is built but the model is missing.
    d->nlDownloadPanel          = new QWidget(nlPanel);
    QVBoxLayout* const dlLay    = new QVBoxLayout(d->nlDownloadPanel);
    dlLay->setContentsMargins(0, 0, 0, 0);

    QLabel* const dlInfo        = new QLabel(i18n("The natural-language search model has not been "
                                                  "downloaded yet."), d->nlDownloadPanel);
    dlInfo->setWordWrap(true);

    QPushButton* const dlButton = new QPushButton(i18n("Download Model..."), d->nlDownloadPanel);
    dlButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    dlLay->addWidget(dlInfo);
    dlLay->addWidget(dlButton);

    connect(dlButton, &QPushButton::clicked,
            this, &SearchWindow::slotDownloadNlModel);

    nlLayout->addWidget(d->nlReadyPanel);
    nlLayout->addWidget(d->nlDownloadPanel);

    updateNlPanelState();

#else

    Q_UNUSED(nlLayout);
    nlPanel->hide();

#endif

    layout->addWidget(nlPanel);

    d->scrollArea             = new QScrollArea(this);
    d->scrollArea->setWidgetResizable(true);
    d->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    d->searchView             = new SearchView;
    d->searchView->setup();

    d->bottomBar              = new SearchViewBottomBar(d->searchView);
    d->searchView->setBottomBar(d->bottomBar);

    d->scrollArea->setWidget(d->searchView);
    d->scrollArea->setFrameStyle(QFrame::NoFrame);

    layout->addWidget(d->scrollArea);
    layout->addWidget(d->bottomBar);
    layout->setContentsMargins(QMargins());
    layout->setSpacing(0);
    setLayout(layout);

    setVisible(false);
    setWindowTitle(i18nc("@title:window", "Advanced Search"));

    QScreen* screen = qApp->primaryScreen();

    if (QWidget* const widget = qApp->activeWindow())
    {
        if (QWindow* const window = widget->windowHandle())
        {
            screen = window->screen();
        }
    }

    QRect srect = screen->availableGeometry();
    QSize wsize = QSize(1024 <= srect.width()  ? 1024 : srect.width(),
                         800 <= srect.height() ?  800 : srect.height());

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group      = config->group(QLatin1String("AdvancedSearch Widget"));

    if (group.exists())
    {
        QSize confSize = group.readEntry(QLatin1String("Widget Size"), wsize);
        resize(confSize);
    }
    else
    {
        resize(wsize);
    }

    connect(d->searchView, SIGNAL(searchOk()),
            this, SLOT(searchOk()));

    connect(d->searchView, SIGNAL(searchCancel()),
            this, SLOT(searchCancel()));

    connect(d->searchView, SIGNAL(searchTryout()),
            this, SLOT(searchTryout()));

    connect(d->translateCheck, &QCheckBox::toggled,
            this, [this](bool on)
            {
                d->nlTranslate          = on;
                KSharedConfigPtr config = KSharedConfig::openConfig();
                KConfigGroup group      = config->group(QLatin1String("NL Search Settings"));
                group.writeEntry(QLatin1String("Translate Queries"), on);
                group.sync();
            });

    setupNlSearch();

    connect(d->describeEdit, &QLineEdit::returnPressed,
            this, &SearchWindow::slotDescribeSearchSubmitted);
}

SearchWindow::~SearchWindow()
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group      = config->group(QLatin1String("AdvancedSearch Widget"));
    group.writeEntry(QLatin1String("Widget Size"), size());

    delete d->nlResolver;     

    delete d;
}

void SearchWindow::readSearch(int id, const QString& xml)
{
    d->currentId     = id;
    d->hasTouchedXml = false;
    d->oldXml        = xml;
    d->searchView->read(xml);
}

void SearchWindow::reset()
{
    d->currentId     = -1;
    d->hasTouchedXml = false;
    d->oldXml.clear();
    d->nlCommittedXml.clear();          
    d->searchView->read(QString());
}

QString SearchWindow::search() const
{
    return d->searchView->write();
}

void SearchWindow::searchOk()
{
    if (!isVisible()) { return; }

    const QString describe = d->describeEdit->text().trimmed();
    if (!describe.isEmpty() && d->nlCommittedXml.isEmpty())
    {
        d->nlFromDialog = true;
        d->pendingOkAfterInterpret = true;   
        startInterpretation(describe);
        return;                              
    }

    QString xml = d->nlCommittedXml.isEmpty() ? search() : d->nlCommittedXml;
    if (xml.contains(QLatin1String("<group/>")) ||
        xml.contains(QLatin1String("<group></group>")))
    {
        hide();
        return;
    }

    d->hasTouchedXml = true;
    Q_EMIT searchEdited(d->currentId, xml);
    hide();
}

void SearchWindow::searchCancel()
{
    qCDebug(DIGIKAM_GENERAL_LOG) << "SearchWindow: search cancelled";

    if (d->hasTouchedXml)
    {
        Q_EMIT searchEdited(d->currentId, d->oldXml);

        d->hasTouchedXml = false;
    }

    hide();
}

void SearchWindow::searchTryout()
{
    if (!isVisible()) { return; }

    // If the user has typed a natural-language description, interpret that
    // instead of running the (possibly empty) structured form.
    if (d->describeEdit)
    {
        const QString describe = d->describeEdit->text().trimmed();

        if (!describe.isEmpty())
        {
            d->nlFromDialog = true;
            startInterpretation(describe);

            return;
        }
    }

    const QString xml = search();
    
    if (xml.contains(QLatin1String("<group/>")) ||
        xml.contains(QLatin1String("<group></group>")))
    {
        return;   // not to let an empty tryout overwrite committed results
    }

    qCDebug(DIGIKAM_GENERAL_LOG) << "SearchWindow: search tryout";
    d->hasTouchedXml = true;
    Q_EMIT searchEdited(d->currentId, xml);
}

void SearchWindow::keyPressEvent(QKeyEvent* e)
{
    if (d->describeEdit && d->describeEdit->hasFocus() &&
        ((e->key() == Qt::Key_Return) || (e->key() == Qt::Key_Enter)))
    {
        QWidget::keyPressEvent(e);
        return;
    }

    if (!e->modifiers() || ((e->modifiers() & Qt::KeypadModifier) && (e->key() == Qt::Key_Enter)))
    {
        switch (e->key())
        {
            case Qt::Key_Enter:
            case Qt::Key_Return:
            case Qt::Key_Select:
            {
                searchOk();
                break;
            }

            case Qt::Key_F4:
            case Qt::Key_Escape:
            case Qt::Key_Back:
            {
                searchCancel();
                break;
            }

            default:
            {
                break;
            }
        }
    }
}

 
void SearchWindow::setupNlSearch()
{
    d->nlResolver = new SearchIntentResolver(&d->nlDictionary);
 
    #ifdef HAVE_LLAMACPP

        if (SearchNlModelManager::isModelAvailable())
        {
            SearchLlamaBackend* const llamaBackend = new SearchLlamaBackend(this);
            llamaBackend->loadModel(SearchNlModelManager::defaultModelPath());
            d->nlBackend = llamaBackend;

            qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search: using llama.cpp backend";
        }
        else

    #endif

        {
            d->nlBackend = new SearchMockBackend(this);
            d->nlBackend->loadModel(QString());

            qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search: using mock backend"
                                        << "(model not available or built without llama.cpp)";
        }
 
    d->nlEngine   = new SearchQueryEngine(this);
    d->nlEngine->setBackend(d->nlBackend);
    d->nlEngine->setPromptBuilder(&d->nlPromptBuilder);
    d->nlEngine->setParser(&d->nlParser);
    d->nlEngine->setResolver(d->nlResolver);
    d->nlEngine->setCache(&d->nlCache);
 
    // TODO: collection-aware prompt hints
 
    connect(d->nlEngine, &SearchQueryEngine::signalIntentReady,
            this, &SearchWindow::slotNlIntentReady);
 
    connect(d->nlEngine, &SearchQueryEngine::signalClarificationRequired,
            this, &SearchWindow::slotNlClarificationRequired);
 
    connect(d->nlEngine, &SearchQueryEngine::signalErrorOccurred,
            this, &SearchWindow::slotNlError);
 
    connect(d->nlEngine, &SearchQueryEngine::signalStatusMessage,
            this, &SearchWindow::slotNlStatus);
 
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup group      = config->group(QLatin1String("NL Search Settings"));
    d->nlTranslate          = group.readEntry(QLatin1String("Translate Queries"), false);

    if (d->translateCheck)
    {
        d->translateCheck->setChecked(d->nlTranslate);
    }
}
 
SearchQueryEngine* SearchWindow::queryEngine() const
{
    return d->nlEngine;
}
 
void SearchWindow::slotDescribeSearchSubmitted()
{
    const QString text = d->describeEdit->text().trimmed();
    if (text.isEmpty()) { return; }

    d->nlFromDialog = true;          
    startInterpretation(text);
}
 
void SearchWindow::startInterpretation(const QString& text)
{
    d->pendingNlText = text;

    d->nlStatusLabel->setAutoFillBackground(false);
    d->nlStatusLabel->setText(i18n("Interpreting the search..."));
    d->nlStatusLabel->show();
    d->nlSpinnerIndex = 0;
    d->nlSpinnerTimer->start(100);
    d->nlSpinnerLabel->show();

    if (!d->nlTranslate)
    {
        // Translation disabled: pass the original text straight through.
        // Empty normalizedText makes the engine reuse the original.
        d->nlEngine->slotInterpretQuery(text, QString());

        return;
    }

    // Translation enabled: normalize the query to English first, then
    // interpret. On failure we fall back to the original in the finished slot.
    if (!d->nlTranslator)
    {
        d->nlTranslator = new DOnlineTranslator(this);

        connect(d->nlTranslator, &DOnlineTranslator::signalFinished,
                this, &SearchWindow::slotTranslationFinished);
    }

    d->nlTranslator->translate(text,
                               DOnlineTranslator::Google,
                               DOnlineTranslator::English,   // translate INTO English
                               DOnlineTranslator::Auto);     // auto-detect source
}
 
void SearchWindow::slotTranslationFinished()
{
    QString normalized;

    if (d->nlTranslator &&
        (d->nlTranslator->error() == DOnlineTranslator::NoError))
    {
        const QString translated = d->nlTranslator->translation();

        // Only use the translation if it actually produced something and
        // differs from the source; otherwise fall back to the original.
        if (!translated.isEmpty())
        {
            normalized = translated;
        }
    }

    // normalized may be empty here (translation disabled/failed/no-op),
    // in which case the engine reuses the original pendingNlText.
    d->nlEngine->slotInterpretQuery(d->pendingNlText, normalized);
}
 
void SearchWindow::slotNlIntentReady(const SearchQueryIntent& intent)
{
    d->nlSpinnerTimer->stop();
    d->nlSpinnerLabel->hide();

    d->describeEdit->setEnabled(true);
    d->nlStatusLabel->setAutoFillBackground(false);
    d->nlStatusLabel->hide();

    if (d->nlFromDialog)
    {
        applyResolvedIntent(intent);
        d->nlFromDialog = false;

        if (!d->nlCommittedXml.isEmpty())
        {
            d->hasTouchedXml = true;
            Q_EMIT searchEdited(d->currentId, d->nlCommittedXml);
        }

        if (d->pendingOkAfterInterpret)
        {
            d->pendingOkAfterInterpret = false;
            hide();
        }
    }
}
 
void SearchWindow::slotNlClarificationRequired(const SearchQueryIntent& intent)
{
    d->nlSpinnerTimer->stop();
    d->nlSpinnerLabel->hide();
    d->describeEdit->setEnabled(true);

    QMenu menu(this);

    if (!intent.clarificationMessage.isEmpty())
    {
        QAction* const header = menu.addAction(intent.clarificationMessage);
        header->setEnabled(false);
        menu.addSeparator();
    }

    // Build one action per option, stashing its index in the action data.
    for (int i = 0 ; i < intent.clarificationOptions.size() ; ++i)
    {
        QAction* const a = menu.addAction(intent.clarificationOptions.at(i).displayText);
        a->setData(i);
    }

    QAction* const picked = menu.exec(d->describeEdit->mapToGlobal(
                                      QPoint(0, d->describeEdit->height())));

    if (picked && picked->isEnabled() && picked->data().isValid())
    {
        const int idx = picked->data().toInt();

        if ((idx >= 0) && (idx < intent.clarificationOptions.size()))
        {
            const AmbiguityChoice& choice = intent.clarificationOptions.at(idx);

            // Apply the chosen meaning directly - no second inference pass,
            // so the ambiguous word can't re-trigger clarification.
            SearchQueryIntent resolved;
            resolved.parseSucceeded = true;
            resolved.originalQuery  = intent.originalQuery;

            SearchQueryConstraint c;
            c.field = choice.field;
            c.value = choice.value;
            c.op    = (choice.field == QLatin1String("rating")) ? QLatin1String("gte")
                                                                : QLatin1String("eq");
            resolved.constraints << c;

            applyResolvedIntent(resolved);
        }
    }
}
 
void SearchWindow::slotNlError(const QString& message)
{
    d->nlSpinnerTimer->stop();
    d->nlSpinnerLabel->setPixmap(QPixmap());

    d->describeEdit->setEnabled(true);
    d->nlStatusLabel->setText(message);

    QPalette pal = d->nlStatusLabel->palette();
    pal.setColor(QPalette::Active, QPalette::Window, QColor(220, 140, 140));   
    pal.setColor(QPalette::Active, QPalette::WindowText, Qt::black);
    d->nlStatusLabel->setPalette(pal);
    d->nlStatusLabel->setAutoFillBackground(true);

    d->nlStatusLabel->show();
}
 
void SearchWindow::slotNlStatus(const QString& message)
{
    d->nlStatusLabel->setText(message);
    d->nlStatusLabel->show();
}
 
void SearchWindow::applyResolvedIntent(const SearchQueryIntent& intent)
{
    const QString xml = intentToXml(intent);

    if (xml.contains(QLatin1String("<group/>")))
    {
        d->nlSpinnerTimer->stop();
        d->nlSpinnerLabel->setPixmap(QPixmap());

        d->nlStatusLabel->setText(i18nc("@info", "None of that could be turned into "
                                                 "a search. Try describing tags, dates, "
                                                 "ratings, labels, people, or places."));

        QPalette pal = d->nlStatusLabel->palette();
        pal.setColor(QPalette::Active, QPalette::Window, QColor(220, 140, 140));
        pal.setColor(QPalette::Active, QPalette::WindowText, Qt::black);
        d->nlStatusLabel->setPalette(pal);
        d->nlStatusLabel->setAutoFillBackground(true);

        d->nlStatusLabel->show();

        return;
    }

    d->nlStatusLabel->setAutoFillBackground(false);   // clear highlight on success
    d->nlStatusLabel->hide();
    d->searchView->read(xml);
    d->nlCommittedXml = xml;
}
 
QString SearchWindow::intentToXml(const SearchQueryIntent& intent) const
{
    SearchXmlWriter writer;
    writer.writeGroup();

    for (const SearchQueryConstraint& c : intent.constraints)
    {
        writeConstraintToXml(writer, c);
    }

    writer.finishGroup();
    writer.finish();

    const QString xml = writer.xml();

    return xml;
}
 
void SearchWindow::writeConstraintToXml(SearchXmlWriter& writer,
                                        const SearchQueryConstraint& c) const
{
    if (c.field == QLatin1String("tag"))
    {
        QList<int> ids = TagsCache::instance()->tagsForName(c.value);

        if (ids.isEmpty())
        {
            // The model lowercases values, but tag names are stored with
            // their original case and tagsForName() matches exactly.
            // tagsContaining() matches case-insensitively, so use it to
            // recover the tag when only the case differs.

            const QList<int> candidates = TagsCache::instance()->tagsContaining(c.value);

            for (int id : candidates)
            {
                const QString name = TagsCache::instance()->tagName(id);

                if (name.compare(c.value, Qt::CaseInsensitive) == 0)
                {
                    ids << id;
                }
            }
        }

        for (int id : ids)
        {
            writer.writeField(QLatin1String("tagid"), SearchXml::Equal);
            writer.writeValue(id);
            writer.finishField();
        }

        // If still empty: tag not in collection, emit nothing.
    }
    else if (c.field == QLatin1String("colorlabel"))
    {
        static const QMap<QString, int> nameToEnum = {
            { QLatin1String("none"),    NoColorLabel },
            { QLatin1String("red"),     RedLabel     },
            { QLatin1String("orange"),  OrangeLabel  },
            { QLatin1String("yellow"),  YellowLabel  },
            { QLatin1String("green"),   GreenLabel   },
            { QLatin1String("blue"),    BlueLabel    },
            { QLatin1String("magenta"), MagentaLabel },
            { QLatin1String("gray"),    GrayLabel    },
            { QLatin1String("black"),   BlackLabel   },
            { QLatin1String("white"),   WhiteLabel   },
        };

        const int labelEnum = nameToEnum.value(c.value.toLower(), -1);

        if (labelEnum >= 0)
        {
            const int labelTagId = TagsCache::instance()->tagForColorLabel(labelEnum);

            if (labelTagId)
            {
                writer.writeField(QLatin1String("labels"), SearchXml::InTree);
                writer.writeValue(labelTagId);
                writer.finishField();
            }
        }
    }
    else if ((c.field == QLatin1String("keyword")) || (c.field == QLatin1String("caption")))
    {
        writer.writeField(QLatin1String("keyword"), SearchXml::Like);
        writer.writeValue(c.value);
        writer.finishField();
    }
    else if (c.field == QLatin1String("daterange"))
    {
        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            const QDateTime start = QDateTime::fromString(parts.at(0), QLatin1String("yyyy-MM-dd"));
            const QDateTime end   = QDateTime::fromString(parts.at(1), QLatin1String("yyyy-MM-dd"));

            if (start.isValid() && end.isValid())
            {
                writer.writeField(QLatin1String("creationdate"), SearchXml::Interval);
                writer.writeValue(QList<QDateTime>() << start << end);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("rating"))
    {
        SearchXml::Relation rel = SearchXml::Equal;

        if      (c.op == QLatin1String("gte")) { rel = SearchXml::GreaterThanOrEqual; }
        else if (c.op == QLatin1String("lte")) { rel = SearchXml::LessThanOrEqual;    }

        writer.writeField(QLatin1String("rating"), rel);
        writer.writeValue(c.value.toInt());
        writer.finishField();
    }
    else if (c.field == QLatin1String("picklabel"))
    {
        static const QMap<QString, int> nameToEnum = {
            { QLatin1String("none"),     NoPickLabel   },
            { QLatin1String("rejected"), RejectedLabel },
            { QLatin1String("pending"),  PendingLabel  },
            { QLatin1String("accepted"), AcceptedLabel },
        };

        const int labelEnum = nameToEnum.value(c.value.toLower(), -1);

        if (labelEnum >= 0)
        {
            const int labelTagId = TagsCache::instance()->tagForPickLabel(labelEnum);

            if (labelTagId)
            {
                writer.writeField(QLatin1String("labels"), SearchXml::InTree);
                writer.writeValue(labelTagId);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("person"))
    {
        QList<int> ids = TagsCache::instance()->tagsForName(c.value);

        if (ids.isEmpty())
        {
            // Same case-insensitive fallback as for tags: the model
            // lowercases values, but people tag names keep their original case.

            const QList<int> candidates = TagsCache::instance()->tagsContaining(c.value);

            for (int id : candidates)
            {
                const QString name = TagsCache::instance()->tagName(id);

                if (name.compare(c.value, Qt::CaseInsensitive) == 0)
                {
                    ids << id;
                }
            }
        }

        for (int id : ids)
        {
            writer.writeField(QLatin1String("tagid"), SearchXml::Equal);
            writer.writeValue(id);
            writer.finishField();
        }
    }
    else if (c.field == QLatin1String("album"))
    {
        writer.writeField(QLatin1String("albumname"), SearchXml::Like);
        writer.writeValue(c.value);
        writer.finishField();
    }
    else if (c.field == QLatin1String("orientation"))
    {
        // Codes come from SearchFieldPageOrientation: 1 = landscape, 2 = portrait.

        const QString v = c.value.toLower();
        int code        = -1;

        if      (v == QLatin1String("landscape")) { code = 1; }
        else if (v == QLatin1String("portrait"))  { code = 2; }

        if (code > 0)
        {
            writer.writeField(QLatin1String("pageorientation"), SearchXml::Equal);
            writer.writeValue(code);
            writer.finishField();
        }
    }
    else if (c.field == QLatin1String("place"))
    {
        writer.writeGroup();
        writer.setDefaultFieldOperator(SearchXml::Or);

        const char* const locFields[] = { "city", "country", "location" };

        for (const char* const lf : locFields)
        {
            const QString field = QLatin1String(lf);

            // The model lowercases values, but location values are stored with
            // their original case. Match the value against the collection's
            // known values case-insensitively and emit the stored spelling.
            const QStringList known = CoreDbAccess().db()->getAllImagePropertiesByName(field);

            for (const QString& kv : known)
            {
                if (kv.compare(c.value, Qt::CaseInsensitive) == 0)
                {
                    writer.writeField(field, SearchXml::Equal);
                    writer.writeValue(kv);
                    writer.finishField();
                    break;
                }
            }
        }

        writer.finishGroup();
    }
    else if (c.field == QLatin1String("videoduration"))
    {
        auto toSeconds = [](const QString& token, bool* ok) -> int
        {
            QString t = token.trimmed();
            int mult = 1;

            if      (t.endsWith(QLatin1Char('m'))) { mult = 60; t.chop(1); }
            else if (t.endsWith(QLatin1Char('s'))) { mult = 1;  t.chop(1); }

            const int n = t.toInt(ok);
            return n * mult;
        };

        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            bool okLo = false;
            bool okHi = false;
            const int lo = toSeconds(parts.at(0), &okLo);
            const int hi = toSeconds(parts.at(1), &okHi);

            if (okLo && okHi)
            {
                writer.writeField(QLatin1String("videoduration"), SearchXml::Interval);
                writer.writeValue(QList<int>() << lo << hi);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("videoframerate"))
    {
        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            bool okLo = false;
            bool okHi = false;
            const double lo = parts.at(0).toDouble(&okLo);
            const double hi = parts.at(1).toDouble(&okHi);

            if (okLo && okHi)
            {
                writer.writeField(QLatin1String("videoframerate"), SearchXml::Interval);
                writer.writeValue(QList<double>() << lo << hi);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("videoaudiobitrate"))
    {
        const QStringList parts = c.value.split(QLatin1String(".."));

        if (parts.size() == 2)
        {
            bool okLo = false;
            bool okHi = false;
            const int lo = parts.at(0).toInt(&okLo);
            const int hi = parts.at(1).toInt(&okHi);

            if (okLo && okHi)
            {
                writer.writeField(QLatin1String("videoaudiobitrate"), SearchXml::Interval);
                writer.writeValue(QList<int>() << lo << hi);
                writer.finishField();
            }
        }
    }
    else if (c.field == QLatin1String("format"))
    {
        const QString fmt = c.value.toUpper();

        if (fmt == QLatin1String("RAW"))
        {
            // RAW files are stored as "RAW-<EXT>" (RAW-CR2, RAW-NEF, ...).
            // Match by wildcard the way digiKam's own MIME filter does with
            // startsWith("RAW").
            writer.writeField(QLatin1String("format"), SearchXml::Like);
            writer.writeValue(QLatin1String("RAW*"));
            writer.finishField();
        }
        else
        {
            writer.writeField(QLatin1String("format"), SearchXml::Equal);
            writer.writeValue(fmt);
            writer.finishField();
        }
    }
    else
    {
        qCDebug(DIGIKAM_NLSEARCH_LOG) << "NL search: no XML mapping for field"
                                      << c.field << "value" << c.value;
    }
}

void SearchWindow::updateNlPanelState()
{
#ifdef HAVE_LLAMACPP

    const bool ready = SearchNlModelManager::isModelAvailable();

    if (d->nlReadyPanel)    { d->nlReadyPanel->setVisible(ready);     }
    if (d->nlDownloadPanel) { d->nlDownloadPanel->setVisible(!ready); }

#endif
}

void SearchWindow::slotDownloadNlModel()
{
#ifdef HAVE_LLAMACPP

    QPointer<FilesDownloader> floader = new FilesDownloader(this);
    floader->startDownload();
    delete floader;

    if (SearchNlModelManager::isModelAvailable() && d->nlEngine)
    {
        SearchLlamaBackend* const llamaBackend = new SearchLlamaBackend(this);
        llamaBackend->loadModel(SearchNlModelManager::defaultModelPath());
        d->nlBackend = llamaBackend;
        d->nlEngine->setBackend(d->nlBackend);

        qCInfo(DIGIKAM_NLSEARCH_LOG) << "NL search: llama.cpp backend loaded after download";
    }

    updateNlPanelState();

#endif
}

} // namespace Digikam

#include "moc_searchwindow.cpp"
