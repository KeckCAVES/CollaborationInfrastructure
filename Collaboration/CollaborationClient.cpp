/***********************************************************************
CollaborationClient - Class to support collaboration between
applications in spatially distributed (immersive) visualization
environments.
Copyright (c) 2007-2010 Oliver Kreylos

This file is part of the Vrui remote collaboration infrastructure.

The Vrui remote collaboration infrastructure is free software; you can
redistribute it and/or modify it under the terms of the GNU General
Public License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

The Vrui remote collaboration infrastructure is distributed in the hope
that it will be useful, but WITHOUT ANY WARRANTY; without even the
implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Vrui remote collaboration infrastructure; if not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA
***********************************************************************/

#include <Collaboration/CollaborationClient.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <Misc/ThrowStdErr.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/CompoundValueCoders.h>
#include <Comm/MulticastPipe.h>
#include <Geometry/OrthogonalTransformation.h>
#include <GL/gl.h>
#include <GL/GLFont.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/RowColumn.h>
#include <GLMotif/Margin.h>
#include <GLMotif/Separator.h>
#include <GLMotif/TextField.h>
#include <GLMotif/ToggleButton.h>
#include <Vrui/Vrui.h>

namespace Collaboration {

/***************************************************
Methods of class CollaborationClient::Configuration:
***************************************************/

CollaborationClient::Configuration::Configuration(void)
	:configFile(COLLABORATION_CONFIGFILENAME),
	 cfg(configFile.getSection("/CollaborationClient"))
	{
	}

void CollaborationClient::Configuration::setServer(std::string newServerHostName,int newServerPortId)
	{
	cfg.storeString("./serverHostName",newServerHostName);
	cfg.storeValue<int>("./serverPortId",newServerPortId);
	}

void CollaborationClient::Configuration::setClientName(std::string newClientName)
	{
	cfg.storeString("./clientName",newClientName);
	}

/********************************************
Methods of class CollaborationClient::Client:
********************************************/

CollaborationClient::Client::~Client(void)
	{
	/* Delete all protocol client state objects: */
	for(RemoteClientProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		delete pIt->protocolClientState;
	}

/*************************************************
Methods of class CollaborationClient::ServerState:
*************************************************/

CollaborationClient::ServerState::ServerState(void)
	:numClients(0),clientIDs(0),clientStates(0)
	{
	}

CollaborationClient::ServerState::~ServerState(void)
	{
	delete[] clientIDs;
	delete[] clientStates;
	}

void CollaborationClient::ServerState::resize(unsigned int newNumClients)
	{
	if(newNumClients!=numClients)
		{
		delete[] clientIDs;
		delete[] clientStates;
		numClients=newNumClients;
		clientIDs=numClients!=0?new unsigned int[numClients]:0;
		clientStates=numClients!=0?new CollaborationPipe::ClientState[numClients]:0;
		}
	}

/************************************
Methods of class CollaborationClient:
************************************/

void CollaborationClient::createClientDialog(void)
	{
	GLMotif::WidgetManager* wm=Vrui::getWidgetManager();
	const GLMotif::StyleSheet* ss=wm->getStyleSheet();
	
	/* Create the remote client list dialog window: */
	clientDialogPopup=new GLMotif::PopupWindow("ClientDialogPopup",wm,"Collaboration Client");
	clientDialogPopup->setResizableFlags(true,false);
	
	GLMotif::RowColumn* clientDialog=new GLMotif::RowColumn("ClientDialog",clientDialogPopup,false);
	clientDialog->setOrientation(GLMotif::RowColumn::VERTICAL);
	clientDialog->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	clientDialog->setNumMinorWidgets(1);
	
	GLMotif::Margin* showSettingsMargin=new GLMotif::Margin("ShowSettingsMargin",clientDialog,false);
	showSettingsMargin->setAlignment(GLMotif::Alignment::LEFT);
	
	showSettingsToggle=new GLMotif::ToggleButton("ShowSettingsToggle",showSettingsMargin,"Show Settings");
	showSettingsToggle->setBorderWidth(0.0f);
	showSettingsToggle->setHAlignment(GLFont::Left);
	showSettingsToggle->setToggle(false);
	showSettingsToggle->getValueChangedCallbacks().add(this,&CollaborationClient::showSettingsToggleValueChangedCallback);
	
	showSettingsMargin->manageChild();
	
	GLMotif::RowColumn* remoteClientsTitle=new GLMotif::RowColumn("RemoteClientsTitle",clientDialog,false);
	remoteClientsTitle->setOrientation(GLMotif::RowColumn::HORIZONTAL);
	remoteClientsTitle->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	remoteClientsTitle->setNumMinorWidgets(1);
	
	new GLMotif::Separator("Sep1",remoteClientsTitle,GLMotif::Separator::HORIZONTAL,ss->fontHeight,GLMotif::Separator::LOWERED);
	new GLMotif::Label("Title",remoteClientsTitle,"Remote Clients");
	new GLMotif::Separator("Sep2",remoteClientsTitle,GLMotif::Separator::HORIZONTAL,ss->fontHeight,GLMotif::Separator::LOWERED);
	
	remoteClientsTitle->manageChild();
	
	/* Create the client list widget: */
	clientListRowColumn=new GLMotif::RowColumn("ClientListRowColumn",clientDialog);
	clientListRowColumn->setOrientation(GLMotif::RowColumn::VERTICAL);
	clientListRowColumn->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	clientListRowColumn->setNumMinorWidgets(3);
	clientListRowColumn->setColumnWeight(0,1.0f);
	
	clientDialog->manageChild();
	}

void CollaborationClient::createSettingsDialog(void)
	{
	GLMotif::WidgetManager* wm=Vrui::getWidgetManager();
	const GLMotif::StyleSheet* ss=wm->getStyleSheet();
	
	/* Create the settings dialog window: */
	settingsDialogPopup=new GLMotif::PopupWindow("SettingsDialogPopup",wm,"Collaboration Client Settings");
	settingsDialogPopup->setCloseButton(true);
	settingsDialogPopup->getCloseCallbacks().add(this,&CollaborationClient::settingsDialogCloseCallback);
	settingsDialogPopup->setResizableFlags(false,false);
	
	GLMotif::RowColumn* settingsDialog=new GLMotif::RowColumn("SettingsDialog",settingsDialogPopup,false);
	settingsDialog->setOrientation(GLMotif::RowColumn::VERTICAL);
	settingsDialog->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	settingsDialog->setNumMinorWidgets(1);
	
	/***************************************************
	Create controls for the collaboration client itself:
	***************************************************/
	
	GLMotif::Margin* togglesMargin=new GLMotif::Margin("TogglesMargin",settingsDialog,false);
	togglesMargin->setAlignment(GLMotif::Alignment::LEFT);
	
	GLMotif::RowColumn* togglesBox=new GLMotif::RowColumn("TogglesBox",togglesMargin,false);
	togglesBox->setOrientation(GLMotif::RowColumn::HORIZONTAL);
	togglesBox->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	togglesBox->setNumMinorWidgets(1);
	
	GLMotif::ToggleButton* fixGlyphScalingToggle=new GLMotif::ToggleButton("FixGlyphScalingToggle",togglesBox,"Fix Glyph Scaling");
	fixGlyphScalingToggle->setBorderWidth(0.0f);
	fixGlyphScalingToggle->setHAlignment(GLFont::Left);
	fixGlyphScalingToggle->setToggle(fixGlyphScaling);
	fixGlyphScalingToggle->getValueChangedCallbacks().add(this,&CollaborationClient::fixGlyphScalingToggleValueChangedCallback);
	
	GLMotif::ToggleButton* renderRemoteEnvironmentsToggle=new GLMotif::ToggleButton("RenderRemoteEnvironmentsToggle",togglesBox,"Render Remote Environments");
	renderRemoteEnvironmentsToggle->setBorderWidth(0.0f);
	renderRemoteEnvironmentsToggle->setHAlignment(GLFont::Left);
	renderRemoteEnvironmentsToggle->setToggle(renderRemoteEnvironments);
	renderRemoteEnvironmentsToggle->getValueChangedCallbacks().add(this,&CollaborationClient::renderRemoteEnvironmentsToggleValueChangedCallback);
	
	togglesBox->manageChild();
	
	togglesMargin->manageChild();
	
	/******************************************************************
	Create controls for all registered protocol clients that want them:
	******************************************************************/
	
	/* Process all protocols: */
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		if((*pIt)->haveSettingsDialog())
			{
			/* Create a divider with the protocol plug-in's name: */
			GLMotif::RowColumn* protocolPluginTitle=new GLMotif::RowColumn("ProtocolPluginTitle",settingsDialog,false);
			protocolPluginTitle->setOrientation(GLMotif::RowColumn::HORIZONTAL);
			protocolPluginTitle->setPacking(GLMotif::RowColumn::PACK_TIGHT);
			protocolPluginTitle->setNumMinorWidgets(1);
			
			new GLMotif::Separator("Sep1",protocolPluginTitle,GLMotif::Separator::HORIZONTAL,ss->fontHeight,GLMotif::Separator::LOWERED);
			new GLMotif::Label("Title",protocolPluginTitle,(*pIt)->getName());
			new GLMotif::Separator("Sep2",protocolPluginTitle,GLMotif::Separator::HORIZONTAL,ss->fontHeight,GLMotif::Separator::LOWERED);
			
			protocolPluginTitle->manageChild();
			
			/* Create the protocol plug-in's controls: */
			(*pIt)->buildSettingsDialog(settingsDialog);
			}
	
	settingsDialog->manageChild();
	}

void CollaborationClient::showSettingsDialog(void)
	{
	typedef GLMotif::WidgetManager::Transformation WTransform;
	typedef WTransform::Vector WVector;
	
	GLMotif::WidgetManager* wm=Vrui::getWidgetManager();
	
	/* Open the settings dialog right next to the client dialog: */
	WTransform transform=wm->calcWidgetTransformation(clientDialogPopup);
	const GLMotif::Box& box=clientDialogPopup->getExterior();
	WVector offset(box.origin[0]+box.size[0],box.origin[1]+box.size[1],0.0f);
	
	offset[0]-=settingsDialogPopup->getExterior().origin[0];
	offset[1]-=settingsDialogPopup->getExterior().origin[1]+settingsDialogPopup->getExterior().size[1];
	transform*=WTransform::translate(offset);
	wm->popupPrimaryWidget(settingsDialogPopup,transform);
	}

void CollaborationClient::showSettingsToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	if(cbData->set)
		{
		/* Show the settings dialog: */
		showSettingsDialog();
		}
	else
		{
		/* Hide the settings dialog: */
		Vrui::popdownPrimaryWidget(settingsDialogPopup);
		}
	}

void CollaborationClient::followClientToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	if(cbData->set)
		{
		int newClientIndex=clientListRowColumn->getChildRow(cbData->toggle);
		
		if(followClientIndex>=0)
			{
			if(newClientIndex!=followClientIndex)
				{
				/* Unset the previously set toggle: */
				GLMotif::ToggleButton* oldToggle=dynamic_cast<GLMotif::ToggleButton*>(clientListRowColumn->getChild(followClientIndex*3+1));
				if(oldToggle!=0)
					oldToggle->setToggle(false);
				
				/* Follow the new client: */
				followClientIndex=newClientIndex;
				}
			}
		else if(faceClientIndex>=0)
			{
			/* Unset the previously set toggle: */
			GLMotif::ToggleButton* oldToggle=dynamic_cast<GLMotif::ToggleButton*>(clientListRowColumn->getChild(faceClientIndex*3+2));
			if(oldToggle!=0)
				oldToggle->setToggle(false);
			
			/* Stop facing the old client: */
			faceClientIndex=-1;
			
			/* Follow the new client: */
			followClientIndex=newClientIndex;
			}
		else
			{
			/* Try to enable following: */
			if(Vrui::activateNavigationTool(reinterpret_cast<Vrui::Tool*>(this)))
				followClientIndex=newClientIndex;
			else
				cbData->toggle->setToggle(false);
			}
		}
	else if(followClientIndex>=0)
		{
		/* Disable following: */
		followClientIndex=-1;
		Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
		}
	}

