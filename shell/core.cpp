/***************************************************************************
 *   Copyright (C) 2007 by Alexander Dymo <adymo@kdevelop.org>             *
 *   Copyright (C) 2007 by Kris Wong <kris.p.wong@gmail.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/
#include "core.h"

#include <QApplication>
#include <QPointer>

#include <kdebug.h>
#include <kglobal.h>

#include <sublime/area.h>
#include <sublime/tooldocument.h>

#include "shellextension.h"

#include "mainwindow.h"
#include "uicontroller.h"
#include "plugincontroller.h"
#include "projectcontroller.h"
#include "partcontroller.h"
#include "languagecontroller.h"
#include "editorintegrator.h"
#include "documentcontroller.h"
#include "backgroundparser.h"

namespace KDevelop {

Core *Core::m_self = 0;

struct CorePrivate {
    CorePrivate(Core *core): m_core(core)
    {
    }
    void initialize()
    {
        uiController = new UiController(m_core);
        pluginController = new PluginController(m_core);
        partController = new PartController(m_core, uiController->defaultMainWindow());
        projectController = new ProjectController(m_core);
        languageController = new LanguageController(m_core);
        documentController = new DocumentController(m_core);

        uiController->initialize();
        languageController->initialize();

        pluginController->loadPlugins( PluginController::Global );

        Sublime::Area *defaultArea = uiController->defaultArea();
        uiController->showArea(defaultArea, uiController->defaultMainWindow());

        uiController->defaultMainWindow()->show();
    }
    ~CorePrivate()
    {
        delete projectController;
        delete languageController;
        delete pluginController;
        delete uiController;
        delete partController;
        delete documentController;
    }

    QPointer<PluginController> pluginController;
    QPointer<UiController> uiController;
    QPointer<ProjectController> projectController;
    QPointer<LanguageController> languageController;
    QPointer<PartController> partController;
    QPointer<DocumentController> documentController;

    Core *m_core;
};

void Core::initialize()
{
    if (m_self)
        return;

    m_self = new Core();
    m_self->d->initialize();
}

Core *KDevelop::Core::self()
{
    return m_self;
}

Core::Core(QObject *parent)
    : ICore(parent)
{
    d = new CorePrivate(this);
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(deleteLater()));
}

Core::~Core()
{
    kDebug() << k_funcinfo << endl;
    cleanup();
    delete d;
}

void Core::cleanup()
{
    d->pluginController->cleanup();
}

IUiController *Core::uiController()
{
    return d->uiController;
}

UiController *Core::uiControllerInternal()
{
    return d->uiController;
}

IPluginController *Core::pluginController()
{
    return d->pluginController;
}

PluginController *Core::pluginControllerInternal()
{
    return d->pluginController;
}

IProjectController *Core::projectController()
{
    return d->projectController;
}

ProjectController *Core::projectControllerInternal()
{
    return d->projectController;
}

KParts::PartManager *Core::partManager()
{
    return d->partController;
}

PartController *Core::partManagerInternal()
{
    return d->partController;
}

ILanguageController *Core::languageController()
{
    return d->languageController;
}

LanguageController *Core::languageControllerInternal()
{
    return d->languageController;
}

IDocumentController *Core::documentController()
{
    return d->documentController;
}

DocumentController *Core::documentControllerInternal()
{
    return d->documentController;
}

}

// kate: space-indent on; indent-width 4; tab-width: 4; replace-tabs on; auto-insert-doxygen on
