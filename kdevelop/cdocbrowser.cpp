/***************************************************************************
           cdocbrowser.cpp - 
                             -------------------                                         

    begin                : 20 Jul 1998                                        
    copyright            : (C) 1998 by Sandy Meier                         
    email                : smeier@rz.uni-potsdam.de                                     
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   * 
 *                                                                         *
 ***************************************************************************/


#include "cdocbrowser.h"

#include <kapp.h>
#include <kconfig.h>
#include <kcolorbtn.h>
#include <kcursor.h>
#include <kglobal.h>
#include <khtmlview.h>
#include <kiconloader.h>
#include <kio/netaccess.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kprocess.h>
#include <krun.h>

#include "resource.h"

#include <qbuttongroup.h>
#include <qcheckbox.h>
#include <qclipboard.h>
#include <qcombobox.h>
#include <qfile.h>
#include <qlabel.h>
#include <qpopupmenu.h>
#include <qpushbutton.h>
#include <qregexp.h>
#include <qradiobutton.h>

#include <X11/Xlib.h>
#undef Unsorted

//#include <iostream.h>

#warning FIXME This needs some rework to use KHTMLParts

int  CDocBrowser::fSize = 3;
QString CDocBrowser::standardFont;
QString CDocBrowser::fixedFont;
QColor CDocBrowser::bgColor;
QColor CDocBrowser::textColor;
QColor CDocBrowser::linkColor;
QColor CDocBrowser::vLinkColor;
bool CDocBrowser::underlineLinks;
bool CDocBrowser::forceDefaults;

CDocBrowser::CDocBrowser(QWidget*parent,const char* name) :
  KHTMLPart(parent, name)
{
  doc_pop = new QPopupMenu();
  doc_pop->insertItem(BarIcon("back"),i18n("Back"),this, SLOT(slotURLBack()),0,ID_HELP_BACK);
  doc_pop->insertItem(BarIcon("forward"),i18n("Forward"),this,SLOT(slotURLForward()),0,ID_HELP_FORWARD);
  doc_pop->insertSeparator();
  doc_pop->insertItem(BarIcon("copy"),i18n("Copy"),this, SLOT(slotCopyText()),0,ID_EDIT_COPY);
  doc_pop->insertItem(i18n("Toggle Bookmark"),this, SIGNAL(signalBookmarkToggle()),0,ID_BOOKMARKS_TOGGLE);
  doc_pop->insertItem(i18n("View in new window"), this, SLOT(slotViewInKFM()),0,ID_VIEW_IN_KFM);
  doc_pop->insertSeparator();
  doc_pop->insertItem(BarIcon("grep"),i18n("grep: "), this, SLOT(slotGrepText()), 0, ID_EDIT_SEARCH_IN_FILES);
  doc_pop->insertItem(BarIcon("lookup"),i18n("look up: "),this, SLOT(slotSearchText()),0,ID_HELP_SEARCH_TEXT);
  
//  view()->setFocusPolicy( QWidget::StrongFocus );
  connect(this, SIGNAL( popupMenu( const QString&, const QPoint & ) ),
          this, SLOT( slotPopupMenu( const QString&, const QPoint & ) ) );
  connect(this, SIGNAL( setWindowCaption ( const QString&) ), this, SLOT( slotSetFileTitle( const QString&) ) );

}

CDocBrowser::~CDocBrowser(){
   delete doc_pop;
   doc_pop=0l;
}



void CDocBrowser::slotViewInKFM()
{
  new KRun(currentURL());
}

