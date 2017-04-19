#pragma once

#include "SettingsGui.h"
#include "_Ui_QNerveTaskGui.h"
#include "nerveDevelopment/MyTaskTools.h"	// This includes the RebroadcastManagerIO
#include "nerveUtil/RebroadcastQtAdapter.h"
#include "nerveDevelopment/Managers.h"

class AdaptiveDecodeTaskGui : public SettingsGui
{
	Q_OBJECT
public:
	AdaptiveDecodeTaskGui(QWidget * parent = 0)
	{
		ui.setupUi(this);
		initializeGui();
	}	
	~AdaptiveDecodeTaskGui()
	{
		delete qta;
		delete rm;
		delete timer;
	}
	void initializeGui();
	RebroadcastManagerIO * getRebroadcastManagerIO(){return rm;}
public slots:
	void refresh();
	//
	void dataDirBrowse();
	void trialIdBrowse();
	//
	void workAround(int val);
	//
private:
	Ui::AdaptiveDecodeGui ui;
protected: 
	QTimer * timer;
	RebroadcastQtAdapter * qta;
	RebroadcastManagerIO * rm;
};