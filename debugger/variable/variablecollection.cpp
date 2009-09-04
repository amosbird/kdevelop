/*
 * KDevelop Debugger Support
 *
 * Copyright 2007 Hamish Rodda <rodda@kde.org>
 * Copyright 2008 Vladimir Prus <ghost@cs.msu.su>
 * Copyright 2009 Niko Sams <niko.sams@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "variablecollection.h"

#include <QFont>

#include <KLocale>
#include <KDebug>
#include <KTextEditor/TextHintInterface>
#include <KTextEditor/Document>
#include <KParts/PartManager>

#include "../../interfaces/icore.h"
#include "../../interfaces/idocumentcontroller.h"
#include "../../interfaces/iuicontroller.h"
#include "../../sublime/controller.h"
#include "../../sublime/view.h"
#include "../../interfaces/idebugcontroller.h"
#include "../interfaces/idebugsession.h"
#include "../interfaces/ivariablecontroller.h"
#include "variabletooltip.h"

namespace KDevelop {

IDebugSession* currentSession()
{
    return ICore::self()->debugController()->currentSession();
}

IDebugSession::DebuggerState currentSessionState()
{
    if (!currentSession()) return IDebugSession::NotStartedState;
    return currentSession()->state();
}

bool hasStartedSession()
{
    IDebugSession::DebuggerState s = currentSessionState();
    return s != IDebugSession::NotStartedState && s != IDebugSession::EndedState;
}

Variable::Variable(TreeModel* model, TreeItem* parent,
                   const QString& expression,
                   const QString& display)
  : TreeItem(model, parent),
    inScope_(true), topLevel_(true)
{
    expression_ = expression;
    // FIXME: should not duplicate the data, instead overload 'data'
    // and return expression_ directly.
    if (display.isEmpty())
        setData(QVector<QVariant>() << expression << QString());
    else
        setData(QVector<QVariant>() << display << QString());
}

QString Variable::varobj() const
{
    return varobj_;
}

QString Variable::expression() const
{
    return expression_;
}

bool Variable::inScope() const
{
    return inScope_;
}

void Variable::setValue(const QString& v)
{
    itemData[1] = v;
    reportChange();
}

void Variable::setTopLevel(bool v)
{
    topLevel_ = v;
}

void Variable::setInScope(bool v)
{
    inScope_ = v;
    for (int i=0; i < childCount(); ++i) {
        if (Variable *var = dynamic_cast<Variable*>(child(i))) {
            var->setInScope(v);
        }
    }
}

void Variable::setVarobj(const QString& v)
{
    Q_ASSERT(varobj_.isEmpty());
    varobj_ = v;
    allVariables_[varobj_] = this;
}

Variable::~Variable()
{
    if (!varobj_.isEmpty())
    {
        // Delete only top-level variable objects.
        if (topLevel_) {
            if (hasStartedSession()) {
                currentSession()->variableController()->deleteVar(this);
            }
        }
        allVariables_.remove(varobj_);
    }
}

void Variable::createVarobjMaybe(QObject *callback, const char *callbackMethod)
{
    if (!varobj_.isEmpty())
        return;

    if (hasStartedSession()) {
        currentSession()->variableController()->createVarobj(this, callback, callbackMethod);
    }
}

void Variable::die()
{
    removeSelf();
    deleteLater();
}

void Variable::fetchMoreChildren()
{
    // FIXME: should not even try this if app is not started.
    // Probably need to disable open, or something
    if (hasStartedSession()) {
        currentSession()->variableController()->fetchMoreChildren(this);
    }
}

QVariant Variable::data(int column, int role) const
{
    if (column == 1 && role == Qt::ForegroundRole
        && !inScope_)
    {
        // FIXME: returning hardcoded gray is bad,
        // but we don't have access to any widget, or pallette
        // thereof, at this point.
        return QColor(128, 128, 128);
    }
    return TreeItem::data(column, role);
}

Variable* Variable::findByName(const QString& name)
{
    if (allVariables_.count(name) == 0)
        return 0;
    return allVariables_[name];
}

void Variable::markAllDead()
{
    QMap<QString, Variable*>::iterator i, e;
    for (i = allVariables_.begin(), e = allVariables_.end(); i != e; ++i)
    {
        i.value()->varobj_.clear();
        i.value()->inScope_ = false;
        i.value()->reportChange();
    }
    allVariables_.clear();
}

QMap<QString, Variable*> Variable::allVariables_;

Watches::Watches(TreeModel* model, TreeItem* parent)
: TreeItem(model, parent), finishResult_(0)
{
    setData(QVector<QVariant>() << "Auto" << QString());
}

Variable* Watches::add(const QString& expression)
{
    Variable* v = new Variable(model(), this, expression);
    appendChild(v);
    v->createVarobjMaybe();
    return v;
}

Variable *Watches::addFinishResult(const QString& convenienceVarible)
{
    if( finishResult_ )
    {
        removeFinishResult();
    }
    finishResult_ = new Variable(model(), this, convenienceVarible, "$ret");
    appendChild(finishResult_);
    finishResult_->createVarobjMaybe();
    return finishResult_;
}

void Watches::removeFinishResult()
{
    if (finishResult_)
    {
        finishResult_->die();
        finishResult_ = 0;
    }
}

QVariant Watches::data(int column, int role) const
{
#if 0
    if (column == 0 && role == Qt::FontRole)
    {
        /* FIXME: is creating font again and agian efficient? */
        QFont f = font();
        f.setBold(true);
        return f;
    }