void CollaborationClient::faceClientToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	if(cbData->set)
		{
		int newClientIndex=clientListRowColumn->getChildRow(cbData->toggle);
		
		if(faceClientIndex>=0)
			{
			if(newClientIndex!=faceClientIndex)
				{
				/* Unset the previously set toggle: */
				GLMotif::ToggleButton* oldToggle=dynamic_cast<GLMotif::ToggleButton*>(clientListRowColumn->getChild(faceClientIndex*3+2));
				if(oldToggle!=0)
					oldToggle->setToggle(false);
				
				/* Face the new client: */
				faceClientIndex=newClientIndex;
				}
			}
		else if(followClientIndex>=0)
			{
			/* Unset the previously set toggle: */
			GLMotif::ToggleButton* oldToggle=dynamic_cast<GLMotif::ToggleButton*>(clientListRowColumn->getChild(followClientIndex*3+1));
			if(oldToggle!=0)
				oldToggle->setToggle(false);
			
			/* Stop following the old client: */
			followClientIndex=-1;
			
			/* Face the new client: */
			faceClientIndex=newClientIndex;
			}
		else
			{
			/* Try to enable facing: */
			if(Vrui::activateNavigationTool(reinterpret_cast<Vrui::Tool*>(this)))
				faceClientIndex=newClientIndex;
			else
				cbData->toggle->setToggle(false);
			}
		}
	else if(faceClientIndex>=0)
		{
		/* Disable facing: */
		faceClientIndex=-1;
		Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
		}
	}

