/*
 * This file is part of KDevelop
 *
 * Copyright 2007 David Nolden <david.nolden.kdevelop@art-master.de>
 * Copyright 2016 Kevin Funk <kfunk@kde.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Library General Public License as
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

#include "quickopenwidget.h"
#include <KParts/MainWindow>
#include "debug.h"

#include "expandingtree/expandingdelegate.h"
#include "quickopenmodel.h"

#include <icore.h>
#include <iuicontroller.h>

#include <qtcompat_p.h>

#include <QDialog>
#include <QMenuBar>
#include <QKeyEvent>
#include <QScrollBar>

#include <KTextEditor/CodeCompletionModel>

using namespace KDevelop;

class QuickOpenDelegate : public ExpandingDelegate {
    Q_OBJECT
public:
  QuickOpenDelegate(ExpandingWidgetModel* model, QObject* parent = nullptr) : ExpandingDelegate(model, parent) {
  }
  QList<QTextLayout::FormatRange> createHighlighting(const QModelIndex& index, QStyleOptionViewItem& option) const override {
    QList<QVariant> highlighting = index.data(KTextEditor::CodeCompletionModel::CustomHighlight).toList();
    if(!highlighting.isEmpty())
      return highlightingFromVariantList(highlighting);
    return ExpandingDelegate::createHighlighting( index, option );
  }

};

QuickOpenWidget::QuickOpenWidget( QString title, QuickOpenModel* model, const QStringList& initialItems, const QStringList& initialScopes, bool listOnly, bool noSearchField )
  : m_model(model)
  , m_expandedTemporary(false)
  , m_hadNoCommandSinceAlt(true)
{
  m_filterTimer.setSingleShot(true);
  connect(&m_filterTimer, &QTimer::timeout, this, &QuickOpenWidget::applyFilter);

  Q_UNUSED( title );
  ui.setupUi( this );
  ui.list->header()->hide();
  ui.list->setRootIsDecorated( false );
  ui.list->setVerticalScrollMode( QAbstractItemView::ScrollPerItem );

  connect(ui.list->verticalScrollBar(), &QScrollBar::valueChanged, m_model, &QuickOpenModel::placeExpandingWidgets);

  ui.searchLine->setFocus();

  ui.list->setItemDelegate( new QuickOpenDelegate( m_model, ui.list ) );

  if(!listOnly) {
    QStringList allTypes = m_model->allTypes();
    QStringList allScopes = m_model->allScopes();

    QMenu* itemsMenu = new QMenu(this);

    foreach( const QString &type, allTypes )
    {
      QAction* action = new QAction(type, itemsMenu);
      action->setCheckable(true);
      action->setChecked(initialItems.isEmpty() || initialItems.contains( type ));
      connect( action, &QAction::toggled, this, &QuickOpenWidget::updateProviders, Qt::QueuedConnection );
      itemsMenu->addAction(action);
    }

    ui.itemsButton->setMenu(itemsMenu);

    QMenu* scopesMenu = new QMenu(this);

    foreach( const QString &scope, allScopes )
    {
      QAction* action = new QAction(scope, scopesMenu);
      action->setCheckable(true);
      action->setChecked(initialScopes.isEmpty() || initialScopes.contains( scope ) );

      connect( action, &QAction::toggled, this, &QuickOpenWidget::updateProviders, Qt::QueuedConnection );
      scopesMenu->addAction(action);
    }

    ui.scopesButton->setMenu(scopesMenu);

  }else{
    ui.list->setFocusPolicy(Qt::StrongFocus);
    ui.scopesButton->hide();
    ui.itemsButton->hide();
    ui.label->hide();
    ui.label_2->hide();
  }

  showSearchField(!noSearchField);

  ui.okButton->hide();
  ui.cancelButton->hide();

  ui.searchLine->installEventFilter( this );
  ui.list->installEventFilter( this );
  ui.list->setFocusPolicy(Qt::NoFocus);
  ui.scopesButton->setFocusPolicy(Qt::NoFocus);
  ui.itemsButton->setFocusPolicy(Qt::NoFocus);

  connect( ui.searchLine, &QLineEdit::textChanged, this, &QuickOpenWidget::textChanged );

  connect( ui.list, &ExpandingTree::doubleClicked, this, &QuickOpenWidget::doubleClicked );

  connect(ui.okButton, &QPushButton::clicked, this, &QuickOpenWidget::accept);
  connect(ui.okButton, &QPushButton::clicked, this, &QuickOpenWidget::ready);
  connect(ui.cancelButton, &QPushButton::clicked, this, &QuickOpenWidget::ready);

  updateProviders();
  updateTimerInterval(true);

// no need to call this, it's done by updateProviders already
//   m_model->restart();
}

void QuickOpenWidget::showStandardButtons(bool show)
{
  if(show) {
    ui.okButton->show();
    ui.cancelButton->show();
  }else{
    ui.okButton->hide();
    ui.cancelButton->hide();
  }
}

void QuickOpenWidget::updateTimerInterval(bool cheapFilterChange)
{
  const int MAX_ITEMS = 10000;
  if ( cheapFilterChange && m_model->rowCount(QModelIndex()) < MAX_ITEMS ) {
    // cheap change and there are currently just a few items,
    // so apply filter instantly
    m_filterTimer.setInterval(0);
  } else if ( m_model->unfilteredRowCount() < MAX_ITEMS ) {
    // not a cheap change, but there are generally
    // just a few items in the list: apply filter instantly
    m_filterTimer.setInterval(0);
  } else {
    // otherwise use a timer to prevent sluggishness while typing
    m_filterTimer.setInterval(300);
  }
}

void QuickOpenWidget::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);

    // The column width only has an effect _after_ the widget has been shown
    ui.list->setColumnWidth( 0, 20 );
}

void QuickOpenWidget::setAlternativeSearchField(QLineEdit* alterantiveSearchField)
{
    ui.searchLine = alterantiveSearchField;
    ui.searchLine->installEventFilter( this );
    connect( ui.searchLine, &QLineEdit::textChanged, this, &QuickOpenWidget::textChanged );
}


void QuickOpenWidget::showSearchField(bool b)
{
  if(b){
    ui.searchLine->show();
    ui.searchLabel->show();
  }else{
    ui.searchLine->hide();
    ui.searchLabel->hide();
  }
}

void QuickOpenWidget::prepareShow()
{
  ui.list->setModel( nullptr );
  ui.list->setVerticalScrollMode(QAbstractItemView::ScrollPerItem);
  m_model->setTreeView( ui.list );
  ui.list->setModel( m_model );

  m_filterTimer.stop();
  m_filter = QString();

  if (!m_preselectedText.isEmpty())
  {
    ui.searchLine->setText(m_preselectedText);
    ui.searchLine->selectAll();
  }

  m_model->restart(false);

  connect( ui.list->selectionModel(), &QItemSelectionModel::currentRowChanged,
           this, &QuickOpenWidget::callRowSelected );
  connect( ui.list->selectionModel(), &QItemSelectionModel::selectionChanged,
           this, &QuickOpenWidget::callRowSelected );
}

void QuickOpenWidgetDialog::run() {
  m_widget->prepareShow();
  m_dialog->show();
}

QuickOpenWidget::~QuickOpenWidget() {
  m_model->setTreeView( nullptr );
}


QuickOpenWidgetDialog::QuickOpenWidgetDialog(QString title, QuickOpenModel* model, const QStringList& initialItems, const QStringList& initialScopes, bool listOnly, bool noSearchField)
{
  m_widget = new QuickOpenWidget(title, model, initialItems, initialScopes, listOnly, noSearchField);
  // the QMenu might close on esc and we want to close the whole dialog then
  connect( m_widget, &QuickOpenWidget::aboutToHide, this, &QuickOpenWidgetDialog::deleteLater );

  m_dialog = new QDialog( ICore::self()->uiController()->activeMainWindow() );
  m_dialog->resize(QSize(800, 400));

  m_dialog->setWindowTitle(title);
  QVBoxLayout* layout = new QVBoxLayout(m_dialog);
  layout->addWidget(m_widget);
  m_widget->showStandardButtons(true);
  connect(m_widget, &QuickOpenWidget::ready, m_dialog, &QDialog::close);
  connect( m_dialog, &QDialog::accepted, m_widget, &QuickOpenWidget::accept );
}


QuickOpenWidgetDialog::~QuickOpenWidgetDialog()
{
  delete m_dialog;
}

void QuickOpenWidget::setPreselectedText(const QString& text)
{
  m_preselectedText = text;
}

void QuickOpenWidget::updateProviders() {
  if(QAction* action = (sender() ? qobject_cast<QAction*>(sender()) : nullptr)) {
    QMenu* menu = qobject_cast<QMenu*>(action->parentWidget());
    if(menu) {
      menu->show();
      menu->setActiveAction(action);
    }
  }

  QStringList checkedItems;

  if(ui.itemsButton->menu()) {

    foreach( QObject* obj, ui.itemsButton->menu()->children() ) {
      QAction* box = qobject_cast<QAction*>( obj );
      if( box ) {
        if( box->isChecked() )
          checkedItems << box->text().remove('&');
      }
    }
        ui.itemsButton->setText(checkedItems.join(QStringLiteral(", ")));
  }

  QStringList checkedScopes;

  if(ui.scopesButton->menu()) {

    foreach( QObject* obj, ui.scopesButton->menu()->children() ) {
      QAction* box = qobject_cast<QAction*>( obj );
      if( box ) {
        if( box->isChecked() )
          checkedScopes << box->text().remove('&');
      }
    }

    ui.scopesButton->setText(checkedScopes.join(QStringLiteral(", ")));
  }

  emit itemsChanged( checkedItems );
  emit scopesChanged( checkedScopes );
  m_model->enableProviders( checkedItems, checkedScopes );
}

void QuickOpenWidget::textChanged( const QString& str )
{
  // "cheap" when something was just appended to the current filter
  updateTimerInterval(str.startsWith(m_filter));
  m_filter = str;
  m_filterTimer.start();
}

void QuickOpenWidget::applyFilter()
{
  m_model->textChanged( m_filter );

  QModelIndex currentIndex = m_model->index(0, 0, QModelIndex());
    ui.list->selectionModel()->setCurrentIndex( currentIndex, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows | QItemSelectionModel::Current );

  callRowSelected();
}

void QuickOpenWidget::callRowSelected() {
  QModelIndex currentIndex = ui.list->selectionModel()->currentIndex();
  if( currentIndex.isValid() )
    m_model->rowSelected( currentIndex );
  else
    qCDebug(PLUGIN_QUICKOPEN) << "current index is not valid";
}

void QuickOpenWidget::accept() {
  QString filterText = ui.searchLine->text();
  m_model->execute( ui.list->currentIndex(), filterText );
}

void QuickOpenWidget::doubleClicked ( const QModelIndex & index ) {
  // crash guard: https://bugs.kde.org/show_bug.cgi?id=297178
    ui.list->setCurrentIndex(index);
  QMetaObject::invokeMethod(this, "accept", Qt::QueuedConnection);
  QMetaObject::invokeMethod(this, "ready", Qt::QueuedConnection);
}

void QuickOpenWidget::avoidMenuAltFocus() {
    // send an invalid key event to the main menu bar. The menu bar will
    // stop listening when observing another key than ALT between the press
    // and the release.
    QKeyEvent event1(QEvent::KeyPress, 0, Qt::NoModifier);
    QApplication::sendEvent(ICore::self()->uiController()->activeMainWindow()->menuBar(), &event1);
    QKeyEvent event2(QEvent::KeyRelease, 0, Qt::NoModifier);
    QApplication::sendEvent(ICore::self()->uiController()->activeMainWindow()->menuBar(), &event2);
}

bool QuickOpenWidget::eventFilter ( QObject * watched, QEvent * event )
{
  QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>(event);

  if( event->type() == QEvent::KeyRelease ) {
    if(keyEvent->key() == Qt::Key_Alt) {
      if((m_expandedTemporary && m_altDownTime.msecsTo( QTime::currentTime() ) > 300) || (!m_expandedTemporary && m_altDownTime.msecsTo( QTime::currentTime() ) < 300 && m_hadNoCommandSinceAlt)) {
        //Unexpand the item
        QModelIndex row = ui.list->selectionModel()->currentIndex();
        if( row.isValid() ) {
          row = row.sibling( row.row(), 0 );
          if(m_model->isExpanded( row ))
            m_model->setExpanded( row, false );
        }
      }
      m_expandedTemporary = false;
    }
  }

  if( event->type() == QEvent::KeyPress  ) {
    m_hadNoCommandSinceAlt = false;
    if(keyEvent->key() == Qt::Key_Alt) {
      avoidMenuAltFocus();
      m_hadNoCommandSinceAlt = true;
      //Expand
      QModelIndex row = ui.list->selectionModel()->currentIndex();
      if( row.isValid() ) {
        row = row.sibling( row.row(), 0 );
        m_altDownTime = QTime::currentTime();
        if(!m_model->isExpanded( row )) {
          m_expandedTemporary = true;
          m_model->setExpanded( row, true );
        }
      }
    }

    switch( keyEvent->key() ) {
      case Qt::Key_Tab:
        if ( keyEvent->modifiers() == Qt::NoModifier ) {
          // Tab should work just like Down
          QCoreApplication::sendEvent(ui.list, new QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier));
          QCoreApplication::sendEvent(ui.list, new QKeyEvent(QEvent::KeyRelease, Qt::Key_Down, Qt::NoModifier));
          return true;
        }
        break;
      case Qt::Key_Backtab:
         if ( keyEvent->modifiers() == Qt::ShiftModifier ) {
          // Shift + Tab should work just like Up
          QCoreApplication::sendEvent(ui.list, new QKeyEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier));
          QCoreApplication::sendEvent(ui.list, new QKeyEvent(QEvent::KeyRelease, Qt::Key_Up, Qt::NoModifier));
          return true;
        }
        break;
      case Qt::Key_Down:
      case Qt::Key_Up:
      {
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          QWidget* w = m_model->expandingWidget(ui.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            if( keyEvent->key() == Qt::Key_Down )
              interface->down();
            else
              interface->up();
            return true;
          }
          return false;
        }
        Q_FALLTHROUGH();
      }
      case Qt::Key_PageUp:
      case Qt::Key_PageDown:
        if(watched == ui.list )
          return false;
        QApplication::sendEvent( ui.list, event );
      //callRowSelected();
        return true;

      case Qt::Key_Left: {
        //Expand/unexpand
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          //Eventually Send action to the widget
          QWidget* w = m_model->expandingWidget(ui.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            interface->previous();
            return true;
          }
        } else {
          QModelIndex row = ui.list->selectionModel()->currentIndex();
          if( row.isValid() ) {
            row = row.sibling( row.row(), 0 );

            if( m_model->isExpanded( row ) ) {
              m_model->setExpanded( row, false );
              return true;
            }
          }
        }
        return false;
      }
      case Qt::Key_Right: {
        //Expand/unexpand
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          //Eventually Send action to the widget
          QWidget* w = m_model->expandingWidget(ui.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            interface->next();
            return true;
          }
        } else {
          QModelIndex row = ui.list->selectionModel()->currentIndex();
          if( row.isValid() ) {
            row = row.sibling( row.row(), 0 );

            if( !m_model->isExpanded( row ) ) {
              m_model->setExpanded( row, true );
              return true;
            }
          }
        }
        return false;
      }
      case Qt::Key_Return:
      case Qt::Key_Enter: {
        if (m_filterTimer.isActive()) {
          m_filterTimer.stop();
          applyFilter();
        }
        if( keyEvent->modifiers() == Qt::AltModifier ) {
          //Eventually Send action to the widget
          QWidget* w = m_model->expandingWidget(ui.list->selectionModel()->currentIndex());
          if( KDevelop::QuickOpenEmbeddedWidgetInterface* interface =
              dynamic_cast<KDevelop::QuickOpenEmbeddedWidgetInterface*>( w ) ){
            interface->accept();
            return true;
          }
        } else {
          QString filterText = ui.searchLine->text();

          //Safety: Track whether this object is deleted. When execute() is called, a dialog may be opened,
          //which kills the quickopen widget.
          QPointer<QObject> stillExists(this);

          if( m_model->execute( ui.list->currentIndex(), filterText ) ) {

            if(!stillExists)
              return true;

            if(!(keyEvent->modifiers() & Qt::ShiftModifier))
              emit ready();
          } else {
            //Maybe the filter-text was changed:
            if( filterText != ui.searchLine->text() ) {
              ui.searchLine->setText( filterText );
            }
          }
        }
        return true;
      }
    }
  }

  return false;
}

#include "quickopenwidget.moc"