void CDocBrowser::showURL(QString url, bool reload)
{
  // in some cases KHTMLView return "file:/file:/...." (which might be a bug in kdoc?)
  // Anyway clean up the url from this error
  if (url.left(12)=="file:/file:/")
    url=url.mid(6, url.length());

  QString url_wo_ref=url; // without ref
  QString ref;

  complete_url=url;

  int pos = url.findRev('#');
  int len = url.length();

  ref = (pos!=-1) ? (const char*) url.right(len - pos - 1) : "";
  m_refTitle = ref;

  if (pos!=-1)
   url_wo_ref = url.left(pos);

  if(url.left(7) == "http://" || url_wo_ref.right(4).find("htm", FALSE)==-1)
  {
    new KRun(url);
    return;
  }
  // workaround for kdoc2 malformed urls in crossreferences to Qt-documentation
  if(url.contains("file%253A/"))
    url.replace( QRegExp("file%253A/"), "" );
    
  if(url.contains("file%3A/"))
    url.replace( QRegExp("file%3A/"), "" );

//  setURLCursor( KCursor::waitCursor() );
  kapp->setOverrideCursor( Qt::waitCursor );

  if( (url_wo_ref != old_url) || reload)
  {
    QString tmpFile;
    if( KIO::NetAccess::download( url, tmpFile ) )
    {
      char buffer[256+1];
      QFile file(tmpFile) ;
      if(file.exists())
      {
        emit enableStop(ID_HELP_BROWSER_STOP);
        file.open(IO_ReadOnly);
        QDataStream dstream ( &file );
        QString content;

        begin( url);
#warning FIXME      parse();

        while ( !dstream.eof() )
        {
          buffer[256]='\0';
          dstream.readRawBytes(buffer, 256);
          write(buffer);
        }

        end();
        show();
        KIO::NetAccess::removeTempFile(tmpFile);
        file.close();
      }
      else
      {
        KMessageBox::information(0,"file: \"" + tmpFile + i18n("\" not found!"),i18n("Not found!"));
        return;
      }
    }
  }

  kapp->restoreOverrideCursor();
  if (pos != -1)
    gotoAnchor(ref);
  else
    if (url_wo_ref == old_url)
      view()->setContentsPos(0,0);

//  if (url_wo_ref == old_url)
//    emit completed();  // simulate documentDone to put it in history...

  old_url = url_wo_ref;

  emit completed();  // simulate documentDone to put it in history...
}

QString CDocBrowser::currentURL(){
  return complete_url;
}

void CDocBrowser::setDocBrowserOptions(){

  KConfig *config = KGlobal::config();
  config->setGroup( "DocBrowserAppearance" );

  QString fs = config->readEntry( "BaseFontSize" );
  if ( !fs.isEmpty() )
  fSize = fs.toInt();
  fs = "times";
  standardFont = config->readEntry( "StandardFont", fs );

  fs = "courier";
  fixedFont = config->readEntry( "FixedFont", fs );

  bgColor = config->readColorEntry( "BgColor", &white );
  textColor = config->readColorEntry( "TextColor", &black );
  linkColor = config->readColorEntry( "LinkColor", &blue );
  vLinkColor = config->readColorEntry( "VLinkColor", &darkMagenta );
  underlineLinks = config->readBoolEntry( "UnderlineLinks", true );
  forceDefaults = config->readBoolEntry( "ForceDefaultColors", false );

#warning FIXME KHTMLSettings
  setFixedFont( fixedFont);
  setStandardFont( standardFont );
  setURLCursor( KCursor::handCursor() );

//  KHTMLSettings* htmlsettings=settings();
//  setDefaultFontBase( fSize );
//  htmlsettings->setUnderlineLinks(underlineLinks);
//  setForceDefault( forceDefaults );
//  setDefaultBGColor( bgColor );
}

void CDocBrowser::slotDocFontSize(int size){
  fSize = size;
#warning FIXME KHTMLSettings
//  KHTMLView* htmlview=view();
//  htmlview->setDefaultFontBase( size );
//  htmlview->parse();
  showURL(complete_url, true);
//  busy = true;
//  emit enableMenuItems();
}

void CDocBrowser::slotDocStandardFont(const QString& n){
  standardFont = n;
#warning FIXME KHTMLSettings
//  KHTMLView* htmlview=view();
//  htmlview->setStandardFont( n );
//  htmlview->parse();
  showURL(complete_url, true);
//  busy = true;
//  emit enableMenuItems();
}

void CDocBrowser::slotDocFixedFont(const QString& n){
  fixedFont = n;
#warning FIXME KHTMLSettings
//  KHTMLView* htmlview=view();
//  htmlview->setFixedFont( n );
//  htmlview->parse();
  showURL(complete_url, true);
//  busy = true;
//  emit enableMenuItems();
}

void CDocBrowser::slotDocColorsChanged( const QColor &bg, const QColor &text,
  const QColor &link, const QColor &vlink, const bool uline, const bool force)
{
#warning FIXME KHTMLSettings
//  KHTMLView* htmlview=view();
//  htmlview->setForceDefault( force );
//  htmlview->setDefaultBGColor( bg );
//  htmlview->setDefaultTextColors( text, link, vlink );
//  htmlview->setUnderlineLinks(uline);
//  htmlview->parse();
  showURL(complete_url, true);
//  busy = true;
//  emit enableMenuItems();){
}

