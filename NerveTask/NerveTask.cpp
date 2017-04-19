/******************************************************************************************************/
/*******          This is included for dll export, remove if used in regular console app        *******/
/******************************************************************************************************/
#include "NerveTask.h"
#include "TaskRegistry.h"

extern "C" __declspec(dllexport) void __cdecl RegisterNerveTask(TaskRegistry * registry)
{
	printf("registering task");
      registry->accept(new AdaptiveDecodeTask());
}
BOOL APIENTRY DllMain( HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
/******************************************************************************************************/
/******************************************************************************************************/


#include "NerveTask.h"
#include "nerveUtil/CameraElement.h" // need this to move the camera
#include "windows.h"
#include <iostream>

AdaptiveDecodeTask::AdaptiveDecodeTask()
{
	taskname = "co_stim";
};
AdaptiveDecodeTask::~AdaptiveDecodeTask()
{
	// Stop Threads
	vrEventThread->cancel();
	while (vrEventThread->isRunning())
	{
		Sleep(10);
	}
	delete vrEventThread;
	ctrlAndJoyThread->cancel();
	while (ctrlAndJoyThread->isRunning())
	{
		Sleep(10);
	}
	delete ctrlAndJoyThread;
	tdtThread->cancel();
	while (tdtThread->isRunning())
	{
		Sleep(10);
	}
	delete tdtThread;
	fileThread->cancel();
	while(fileThread->isRunning())
	{
		Sleep(10);
	}
	delete fileThread;
	// Close Matlab engine
	engClose(ep);

	delete rm;
	delete rmFileIO;
	delete cursor;
	printf("Task destructing\n");
};
Task * AdaptiveDecodeTask::spawnTask(const Task * current)
{
	Task * nt = NULL;
	if(!dynamic_cast<const AdaptiveDecodeTask*>(current))
	{
		nt = new AdaptiveDecodeTask();
		printf("New Adaptive Decode task created\n");
	}
	return nt;
}
SettingsGui * AdaptiveDecodeTask::spawnGui(const SettingsGui * current)
{
	SettingsGui * sg = NULL;
	if(!dynamic_cast<const AdaptiveDecodeTask*>(current))
	{
		sg = new AdaptiveDecodeTaskGui();
		printf("New Adaptive Decode gui created\n");
	}
	return sg;
}
void AdaptiveDecodeTask::createTaskEnvironment()
{
	printf("creating task environment...\n");

	//Initialize triple buffers
	float init = 0;
	for (int i=0; i<NUM_FEATURES; i++)
	{
		brainBuffer[i].setData(init);
		brainBufferMean[i].setData(init);
		brainBufferStd[i].setData(init);
		xTransWeights[i].setData(init);
		zTransWeights[i].setData(init);
	}
	for (int i=0; i<NUM_DIMENSIONS; i++)
		biasBuffer[i].setData(init);

	// Initialize bci control signals
	bciData._position = osg::Vec3(0.0, 0.0, 0.0);
	bciData._velocity = osg::Vec3(0.0, 0.0, 0.0);
	bciBuffer.setData(&bciData);

	weight_id = 0;

	srand((unsigned int)time(NULL));
	// Setup gui data through rebroadcast manager
	RebroadcastManagerIO * guiRM = (((AdaptiveDecodeTaskGui*)settingsGui)->getRebroadcastManagerIO());
	rm = guiRM->createReciprocallyConnectedManager();
	rmFileIO = guiRM->createReciprocallyConnectedManager();

	// Mouse Thread
	mouseBuffer.newDataContainer(&mouseData);													
	vrEventThread = new VREventThread(_windowevents, &mouseBuffer);
	vrEventThread->start();
	// TDT Thread
	tdtThread = new BasicNerveThread();	
	tdtThread->requestAddUser(&tdtModUser);
	tdtModUser.requestAddModule(&tdtInterface);
	rm->getRebroadcastVoid("ConnectTDT")->signal_toSameThread.connect(&tdtInterface, &TDTInterfaceThreaded::slot_connectInterface);
	tdtInterface.signal_status.connect(rm->getRebroadcastDouble("TDTConnectedLight"), &RebroadcastDouble::slot_connectToSameThread);	
	TimerModule * tdtTimer = new TimerModule(NerveModule::DELETE_MODULE);
	tdtTimer->signal_operationsRate_kHz.connect(rm->getRebroadcastDouble("TDTLoopRate"), &RebroadcastDouble::slot_connectToSameThread);
	tdtModUser.requestAddModule(tdtTimer);
	tdtThread->start();
	// Joystick and Control Thread
	ctrlAndJoyThread = new BasicNerveThread();
	ctrlAndJoyThread->requestAddUser(&ctrlAndJoyModUser);
	rm->getRebroadcastVoid("ConnectJoystick")->signal_toSameThread.connect(&joyInterface, &JoystickInterface::slot_connectInterface);
	joyInterface.signal_status.connect(rm->getRebroadcastDouble("JoystickConnectedLight"), &RebroadcastDouble::slot_connectToSameThread);
	ctrlAndJoyThread->start();
	// File Thread
	fileThread = new BasicNerveThread();
	fileThread->requestAddUser(&fileModUser);
	fileThread->start();

	// Connect Interfaces
	joyInterface.connectInterface();
	tdtInterface.connectInterface();

	// Define colors				R		G		B		Alpha(does nothing)
	cursorColor			= osg::Vec4(0.85f,	0.4f,	0.1f,	1);
	cursorRewardColor	= osg::Vec4(0.9f,	0.9f,	0.9f,	1);
	cursorWrongColor	= osg::Vec4(0.2f,	0.2f,	0.2f,	1);
	cursorCorrectColor	= osg::Vec4(0.7f,	1.0f,	0.2f,	1);
	targetColor			= osg::Vec4(0.0f,	0.8f,	0.8f,	1);
	targetCorrectColor	= osg::Vec4(0.7f,	1.0f,	0.2f,	1);
	targetWrongColor	= osg::Vec4(1.0,	0.0f,	0.0f,	1);

	// Create cursor and give it to the ControlModuleManager
	cursor = new SphereElement();
	sceneManager.addToScene(cursor);
	ctrlModManager.setControlledElement(cursor);
	ctrlModManager.setNerveModuleUser(&ctrlAndJoyModUser);
	ctrlModManager.setRebroadcastManager(rm);
	ctrlModManager.setData(ControlModuleManager3D::MOUSE_DEVICE, &mouseBuffer);
	ctrlModManager.setData(ControlModuleManager3D::JOYSTICK_DEVICE, &joyBuffer);
	ctrlModManager.setData(ControlModuleManager3D::BCI_DEVICE, &bciBuffer);
	// Create the rest of the world objects using SceneManager
	allObjects = sceneManager.createEleMobile("allObjects");
	//allDecoys = sceneManager.createEleMobile("allDecoys");
	targetIndicator = sceneManager.createEleSphere("targetIndicator");
	center = sceneManager.createEleSphere("center");
	blackOut = sceneManager.createEleSphere("blackOut");
	target = sceneManager.createEleTorus("target");
	decoy90 = sceneManager.createEleTorus("decoy90");
	decoy180 = sceneManager.createEleTorus("decoy180");
	decoy270 = sceneManager.createEleTorus("decoy270");
	allObjects->addChild(target);
	allObjects->addChild(targetIndicator);
	allObjects->addChild(center);
	allObjects->addChild(blackOut);
	allObjects->addChild(decoy90);
	allObjects->addChild(decoy180);
	allObjects->addChild(decoy270);
	//allDecoys->addChild(decoyU);
	//allDecoys->addChild(decoyD);
	//allDecoys->addChild(decoyL);
	//allDecoys->addChild(decoyR);
	blackOut->setScale(1000);
	blackOut->turnOff();
	sceneManager.addToScene(allObjects);
	allObjects->turnOff();

	_sceneRoot = sceneManager.getSceneRoot();
	sceneManager.getEleCameraExperimenter()->setViewMatrix(osg::Matrix::lookAt(osg::Vec3f(0,-110,0), osg::Vec3f(0,0,0), osg::Vec3f(0,0,1)));
	sceneManager.getEleCameraSubject()->setViewMatrix(osg::Matrix::lookAt(osg::Vec3f(0,-110,0), osg::Vec3f(0,0,0), osg::Vec3f(0,0,1)));

	// Initial display
	cursor->setColor(cursorColor);
	targetIndicator->setColor(targetColor);
	target->setColor(targetColor);
	center->setColor(targetColor);
	decoy90->setColor(targetColor);
	decoy180->setColor(targetColor);
	decoy270->setColor(targetColor);

	// Setup collision detection
	touchingTarget.setSphere(cursor);
	touchingTarget.addTargetElement(target);
	touchingCenter.setSphere(cursor);
	touchingCenter.addTargetElement(center);

	// Record Cursor Position
	TDTWriteElementPos2DModule * cursRecorder = new TDTWriteElementPos2DModule("RX5(1).CursorData", cursor, 60);
	cursRecorder->setRemoveAction(NerveModule::DELETE_MODULE);
	tdtInterface.requestAddModule(cursRecorder, "CursorRecorder");

	// Read Brain Signals from TDT
	TDTReadMultiBandModule * brainReader = new TDTReadMultiBandModule("RX5(1).Signals", 0, NUM_FEATURES, brainBuffer, brainBufferMean, brainBufferStd, 100);
	brainReader->setRemoveAction(NerveModule::DELETE_MODULE);
	rm->getRebroadcastVoid("ResetDecode")->signal_toSameThread.connect(brainReader, &TDTReadMultiBandModule::resetNormalization);
	rm->getRebroadcastDouble("DecodeLP")->signal_toSameThread.connect(brainReader, &TDTReadMultiBandModule::lowPass);
	rm->getRebroadcastDouble("DecodeLPCutoff")->signal_toSameThread.connect(brainReader, &TDTReadMultiBandModule::setLowPassCutoff);
	tdtInterface.requestAddModule(brainReader, "BrainReader");

	// Decoder
	DecodeModule2D * decoder = new DecodeModule2D(NerveModule::DELETE_MODULE);
	decoder->setup(brainBuffer, xTransWeights, zTransWeights, &bciBuffer, NUM_FEATURES, 100);
	//rm->getRebroadcastDouble("SmoothHyperbolic")->signal_toSameThread.connect(decoder, &DecodeModule2D::setSmoothHyperbolic);
	//rm->getRebroadcastDouble("SmoothThreshold1")->signal_toSameThread.connect(decoder, &DecodeModule2D::setSmoothThreshold1);
	//rm->getRebroadcastDouble("SmoothThreshold2")->signal_toSameThread.connect(decoder, &DecodeModule2D::setSmoothThreshold2);
	ctrlAndJoyModUser.requestAddModule(decoder, "Decoder");

	// Record Features for Adaptation
	recorder.setup(brainBuffer, NUM_FEATURES, biasBuffer, xTransWeights, zTransWeights, "", 20);
	rm->getRebroadcastDouble("Alpha")->signal_toSameThread.connect(&recorder, &RecordFeaturesModule2D_2::setAlpha);
	rm->getRebroadcastString("DataDirName")->signal_toSameThread.connect(&recorder, &RecordFeaturesModule2D_2::setFileName);
	rm->getRebroadcastVoid("PauseRecorder")->signal_toSameThread.connect(&recorder, &RecordFeaturesModule2D_2::pause);
	ctrlAndJoyModUser.requestAddModule(&recorder, "Recorder");

	// Signal Log
	signalLog.openFile("SignalLog.txt", "w");
	writeSignals.setFileInterface(&signalLog);
	fileModUser.requestAddModule(&writeSignals);
	streamSignals.setup(&writeSignals, brainBuffer, brainBufferMean, brainBufferStd, NUM_FEATURES, 20);
	fileModUser.requestAddModule(&streamSignals, "StreamSignals");

	// Joystick Module
	JoystickContinuousReadModule * joyMod = new JoystickContinuousReadModule(NerveModule::DELETE_MODULE);
	joyMod->setup(&joyInterface, &joyBuffer, 20);
	ctrlAndJoyModUser.requestAddModule(joyMod, "ReadJoyModule");

	// Record Joystick Module
	//TDTContinuousWriteInterfaceDataModule * recordJoy = new TDTContinuousWriteInterfaceDataModule("RX5(1).JoyData", 0, 2, &joyBuffer, 20);
	//recordJoy->setRemoveAction(NerveModule::DELETE_MODULE);
	//tdtInterface.requestAddModule(recordJoy, "WriteJoyModule");

	// Cursor Bias
	biasMod.setElement(cursor);
	biasMod.setTarget(center);
	biasMod.setDirectionBuffer(biasBuffer);
	biasMod.setRebroadcastManager(rm);
	rm->getRebroadcastDouble("DistMargin")->signal_toSameThread.connect(&biasMod, &MovementBiasToTargetControlModule3D::setDistMargin);
	ctrlAndJoyModUser.requestAddModule(&biasMod, "BiasModule");

	// Setup Managers
	taskInfoManager.setRebroadcastManager(rm);	
	rewardManager.setRebroadcastManager(rm);
	rewardManager.setTDTInterface(&tdtInterface);
	randTargetManager.setRebroadcastManager(rm);
	randTargetManager.setTaskInfoManager(&taskInfoManager);
	randTargetManager.setup();
	dataLogManager.setTaskInfoManager(&taskInfoManager);
	dataLogManager.setNerveModuleUser(&fileModUser);
	dataLogManager.setRebroadcastManager(rm);

	// Start Matlab engine
	if (!(ep = engOpen("\0"))) 
		fprintf(stderr, "\nCan't start MATLAB engine\n");
	else
		fprintf(stderr, "\nStarted MATLAB engine\n");

	// Sync GUI
	rm->getRebroadcastVoid("SyncAll")->signal_toOtherThreads.broadcast();
	rm->rebroadcastAll();

	// Load Decode Weights
	rm->getRebroadcastVoid("LoadDecodeWeights")->signal_toSameThread.connect(this, &AdaptiveDecodeTask::loadDecodeWeights);
	loadDecodeWeights();// load any initial weights, if there are any present

	tdtWaitTimer.setStartTick();
}
void AdaptiveDecodeTask::taskRunningMethods()
{	
	updateAll();
	if (prevRunState == STOPPED)
	{
		resetTask();
		dataLogManager.openFile();
		dataLogManager.addScript("Event_codes ZILCH=0 START=1 CENTER_HOLD=2 STIM=3 MOVEMENT=4 TARGET_HOLD=4 CORRECT=5 INCORRECT=6 SETUP_TRIAL=7\n\n");
		dataLogManager.writeRebroadcastManagerIO(rmFileIO);
		prevRunState = RUNNING;
	}
	if ( rm->getCurrentValue_double("BlackOut") > 0 )
		blackOutScreen();
	if (tdtWaitFlag)
	{
		if (tdtWaitTimer.time_m() < tdtWaitTime)
		{
			Sleep(1);
			return;
		}
		else
		{
			tdtWaitTimer.setStartTick();
			tdtWaitFlag = false;
		}
	}
	else
	{
		switch(currentState)
		{
			case SETUP_TRIAL: setupTrial(); break;
			case START: start(); break;
			case CENTER_HOLD: centerHold(); break;
			case MOVEMENT: movement(); break;
			case TARGET_HOLD: targetHold(); break;
			case CORRECT: correct(); break;
			case INCORRECT: incorrect(); break;
		}
	}
}
void AdaptiveDecodeTask::taskStoppedMethods()
{
	updateAll();
	if (prevRunState == RUNNING)
	{
		recorder.stop();
		biasMod.pause();
	}
	prevRunState = STOPPED;
	currentState = SETUP_TRIAL;
	prevState = ZILCH;
}
void AdaptiveDecodeTask::updateAll()
{
	// Update data
	rm->rebroadcastAll(); // must make this call to send and receive across threads to sync with Gui Data
	rmFileIO->rebroadcastAll();

	// Update world objects
	cursor->setScale(osg::Vec3(rm->getCurrentValue_double("CursorSize"), rm->getCurrentValue_double("CursorSize"), rm->getCurrentValue_double("CursorSize")));
	target->setScaleTorus(rm->getCurrentValue_double("TargetSize")-rm->getCurrentValue_double("RingThickness"), rm->getCurrentValue_double("RingThickness"));
	//decoyD->setScale(osg::Vec3(rm->getCurrentValue_double("TargetSize"), rm->getCurrentValue_double("RingThickness"), rm->getCurrentValue_double("RingThickness")));
	//decoyL->setScale(osg::Vec3(rm->getCurrentValue_double("TargetSize"), rm->getCurrentValue_double("RingThickness"), rm->getCurrentValue_double("RingThickness")));
	//decoyR->setScale(osg::Vec3(rm->getCurrentValue_double("TargetSize"), rm->getCurrentValue_double("RingThickness"), rm->getCurrentValue_double("RingThickness")));
	
	center->setScale(rm->getCurrentValue_double("TargetSize"));
	target->update();
	cursor->update();
	center->update();
	//decoyD->update();
	//decoyL->update();
	//decoyR->update();

	sceneManager.updateAll();
};
void AdaptiveDecodeTask::resetTask()
{
	printf("reset task\n");
	rm->rebroadcastAll();
	randTargetManager.setup();
}
void AdaptiveDecodeTask::setupTrial()
{
	if (prevState != currentState)
	{
		printf("setupTrial()\n");
		stateTimer.setStartTick();
		tdtInterface.setTargetVal("RX5(1).EventCode", SETUP_TRIAL);

		// Attempt to select a new node
		randTargetManager.selectNewTrial();
		if (randTargetManager.getTrialTarget() == -1)
		{
			printf("task complete\n");
			if (rm->getCurrentValue_double("AdaptiveDecode"))
				updateDecodeWeights();
			randTargetManager.setup();
			randTargetManager.selectNewTrial();
			//this->setCompletedStatus(true);
			//return;
		}
		printf("\nTrials remaining: %i of: %i\n", randTargetManager.getNumTrialsRemaining(), randTargetManager.getNumTotalTrials());
		
		//target->setPosition(osg::Vec3(rm->getCurrentValue_double("TargetDistance"), 0, 0));
		target_distance = rm->getCurrentValue_double("TargetDistance");
		target->setPosition(osg::Vec3(target_distance, 0, 0));
		//targetIndicator->setScale(osg::Vec3(rm->getCurrentValue_double("TargetSize"), rm->getCurrentValue_double("TargetSize"), rm->getCurrentValue_double("TargetSize")));
		decoy90->setScaleTorus(rm->getCurrentValue_double("TargetSize") - rm->getCurrentValue_double("RingThickness"), rm->getCurrentValue_double("RingThickness"));
		decoy180->setScaleTorus(rm->getCurrentValue_double("TargetSize") - rm->getCurrentValue_double("RingThickness"), rm->getCurrentValue_double("RingThickness"));
		decoy270->setScaleTorus(rm->getCurrentValue_double("TargetSize") - rm->getCurrentValue_double("RingThickness"), rm->getCurrentValue_double("RingThickness"));
		//targetIndicator->setPosition(osg::Vec3(target_distance, 0, 0));
		decoy180->setPosition(osg::Vec3(-target_distance, 0, 0));
		decoy90->setPosition(osg::Vec3(0, 0, target_distance));
		decoy270->setPosition(osg::Vec3(0,0,-target_distance));
		allObjects->setRotation(osg::Quat(randTargetManager.getTrialTarget()*2*osg::PI/randTargetManager.getNumTargets(), osg::Vec3(0., -1., 0.) ));
		
		center->setColor(targetColor);
		center->turnOn();

		tdtInterface.setTargetVal("RX5(1).TrialID", taskInfoManager.getTrialID());
		tdtInterface.setTargetVal("RX5(1).Target", randTargetManager.getTrialTarget());
		//streamSignals.setTrialID(taskInfoManager.getTrialID());
		//streamSignals.setTarget(randTargetManager.getTrialTarget());
		streamSignals.setAll(taskInfoManager.getTrialID(), SETUP_TRIAL, randTargetManager.getTrialTarget());
		recorder.setID(taskInfoManager.getTrialID());
		prevState = SETUP_TRIAL;
	}
	if (stateTimer.time_m() > rm->getCurrentValue_double("ITI"))
		changeState(START);
};
void AdaptiveDecodeTask::start()
{
	if (prevState != currentState)
	{
		printf("start\n");
		tdtInterface.setTargetVal("RX5(1).EventCode", START);
		streamSignals.setEventCode(START);
		biasMod.setTarget(center);
		cursor->setColor(cursorColor);
		target->setColor(targetColor);
		cursor->turnOn();
		allObjects->turnOn();
		target->turnOff(); 
		targetIndicator->turnOff();
		decoy90->turnOff();
		decoy180->turnOff();
		decoy270->turnOff();
		prevState = currentState;
		stateTimer.setStartTick();
		recorder.stop();
		biasMod.unpause();
	}
	if (rm->getCurrentValue_double("AutoCenterHold") > 0)
	{
		changeState(CENTER_HOLD);
		return;
	}
	if (touchingCenter.query())
	{
		cursor->setColor(cursorCorrectColor);
		center->setColor(targetCorrectColor);
	}
	else
	{
		cursor->setColor(cursorColor);
		center->setColor(targetColor);
	}
	if (touchingCenter.query())
		changeState(CENTER_HOLD);
}
void AdaptiveDecodeTask::centerHold()
{
	if (prevState != currentState)
	{
		printf("center hold\n");
		if (rm->getCurrentValue_double("UseAuxiliaryRewards")>0)
			rewardManager.giveAuxilaryReward();
		tdtInterface.setTargetVal("RX5(1).EventCode", CENTER_HOLD);
		streamSignals.setEventCode(CENTER_HOLD);
		if (rm->getCurrentValue_double("AutoCenterHold") > 0)
		{	center->turnOff();
			target->turnOn();
			decoy90->turnOn();
			decoy180->turnOn();
			decoy270->turnOn();
		}
		else
		{
			cursor->setColor(cursorCorrectColor);
			center->setColor(targetCorrectColor);
			target->turnOn();
			decoy90->turnOn();
			decoy180->turnOn();
			decoy270->turnOn();
		}
		
		prevState = currentState;
		stateTimer.setStartTick();
		recorder.stop();
		biasMod.unpause();
	}
	if (rm->getCurrentValue_double("AutoCenterHold") > 0)
	{
		biasMod.unpause();
		if (stateTimer.time_m() < rm->getCurrentValue_double("CenterHoldTime"))
		{
			cursor->setPosition(0,0,0);
			return;
		}
		else
		{
			changeState(MOVEMENT);
			return;
		}
	}
	if (!touchingCenter.query())
	{
		changeState(INCORRECT);
		return;
	}
	if (stateTimer.time_m() > rm->getCurrentValue_double("CenterHoldTime"))
	{
		changeState(MOVEMENT);
	}
}
void AdaptiveDecodeTask::movement()
{
	if (prevState != currentState)
	{
		printf("movement\n");
		tdtInterface.setTargetVal("RX5(1).EventCode", MOVEMENT);
		streamSignals.setEventCode(MOVEMENT);
		biasMod.setTarget(target);
		recorder.start();
		biasMod.unpause();
		cursor->setColor(cursorColor);
		target->setColor(targetColor);
		target->turnOn();
		center->turnOff();
		allObjects->turnOn();
		prevState = currentState;
		stateTimer.setStartTick();
	}
	if (stateTimer.time_m() > rm->getCurrentValue_double("MaxRecordTime"))
		recorder.stop();
	if (stateTimer.time_m() < rm->getCurrentValue_double("MaxMovementTime"))
	{
		if ((target->getWorldPosition()-cursor->getWorldPosition()).length() < (rm->getCurrentValue_double("TargetSize")+rm->getCurrentValue_double("CursorSize")))
		{
			cursor->setColor(cursorCorrectColor);
			target->setColor(targetCorrectColor);
			//recorder.stop();
			changeState(TARGET_HOLD);
			movement_time = stateTimer.time_m();
		}
	}
	else
	{
		recorder.stop();
		changeState(INCORRECT);
	}
}
void AdaptiveDecodeTask::targetHold()
{
	if (prevState != currentState)
	{
		printf("target hold\n");
		tdtInterface.setTargetVal("RX5(1).EventCode", TARGET_HOLD);
		streamSignals.setEventCode(TARGET_HOLD);
		cursor->setColor(cursorCorrectColor);
		target->setColor(targetCorrectColor);
		center->turnOff();		
		//allObjects->turnOn();
		//allDecoys->turnOff();
		targetIndicator->turnOff();
		decoy90->turnOff();
		decoy180->turnOff();
		decoy270->turnOff();
		prevState = currentState;
		stateTimer.setStartTick();
//		recorder.stop();
		biasMod.unpause();
	}
	if (!((target->getWorldPosition()-cursor->getWorldPosition()).length() < (rm->getCurrentValue_double("TargetSize")+rm->getCurrentValue_double("CursorSize"))))
		changeState(INCORRECT);
	else if (stateTimer.time_m() > rm->getCurrentValue_double("TargetHoldTime"))
		changeState(CORRECT);
}
void AdaptiveDecodeTask::correct()
{
	if (prevState != currentState)
	{
		printf("correct\n");
		tdtInterface.setTargetVal("RX5(1).EventCode", CORRECT);
		streamSignals.setEventCode(CORRECT);
		recorder.stop();
		biasMod.unpause();
		stateTimer.setStartTick();
		target->setColor(targetCorrectColor);
		cursor->setColor(cursorRewardColor);
		printTaskData();
		randTargetManager.removeCurrentTrial();
		rewardManager.givePrimaryReward();
		training_data_flag = 1;
		recorder.saveData();
		prevState = currentState;
	}
	if (stateTimer.time_m() > 750)
	{
		changeState(SETUP_TRIAL);
		cursor->setColor(cursorColor);
		target->turnOff();
		center->turnOff();
		biasMod.pause();
	}
}	
void AdaptiveDecodeTask::incorrect()
{
	if (prevState != currentState)
	{
		printf("incorrect\n");
		tdtInterface.setTargetVal("RX5(1).EventCode", INCORRECT);
		streamSignals.setEventCode(INCORRECT);
		training_data_flag = 0;
		recorder.stop();
		biasMod.unpause();
		stateTimer.setStartTick();
		target->setColor(targetWrongColor);
		center->setColor(targetWrongColor);
		cursor->setColor(cursorWrongColor);
		prevState = currentState;
		printTaskData();
	}
	if (stateTimer.time_m() > 750)
	{
		changeState(SETUP_TRIAL);
		cursor->setColor(cursorColor);
		target->turnOff();
		center->turnOff();
		biasMod.pause();
	}
}	
void AdaptiveDecodeTask::updateDecodeWeights()
{
	allObjects->turnOff();
	sceneManager.updateAll();
	cursor->turnOff();
	cursor->update();
	// Stop recording features (close the file so matlab can read it)
	recorder.saveData();
	// Call matlab
	char buffer[256];
	sprintf(buffer, "%i", taskInfoManager.getTrialID());
	std::string functionCall = "AdReach2D_32x5('";
	functionCall.append(rm->getCurrentValue_string("SubjectName"));
	functionCall.append("', '");
	functionCall.append(rm->getCurrentValue_string("DataDirName"));
	functionCall.append("', ");
	functionCall.append(std::string(buffer));
	functionCall.append(");");
	printf("%s\n", functionCall.c_str());
	engEvalString(ep, functionCall.c_str());
	loadDecodeWeights();
	recorder.clearData();
	allObjects->turnOn();
	cursor->turnOn();
};
void AdaptiveDecodeTask::loadDecodeWeights()
{
	recorder.stop();
	// Update all the weights
	recorder.updateWeights();
	weight_id++;
	// Record all the weights
	float weight;
	dataLogManager.addScript("X_trans_weights          \n");
	for (int n=0; n<NUM_FEATURES; n++)
	{
		xTransWeights[n].getData(weight);
		dataLogManager.addScript("%f	", weight);
	}
	//dataLogManager.addScript("\nY_trans_weights          \n");
	//for (int n=0; n<NUM_FEATURES; n++)
	//{
	//	yTransWeights[n].getData(weight);
	//	dataLogManager.addScript("%f	", weight);
	//}
	dataLogManager.addScript("\nZ_trans_weights          \n");
	for (int n=0; n<NUM_FEATURES; n++)
	{
		zTransWeights[n].getData(weight);
		dataLogManager.addScript("%f	", weight);
	}
	//dataLogManager.addScript("\nX_rot_weights          \n");
	//for (int n=0; n<NUM_FEATURES; n++)
	//{
	//	xRotWeights[n].getData(weight);
	//	dataLogManager.addScript("%f	", weight);
	//}
	//dataLogManager.addScript("\nY_rot_weights          \n");
	//for (int n=0; n<NUM_FEATURES; n++)
	//{
	//	yRotWeights[n].getData(weight);
	//	dataLogManager.addScript("%f	", weight);
	//}
	//dataLogManager.addScript("\nZ_rot_weights          \n");
	//for (int n=0; n<NUM_FEATURES; n++)
	//{
	//	zRotWeights[n].getData(weight);
	//	dataLogManager.addScript("%f	", weight);
	//}
	dataLogManager.addScript("\n");
	printf("loaded weights\n");
};
void AdaptiveDecodeTask::blackOutScreen()
{
	recorder.stop();
	cursor->turnOff();
	target->turnOff();
	blackOut->turnOn();
	sceneManager.updateAll();
	while (rm->getCurrentValue_double("BlackOut") > 0)
	{
		rm->rebroadcastAll();
		Sleep(100);
	}
	cursor->turnOn();
	target->turnOn();
	blackOut->turnOff();
	sceneManager.updateAll();
};
void AdaptiveDecodeTask::printTaskData()
{
	dataLogManager.addScript("\n\nTask          AdaptiveDecoding\n");
	dataLogManager.addScript("Trial_id            %i\n", taskInfoManager.getTrialID());
	dataLogManager.addScript("Target_num          %i\n", randTargetManager.getTrialTarget());
	dataLogManager.addScript("Total_targets       %f\n", rm->getCurrentValue_double("NumTargets"));
	dataLogManager.addScript("Center_hold_time    %f\n", movement_time);
	dataLogManager.addScript("Movement_time       %f\n", rm->getCurrentValue_double("CenterHoldTime"));
	dataLogManager.addScript("Max_movement_time   %f\n", rm->getCurrentValue_double("MaxMovementTime"));
	dataLogManager.addScript("Target_hold_time    %f\n", rm->getCurrentValue_double("TargetHoldTime"));
	dataLogManager.addScript("ITI                 %f\n", rm->getCurrentValue_double("ITI"));
	dataLogManager.addScript("Dist_margin         %f\n", rm->getCurrentValue_double("DistMargin"));
	dataLogManager.addScript("Cursor_size         %f\n", rm->getCurrentValue_double("CursorSize"));
	dataLogManager.addScript("Target_size         %f\n", rm->getCurrentValue_double("TargetSize"));
	dataLogManager.addScript("Target_distance     %f\n", rm->getCurrentValue_double("TargetDistance"));
	dataLogManager.addScript("Trans_gain_x        %f\n", rm->getCurrentValue_double("BCIVelocityTranslationXGain"));
	dataLogManager.addScript("Trans_gain_z        %f\n", rm->getCurrentValue_double("BCIVelocityTranslationZGain"));
    dataLogManager.addScript("Weight_id           %i\n", weight_id);
	dataLogManager.addScript("Training_data_flag  %i\n", training_data_flag);
	dataLogManager.addScript("Trans_Bias_on       %f\n", rm->getCurrentValue_double("TransBias"));
	dataLogManager.addScript("Trans_Bias_gain     %f\n", rm->getCurrentValue_double("TransBiasGain"));
	dataLogManager.addScript("DecodeAlpha         %f\n", rm->getCurrentValue_double("Alpha"));
	dataLogManager.addScript("SmoothLP            %f\n", rm->getCurrentValue_double("SmoothLP"));
	dataLogManager.addScript("SmoothHyperbolic    %f\n", rm->getCurrentValue_double("SmoothHyperbolic"));
	dataLogManager.addScript("SmoothLPCutoff      %f\n", rm->getCurrentValue_double("SmoothLPCutoff"));
	dataLogManager.addScript("SmoothHyperbolicT1  %f\n", rm->getCurrentValue_double("SmoothThreshold1"));
	dataLogManager.addScript("SmoothHyperbolicT2  %f\n", rm->getCurrentValue_double("SmoothThreshold2"));
	dataLogManager.addScript("DistanceMargin      %f\n", rm->getCurrentValue_double("DistMargin"));
	dataLogManager.addScript("MaxRecTime          %f\n", rm->getCurrentValue_double("MaxRecordTime"));
	dataLogManager.addScript("DecodeLP            %f\n", rm->getCurrentValue_double("DecodeLP"));
	dataLogManager.addScript("DecodeLPCutoff      %f\n", rm->getCurrentValue_double("DecodeLPCutoff"));
	dataLogManager.addScript("AutoCenterHold      %f\n", rm->getCurrentValue_double("AutoCenterHold"));
};
void AdaptiveDecodeTask::changeState(TaskState state)
{
	tdtWaitTime = rm->getCurrentValue_double("TDTWait") - tdtWaitTimer.time_m();
	if (tdtWaitTime > 0)
	{
		
		tdtWaitTimer.setStartTick();
		tdtWaitFlag = true;
	}
	else
	{
		tdtWaitTimer.setStartTick();
	}
	prevState = currentState;
	currentState = state;
};