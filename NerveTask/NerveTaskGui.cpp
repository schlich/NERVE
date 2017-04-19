#include "NerveTaskGui.h"

#include <iostream>

#include <Qt/QTimer.h>
#include <Qt/qapplication.h>
//#include <qwt_plot.h>
//#include <qwt_plot_marker.h>
//#include <qwt_plot_curve.h>
//#include <qwt_legend.h>
//#include <qwt_data.h>
//#include <qwt_text.h>
//#include <qwt_math.h>
#include <math.h>

#include <QtGui/QFileDialog>

// reset widgets and data
void AdaptiveDecodeTaskGui::initializeGui()
{
	// Set up timer to emit signal every 100 ms which will update the Gui
	timer = new QTimer(this);
	timer->start(100);	// this timer signals to update all widgets
	connect(timer, SIGNAL(timeout()), this, SLOT(refresh()));
	// Configuration browse functions
	connect(ui.DataDirBrowse, SIGNAL(clicked()), this, SLOT(dataDirBrowse()));
	connect(ui.TrialIdDirBrowse, SIGNAL(clicked()), this, SLOT(trialIdBrowse()));
	// Initialize the rebroadcast manager and qt rebroadcast adapter, and add all widget children from the gui
	rm = new RebroadcastManagerIO();
	qta = new RebroadcastQtAdapter(rm);
	qta->addWithChildren(this);
	//ui.CursorSize->lineEdit()->setValidator(0);
	// Work around for loading config data
	connect(ui.WorkAround, SIGNAL(stateChanged(int)), this, SLOT(workAround(int)));
};

void AdaptiveDecodeTaskGui::refresh()
{
	rm->rebroadcastAll();	// This call updates all Gui information so that it is synced with changes in other threads
};

void AdaptiveDecodeTaskGui::dataDirBrowse()
{
	QString path = QFileDialog::getExistingDirectory (this, tr("Select a directory to save data to..."));    
	if ( path.isNull() == false )    
	{        
		rm->setValue_string("DataDirName", path.toStdString());
		rm->setValue_string("SubjectName", ui.SubjectName->text().toStdString());
		rm->setValue_string("DataFileSuffix", ui.DataFileSuffix->text().toStdString());
	}
};
void AdaptiveDecodeTaskGui::trialIdBrowse()
{
	QString path = QFileDialog::getOpenFileName (this, tr("Select a file to read/write trial IDs..."), "", tr("Text Files (*.txt)"));    
	if ( path.isNull() == false )    
	{      
		if (!path.endsWith(".txt"))
		{
			path.append(".txt");
		}
	}	
	else
	{
		path = QString("TrialID.txt");	
	}
	rm->setValue_string("TrialIdFileName", path.toStdString());
};
void AdaptiveDecodeTaskGui::workAround(int val)
{
		rm->setValue_string("DataDirName", ui.DataDirName->text().toStdString());
		rm->setValue_string("SubjectName", ui.SubjectName->text().toStdString());
		rm->setValue_string("DataFileSuffix", ui.DataFileSuffix->text().toStdString());
		rm->setValue_string("TrialIdFileName", ui.TrialIdFileName->text().toStdString());
};