void CDocBrowser::slotPopupMenu( const QString&/*url*/, const QPoint & pnt){
  QString text;
  int pos;
  if (hasSelection())
  {
    text = selectedText();
    text.replace(QRegExp("^\n"), "");
    pos=text.find("\n");
    if (pos>-1)
     text=text.left(pos);
  }

  if (!text.isEmpty())
  {
    doc_pop->setItemEnabled(ID_EDIT_COPY,true);
    doc_pop->setItemEnabled(ID_HELP_SEARCH_TEXT,true);
    doc_pop->setItemEnabled(ID_EDIT_SEARCH_IN_FILES,true);

    if(text.length() > 20 ){
      text = text.left(20) + "...";
    }
    doc_pop->changeItem(BarIcon("grep"),i18n("grep: ")+text, ID_EDIT_SEARCH_IN_FILES);
    doc_pop->changeItem(BarIcon("lookup"),i18n("look up: ")+ text,ID_HELP_SEARCH_TEXT);
  }
  else
  {
    doc_pop->setItemEnabled(ID_EDIT_COPY,false);
    doc_pop->setItemEnabled(ID_HELP_SEARCH_TEXT,false);
    doc_pop->setItemEnabled(ID_EDIT_SEARCH_IN_FILES,false);
    doc_pop->changeItem(BarIcon("grep"),i18n("grep: "), ID_EDIT_SEARCH_IN_FILES);
    doc_pop->changeItem(BarIcon("lookup"),i18n("look up: "),ID_HELP_SEARCH_TEXT);
  }
  doc_pop->popup(pnt);
}

void CDocBrowser::slotCopyText()
{
  QString text = selectedText();
  if (!text.isEmpty())
  {
    QClipboard *cb = kapp->clipboard();
    cb->setText( text );
  }
}

void CDocBrowser::slotFindTextNext(QString str){
  findTextNext(QRegExp(str), true);
}

void CDocBrowser::slotSearchText(){
  emit signalSearchText();
}
void CDocBrowser::slotGrepText(){
  QString text = selectedText();

  emit signalGrepText(text);
}

void CDocBrowser::slotURLBack(){
  emit signalURLBack();
}

void CDocBrowser::slotURLForward(){
  emit signalURLForward();
}

void CDocBrowser::slotSetFileTitle( const QString& title ){
  m_title= title;
}

QString CDocBrowser::currentTitle(){
  return (m_refTitle.isEmpty()) ? m_title : m_refTitle+" - "+m_title;  
}


void  CDocBrowser::urlSelected ( const QString &url, int button, int state, const QString &_target)
{
  KHTMLPart::urlSelected (url, button, state,_target);

  KURL cURL = completeURL( url );
  showURL( cURL.url() ) ;
}

//
// KDE Help Options
//
// (c) Martin R. Jones 1996
//



//-----------------------------------------------------------------------------

CDocBrowserFont::CDocBrowserFont( QWidget *parent, const char *name )
  : QWidget( parent, name )
{
  readOptions();

  QRadioButton *rb;
  QLabel *label;

  QButtonGroup *bg = new QButtonGroup( i18n("Font Size"), this );
  bg->setExclusive( TRUE );
  bg->setGeometry( 15, 15, 300, 50 );

  rb = new QRadioButton( i18n("Small"), bg );
  rb->setGeometry( 10, 20, 80, 20 );
  rb->setChecked( fSize == 3 );

  rb = new QRadioButton( i18n("Medium"), bg );
  rb->setGeometry( 100, 20, 80, 20 );
  rb->setChecked( fSize == 4 );

  rb = new QRadioButton( i18n("Large"), bg );
  rb->setGeometry( 200, 20, 80, 20 );
  rb->setChecked( fSize == 5 );

  label = new QLabel( i18n("Standard Font"), this );
  label->setGeometry( 15, 90, 100, 20 );

  QComboBox *cb = new QComboBox( false, this );
  cb->setGeometry( 120, 90, 180, 25 );
  getFontList( standardFonts, "-*-*-*-*-*-*-*-*-*-*-p-*-*-*" );
  cb->insertStringList( standardFonts );
  
  int i=0;
  for ( QStringList::Iterator it = standardFonts.begin(); it != standardFonts.end(); ++it, i++ )
  {
    if (stdName == *it)
    {
      cb->setCurrentItem( i );
      break;
    }
  }
  connect( cb, SIGNAL( activated( const QString& ) ),
            SLOT( slotStandardFont( const QString& ) ) );

  label = new QLabel( i18n( "Fixed Font"), this );
  label->setGeometry( 15, 130, 100, 20 );

  cb = new QComboBox( false, this );
  cb->setGeometry( 120, 130, 180, 25 );
  getFontList( fixedFonts, "-*-*-*-*-*-*-*-*-*-*-m-*-*-*" );
  getFontList( fixedFonts, "-*-*-*-*-*-*-*-*-*-*-c-*-*-*" );
  cb->insertStringList( fixedFonts );
  
  i=0;
  for ( QStringList::Iterator it = fixedFonts.begin(); it != fixedFonts.end(); ++it, i++ )
  {
    if ( fixedName == *it )
    {
      cb->setCurrentItem( i );
      break;
    }
  }
  connect( cb, SIGNAL( activated( const QString& ) ),
    SLOT( slotFixedFont( const QString& ) ) );

  connect( bg, SIGNAL( clicked( int ) ), SLOT( slotFontSize( int ) ) );
}

       
void CDocBrowserFont::readOptions()
{
  KConfig *config = KGlobal::config();
  config->setGroup( "DocBrowserAppearance" );
  
  QString fs = config->readEntry( "BaseFontSize" );
  if ( !fs.isEmpty() )
  {
    fSize = fs.toInt();
    if ( fSize < 3 )
      fSize = 3;
    else if ( fSize > 5 )
      fSize = 5;
  }
  else
    fSize = 3;

  stdName = config->readEntry( "StandardFont" );
  if ( stdName.isEmpty() )
    stdName = "times";

  fixedName = config->readEntry( "FixedFont" );
  if ( fixedName.isEmpty() )
    fixedName = "courier";
}