#endif
    return TreeItem::data(column, role);
}

void Watches::reinstall()
{
    for (int i = 0; i < childItems.size(); ++i)
    {
        Variable* v = static_cast<Variable*>(child(i));
        v->createVarobjMaybe();
    }
}

Locals::Locals(TreeModel* model, TreeItem* parent)
: TreeItem(model, parent)
{
    setData(QVector<QVariant>() << "Locals" << QString());
}

void Locals::updateLocals(QStringList locals)
{
    setHasMore(false);

    QSet<QString> existing, current;
    for (int i = 0; i < childItems.size(); i++)
    {
        Q_ASSERT(dynamic_cast<KDevelop::Variable*>(child(i)));
        existing << static_cast<KDevelop::Variable*>(child(i))->expression();
    }

    foreach (const QString& var, locals) {
        current << var;
        // If we currently don't display this local var, add it.
        if( !existing.contains( var ) ) {
            KDevelop::Variable* v = new KDevelop::Variable(
                ICore::self()->debugController()->variableCollection(),
                this, var );
            appendChild( v, false );
            v->createVarobjMaybe();
        }
    }

    for (int i = 0; i < childItems.size(); ++i) {
        KDevelop::Variable* v = static_cast<KDevelop::Variable*>(child(i));
        if (!current.contains(v->expression())) {
            removeChild(i);
            --i;
            // FIXME: check that -var-delete is sent.
            delete v;
        }
    }

    // Variables which changed just value are updated by a call to -var-update.
    // Variables that changed type -- likewise.
}

VariablesRoot::VariablesRoot(TreeModel* model)
: TreeItem(model)
{
    watches_ = new Watches(model, this);
    appendChild(watches_, true);
    locals_ = new Locals(model, this);
    appendChild(locals_, true);
}

VariableCollection::VariableCollection(IDebugController* controller)
: TreeModel(QVector<QString>() << "Name" << "Value", controller), m_widgetVisible(false)
{
    universe_ = new VariablesRoot(this);
    setRootItem(universe_);

    //new ModelTest(this);

    connect (ICore::self()->documentController(),
         SIGNAL( textDocumentCreated( KDevelop::IDocument* ) ),
         this,
         SLOT( textDocumentCreated( KDevelop::IDocument* ) ) );

    connect(controller, SIGNAL(currentSessionChanged(KDevelop::IDebugSession*)),
             SLOT(updateAutoUpdate(KDevelop::IDebugSession*)));

    connect(locals(), SIGNAL(expanded()), SLOT(updateAutoUpdate()));
    connect(locals(), SIGNAL(collapsed()), SLOT(updateAutoUpdate()));
    connect(watches(), SIGNAL(expanded()), SLOT(updateAutoUpdate()));
    connect(watches(), SIGNAL(collapsed()), SLOT(updateAutoUpdate()));
}

void VariableCollection::variableWidgetHidden()
{
    m_widgetVisible = false;
    updateAutoUpdate();
}

void VariableCollection::variableWidgetShown()
{
    m_widgetVisible = true;
    updateAutoUpdate();
}

void VariableCollection::updateAutoUpdate(IDebugSession* session)
{
    if (!session) return;

    if (!m_widgetVisible) {
        session->variableController()->setAutoUpdate(IVariableController::UpdateNone);
    } else {
        QFlags<IVariableController::UpdateType> t = IVariableController::UpdateNone;
        if (locals()->isExpanded()) t |= IVariableController::UpdateLocals;
        if (watches()->isExpanded()) t |= IVariableController::UpdateWatches;
        session->variableController()->setAutoUpdate(t);
    }
}

VariableCollection::~ VariableCollection()
{
}

void VariableCollection::textDocumentCreated(IDocument* doc)
{
  connect( doc->textDocument(),
       SIGNAL( viewCreated( KTextEditor::Document* , KTextEditor::View* ) ),
       this, SLOT( viewCreated( KTextEditor::Document*, KTextEditor::View* ) ) );

  foreach( KTextEditor::View* view, doc->textDocument()->views() )
    viewCreated( doc->textDocument(), view );
}

void VariableCollection::viewCreated(KTextEditor::Document* doc,
                                     KTextEditor::View* view)
{
    Q_UNUSED(doc);
    using namespace KTextEditor;
    TextHintInterface *iface = dynamic_cast<TextHintInterface*>(view);
    if( !iface )
        return;

    iface->enableTextHints(500);

    connect(view,
            SIGNAL(needTextHint(const KTextEditor::Cursor&, QString&)),
            this,
            SLOT(textHintRequested(const KTextEditor::Cursor&, QString&)));
}

void VariableCollection::
textHintRequested(const KTextEditor::Cursor& cursor, QString&)
{
    // Don't do anything if there's already an open tooltip.
    if (activeTooltip_)
        return;

    if (!hasStartedSession())
        return;

    // Figure what is the parent widget and what is the text to show
    KTextEditor::View* view = dynamic_cast<KTextEditor::View*>(sender());
    if (!view)
        return;

    KTextEditor::Document* doc = view->document();

    QString expression = currentSession()->variableController()->expressionUnderCursor(doc, cursor);

    if (expression.isEmpty())
        return;

    QPoint local = view->cursorToCoordinate(cursor);
    QPoint global = view->mapToGlobal(local);
    QWidget* w = view->childAt(local);
    if (!w)
        w = view;

    activeTooltip_ = new VariableToolTip(w, global+QPoint(30,30), expression);
}

}

#include "variablecollection.moc"
