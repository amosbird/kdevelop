/***************************************************************************
                          cupdatekdedocdlg.cpp  -  description                              
                             -------------------                                         
         
    begin                : Mon Nov 9 1998                                           
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

#include "cupdatekdedocdlg.h"

#include <kconfig.h>
#include <kfiledialog.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <kprocess.h>
#include <kstddirs.h>

#include <qbuttongroup.h>
#include <qdir.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qprogressdialog.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qwhatsthis.h>
#include <qwidget.h>
#include <qlayout.h>
#include <qgrid.h>
#include <kbuttonbox.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


CUpdateKDEDocDlg::CUpdateKDEDocDlg(KShellProcess* proc, KConfig* config, QWidget *parent, const char *name) : QDialog(parent,name,true)
{
  conf = config;
  config->setGroup("Doc_Location");
  this->doc_path = config->readEntry("doc_kde", KDELIBS_DOCDIR);
  this->proc = proc;

  setCaption(i18n("KDE Library Documentation Update..."));
  QGridLayout *grid1 = new QGridLayout(this,6,3,15,7);  
  install_box = new QButtonGroup( this, "install_box" );
  install_box->setFocusPolicy( QWidget::NoFocus );
  install_box->setBackgroundMode( QWidget::PaletteBackground );
  install_box->setFontPropagation( QWidget::NoChildren );
  install_box->setPalettePropagation( QWidget::NoChildren );
  install_box->setFrameStyle( 49 );
  install_box->setTitle(i18n("Choose installation mode:") );
  install_box->setAlignment( 1 );
  grid1->addMultiCellWidget(install_box,1,3,0,2);


  source_label = new QLabel( this, "source_label" );
  source_label->setFocusPolicy( QWidget::NoFocus );
  source_label->setBackgroundMode( QWidget::PaletteBackground );
  source_label->setFontPropagation( QWidget::NoChildren );
  source_label->setPalettePropagation( QWidget::NoChildren );
  source_label->setText(i18n("new KDE Libs sources path:") );
  source_label->setAlignment( 289 );
  source_label->setMargin( -1 );
  grid1->addWidget(source_label,0,0);

  source_edit = new QLineEdit( this, "source_edit" );
  source_edit->setFocusPolicy( QWidget::StrongFocus );
  source_edit->setBackgroundMode( QWidget::PaletteBase );
  source_edit->setFontPropagation( QWidget::NoChildren );
  source_edit->setPalettePropagation( QWidget::NoChildren );
  source_edit->setText( QDir::homeDirPath() );
  source_edit->setMaxLength( 32767 );
  source_edit->setEchoMode( QLineEdit::Normal );
  source_edit->setFrame( TRUE );
  grid1->addWidget(source_edit,0,1);

  source_button = new QPushButton( this, "source_button" );
  source_button->setFocusPolicy( QWidget::TabFocus );
  source_button->setBackgroundMode( QWidget::PaletteBackground );
  source_button->setFontPropagation( QWidget::NoChildren );
  source_button->setPalettePropagation( QWidget::NoChildren );
  QPixmap pix = BarIcon("open");
  source_button->setPixmap(pix);
  source_button->setAutoRepeat( FALSE );
  source_button->setAutoResize( FALSE );
  grid1->addWidget(source_button,0,2);

  QGridLayout *grid2 = new QGridLayout(install_box,3,1,15,7); 


  del_recent_radio_button = new QRadioButton( install_box, "del_recent_radio_button" );
  del_recent_radio_button->setFocusPolicy( QWidget::TabFocus );
  del_recent_radio_button->setBackgroundMode( QWidget::PaletteBackground );
  del_recent_radio_button->setFontPropagation( QWidget::NoChildren );
  del_recent_radio_button->setPalettePropagation( QWidget::NoChildren );
  del_recent_radio_button->setText(i18n("Delete old Documentation and install to recent Documentation path"));
  del_recent_radio_button->setAutoRepeat( FALSE );
  del_recent_radio_button->setAutoResize( FALSE );
  del_recent_radio_button->setChecked( TRUE );
  QWhatsThis::add(del_recent_radio_button,
      i18n("Checking this will delete the current documentation\n"
     "and replace it with the new generated documentation\n"
     "in the same path."));
  grid2->addWidget(del_recent_radio_button,0,0);


  del_new_radio_button = new QRadioButton( install_box, "del_new_radio_button" );
  del_new_radio_button->setFocusPolicy( QWidget::TabFocus );
  del_new_radio_button->setBackgroundMode( QWidget::PaletteBackground );
  del_new_radio_button->setFontPropagation( QWidget::NoChildren );
  del_new_radio_button->setPalettePropagation( QWidget::NoChildren );
  del_new_radio_button->setText(i18n("Delete old Documentation and install to new Documentation path") );
  del_new_radio_button->setAutoRepeat( FALSE );
  del_new_radio_button->setAutoResize( FALSE );
  QWhatsThis::add(del_new_radio_button,
      i18n("Checking this will delete the current documentation\n"
     "and lets you choose a path in the input field below\n"
     "where the new generated documentation will be"
     "installed."));

  grid2->addWidget(del_new_radio_button,1,0);

  leave_new_radio_button = new QRadioButton( install_box, "leave_new_radio_button" );
  leave_new_radio_button->setFocusPolicy( QWidget::TabFocus );
  leave_new_radio_button->setBackgroundMode( QWidget::PaletteBackground );
  leave_new_radio_button->setFontPropagation( QWidget::NoChildren );
  leave_new_radio_button->setPalettePropagation( QWidget::NoChildren );
  leave_new_radio_button->setText(i18n("Leave old Documention untouched and install to new Documention path") );
  leave_new_radio_button->setAutoRepeat( FALSE );
  leave_new_radio_button->setAutoResize( FALSE );
  QWhatsThis::add(leave_new_radio_button,
      i18n("This doesn't delete your current documentation, leaves it\n"
     "where it is now and you can select a new path for the new kdelibs\n"
     "documentation. CAUTION: Don't insert the same path as\n"
     "for your recent documentation - this may mess up\n"
     "the documentation by mixing old and new files!"));
  grid2->addWidget(leave_new_radio_button,2,0);

  doc_label = new QLabel( this, "doc_label" );
  doc_label->setFocusPolicy( QWidget::NoFocus );
  doc_label->setBackgroundMode( QWidget::PaletteBackground );
  doc_label->setFontPropagation( QWidget::NoChildren );
  doc_label->setPalettePropagation( QWidget::NoChildren );
  doc_label->setText(i18n("new KDE Libs Documentation path:") );
  doc_label->setAlignment( 289 );
  doc_label->setMargin( -1 );
  doc_label->setEnabled(false);
  grid1->addWidget( doc_label,4,0);

  doc_edit = new QLineEdit( this, "doc_edit" );
  doc_edit->setFocusPolicy( QWidget::StrongFocus );
  doc_edit->setBackgroundMode( QWidget::PaletteBase );
  doc_edit->setFontPropagation( QWidget::NoChildren );
  doc_edit->setPalettePropagation( QWidget::NoChildren );
  doc_edit->setText(doc_path);
  doc_edit->setMaxLength( 32767 );
  doc_edit->setEchoMode( QLineEdit::Normal );
  doc_edit->setFrame( TRUE );
  doc_edit->setEnabled(false);
  grid1->addWidget( doc_edit,4,1);

  doc_button = new QPushButton( this, "doc_button" );
  doc_button->setFocusPolicy( QWidget::TabFocus );
  doc_button->setBackgroundMode( QWidget::PaletteBackground );
  doc_button->setFontPropagation( QWidget::NoChildren );
  doc_button->setPalettePropagation( QWidget::NoChildren );
  doc_button->setPixmap(pix);
  doc_button->setAutoRepeat( FALSE );
  doc_button->setAutoResize( FALSE );
  doc_button->setEnabled(false);
  grid1->addWidget(doc_button,4,2);


  QString sourceHelp = i18n("Insert the path to the current\n"
                     "KDE-Libs sourcecodes here. This is\n"
                     "where you have unpacked e.g. a kdelibs\n"
                     "snapshot a la /snapshot/kdelibs.");
  QWhatsThis::add(source_label, sourceHelp);
  QWhatsThis::add(source_edit, sourceHelp);
  QWhatsThis::add(source_button, sourceHelp);

  QString docMsg = i18n("Insert the path where you want to have\n"
                          "the new generated documentation installed\n"
                          "Note: the path information in Setup will\n"
                          "be updated automatically, you don't have\n"
                          "to change them to the new doc path.");
  QWhatsThis::add(doc_label, docMsg);
  QWhatsThis::add(doc_edit, docMsg);
  QWhatsThis::add(doc_button, docMsg);

  KButtonBox *bb = new KButtonBox( this );
  bb->addStretch();
  ok_button =bb->addButton( i18n("OK") );
  ok_button->setFocusPolicy( QWidget::TabFocus );
  ok_button->setBackgroundMode( QWidget::PaletteBackground );
  ok_button->setFontPropagation( QWidget::NoChildren );
  ok_button->setPalettePropagation( QWidget::NoChildren );
  ok_button->setAutoRepeat( FALSE );
  ok_button->setAutoResize( FALSE );
  ok_button->setDefault( true );

  cancel_button =bb->addButton( i18n("Cancel") );
  cancel_button->setFocusPolicy( QWidget::TabFocus );
  cancel_button->setBackgroundMode( QWidget::PaletteBackground );
  cancel_button->setFontPropagation( QWidget::NoChildren );
  cancel_button->setPalettePropagation( QWidget::NoChildren );
  cancel_button->setAutoRepeat( FALSE );
  cancel_button->setAutoResize( FALSE );
  bb->layout();
  grid1->addWidget(bb,5,1);


  bUpdated=false;

  resize( 500,380 );
  setMinimumSize( 0, 0 );
  setMaximumSize( 32767, 32767 );
  
  connect(cancel_button,SIGNAL(clicked()),SLOT(reject()));
  connect(ok_button,SIGNAL(clicked()),SLOT(OK()));
  connect(leave_new_radio_button,SIGNAL(clicked()),SLOT(slotLeaveNewRadioButtonClicked()));
  connect(del_new_radio_button,SIGNAL(clicked()),SLOT(slotDelNewRadioButtonClicked()));
  connect(del_recent_radio_button,SIGNAL(clicked()),SLOT(slotDelRecentRadioButtonClicked()));

  connect(doc_button,SIGNAL(clicked()),SLOT(slotDocButtonClicked()));
  connect(source_button,SIGNAL(clicked()),SLOT(slotSourceButtonClicked()));

}


CUpdateKDEDocDlg::~CUpdateKDEDocDlg(){
}


void CUpdateKDEDocDlg::OK(){
  KShellProcess proc_rm;
  QString kdelibs_path = source_edit->text();
  
  if(kdelibs_path.right(1) != "/")
    kdelibs_path = kdelibs_path +"/";

  // check if path (TO GENERATE the doc from the sources) is set correctly
  QString kde_testfile=kdelibs_path+"kdoc.rules";
  if(!QFileInfo(kde_testfile).exists())
  {
    KMessageBox::error(this,i18n("The chosen path for the KDE-Libs does not\n"
                   "lead to the KDE Libraries. Please choose the\n"
                   "correct path.This is where you have unpacked\n"
                   "e.g. a kdelibs snapshot a la /snapshot/kdelibs."),
                   i18n("The selected path is not correct!"));
    return;
  }
  
  QString new_doc_path = doc_path;
  if(!del_recent_radio_button->isChecked())
  { // not recent doc path
    new_doc_path = doc_edit->text();
    conf->setGroup("Doc_Location");
    conf->writeEntry("doc_kde",new_doc_path);
  }

  if(new_doc_path.right(1) != "/")
    new_doc_path += "/";

  if(doc_path.right(1) != "/")
    doc_path += "/";

  QDir().mkdir(new_doc_path);
  if(!QFileInfo(new_doc_path).isWritable())
  {
    KMessageBox::error(this,
                        i18n("You need write permission to create\n"
                          "the documentation in\n")+new_doc_path,
                  i18n("Error in creating documentation!"));
    return;
  }

  if(!leave_new_radio_button->isChecked())
  {
    // ok,let's delete it,attentation!!!
    proc_rm.clearArguments();
    if(!QFileInfo(doc_path).exists())
    {
      KMessageBox::error(this,
                          i18n("The old documentation path\n")+
                            doc_path+
                            i18n("\ndoesn�t exist anymore.")+
                            i18n("\nProcess will continue without deletion..."),
                          i18n("Old documentation deletion!"));
    }
    else
    {
      if(!QFileInfo(doc_path).isWritable())
      {
        KMessageBox::error(this,
                    i18n("You have no write permission to delete\n"
                            "the old documentation in\n")+doc_path+
                            i18n("\nProcess will continue without deletion..."),
                    i18n("Old documentation deletion!"));
      }
      else
      {
        if (QDir::setCurrent(doc_path))
        {
          QString command;
          // protect the rest of the files in the directory...
          //   maybe someone installs the htmls in the source dir of the
          //   kdelib
          command=  "rm -f -r kdoc-reference/;rm -f -r kdecore/*.htm*;"
                    "rm -f -r kdeui/*.htm*;rm -f -r kio/*.htm*;"
                    "rm -f -r kimgio/*.htm*;rm -f -r mediatool/*.htm*;"
                    "rm -f -r kdeutils/*.htm*;"
                    "rm -f -r jscript/*.htm*;rm -f -r kfile/*.htm*;"
                    "rm -f -r khtmlw/*.htm*;rm -f -r kfmlib/*.htm*;"
                    "rm -f -r kab/*.htm*;rm -f -r kspell/*.htm*;"
          // use here rmdir (so the directory won't be deleted if there are other
          //  files than the documentation
                    "rmdir kdecore; rmdir kdeui;rmdir kio;"
                    "rmdir kimgio; rmdir mediatool; rmdir kdeutils;"
                    "rmdir jscript; rmdir kfile; rmdir khtmlw; rmdir kfmlib;"
                    "rmdir kab; rmdir kspell";
          //  if the old path and the new doc path differs then
          //  delete the old doc dir
          if (doc_path!=new_doc_path)
            command += "; cd ~; rmdir "+doc_path;

          proc_rm << command;
          proc_rm.start(KShellProcess::Block,KShellProcess::AllOutput);
        }
      }
    }
  }

  proc->clearArguments();
  QDir::setCurrent(kdelibs_path);

  conf->setGroup("Doc_Location");
  QString qtPath=conf->readEntry("doc_qt", QT_DOCDIR);
  if(qtPath.right(1) != "/")
    qtPath += "/";

  bool qt_test=false;
  QString qt_testfile=qtPath+"classes.html";
  if(QFileInfo(qt_testfile).exists())
    qt_test=true;

  if(! qt_test)
  {
    int qt_set=KMessageBox::questionYesNo(this,i18n("The Qt-Documentation path is not set correctly.\n"
                                                "If you want your KDE-library documentation to\n"
                                                "be cross-referenced to the Qt-library, you have\n"
                                                "to set the correct path to your Qt-library\n"
                                                "documentation first.\n"
                                                "Do you want to set the Qt-Documentation path first ?"));
    if (qt_set==KMessageBox::Yes)
      return;
  }
  else
  {
    QString qt_kdoc=new_doc_path+"kdoc-reference/qt.kdoc.gz";
    if (QFileInfo(qt_kdoc).exists())
      QFile::remove(qt_kdoc);

    // don�t try to create the qt.kdoc.gz file if it isn't removed...
    //  it would block KDevelop
    if (!QFileInfo(qt_kdoc).exists())
    {
      // try to create, if doesn�t qt2kdoc would fail
      QDir().mkdir(new_doc_path+"kdoc-reference");

      *proc << "qt2kdoc";
      *proc << "--url=file:"  + qtPath;
      *proc << "--outdir="   + new_doc_path+"kdoc-reference";
      *proc << "--compress";
      *proc << qtPath;
      *proc << ";\n";
    }
  }

  *proc << "makekdedoc";
  *proc << "--libdir="    + new_doc_path+"kdoc-reference";
  *proc << "--outputdir=" + new_doc_path;
  *proc << "--srcdir="   + kdelibs_path;


  proc->start(KShellProcess::NotifyOnExit,KShellProcess::AllOutput);
  bUpdated=true;
  doc_path=new_doc_path; // all went ok... so set the new doc_path
  accept();
}


void CUpdateKDEDocDlg::slotLeaveNewRadioButtonClicked(){
  doc_button->setEnabled(true);
  doc_edit->setEnabled(true);
  doc_label->setEnabled(true);
}


void CUpdateKDEDocDlg::slotDelNewRadioButtonClicked(){
  doc_button->setEnabled(true);
  doc_edit->setEnabled(true);
  doc_label->setEnabled(true);
}


void CUpdateKDEDocDlg::slotDelRecentRadioButtonClicked(){
  doc_button->setEnabled(false);
  doc_edit->setEnabled(false);
  doc_label->setEnabled(false);
}


void CUpdateKDEDocDlg::slotDocButtonClicked(){
  QString name = KFileDialog::getExistingDirectory(doc_edit->text(),this,i18n("New KDE Documentation Directory..."));
  if(!name.isEmpty()){
    doc_edit->setText(name);
  }
}


void CUpdateKDEDocDlg::slotSourceButtonClicked(){
  QString dir = KFileDialog::getExistingDirectory(source_edit->text(),this,i18n("KDE Libs Directory..."));
  if(!dir.isEmpty()){
      source_edit->setText(dir);
  }

}