void CollaborationClient::fixGlyphScalingToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	fixGlyphScaling=cbData->set;
	}

void CollaborationClient::renderRemoteEnvironmentsToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	renderRemoteEnvironments=cbData->set;
	}

void CollaborationClient::settingsDialogCloseCallback(Misc::CallbackData* cbData)
	{
	showSettingsToggle->setToggle(false);
	}

void* CollaborationClient::communicationThreadMethod(void)
	{
	/* Enable immediate cancellation of this thread: */
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	/* Run the server communication state machine until the client disconnects or there is a communication error: */
	try
		{
		bool keepGoing=true;
		while(keepGoing)
			{
			/* Wait for the next message: */
			CollaborationPipe::MessageIdType message=pipe->readMessage();
			
			/* Process the message: */
			switch(message)
				{
				case CollaborationPipe::DISCONNECT_REPLY:
					/* Let protocol plug-ins receive their own disconnect reply messages: */
					for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
						(*pIt)->receiveDisconnectReply(*pipe);
					
					/* Process higher-level protocols: */
					receiveDisconnectReply();

					/* Bail out: */
					keepGoing=false;
					break;
				
				case CollaborationPipe::CLIENT_CONNECT:
					{
					/* Create a new client state structure: */
					Client* newClient=new Client;
					
					/* Receive the new client's state: */
					newClient->clientID=pipe->read<unsigned int>();
					newClient->name=pipe->read<std::string>();
					
					#ifdef VERBOSE
					std::cout<<"CollaborationClient: Connecting new remote client "<<newClient->clientID<<", name "<<newClient->name<<std::endl;
					#endif
					
					/* Receive the list of protocols shared with the remote client, and let the plug-ins read their message payloads: */
					unsigned int numProtocols=pipe->read<unsigned int>();
					for(unsigned int i=0;i<numProtocols;++i)
						{
						/* Read the protocol index and get the protocol plug-in: */
						unsigned int protocolIndex=pipe->read<unsigned int>();
						ProtocolClient* protocol=protocols[protocolIndex];
						
						/* Let the protocol plug-in read its message payload: */
						ProtocolRemoteClientState* prcs=protocol->receiveClientConnect(*pipe);
						
						/* Store the shared protocol: */
						newClient->protocols.push_back(Client::ProtocolListEntry(protocol,prcs));
						}
					
					/* Process higher-level protocols: */
					receiveClientConnect(newClient->clientID);
					
					{
					/* Ask to have the new client added to the list: */
					Threads::Mutex::Lock clientListLock(clientListMutex);
					clientHash.setEntry(ClientHash::Entry(newClient->clientID,newClient));
					actionList.push_back(ClientListAction(ClientListAction::ADD_CLIENT,newClient->clientID,newClient));
					}
					
					/* Wake up the main program: */
					Vrui::requestUpdate();
					break;
					}
				
				case CollaborationPipe::CLIENT_DISCONNECT:
					{
					/* Read the disconnected client's ID: */
					unsigned int clientID=pipe->read<unsigned int>();
					
					{
					/* Ask to have the client removed from the list: */
					Threads::Mutex::Lock clientListLock(clientListMutex);
					clientHash.removeEntry(clientHash.findEntry(clientID));
					actionList.push_back(ClientListAction(ClientListAction::REMOVE_CLIENT,clientID,0));
					}
					
					/* Wake up the main program: */
					Vrui::requestUpdate();
					break;
					}
				
				case CollaborationPipe::SERVER_UPDATE:
					{
					/* Update the next free slot in the server state triple buffer: */
					ServerState& ss=serverState.startNewValue();
					
					/* Receive the number of clients: */
					unsigned int numClients=pipe->read<unsigned int>();
					
					/* Re-allocate the server state structure: */
					ss.resize(numClients);
					
					/* Process plug-in protocols: */
					for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
						(*pIt)->receiveServerUpdate(*pipe);
					
					/* Process higher-level protocols: */
					receiveServerUpdate();
					
					/* Receive the new state of all other connected clients: */
					for(unsigned int clientIndex=0;clientIndex<numClients;++clientIndex)
						{
						/* Read the client's ID: */
						ss.clientIDs[clientIndex]=pipe->read<unsigned int>();
						
						/* Read the client's transient state: */
						pipe->readClientState(ss.clientStates[clientIndex]);
						
						/* Find the client's state object in the client hash: */
						Client* client=clientHash.getEntry(ss.clientIDs[clientIndex]).getDest();
						
						/* Process plug-in protocols shared with the remote client: */
						for(Client::RemoteClientProtocolList::const_iterator cplIt=client->protocols.begin();cplIt!=client->protocols.end();++cplIt)
							cplIt->protocol->receiveServerUpdate(cplIt->protocolClientState,*pipe);
						
						/* Process higher-level protocols: */
						receiveServerUpdate(ss.clientIDs[clientIndex]);
						}
					
					/* Post the new server state: */
					serverState.postNewValue();
					
					/* Process higher-level protocols: */
					beforeClientUpdate();
					
					/* Send a client update packet to the server if state has been updated: */
					if(localState.lockNewValue())
						{
						Threads::Mutex::Lock pipeLock(pipe->getMutex());
						pipe->writeMessage(CollaborationPipe::CLIENT_UPDATE);
						pipe->writeClientState(localState.getLockedValue());
						
						/* Let protocol plug-ins send their own client update messages: */
						for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
							(*pIt)->sendClientUpdate(*pipe);
						
						/* Process higher-level protocols: */
						sendClientUpdate();
						
						/* Finish the message: */
						pipe->flush();
						}
					
					/* Wake up the main program: */
					Vrui::requestUpdate();
					break;
					}
				
				default:
					{
					/* Find the protocol that registered itself for this message ID: */
					if(message<messageTable.size())
						{
						ProtocolClient* protocol=messageTable[message];
						
						/* Call on the protocol plug-in to handle the message: */
						if(protocol==0||!protocol->handleMessage(message-protocol->messageIdBase,*pipe))
							{
							/* Protocol failure, bail out: */
							Misc::throwStdErr("Protocol error, received message %d",int(message));
							}
						}
					else
						{
						/* Check for higher-level protocol messages: */
						if(!handleMessage(message))
							{
							/* Protocol failure, bail out: */
							Misc::throwStdErr("Protocol error, received message %d",int(message));
							}
						}
					}
				}
			}
		}
	catch(std::runtime_error err)
		{
		/* Print error message to stderr and disconnect all remote clients: */
		std::cerr<<"CollaborationClient: Caught exception "<<err.what()<<std::endl<<std::flush;
		
		/* Kill the pipe: */
		delete pipe;
		pipe=0;
		
		{
		Threads::Mutex::Lock clientListLock(clientListMutex);
		for(ClientList::const_iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
			actionList.push_back(ClientListAction(ClientListAction::REMOVE_CLIENT,(*clIt)->clientID,0));
		}
		
		/* Create an empty server state: */
		ServerState& ss=serverState.startNewValue();
		ss.resize(0);
		serverState.postNewValue();
		
		/* Wake up the main thread: */
		Vrui::requestUpdate();
		}
	catch(...)
		{
		/* Print error message to stderr and disconnect all remote clients: */
		std::cerr<<"CollaborationClient: Caught spurious exception"<<std::endl<<std::flush;
		
		/* Kill the pipe: */
		delete pipe;
		pipe=0;
		
		{
		Threads::Mutex::Lock clientListLock(clientListMutex);
		for(ClientList::const_iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
			actionList.push_back(ClientListAction(ClientListAction::REMOVE_CLIENT,(*clIt)->clientID,0));
		}
		
		/* Create an empty server state: */
		ServerState& ss=serverState.startNewValue();
		ss.resize(0);
		serverState.postNewValue();
		
		/* Wake up the main thread: */
		Vrui::requestUpdate();
		}
	
	return 0;
	}

CollaborationClient::CollaborationClient(CollaborationClient::Configuration* sConfiguration)
	:configuration(sConfiguration!=0?sConfiguration:new Configuration),
	 protocolLoader(configuration->cfg.retrieveString("./pluginDsoNameTemplate",COLLABORATION_PLUGINDSONAMETEMPLATE)),
	 pipe(new CollaborationPipe(configuration->cfg.retrieveString("./serverHostName").c_str(),configuration->cfg.retrieveValue<int>("./serverPortId"),Vrui::openPipe())),
	 clientHash(17),
	 followClientIndex(-1),faceClientIndex(-1),
	 clientDialogPopup(0),showSettingsToggle(0),clientListRowColumn(0),
	 settingsDialogPopup(0),
	 fixGlyphScaling(false),renderRemoteEnvironments(false)
	{
	typedef std::vector<std::string> StringList;
	
	/* Get additional search paths from configuration file section and add them to the object loader: */
	StringList pluginSearchPaths=configuration->cfg.retrieveValue<StringList>("./pluginSearchPaths",StringList());
	for(StringList::const_iterator tspIt=pluginSearchPaths.begin();tspIt!=pluginSearchPaths.end();++tspIt)
		{
		/* Add the path: */
		protocolLoader.getDsoLocator().addPath(*tspIt);
		}
	
	/* Initialize the remote viewer and input device glyphs early: */
	viewerGlyph.enable(Vrui::Glyph::CROSSBALL,GLMaterial(GLMaterial::Color(0.5f,0.5f,0.5f),GLMaterial::Color(0.5f,0.5f,0.5f),25.0f));
	#ifdef COLLABORATION_SHARE_DEVICES
	inputDeviceGlyph.enable(Vrui::Glyph::CONE,GLMaterial(GLMaterial::Color(0.5f,0.5f,0.5f),GLMaterial::Color(0.5f,0.5f,0.5f),25.0f));
	#endif
	
	/* Load persistent configuration data: */
	viewerGlyph.configure(configuration->cfg,"remoteViewerGlyphType","remoteViewerGlyphMaterial");
	#ifdef COLLABORATION_SHARE_DEVICES
	inputDeviceGlyph.configure(configuration->cfg,"remoteInputDeviceGlyphType","remoteInputDeviceGlyphMaterial");
	#endif
	fixGlyphScaling=configuration->cfg.retrieveValue<bool>("./fixRemoteGlyphScaling",fixGlyphScaling);
	renderRemoteEnvironments=configuration->cfg.retrieveValue<bool>("./renderRemoteEnvironments",renderRemoteEnvironments);
	
	/* Initialize the protocol message table to have invalid entries for the collaboration pipe's own messages: */
	for(unsigned int i=0;i<CollaborationPipe::MESSAGES_END;++i)
		messageTable.push_back(0);
	
	/* Register all protocols listed in the configuration file section: */
	#ifdef VERBOSE
	std::cout<<"Registering protocols:"<<std::endl;
	#endif
	StringList protocolNames=configuration->cfg.retrieveValue<StringList>("./protocols",StringList());
	for(StringList::const_iterator pnIt=protocolNames.begin();pnIt!=protocolNames.end();++pnIt)
		{
		/* Load a protocol plug-in: */
		#ifdef VERBOSE
		std::cout<<"  "<<*pnIt<<": ";
		#endif
		try
			{
			ProtocolClient* newProtocol=protocolLoader.createObject((*pnIt+"Client").c_str());
			
			/* Initialize the protocol plug-in: */
			Misc::ConfigurationFileSection protocolSection=configuration->cfg.getSection(pnIt->c_str());
			newProtocol->initialize(*this,protocolSection);
			
			/* Append the new protocol plug-in to the list: */
			protocols.push_back(newProtocol);
			
			#ifdef VERBOSE
			std::cout<<"OK"<<std::endl;
			#endif
			}
		catch(std::runtime_error err)
			{
			/* Ignore the error and the protocol: */
			#ifdef VERBOSE
			std::cout<<"Failed due to excpetion "<<err.what()<<std::endl;
			#endif
			}
		}
	}

CollaborationClient::~CollaborationClient(void)
	{
	if(pipe!=0)
		{
		{
		Threads::Mutex::Lock pipeLock(pipe->getMutex());
		
		/* Send a disconnect message to the server: */
		pipe->writeMessage(CollaborationPipe::DISCONNECT_REQUEST);
		
		/* Process plug-in protocols: */
		for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
			(*pIt)->sendDisconnectRequest(*pipe);
		
		/* Process higher-level protocols: */
		sendDisconnectRequest();
		
		/* Finish the message: */
		pipe->flush();
		pipe->shutdown(false,true);
		}
		}
	
	/* Wait until the communication thread receives the disconnect reply and terminates: */
	communicationThread.join();
	
	delete pipe;
	
	/* Delete the user interface: */
	delete clientDialogPopup;
	delete settingsDialogPopup;
	
	/* Disconnect all remote clients: */
	for(ClientList::iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		delete *clIt;
	
	/* Delete all protocol plug-ins: */
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		if(!protocolLoader.isManaged(*pIt))
			delete *pIt;
	
	/* Delete the configuration object: */
	delete configuration;
	}

void CollaborationClient::registerProtocol(ProtocolClient* newProtocol)
	{
	/* Store the new protocol plug-in in the list: */
	protocols.push_back(newProtocol);
	}

void CollaborationClient::connect(void)
	{
	/* Retrieve the client name: */
	const char* clientNameS=getenv("HOSTNAME");
	if(clientNameS==0)
		clientNameS=getenv("HOST");
	if(clientNameS==0)
		clientNameS="Anonymous Coward";
	std::string clientName=configuration->cfg.retrieveString("./clientName",clientNameS);
	
	#ifdef VERBOSE
	std::cout<<"Connecting to server "<<""<<" under client name "<<clientName<<std::endl;
	#endif
	
	/* Send the connection initiation message: */
	{
	Threads::Mutex::Lock pipeLock(pipe->getMutex());
	
	pipe->writeMessage(CollaborationPipe::CONNECT_REQUEST);
	pipe->write<std::string>(clientName);
	
	/* Write names and message payloads of all registered protocols: */
	#ifdef VERBOSE
	std::cout<<"Requesting protocols";
	#endif
	pipe->write<unsigned int>(protocols.size());
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		{
		/* Write the protocol name: */
		pipe->write<std::string>(std::string((*pIt)->getName()));
		
		#ifdef VERBOSE
		std::cout<<' '<<(*pIt)->getName();
		#endif
		
		/* Write the protocol's message payload (protocol writes length first): */
		(*pIt)->sendConnectRequest(*pipe);
		}
	#ifdef VERBOSE
	std::cout<<std::endl;
	#endif
	
	/* Process higher-level protocols: */
	sendConnectRequest();
	
	/* Finish the message: */
	pipe->flush();
	}
	
	/* Wait for the connection reply from the server: */
	#ifdef VERBOSE
	std::cout<<"Waiting for connection reply..."<<std::flush;
	#endif
	CollaborationPipe::MessageIdType message=pipe->readMessage();
	if(message==CollaborationPipe::CONNECT_REJECT)
		{
		#ifdef VERBOSE
		std::cout<<" rejected"<<std::endl;
		#endif
		
		/* Read the list of negotiated protocols and their message payloads: */
		unsigned int numNegotiatedProtocols=pipe->read<unsigned int>();
		for(unsigned int i=0;i<numNegotiatedProtocols;++i)
			{
			/* Read the next protocol index: */
			unsigned int protocolIndex=pipe->read<unsigned int>();
			
			/* Let the negotiated protocol read its payload: */
			protocols[protocolIndex]->receiveConnectReject(*pipe);
			}
		
		/* Process higher-level protocols: */
		receiveConnectReject();
		
		/* Bail out: */
		delete pipe;
		Misc::throwStdErr("CollaborationClient::CollaborationClient: Connection refused by collaboration server");
		}
	else if(message!=CollaborationPipe::CONNECT_REPLY)
		{
		#ifdef VERBOSE
		std::cout<<" error"<<std::endl;
		#endif
		
		/* Bail out: */
		delete pipe;
		Misc::throwStdErr("CollaborationClient::CollaborationClient: Protocol error during connection initialization");
		}
	#ifdef VERBOSE
	std::cout<<" accepted"<<std::endl;
	#endif
	
	/* Read the list of negotiated protocols and their message payloads: */
	unsigned int numNegotiatedProtocols=pipe->read<unsigned int>();
	ProtocolList negotiatedProtocols;
	negotiatedProtocols.reserve(numNegotiatedProtocols);
	for(unsigned int i=0;i<numNegotiatedProtocols;++i)
		{
		/* Read the protocol index: */
		unsigned int protocolIndex=pipe->read<unsigned int>();
		
		/* Move the protocol plug-in from the original list to the negotiated list: */
		ProtocolClient* protocol=protocols[protocolIndex];
		negotiatedProtocols.push_back(protocol);
		protocols[protocolIndex]=0;
		
		/* Assign the protocol's message ID base and update the message ID table: */
		protocol->messageIdBase=pipe->read<unsigned int>();
		while(messageTable.size()<protocol->messageIdBase)
			messageTable.push_back(0);
		unsigned int numMessages=protocol->getNumMessages();
		for(unsigned int i=0;i<numMessages;++i)
			messageTable.push_back(protocol);
		
		/* Let the protocol plug-in read its message payload: */
		protocol->receiveConnectReply(*pipe);
		#ifdef VERBOSE
		std::cout<<"Negotiated protocol "<<protocol->getName()<<" with message IDs "<<protocol->messageIdBase<<" to "<<protocol->messageIdBase+numMessages-1<<std::endl;
		#endif
		}
	
	/* Delete all protocol plug-ins still in the original list: */
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		if(*pIt!=0)
			{
			(*pIt)->rejectedByServer();
			delete *pIt;
			}
	
	/* Store the list of negotiated protocols: */
	protocols=negotiatedProtocols;
	
	/* Process higher-level protocols: */
	receiveConnectReply();
	
	/* Start server communication thread: */
	communicationThread.start(this,&CollaborationClient::communicationThreadMethod);
	
	/* Create the client's user interface: */
	createClientDialog();
	createSettingsDialog();
	}

ProtocolClient* CollaborationClient::getProtocol(const char* protocolName)
	{
	ProtocolClient* result=0;
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		if(strcmp((*pIt)->getName(),protocolName)==0)
			{
			result=*pIt;
			break;
			}
	return result;
	}

void CollaborationClient::setFixGlyphScaling(bool enable)
	{
	fixGlyphScaling=enable;
	}

void CollaborationClient::setRenderRemoteEnvironments(bool enable)
	{
	renderRemoteEnvironments=enable;
	}

void CollaborationClient::showDialog(void)
	{
	/* Pop up the dialog: */
	Vrui::popupPrimaryWidget(clientDialogPopup);
	
	/* Pop up the settings dialog if it is selected: */
	if(showSettingsToggle->getToggle())
		showSettingsDialog();
	}

void CollaborationClient::hideDialog(void)
	{
	/* Pop down the settings dialog if it is selected: */
	if(showSettingsToggle->getToggle())
		Vrui::popdownPrimaryWidget(settingsDialogPopup);
	
	/* Pop down the dialog: */
	Vrui::popdownPrimaryWidget(clientDialogPopup);
	}

void CollaborationClient::frame(void)
	{
	/* Process the action list: */
	{
	Threads::Mutex::Lock clientListLock(clientListMutex);
	for(ActionList::iterator alIt=actionList.begin();alIt!=actionList.end();++alIt)
		{
		switch(alIt->action)
			{
			case ClientListAction::ADD_CLIENT:
				{
				Client* client=alIt->client;
				
				#ifdef VERBOSE
				std::cout<<"Adding new remote client "<<client->name<<", ID "<<alIt->clientID<<std::endl;
				#endif
				
				/* Store the new client state in the list: */
				clientList.push_back(client);
				
				/* Add a row for the new client to the client list dialog: */
				char textFieldName[40];
				snprintf(textFieldName,sizeof(textFieldName),"ClientName%u",alIt->clientID);
				GLMotif::TextField* clientName=new GLMotif::TextField(textFieldName,clientListRowColumn,20);
				clientName->setHAlignment(GLFont::Left);
				clientName->setString(client->name.c_str());
				
				char toggleName[40];
				snprintf(toggleName,sizeof(toggleName),"FollowClientToggle%u",alIt->clientID);
				GLMotif::ToggleButton* followClientToggle=new GLMotif::ToggleButton(toggleName,clientListRowColumn,"Follow");
				followClientToggle->setToggleType(GLMotif::ToggleButton::RADIO_BUTTON);
				followClientToggle->getValueChangedCallbacks().add(this,&CollaborationClient::followClientToggleValueChangedCallback);
				
				snprintf(toggleName,sizeof(toggleName),"FaceClientToggle%u",alIt->clientID);
				GLMotif::ToggleButton* faceClientToggle=new GLMotif::ToggleButton(toggleName,clientListRowColumn,"Face");
				faceClientToggle->setToggleType(GLMotif::ToggleButton::RADIO_BUTTON);
				faceClientToggle->getValueChangedCallbacks().add(this,&CollaborationClient::faceClientToggleValueChangedCallback);
				
				/* Process protocol plug-ins: */
				for(Client::RemoteClientProtocolList::iterator pIt=client->protocols.begin();pIt!=client->protocols.end();++pIt)
					pIt->protocol->connectClient(pIt->protocolClientState);
				
				break;
				}
			
			case ClientListAction::REMOVE_CLIENT:
				{
				/* Find the removed client's client state structure: */
				ClientList::iterator clIt;
				int clientIndex=0;
				for(clIt=clientList.begin();clIt!=clientList.end()&&(*clIt)->clientID!=alIt->clientID;++clIt,++clientIndex)
					;
				if(clIt!=clientList.end())
					{
					#ifdef VERBOSE
					std::cout<<"Removing remote client "<<(*clIt)->name<<", ID "<<(*clIt)->clientID<<std::endl;
					#endif
					
					/* Update the index of the followed/faced client: */
					if(followClientIndex==clientIndex)
						{
						/* Disable client following: */
						followClientIndex=-1;
						Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
						}
					else if(followClientIndex>clientIndex)
						--followClientIndex;
					if(faceClientIndex==clientIndex)
						{
						/* Disable client facing: */
						faceClientIndex=-1;
						Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
						}
					else if(faceClientIndex>clientIndex)
						--faceClientIndex;
					
					/* Process protocol plug-ins: */
					for(Client::RemoteClientProtocolList::iterator pIt=(*clIt)->protocols.begin();pIt!=(*clIt)->protocols.end();++pIt)
						pIt->protocol->disconnectClient(pIt->protocolClientState);
					
					/* Process higher-level protocols: */
					disconnectClient((*clIt)->clientID);
					
					/* Remove the client state structure from the client list: */
					delete *clIt;
					clientList.erase(clIt);
					
					/* Remove the client from the client list dialog: */
					clientListRowColumn->removeWidgets(clientIndex);
					}
				break;
				}
			}
		}
	
	/* Clear the action list: */
	actionList.clear();
	}
	
	/* Update the local client state structure: */
	CollaborationPipe::ClientState& ls=localState.startNewValue();
	ls.updateFromVrui();
	localState.postNewValue();
	
	/* Check for new server states: */
	if(serverState.lockNewValue())
		{
		if(followClientIndex>=0)
			{
			const ServerState& ss=serverState.getLockedValue();
			
			/* Update the navigation transformation: */
			unsigned int followClientID=clientList[followClientIndex]->clientID;
			for(unsigned int clientIndex=0;clientIndex<ss.numClients;++clientIndex)
				if(ss.clientIDs[clientIndex]==followClientID)
					{
					const CollaborationPipe::ClientState& cs=ss.clientStates[clientIndex];
					
					Vrui::NavTransform nav=Vrui::NavTransform::identity;
					nav*=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
					nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(Geometry::cross(Vrui::getForwardDirection(),Vrui::getUpDirection()),Vrui::getForwardDirection()));
					nav*=Vrui::NavTransform::scale(Vrui::getDisplaySize()/Vrui::Scalar(cs.size));
					nav*=Vrui::NavTransform::rotate(Geometry::invert(Vrui::Rotation::fromBaseVectors(Vrui::Vector(Geometry::cross(cs.forward,cs.up)),Vrui::Vector(cs.forward))));
					nav*=Vrui::NavTransform::translateToOriginFrom(Vrui::Point(cs.center));
					Vrui::setNavigationTransformation(nav);
					}
			}
		else if(faceClientIndex>=0)
			{
			const ServerState& ss=serverState.getLockedValue();
			
			/* Update the navigation transformation: */
			unsigned int faceClientID=clientList[faceClientIndex]->clientID;
			for(unsigned int clientIndex=0;clientIndex<ss.numClients;++clientIndex)
				if(ss.clientIDs[clientIndex]==faceClientID)
					{
					const CollaborationPipe::ClientState& cs=ss.clientStates[clientIndex];
					
					Vrui::NavTransform nav=Vrui::NavTransform::identity;
					nav*=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
					nav*=Vrui::NavTransform::rotate(Vrui::Rotation::rotateAxis(Vrui::getUpDirection(),Math::rad(Vrui::Scalar(180))));
					nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(Geometry::cross(Vrui::getForwardDirection(),Vrui::getUpDirection()),Vrui::getForwardDirection()));
					nav*=Vrui::NavTransform::scale(Vrui::getInchFactor()/Vrui::Scalar(cs.inchScale));
					nav*=Vrui::NavTransform::rotate(Geometry::invert(Vrui::Rotation::fromBaseVectors(Vrui::Vector(Geometry::cross(cs.forward,cs.up)),Vrui::Vector(cs.forward))));
					nav*=Vrui::NavTransform::translateToOriginFrom(Vrui::Point(cs.center));
					Vrui::setNavigationTransformation(nav);
					}
			}
		}
	
	/* Call all protocol plug-ins' frame methods: */
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		(*pIt)->frame();
	
	/* Call the client-specific protocol plug-in frame method for each remote client: */
	for(ClientList::iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		{
		/* Process all protocols shared with the remote client: */
		for(Client::RemoteClientProtocolList::iterator cpIt=(*clIt)->protocols.begin();cpIt!=(*clIt)->protocols.end();++cpIt)
			cpIt->protocol->frame(cpIt->protocolClientState);
		}
	}

void CollaborationClient::display(GLContextData& contextData) const
	{
	/* Display the currently locked server state: */
	const ServerState& ss=serverState.getLockedValue();
	for(unsigned int clientIndex=0;clientIndex<ss.numClients;++clientIndex)
		{
		const CollaborationPipe::ClientState& cs=ss.clientStates[clientIndex];
		
		if(renderRemoteEnvironments)
			{
			/* Render this client's physical environment: */
			glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
			glDisable(GL_LIGHTING);
			glLineWidth(3.0f);
			
			glBegin(GL_LINES);
			glColor3f(1.0f,0.0f,0.0f);
			glVertex(cs.center);
			glVertex(cs.center+cs.forward*(cs.size/CollaborationPipe::Scalar(Geometry::mag(cs.forward))));
			glColor3f(0.0f,1.0f,0.0f);
			glVertex(cs.center);
			glVertex(cs.center+cs.up*(cs.size/CollaborationPipe::Scalar(Geometry::mag(cs.up))));
			glEnd();
			
			glPopAttrib();
			}
		
		/* Render all viewers of this client: */
		for(unsigned int i=0;i<cs.numViewers;++i)
			if(fixGlyphScaling)
				{
				Vrui::NavTransform temp=cs.viewerStates[i];
				temp.getScaling()=Vrui::Scalar(1.0)/Vrui::getNavigationTransformation().getScaling();
				Vrui::renderGlyph(viewerGlyph,temp,contextData);
				}
			else
				Vrui::renderGlyph(viewerGlyph,cs.viewerStates[i],contextData);
		
		#ifdef COLLABORATION_SHARE_DEVICES
		/* Render all input devices of this client: */
		for(unsigned int i=0;i<cs.numInputDevices;++i)
			if(fixGlyphScaling)
				{
				Vrui::NavTransform temp=cs.inputDeviceStates[i];
				temp.getScaling()=Vrui::Scalar(1.0)/Vrui::getNavigationTransformation().getScaling();
				Vrui::renderGlyph(inputDeviceGlyph,temp,contextData);
				}
			else
				Vrui::renderGlyph(inputDeviceGlyph,cs.inputDeviceStates[i],contextData);
		#endif
		}
	
	/* Call all protocol plug-ins' GL render actions: */
	for(ProtocolList::const_iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		(*pIt)->glRenderAction(contextData);
	
	/* Call the client-specific protocol plug-in GL render actions for each remote client: */
	for(ClientList::const_iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		{
		/* Process all protocols shared with the remote client: */
		for(Client::RemoteClientProtocolList::const_iterator cpIt=(*clIt)->protocols.begin();cpIt!=(*clIt)->protocols.end();++cpIt)
			cpIt->protocol->glRenderAction(cpIt->protocolClientState,contextData);
		}
	}

void CollaborationClient::sound(ALContextData& contextData) const
	{
	/* Call all protocol plug-ins' AL render actions: */
	for(ProtocolList::const_iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		(*pIt)->alRenderAction(contextData);
	
	/* Call the client-specific protocol plug-in AL render actions for each remote client: */
	for(ClientList::const_iterator clIt=clientList.begin();clIt!=clientList.end();++clIt)
		{
		/* Process all protocols shared with the remote client: */
		for(Client::RemoteClientProtocolList::const_iterator cpIt=(*clIt)->protocols.begin();cpIt!=(*clIt)->protocols.end();++cpIt)
			cpIt->protocol->alRenderAction(cpIt->protocolClientState,contextData);
		}
	}

void CollaborationClient::sendConnectRequest(void)
	{
	}

void CollaborationClient::receiveConnectReply(void)
	{
	}

void CollaborationClient::receiveConnectReject(void)
	{
	}

void CollaborationClient::sendDisconnectRequest(void)
	{
	}

void CollaborationClient::receiveDisconnectReply(void)
	{
	}

void CollaborationClient::sendClientUpdate(void)
	{
	}

void CollaborationClient::receiveClientConnect(unsigned int clientID)
	{
	}

void CollaborationClient::receiveServerUpdate(void)
	{
	}

void CollaborationClient::receiveServerUpdate(unsigned int clientID)
	{
	}

bool CollaborationClient::handleMessage(CollaborationPipe::MessageIdType messageId)
	{
	return false;
	}

void CollaborationClient::beforeClientUpdate(void)
	{
	}

void CollaborationClient::disconnectClient(unsigned int clientID)
	{
	}

}