void CDocBrowserFont::getFontList( QStringList &list, const char *pattern )
{
  int num;

  char **xFonts = XListFonts( qt_xdisplay(), pattern, 2000, &num );

  for ( int i = 0; i < num; i++ )
  {
    addFont( list, xFonts[i] );
  }

  XFreeFontNames( xFonts );
}

void CDocBrowserFont::addFont( QStringList &list, const char *xfont )
{
  const char *ptr = strchr( xfont, '-' );
  if ( !ptr )
    return;
  
  ptr = strchr( ptr + 1, '-' );
  if ( !ptr )
    return;

  QString font = ptr + 1;

  int pos;
  if ( ( pos = font.find( '-' ) ) > 0 )
  {
    font.truncate( pos );

    if ( font.find( "open look", 0, false ) >= 0 )
      return;

    
    for ( QStringList::Iterator it = list.begin(); it != list.end(); ++it )
    {
        if ( *it == font )
          return;
    }

    list.append( font );
  }
}

void CDocBrowserFont::slotApplyPressed()
{
  QString o;

  KConfig *config = KGlobal::config();
  config->setGroup( "DocBrowserAppearance" );

  QString fs;
  fs.setNum( fSize );
  o = config->writeEntry( "BaseFontSize", fs );
  if ( o.isNull() || o.toInt() != fSize )
    emit fontSize( fSize );

  o = config->writeEntry( "StandardFont", stdName );
  if ( o.isNull() || o != stdName )
    emit standardFont( stdName );

  o = config->writeEntry( "FixedFont", fixedName );
  if ( o.isNull() || o != fixedName )
    emit fixedFont( fixedName );

  config->sync();
}

void CDocBrowserFont::slotFontSize( int i )
{
  fSize = i+3;
}

void CDocBrowserFont::slotStandardFont( const QString& n )
{
  stdName = n;
}

void CDocBrowserFont::slotFixedFont( const QString& n )
{
  fixedName = n;
}

//-----------------------------------------------------------------------------

