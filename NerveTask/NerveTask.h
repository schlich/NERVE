#pragma once

#include <sstream>

#include <iostream>
#include "NerveTaskGui.h"					// needs to be my task gui
#include "nerveDevelopment/TaskBases.h"
#include "SettingsGui.h"

// Interfaces
#include "nerveInterface/TDTInterface.h"
#include "nerveInterface/JoystickInterface.h"
#include "nerveInterface/SerialPortInterface.h"
#include "nerveInterface/FileInterface.h"

// Modules
#include "nerveModule/ControlModules.h"
#include "nerveModule/JoystickModules.h"
#include "nerveModule/TDTModules.h"
#include "nerveModule/FileModules.h"
#include "nerveModule/DecodeModules.h"

// Threads
#include "VREventThread.h" 
#include "NerveThread.h"

// Scene Elements
#include "nerveUtil/MobileElement.h"
#include "nerveUtil/SphereElement.h"
#include "nerveUtil/BoxElement.h"
#include "nerveUtil/TorusElement.h"
#include "nerveUtil/SceneElementManager.h"

// Task Elements
#include "nerveDevelopment/Managers.h"
#include "nerveDevelopment/TrialComponents.h"
#include "GMutex.h"
#include "TriBuf.h"
#include "InterfaceDeviceData.h"

#include "nerveDevelopment/MyTaskTools.h"

#include <Qt/QTimer.h>
#include <Qt/qapplication.h>

#include "SignalsAndSlots.h"
#include "engine.h"

#define NUM_FEATURES 160
#define NUM_DIMENSIONS 3	// even though using 2d control, leave this as 6

#define EVENT_CODE_SLEEP_TIME 100 // (msec) This is needed to ensure that event codes are not changed so quickly that they are overwritten

class AdaptiveDecodeTask : public SuperTask, public UsesSlots
{
public:
	AdaptiveDecodeTask();
	Task * spawnTask(const Task * current);
	SettingsGui * spawnGui(const SettingsGui * current);
	~AdaptiveDecodeTask();
protected:

	// Reserved functions
	void taskRunningMethods();
	void taskStoppedMethods();
	void createTaskEnvironment();
	// Task-specific functions
	void updateAll();
	void updateDecodeWeights();
	void loadDecodeWeights();
	enum RunState
	{
		RUNNING,
		STOPPED
	};
	enum TaskState
	{
		SETUP_TRIAL,
		START,
		CENTER_HOLD,
		MOVEMENT,
		TARGET_HOLD,
		CORRECT,
		INCORRECT,
		ZILCH
	};
	void resetTask();
	void setupTrial();
	void start();
	void centerHold();
	void movement();
	void targetHold();
	void correct();
	void incorrect();
	void blackOutScreen();
	void printTaskData();
	void changeState(TaskState state);

	// Mutexes and data containers
	TripleBuffer		mouseBuffer;
	TripleBuffer		joyBuffer;
	TripleBuffer		bciBuffer;
	TripleBuffer		cursorBuffer;
	TriBuf<float>		brainBuffer[NUM_FEATURES];
	TriBuf<float>		brainBufferMean[NUM_FEATURES];
	TriBuf<float>		brainBufferStd[NUM_FEATURES];
	TriBuf<float>		xTransWeights[NUM_FEATURES];
	TriBuf<float>		zTransWeights[NUM_FEATURES];
	TriBuf<float>		biasBuffer[NUM_DIMENSIONS];

	InterfaceDeviceData mouseData;
	InterfaceDeviceData joyData;
	InterfaceDeviceData bciData;

	// Interfaces
	TDTInterfaceThreaded	tdtInterface;
	JoystickInterface		joyInterface;
	FileInterface			signalLog;
	
	// World objects
	SceneElementManager sceneManager;
	SphereElement * cursor;
	
	MobileElement * allObjects;
	MobileElement * allDecoys;
	SphereElement * targetIndicator;
	SphereElement * center;
	SphereElement * blackOut;
	TorusElement * target;
	TorusElement * decoy90;
	TorusElement * decoy180;
	TorusElement * decoy270;

	// Modules
	MovementBiasToTargetControlModule3D	biasMod;
	RecordFeaturesModule2D_2			recorder;
	FileStreamModule					writeSignals;
	SignalStream						streamSignals;

	// Users
	NamedModuleUser		ctrlAndJoyModUser;
	NamedModuleUser     tdtModUser;
	NamedModuleUser		fileModUser;

	// Threads
	VREventThread		* vrEventThread;
	BasicNerveThread	* ctrlAndJoyThread;
	BasicNerveThread	* tdtThread;
	BasicNerveThread	* fileThread;

	// Object Collisions
	TouchingCondition touchingTarget;
	TouchingCondition touchingCenter;

	// Colors
	osg::Vec4 cursorColor;
	osg::Vec4 cursorCorrectColor;
	osg::Vec4 cursorWrongColor;
	osg::Vec4 cursorRewardColor;
	osg::Vec4 targetColor;
	osg::Vec4 targetCorrectColor;
	osg::Vec4 targetWrongColor;
	osg::Vec4 decoyColor;

	// Managers
	ControlModuleManager3D		ctrlModManager;								// Control of cursor
	RandTargetManager			randTargetManager;
	RewardManager				rewardManager;
	TaskInfoManager				taskInfoManager;
	TaskDataLogManager			dataLogManager;

	// Task data
	RunState prevRunState;
	TaskState currentState;
	TaskState prevState;
	osg::Timer stateTimer;
	float movement_time;
	float target_distance; 
	int weight_id;
	int training_data_flag;
	bool tdtWaitFlag;
	osg::Timer tdtWaitTimer;
	float tdtWaitTime;

	// Gui Rebroadcasters
	RebroadcastManagerIO * rm;
	RebroadcastManagerIO * rmFileIO;

	// Matlab engine
	engine * ep;
};


