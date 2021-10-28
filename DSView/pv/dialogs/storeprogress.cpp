/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Joel Holdsworth <joel@airwebreathe.org.uk>
 * Copyright (C) 2016 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "storeprogress.h" 
#include "pv/sigsession.h"
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QTimer>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox> 

#include "../ui/msgbox.h"
#include "../config/appconfig.h"

namespace pv {
namespace dialogs {

StoreProgress::StoreProgress(SigSession &session, QWidget *parent) :
    DSDialog(parent),
    _store_session(session)
{
    _fileLab = NULL;
    _ckOrigin = NULL;

    this->setMinimumSize(550, 220);
    this->setModal(true);
 
    _progress.setValue(0);
    _progress.setMaximum(100);

    _isExport = false;
    _done = false;

    QGridLayout *grid = new QGridLayout(); 
    _grid = grid;
    grid->setContentsMargins(10, 20, 10, 10);
    grid->setVerticalSpacing(25);

    grid->setColumnStretch(0, 2);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 1);
    grid->setColumnStretch(3, 1);

    _fileLab = new QLineEdit();
    _fileLab->setEnabled(false);    

    QPushButton *openButton = new QPushButton(this);
    openButton->setText(tr("change"));

    grid->addWidget(&_progress, 0, 0, 1, 4);
    grid->addWidget(_fileLab, 1, 0, 1, 3);
    grid->addWidget(openButton, 1, 3, 1, 1);    

    QDialogButtonBox  *_button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, 
                                        Qt::Horizontal, this);
    grid->addWidget(_button_box, 2, 2, 1, 2, Qt::AlignRight | Qt::AlignBottom);

    layout()->addLayout(grid);

    connect(_button_box, SIGNAL(rejected()), this, SLOT(reject()));
    connect(_button_box, SIGNAL(accepted()), this, SLOT(accept()));

    connect(&_store_session, SIGNAL(progress_updated()),
        this, SLOT(on_progress_updated()), Qt::QueuedConnection);

    connect(openButton, SIGNAL(clicked()),this, SLOT(on_change_file()));
}

StoreProgress::~StoreProgress()
{
    _store_session.wait();
}

 void StoreProgress::on_change_file()
 {
     if (_isExport){
        QString file = _store_session.MakeExportFile(true);
        if (file != "")
            _fileLab->setText(file);
     }
     else{
      QString file = _store_session.MakeSaveFile(true);
       if (file != "")
          _fileLab->setText(file);
     }
 }

void StoreProgress::reject()
{
    using namespace Qt;
    _store_session.cancel();
    save_done();
    DSDialog::reject();
}

void StoreProgress::accept()
{
    if (_store_session.GetFileName() == ""){
        MsgBox::Show(NULL, "you need to select a file name.");
        return;
    }

    if (_isExport && _store_session.IsLogicDataType()){
        bool ck  = _ckOrigin->isChecked();
        AppConfig &app = AppConfig::Instance();
        if (app._appOptions.originalData != ck){
            app._appOptions.originalData = ck;
            app.SaveApp();
        }
    }

    //start done 
    if (_isExport){
        if (_store_session.export_start()){
              QTimer::singleShot(100, this, SLOT(timeout()));        
        }
        else{
            save_done();
            close(); 
            show_error();
        }
    }
    else{
         if (_store_session.save_start()){
              QTimer::singleShot(100, this, SLOT(timeout()));        
        }
        else{
            save_done();
            close(); 
            show_error();
        }
    }
    //do not to call base class method, otherwise the window will be closed;
}

void StoreProgress::timeout()
{
    if (_done) {
        _store_session.session().set_saving(false);
        save_done();
        close(); 
    } else {
        QTimer::singleShot(100, this, SLOT(timeout()));
    }
}

void StoreProgress::save_run(QString session_file)
{
    _isExport = false;
    setTitle(tr("Saving..."));
    QString file = _store_session.MakeSaveFile(false);
    _fileLab->setText(file);
    _store_session._sessionFile = session_file;
    show();  
}

void StoreProgress::export_run()
{
    if (_store_session.IsLogicDataType())
    { 
        QGridLayout *lay = new QGridLayout();
        lay->setContentsMargins(15, 0, 0, 0);
        AppConfig &app = AppConfig::Instance();
        _ckOrigin  = new QCheckBox();
        _ckOrigin->setChecked(app._appOptions.originalData);
        _ckOrigin->setText(tr("all original data"));
        lay->addWidget(_ckOrigin);
        _grid->addLayout(lay, 2, 0, 1, 2);
    }

    _isExport = true;
    setTitle(tr("Exporting..."));
    QString file = _store_session.MakeExportFile(false);
    _fileLab->setText(file);
    show();
}

void StoreProgress::show_error()
{
    _done = true;
    if (!_store_session.error().isEmpty()) { 
        MsgBox::Show(NULL, _store_session.error().toStdString().c_str(), NULL);
    }
}

void StoreProgress::closeEvent(QCloseEvent* e)
{
    _store_session.cancel();
    DSDialog::closeEvent(e);
}

void StoreProgress::on_progress_updated()
{
    const std::pair<uint64_t, uint64_t> p = _store_session.progress();
	assert(p.first <= p.second);
    int percent = p.first * 1.0 / p.second * 100;
    _progress.setValue(percent);

    const QString err = _store_session.error();
	if (!err.isEmpty()) {
		show_error();
	}

    if (p.first == p.second) {
        _done = true;
    }
}

} // dialogs
} // pv