CDocBrowserColor::CDocBrowserColor( QWidget *parent, const char *name )
    : QWidget( parent, name )
{
  readOptions();

  KColorButton *colorBtn;
  QLabel *label;

  label = new QLabel( i18n("Background Color:"), this );
  label->setGeometry( 35, 20, 150, 25 );

  colorBtn = new KColorButton( bgColor, this );
  colorBtn->setGeometry( 185, 20, 80, 30 );
  connect( colorBtn, SIGNAL( changed( const QColor & ) ),
    SLOT( slotBgColorChanged( const QColor & ) ) );

  label = new QLabel( i18n("Normal Text Color:"), this );
  label->setGeometry( 35, 60, 150, 25 );

  colorBtn = new KColorButton( textColor, this );
  colorBtn->setGeometry( 185, 60, 80, 30 );
  connect( colorBtn, SIGNAL( changed( const QColor & ) ),
    SLOT( slotTextColorChanged( const QColor & ) ) );

  label = new QLabel( i18n("URL Link Color:"), this );
  label->setGeometry( 35, 100, 150, 25 );

  colorBtn = new KColorButton( linkColor, this );
  colorBtn->setGeometry( 185, 100, 80, 30 );
  connect( colorBtn, SIGNAL( changed( const QColor & ) ),
    SLOT( slotLinkColorChanged( const QColor & ) ) );

  label = new QLabel( i18n("Followed Link Color:"), this );
  label->setGeometry( 35, 140, 150, 25 );

  colorBtn = new KColorButton( vLinkColor, this );
  colorBtn->setGeometry( 185, 140, 80, 30 );
  connect( colorBtn, SIGNAL( changed( const QColor & ) ),
    SLOT( slotVLinkColorChanged( const QColor & ) ) );

  QCheckBox *underlineBox = new QCheckBox( i18n("Underline links"),
                                          this);
  underlineBox->setGeometry(35, 180, 250, 30 );
  underlineBox->setChecked(underlineLinks);
  connect( underlineBox, SIGNAL( toggled( bool ) ),
    SLOT( slotUnderlineLinksChanged( bool ) ) );

  QCheckBox *forceDefaultBox = new QCheckBox(
                    i18n("Always use my colors"), this);
  forceDefaultBox->setGeometry(35, 210, 250, 30 );
  forceDefaultBox->setChecked(forceDefault);
  connect( forceDefaultBox, SIGNAL( toggled( bool ) ),
    SLOT( slotForceDefaultChanged( bool ) ) );
}

void CDocBrowserColor::readOptions()
{
  KConfig *config = KGlobal::config();
  config->setGroup( "DocBrowserAppearance" );
  
  bgColor = config->readColorEntry( "BgColor", &white );
  textColor = config->readColorEntry( "TextColor", &black );
  linkColor = config->readColorEntry( "LinkColor", &blue );
  vLinkColor = config->readColorEntry( "VLinkColor", &magenta );
  underlineLinks = config->readBoolEntry( "UnderlineLinks", TRUE );
  forceDefault = config->readBoolEntry( "ForceDefaultColors", true );

  changed = false;
}

void CDocBrowserColor::slotApplyPressed()
{
  KConfig *config = KGlobal::config();
  config->setGroup( "DocBrowserAppearance" );

  config->writeEntry( "BgColor", bgColor );
  config->writeEntry( "TextColor", textColor );
  config->writeEntry( "LinkColor", linkColor );
  config->writeEntry( "VLinkColor", vLinkColor );
  config->writeEntry( "UnderlineLinks", underlineLinks );
  config->writeEntry( "ForceDefaultColors", forceDefault );

  if ( changed )
      emit colorsChanged( bgColor, textColor, linkColor, vLinkColor,
                underlineLinks, forceDefault );

  config->sync();
}

void CDocBrowserColor::slotBgColorChanged( const QColor &col )
{
  if ( bgColor != col )
          changed = true;
  bgColor = col;
}

void CDocBrowserColor::slotTextColorChanged( const QColor &col )
{
  if ( textColor != col )
      changed = true;
  textColor = col;
}

void CDocBrowserColor::slotLinkColorChanged( const QColor &col )
{
  if ( linkColor != col )
          changed = true;
  linkColor = col;
}

void CDocBrowserColor::slotVLinkColorChanged( const QColor &col )
{
  if ( vLinkColor != col )
          changed = true;
  vLinkColor = col;
}

void CDocBrowserColor::slotUnderlineLinksChanged( bool ulinks )
{
  if ( underlineLinks != ulinks )
          changed = true;
  underlineLinks = ulinks;
}

void CDocBrowserColor::slotForceDefaultChanged( bool force )
{
  if ( forceDefault != force )
          changed = true;
  forceDefault = force;
}

//-----------------------------------------------------------------------------


CDocBrowserOptionsDlg::CDocBrowserOptionsDlg( QWidget *parent, const char *name )
  : QTabDialog( parent, name,TRUE ){
  setCaption( i18n("Documentation Browser Options") );

  resize( 350, 330 );

        setOKButton( i18n("OK") );
        setCancelButton( i18n("Cancel") );
        setApplyButton( i18n("Apply") );

  fontOptions = new CDocBrowserFont( this, i18n("Fonts") );
  addTab( fontOptions, i18n("Fonts") );
  connect( this, SIGNAL( applyButtonPressed() ),
    fontOptions, SLOT( slotApplyPressed() ) );

  colorOptions = new CDocBrowserColor( this, i18n("Colors") );
  addTab( colorOptions, i18n("Colors") );
  connect( this, SIGNAL( applyButtonPressed() ),
    colorOptions, SLOT( slotApplyPressed() ) );
}


