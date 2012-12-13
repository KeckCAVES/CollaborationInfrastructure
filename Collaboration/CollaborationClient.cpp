/***********************************************************************
CollaborationClient - Class to support collaboration between
applications in spatially distributed (immersive) visualization
environments.
Copyright (c) 2007-2012 Oliver Kreylos

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
#include <Misc/SelfDestructPointer.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/CompoundValueCoders.h>
#include <Misc/StringMarshaller.h>
#include <Cluster/MulticastPipe.h>
#include <Cluster/OpenPipe.h>
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
#include <Vrui/Viewer.h>

namespace Collaboration {

/***************************************************
Methods of class CollaborationClient::Configuration:
***************************************************/

CollaborationClient::Configuration::Configuration(void)
	{
	/* Read the configuration file: */
	if(Vrui::isMaster())
		{
		/* Read the configuration file: */
		configFile.load(COLLABORATION_CONFIGFILENAME);
		
		if(Vrui::getMainPipe()!=0)
			{
			/* Send the configuration file to the slaves: */
			configFile.writeToPipe(*Vrui::getMainPipe());
			}
		}
	else
		{
		/* Receive the configuration file from the master: */
		configFile.readFromPipe(*Vrui::getMainPipe());
		}
	
	/* Get the root section: */
	cfg=configFile.getSection("/CollaborationClient");
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

/*******************************************************
Methods of class CollaborationClient::RemoteClientState:
*******************************************************/

CollaborationClient::RemoteClientState::RemoteClientState(void)
	:clientID(0),
	 updateMask(ClientState::NO_CHANGE),
	 nameTextField(0),followToggle(0),faceToggle(0)
	{
	}

CollaborationClient::RemoteClientState::~RemoteClientState(void)
	{
	/* Delete all protocol client state objects: */
	for(RemoteClientProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		delete pIt->protocolClientState;
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

void CollaborationClient::followClient(const CollaborationProtocol::ClientState& cs) const
	{
	/* Update the navigation transformation: */
	Vrui::NavTransform nav=Vrui::NavTransform::identity;
	nav*=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(Geometry::cross(Vrui::getForwardDirection(),Vrui::getUpDirection()),Vrui::getForwardDirection()));
	nav*=Vrui::NavTransform::scale(Vrui::getDisplaySize());
	nav*=Vrui::NavTransform::scale(Vrui::Scalar(1)/Vrui::Scalar(cs.displaySize));
	nav*=Vrui::NavTransform::rotate(Geometry::invert(Vrui::Rotation::fromBaseVectors(Vrui::Vector(Geometry::cross(cs.forward,cs.up)),Vrui::Vector(cs.forward))));
	nav*=Vrui::NavTransform::translateToOriginFrom(Vrui::Point(cs.displayCenter));
	nav*=cs.navTransform;
	Vrui::setNavigationTransformation(nav);
	}

void CollaborationClient::faceClient(const CollaborationProtocol::ClientState& cs) const
	{
	/* Update the navigation transformation: */
	Vrui::NavTransform nav=Vrui::NavTransform::identity;
	nav*=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::rotateAxis(Vrui::getUpDirection(),Math::rad(Vrui::Scalar(180))));
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(Geometry::cross(Vrui::getForwardDirection(),Vrui::getUpDirection()),Vrui::getForwardDirection()));
	nav*=Vrui::NavTransform::scale(Vrui::getInchFactor());
	nav*=Vrui::NavTransform::scale(Vrui::Scalar(1)/Vrui::Scalar(cs.inchFactor));
	nav*=Vrui::NavTransform::rotate(Geometry::invert(Vrui::Rotation::fromBaseVectors(Vrui::Vector(Geometry::cross(cs.forward,cs.up)),Vrui::Vector(cs.forward))));
	nav*=Vrui::NavTransform::translateToOriginFrom(Vrui::Point(cs.displayCenter));
	nav*=cs.navTransform;
	Vrui::setNavigationTransformation(nav);
	}

void CollaborationClient::followClientToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData,const unsigned int& clientID)
	{
	if(cbData->set)
		{
		if(followClientID!=0)
			{
			if(clientID!=followClientID)
				{
				Threads::Mutex::Lock clientMapLock(clientMapMutex);
				
				/* Unset the previously set toggle: */
				RemoteClientState* oldFollowClient=remoteClientMap.getEntry(followClientID).getDest();
				oldFollowClient->followToggle->setToggle(false);
				
				/* Follow the new client: */
				followClientID=clientID;
				}
			}
		else if(faceClientID!=0)
			{
			Threads::Mutex::Lock clientMapLock(clientMapMutex);
			
			/* Unset the previously set toggle: */
			RemoteClientState* oldFaceClient=remoteClientMap.getEntry(faceClientID).getDest();
			oldFaceClient->faceToggle->setToggle(false);
			
			/* Stop facing the old client: */
			faceClientID=0;
			
			/* Follow the new client: */
			followClientID=clientID;
			}
		else
			{
			/* Try to enable following: */
			if(Vrui::activateNavigationTool(reinterpret_cast<Vrui::Tool*>(this)))
				followClientID=clientID;
			else
				cbData->toggle->setToggle(false);
			}
		}
	else if(followClientID!=0)
		{
		/* Disable following: */
		followClientID=0;
		Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
		}
	
	if(followClientID!=0)
		{
		/* Start facing the client: */
		Threads::Mutex::Lock clientMapLock(clientMapMutex);
		followClient(remoteClientMap.getEntry(followClientID).getDest()->state.getLockedValue());
		}
	}

void CollaborationClient::faceClientToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData,const unsigned int& clientID)
	{
	if(cbData->set)
		{
		if(faceClientID!=0)
			{
			if(clientID!=faceClientID)
				{
				Threads::Mutex::Lock clientMapLock(clientMapMutex);
				
				/* Unset the previously set toggle: */
				RemoteClientState* oldFaceClient=remoteClientMap.getEntry(faceClientID).getDest();
				oldFaceClient->faceToggle->setToggle(false);
				
				/* Face the new client: */
				faceClientID=clientID;
				}
			}
		else if(followClientID!=0)
			{
			Threads::Mutex::Lock clientMapLock(clientMapMutex);
			
			/* Unset the previously set toggle: */
			RemoteClientState* oldFollowClient=remoteClientMap.getEntry(followClientID).getDest();
			oldFollowClient->followToggle->setToggle(false);
			
			/* Stop following the old client: */
			followClientID=0;
			
			/* Face the new client: */
			faceClientID=clientID;
			}
		else
			{
			/* Try to enable facing: */
			if(Vrui::activateNavigationTool(reinterpret_cast<Vrui::Tool*>(this)))
				faceClientID=clientID;
			else
				cbData->toggle->setToggle(false);
			}
		}
	else if(faceClientID!=0)
		{
		/* Disable facing: */
		faceClientID=0;
		Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
		}
	
	if(faceClientID!=0)
		{
		/* Start facing the client: */
		Threads::Mutex::Lock clientMapLock(clientMapMutex);
		faceClient(remoteClientMap.getEntry(faceClientID).getDest()->state.getLockedValue());
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
	// Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	/* Create a private client map: */
	RemoteClientMap myClientMap(17);
	
	enum State // Possible states of the client communication state machine
		{
		START,CONNECTED,FINISH
		};
	
	/* Run the server communication state machine until the client disconnects or there is a communication error: */
	try
		{
		State state=CONNECTED;
		while(state!=FINISH)
			{
			/* Wait for the next message: */
			MessageIdType message=readMessage(*pipe);
			
			/* Process the message: */
			switch(message)
				{
				case DISCONNECT_REPLY:
					/* Let protocol plug-ins receive their own disconnect reply messages: */
					for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
						(*pIt)->receiveDisconnectReply(*pipe);
					
					/* Process higher-level protocols: */
					receiveDisconnectReply();
					
					/* Bail out: */
					state=FINISH;
					
					/* Wake up the main program: */
					Vrui::requestUpdate();
					break;
				
				case CLIENT_CONNECT:
					{
					#ifdef VERBOSE
					std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Received CLIENT_CONNECT message"<<std::endl;
					#endif
					
					/* Create a new client state structure: */
					Misc::SelfDestructPointer<RemoteClientState> newClient(new RemoteClientState);
					
					/* Receive the new client's state: */
					newClient->clientID=pipe->read<Card>();
					ClientState& newState=newClient->state.startNewValue();
					readClientState(newState,*pipe);
					std::string newClientName=newState.clientName;
					newClient->state.postNewValue();
					
					/* Receive the list of protocols shared with the remote client, and let the plug-ins read their message payloads: */
					unsigned int numProtocols=pipe->read<Card>();
					for(unsigned int i=0;i<numProtocols;++i)
						{
						/* Read the protocol index and get the protocol plug-in: */
						unsigned int protocolIndex=pipe->read<Card>();
						ProtocolClient* protocol=protocols[protocolIndex];
						
						/* Let the protocol plug-in read its message payload: */
						ProtocolRemoteClientState* prcs=protocol->receiveClientConnect(*pipe);
						
						/* Store the shared protocol: */
						newClient->protocols.push_back(RemoteClientState::ProtocolListEntry(protocol,prcs));
						}
					
					/* Process higher-level protocols: */
					receiveClientConnect(newClient->clientID);
					
					/* Make the new client permanent: */
					RemoteClientState* rcs=newClient.releaseTarget();
					
					/* Add the client to the private map: */
					myClientMap[rcs->clientID]=rcs;
					
					{
					/* Ask to have the new client added to the list: */
					Threads::Mutex::Lock actionListLock(actionListMutex);
					actionList.push_back(ClientListAction(ClientListAction::ADD_CLIENT,rcs->clientID,rcs));
					}
					
					/* Wake up the main program: */
					Vrui::requestUpdate();
					break;
					}
				
				case CLIENT_DISCONNECT:
					{
					#ifdef VERBOSE
					std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Received CLIENT_DISCONNECT message"<<std::endl;
					#endif
					
					/* Read the disconnected client's ID: */
					unsigned int clientID=pipe->read<Card>();
					
					/* Remove the client from the private map: */
					myClientMap.removeEntry(clientID);
					
					{
					/* Ask to have the client removed from the list: */
					Threads::Mutex::Lock actionListLock(actionListMutex);
					actionList.push_back(ClientListAction(ClientListAction::REMOVE_CLIENT,clientID,0));
					}
					
					/* Wake up the main program: */
					Vrui::requestUpdate();
					break;
					}
				
				case SERVER_UPDATE:
					{
					/*************************************************************
					Process the server's state update packet:
					*************************************************************/
					
					bool mustRefresh=false;
					
					/* Receive the number of clients in this update packet: */
					unsigned int numClients=pipe->read<Card>();
					
					/* Process plug-in protocols: */
					for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
						mustRefresh=(*pIt)->receiveServerUpdate(*pipe)||mustRefresh;
					
					/* Process higher-level protocols: */
					mustRefresh=receiveServerUpdate()||mustRefresh;
					
					/* Receive the new state of all other connected clients: */
					for(unsigned int clientIndex=0;clientIndex<numClients;++clientIndex)
						{
						/* Find the client's state object in the client map: */
						unsigned int clientID=pipe->read<Card>();
						RemoteClientState* client=myClientMap.getEntry(clientID).getDest();
						
						/* Read the client's transient state: */
						ClientState& newState=client->state.startNewValue();
						newState=client->state.getMostRecentValue();
						newState.updateMask=ClientState::NO_CHANGE;
						readClientState(newState,*pipe);
						client->updateMask|=newState.updateMask;
						mustRefresh=mustRefresh||newState.updateMask!=ClientState::NO_CHANGE;
						client->state.postNewValue();
						
						/* Process plug-in protocols shared with the remote client: */
						for(RemoteClientState::RemoteClientProtocolList::const_iterator cplIt=client->protocols.begin();cplIt!=client->protocols.end();++cplIt)
							mustRefresh=cplIt->protocol->receiveServerUpdate(cplIt->protocolClientState,*pipe)||mustRefresh;
						
						/* Process higher-level protocols: */
						mustRefresh=receiveServerUpdate(clientID)||mustRefresh;
						}
					
					/* Wake up the main program if anything changed: */
					if(mustRefresh)
						Vrui::requestUpdate();
					
					/*************************************************************
					Send a client update packet in response to the server update:
					*************************************************************/
					
					/* Let protocol plug-ins insert their own messages before the main update message: */
					for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
						(*pIt)->beforeClientUpdate(*pipe);
					
					/* Process higher-level protocols: */
					beforeClientUpdate();
					
					{
					Threads::Mutex::Lock pipeLock(pipeMutex);
					writeMessage(CLIENT_UPDATE,*pipe);
					
					/* Send the local client state: */
					{
					Threads::Spinlock::Lock clientStateLock(clientStateMutex);
					writeClientState(clientState.updateMask,clientState,*pipe);
					clientState.updateMask=ClientState::NO_CHANGE;
					}
					
					/* Let protocol plug-ins send their own client update messages: */
					for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
						(*pIt)->sendClientUpdate(*pipe);
					
					/* Process higher-level protocols: */
					sendClientUpdate();
					
					/* Finish the message: */
					pipe->flush();
					}
					
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
		/* Disconnect all remote clients: */
		std::cerr<<"Node "<<Vrui::getNodeIndex()<<": "<<"CollaborationClient: Caught exception "<<err.what()<<std::endl<<std::flush;
		
		/* Indicate a disconnect: */
		disconnect=true;
		
		/* Wake up the main thread: */
		Vrui::requestUpdate();
		}
	
	return 0;
	}

void CollaborationClient::updateClientState(void)
	{
	/* Update the physical environment: */
	bool environmentChanged=false;
	Scalar inchFactor=Scalar(Vrui::getInchFactor());
	environmentChanged=environmentChanged||clientState.inchFactor!=inchFactor;
	clientState.inchFactor=inchFactor;
	Point displayCenter=Point(Vrui::getDisplayCenter());
	environmentChanged=environmentChanged||clientState.displayCenter!=displayCenter;
	clientState.displayCenter=displayCenter;
	Scalar displaySize=Scalar(Vrui::getDisplaySize());
	environmentChanged=environmentChanged||clientState.displaySize!=displaySize;
	clientState.displaySize=displaySize;
	Vector forward=Vector(Vrui::getForwardDirection());
	environmentChanged=environmentChanged||clientState.forward!=forward;
	clientState.forward=forward;
	Vector up=Vector(Vrui::getUpDirection());
	environmentChanged=environmentChanged||clientState.up!=up;
	clientState.up=up;
	Plane floorPlane=Plane(Vrui::getFloorPlane());
	environmentChanged=environmentChanged||clientState.floorPlane!=floorPlane;
	clientState.floorPlane=floorPlane;
	if(environmentChanged)
		clientState.updateMask|=ClientState::ENVIRONMENT;
	
	/* Update the positions/orientations of all viewers: */
	bool viewersChanged=clientState.resize(Vrui::getNumViewers());
	for(unsigned int i=0;i<clientState.numViewers;++i)
		{
		ONTransform viewerState=ONTransform(Vrui::getViewer(i)->getHeadTransformation());
		viewersChanged=viewersChanged||clientState.viewerStates[i]!=viewerState;
		clientState.viewerStates[i]=viewerState;
		}
	if(viewersChanged)
		clientState.updateMask|=ClientState::VIEWER;
	
	/* Update the navigation transformation: */
	OGTransform navTransform=OGTransform(Vrui::getNavigationTransformation());
	if(clientState.navTransform!=navTransform)
		{
		clientState.navTransform=navTransform;
		clientState.updateMask|=ClientState::NAVTRANSFORM;
		}
	}

CollaborationClient::CollaborationClient(CollaborationClient::Configuration* sConfiguration)
	:configuration(sConfiguration!=0?sConfiguration:new Configuration),
	 protocolLoader(configuration->cfg.retrieveString("./pluginDsoNameTemplate",COLLABORATION_PLUGINDSONAMETEMPLATE)),
	 disconnect(false),
	 remoteClientMap(17),protocolClientMap(31),
	 followClientID(0),faceClientID(0),
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
	
	/* Retrieve the client's display name: */
	if(Vrui::isMaster())
		{
		const char* clientNameS=getenv("HOSTNAME");
		if(clientNameS==0)
			clientNameS=getenv("HOST");
		if(clientNameS==0)
			clientNameS="Anonymous Coward";
		std::string clientName=configuration->cfg.retrieveString("./clientName",clientNameS);
		setClientName(clientName);
		
		if(Vrui::getMainPipe()!=0)
			{
			/* Send the client name to the slaves: */
			Misc::writeCppString(clientName,*Vrui::getMainPipe());
			Vrui::getMainPipe()->flush();
			}
		}
	else
		{
		/* Receive the client name from the master: */
		setClientName(Misc::readCppString(*Vrui::getMainPipe()));
		}
	
	/* Initialize the remote viewer and input device glyphs early: */
	viewerGlyph.enable(Vrui::Glyph::CROSSBALL,GLMaterial(GLMaterial::Color(0.5f,0.5f,0.5f),GLMaterial::Color(0.5f,0.5f,0.5f),25.0f));
	
	/* Load persistent configuration data: */
	viewerGlyph.configure(configuration->cfg,"remoteViewerGlyphType","remoteViewerGlyphMaterial");
	fixGlyphScaling=configuration->cfg.retrieveValue<bool>("./fixRemoteGlyphScaling",fixGlyphScaling);
	renderRemoteEnvironments=configuration->cfg.retrieveValue<bool>("./renderRemoteEnvironments",renderRemoteEnvironments);
	
	/* Initialize the protocol message table to have invalid entries for the collaboration pipe's own messages: */
	for(unsigned int i=0;i<MESSAGES_END;++i)
		messageTable.push_back(0);
	
	/* Register all protocols listed in the configuration file section: */
	#ifdef VERBOSE
	std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Registering protocols:"<<std::endl;
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
			newProtocol->initialize(this,protocolSection);
			
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
			std::cout<<"Failed due to exception "<<err.what()<<std::endl;
			#endif
			}
		}
	}

CollaborationClient::~CollaborationClient(void)
	{
	if(pipe!=0)
		{
		{
		Threads::Mutex::Lock pipeLock(pipeMutex);
		
		/* Send a disconnect message to the server: */
		writeMessage(DISCONNECT_REQUEST,*pipe);
		
		/* Process plug-in protocols: */
		for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
			(*pIt)->sendDisconnectRequest(*pipe);
		
		/* Process higher-level protocols: */
		sendDisconnectRequest();
		
		/* Finish the message: */
		pipe->flush();
		}
		
		/* Wait until the communication thread receives the disconnect reply and terminates: */
		communicationThread.join();
		
		/* Close the pipe: */
		pipe=0;
		}
	
	/* Disconnect all remote clients: */
	for(RemoteClientMap::Iterator cmIt=remoteClientMap.begin();!cmIt.isFinished();++cmIt)
		{
		RemoteClientState* client=cmIt->getDest();
		
		#ifdef VERBOSE
		std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Removing remote client "<<client->state.getLockedValue().clientName<<", ID "<<client->clientID<<std::endl;
		#endif
		
		/* Update the index of the followed/faced client: */
		if(followClientID==client->clientID)
			{
			/* Disable client following: */
			followClientID=0;
			Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
			}
		if(faceClientID==client->clientID)
			{
			/* Disable client facing: */
			faceClientID=0;
			Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
			}
		
		/* Process protocol plug-ins: */
		for(RemoteClientState::RemoteClientProtocolList::iterator pIt=client->protocols.begin();pIt!=client->protocols.end();++pIt)
			{
			/* Let the protocol client do its disconnection protocol: */
			pIt->protocol->disconnectClient(pIt->protocolClientState);
			}
		
		/* Process higher-level protocols: */
		disconnectClient(client->clientID);
		
		/* Remove the client from the client list dialog: */
		clientListRowColumn->removeWidgets(clientListRowColumn->getChildRow(client->nameTextField));
		
		/* Destroy the client state structure: */
		delete client;
		}
	
	/* Delete the user interface: */
	delete clientDialogPopup;
	delete settingsDialogPopup;
	
	/* Delete all protocol plug-ins: */
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		if(!protocolLoader.isManaged(*pIt))
			delete *pIt;
	
	/* Delete the configuration object: */
	delete configuration;
	}

void CollaborationClient::setClientName(std::string newClientName)
	{
	Threads::Spinlock::Lock clientStateLock(clientStateMutex);
	
	/* Update the client name: */
	clientState.clientName=newClientName;
	clientState.updateMask|=ClientState::CLIENTNAME;
	}

void CollaborationClient::registerProtocol(ProtocolClient* newProtocol)
	{
	/* Store the new protocol plug-in in the list: */
	protocols.push_back(newProtocol);
	}

void CollaborationClient::connect(void)
	{
	{
	Threads::Mutex::Lock pipeLock(pipeMutex);
	
	/* Connect to the remote collaboration server: */
	#ifdef VERBOSE
	std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Connecting to server "<<configuration->cfg.retrieveString("./serverHostName")<<" under client name "<<clientState.clientName<<std::endl;
	#endif
	pipe=Cluster::openTCPPipe(Vrui::getClusterMultiplexer(),configuration->cfg.retrieveString("./serverHostName").c_str(),configuration->cfg.retrieveValue<int>("./serverPortId"));
	pipe->negotiateEndianness();
	
	/* Decouple the writing side of the pipe if it is shared across a local cluster: */
	Cluster::ClusterPipe* cPipe=dynamic_cast<Cluster::ClusterPipe*>(pipe.getPointer());
	if(cPipe!=0)
		{
		pipe->flush();
		cPipe->couple(true,false);
		}
	
	/* Send the connection initiation message: */
	writeMessage(CONNECT_REQUEST,*pipe);
	
	/* Write the initial client state: */
	{
	Threads::Spinlock::Lock clientStateLock(clientStateMutex);
	updateClientState();
	writeClientState(ClientState::FULL_UPDATE,clientState,*pipe);
	clientState.updateMask=ClientState::NO_CHANGE;
	}
	
	/* Write names and message payloads of all registered protocols: */
	#ifdef VERBOSE
	std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Requesting protocols";
	#endif
	pipe->write<Card>(protocols.size());
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		{
		/* Write the protocol name: */
		write(std::string((*pIt)->getName()),*pipe);
		
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
	std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Waiting for connection reply..."<<std::flush;
	#endif
	MessageIdType message=readMessage(*pipe);
	if(message==CONNECT_REJECT)
		{
		#ifdef VERBOSE
		std::cout<<" rejected"<<std::endl;
		#endif
		
		/* Read the list of negotiated protocols and their message payloads: */
		unsigned int numNegotiatedProtocols=pipe->read<Card>();
		for(unsigned int i=0;i<numNegotiatedProtocols;++i)
			{
			/* Read the next protocol index: */
			unsigned int protocolIndex=pipe->read<Card>();
			
			/* Let the negotiated protocol read its payload: */
			protocols[protocolIndex]->receiveConnectReject(*pipe);
			}
		
		/* Process higher-level protocols: */
		receiveConnectReject();
		
		/* Bail out: */
		pipe=0;
		Misc::throwStdErr("CollaborationClient::CollaborationClient: Connection refused by collaboration server");
		}
	else if(message!=CONNECT_REPLY)
		{
		#ifdef VERBOSE
		std::cout<<" error"<<std::endl;
		#endif
		
		/* Bail out: */
		pipe=0;
		Misc::throwStdErr("CollaborationClient::CollaborationClient: Protocol error during connection initialization");
		}
	#ifdef VERBOSE
	std::cout<<" accepted"<<std::endl;
	#endif
	
	/* Read the list of negotiated protocols and their message payloads: */
	unsigned int numNegotiatedProtocols=pipe->read<Card>();
	ProtocolList negotiatedProtocols;
	negotiatedProtocols.reserve(numNegotiatedProtocols);
	for(unsigned int i=0;i<numNegotiatedProtocols;++i)
		{
		/* Read the protocol index: */
		unsigned int protocolIndex=pipe->read<Card>();
		
		/* Move the protocol plug-in from the original list to the negotiated list: */
		ProtocolClient* protocol=protocols[protocolIndex];
		negotiatedProtocols.push_back(protocol);
		protocols[protocolIndex]=0;
		
		/* Assign the protocol's message ID base and update the message ID table: */
		protocol->messageIdBase=pipe->read<Card>();
		while(messageTable.size()<protocol->messageIdBase)
			messageTable.push_back(0);
		unsigned int numMessages=protocol->getNumMessages();
		for(unsigned int i=0;i<numMessages;++i)
			messageTable.push_back(protocol);
		
		/* Let the protocol plug-in read its message payload: */
		protocol->receiveConnectReply(*pipe);
		#ifdef VERBOSE
		if(numMessages>0)
			std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Negotiated protocol "<<protocol->getName()<<" with message IDs "<<protocol->messageIdBase<<" to "<<protocol->messageIdBase+numMessages-1<<std::endl;
		else
			std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Negotiated protocol "<<protocol->getName()<<std::endl;
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
	if(clientDialogPopup==0)
		return;
	
	/* Pop up the dialog: */
	Vrui::popupPrimaryWidget(clientDialogPopup);
	
	/* Pop up the settings dialog if it is selected: */
	if(showSettingsToggle->getToggle())
		showSettingsDialog();
	}

void CollaborationClient::hideDialog(void)
	{
	if(clientDialogPopup==0)
		return;
	
	/* Pop down the settings dialog if it is selected: */
	if(showSettingsToggle->getToggle())
		Vrui::popdownPrimaryWidget(settingsDialogPopup);
	
	/* Pop down the dialog: */
	Vrui::popdownPrimaryWidget(clientDialogPopup);
	}

void CollaborationClient::frame(void)
	{
	/* Bail out if not connected to the server: */
	if(pipe==0)
		return;
	
	/* Check if the server communication thread encountered an error: */
	if(Vrui::getMainPipe()!=0)
		{
		if(Vrui::isMaster())
			{
			Vrui::getMainPipe()->write<char>(disconnect?1:0);
			Vrui::getMainPipe()->flush();
			}
		else
			disconnect=Vrui::getMainPipe()->read<char>()!=0;
		}
	if(disconnect)
		{
		/* Shut down the server communication thread on the slaves nodes: */
		if(!Vrui::isMaster())
			{
			communicationThread.cancel();
			communicationThread.join();
			}
		
		/* Disconnect all remote clients: */
		{
		Threads::Mutex::Lock actionListLock(actionListMutex);
		for(RemoteClientMap::Iterator cmIt=remoteClientMap.begin();!cmIt.isFinished();++cmIt)
			actionList.push_back(ClientListAction(ClientListAction::REMOVE_CLIENT,cmIt->getSource(),0));
		}
		
		/* Close the communication pipe: */
		pipe=0;
		disconnect=false;
		
		/* Show an error message: */
		Vrui::showErrorMessage("CollaborationClient","Disconnected from collaboration server due to communication error");
		}
	
	/* Lock the client map: */
	Threads::Mutex::Lock clientMapLock(clientMapMutex);
	
	/* Process the action list: */
	{
	Threads::Mutex::Lock actionListLock(actionListMutex);
	for(ActionList::iterator alIt=actionList.begin();alIt!=actionList.end();++alIt)
		{
		switch(alIt->action)
			{
			case ClientListAction::ADD_CLIENT:
				{
				RemoteClientState* client=alIt->client;
				client->state.lockNewValue();
				
				#ifdef VERBOSE
				std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Adding new remote client "<<client->state.getLockedValue().clientName<<", ID "<<alIt->clientID<<std::endl;
				#endif
				
				/* Store the new client state in the client map: */
				remoteClientMap[alIt->clientID]=client;
				
				/* Add a row for the new client to the client list dialog: */
				char widgetName[40];
				snprintf(widgetName,sizeof(widgetName),"ClientName%u",alIt->clientID);
				client->nameTextField=new GLMotif::TextField(widgetName,clientListRowColumn,20);
				client->nameTextField->setHAlignment(GLFont::Left);
				client->nameTextField->setString(client->state.getLockedValue().clientName.c_str());
				
				snprintf(widgetName,sizeof(widgetName),"FollowClientToggle%u",alIt->clientID);
				client->followToggle=new GLMotif::ToggleButton(widgetName,clientListRowColumn,"Follow");
				client->followToggle->setToggleType(GLMotif::ToggleButton::RADIO_BUTTON);
				client->followToggle->getValueChangedCallbacks().add(this,&CollaborationClient::followClientToggleValueChangedCallback,client->clientID);
				
				snprintf(widgetName,sizeof(widgetName),"FaceClientToggle%u",alIt->clientID);
				client->faceToggle=new GLMotif::ToggleButton(widgetName,clientListRowColumn,"Face");
				client->faceToggle->setToggleType(GLMotif::ToggleButton::RADIO_BUTTON);
				client->faceToggle->getValueChangedCallbacks().add(this,&CollaborationClient::faceClientToggleValueChangedCallback,client->clientID);
				
				/* Process protocol plug-ins: */
				for(RemoteClientState::RemoteClientProtocolList::iterator pIt=client->protocols.begin();pIt!=client->protocols.end();++pIt)
					{
					/* Add the protocol client state to the remote client look-up map: */
					protocolClientMap[pIt->protocolClientState]=client;
					
					/* Let the protocol client do its connection protocol: */
					pIt->protocol->connectClient(pIt->protocolClientState);
					}
				
				break;
				}
			
			case ClientListAction::REMOVE_CLIENT:
				{
				RemoteClientMap::Iterator cmIt=remoteClientMap.findEntry(alIt->clientID);
				if(!cmIt.isFinished())
					{
					RemoteClientState* client=cmIt->getDest();
					
					#ifdef VERBOSE
					std::cout<<"Node "<<Vrui::getNodeIndex()<<": "<<"Removing remote client "<<client->state.getLockedValue().clientName<<", ID "<<client->clientID<<std::endl;
					#endif
					
					/* Update the index of the followed/faced client: */
					if(followClientID==client->clientID)
						{
						/* Disable client following: */
						followClientID=0;
						Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
						}
					if(faceClientID==client->clientID)
						{
						/* Disable client facing: */
						faceClientID=0;
						Vrui::deactivateNavigationTool(reinterpret_cast<Vrui::Tool*>(this));
						}
					
					/* Process protocol plug-ins: */
					for(RemoteClientState::RemoteClientProtocolList::iterator pIt=client->protocols.begin();pIt!=client->protocols.end();++pIt)
						{
						/* Let the protocol client do its disconnection protocol: */
						pIt->protocol->disconnectClient(pIt->protocolClientState);
						
						/* Remove the protocol client state from the remote client look-up map: */
						protocolClientMap.removeEntry(pIt->protocolClientState);
						}
					
					/* Process higher-level protocols: */
					disconnectClient(client->clientID);
					
					/* Remove the client from the client list dialog: */
					clientListRowColumn->removeWidgets(clientListRowColumn->getChildRow(client->nameTextField));
					
					/* Destroy the client state structure and remove it from the client map: */
					delete client;
					remoteClientMap.removeEntry(cmIt);
					}
				
				break;
				}
			}
		}
	
	/* Clear the action list: */
	actionList.clear();
	}
	
	/* Update the local client state structure: */
	{
	Threads::Spinlock::Lock clientStateLock(clientStateMutex);
	updateClientState();
	}
	
	/* Update all remote clients' states: */
	for(RemoteClientMap::Iterator cmIt=remoteClientMap.begin();!cmIt.isFinished();++cmIt)
		{
		RemoteClientState* client=cmIt->getDest();
		if(client->state.lockNewValue())
			{
			const ClientState& cs=client->state.getLockedValue();
			if(client->updateMask&ClientState::CLIENTNAME)
				client->nameTextField->setString(cs.clientName.c_str());
			if(client->updateMask&(ClientState::ENVIRONMENT|ClientState::NAVTRANSFORM))
				{
				if(client->clientID==followClientID)
					{
					/* Follow this client: */
					followClient(cs);
					}
				if(client->clientID==faceClientID)
					{
					/* Face this client: */
					faceClient(cs);
					}
				}
			client->updateMask=ClientState::NO_CHANGE;
			}
		}
	
	/* Call all protocol plug-ins' frame methods: */
	for(ProtocolList::iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		(*pIt)->frame();
	
	/* Call the client-specific protocol plug-in frame method for each remote client: */
	for(RemoteClientMap::Iterator cmIt=remoteClientMap.begin();!cmIt.isFinished();++cmIt)
		{
		/* Process all protocols shared with the remote client: */
		RemoteClientState* client=cmIt->getDest();
		for(RemoteClientState::RemoteClientProtocolList::iterator cpIt=client->protocols.begin();cpIt!=client->protocols.end();++cpIt)
			cpIt->protocol->frame(cpIt->protocolClientState);
		}
	}

void CollaborationClient::display(GLContextData& contextData) const
	{
	/* Display all client states: */
	for(RemoteClientMap::ConstIterator cmIt=remoteClientMap.begin();!cmIt.isFinished();++cmIt)
		{
		const RemoteClientState* client=cmIt->getDest();
		const ClientState& cs=client->state.getLockedValue();
		
		/* Go to the client's navigational space: */
		glPushMatrix();
		glMultMatrix(Geometry::invert(cs.navTransform));
		
		if(renderRemoteEnvironments)
			{
			/* Render this client's physical environment: */
			glPushAttrib(GL_ENABLE_BIT|GL_LINE_BIT);
			glDisable(GL_LIGHTING);
			glLineWidth(3.0f);
			
			glBegin(GL_LINES);
			glColor3f(1.0f,0.0f,0.0f);
			glVertex(cs.displayCenter);
			glVertex(cs.displayCenter+cs.forward*(cs.displaySize/Scalar(Geometry::mag(cs.forward))));
			glColor3f(0.0f,1.0f,0.0f);
			glVertex(cs.displayCenter);
			glVertex(cs.displayCenter+cs.up*(cs.displaySize/Scalar(Geometry::mag(cs.up))));
			glEnd();
			
			glPopAttrib();
			}
		
		/* Render all viewers of this client: */
		for(unsigned int i=0;i<cs.numViewers;++i)
			if(fixGlyphScaling)
				{
				Vrui::NavTransform temp=cs.viewerStates[i];
				temp.getScaling()=cs.navTransform.getScaling()/Vrui::getNavigationTransformation().getScaling();
				Vrui::renderGlyph(viewerGlyph,temp,contextData);
				}
			else
				Vrui::renderGlyph(viewerGlyph,cs.viewerStates[i],contextData);
		
		glPopMatrix();
		}
	
	/* Call all protocol plug-ins' GL render actions: */
	for(ProtocolList::const_iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		(*pIt)->glRenderAction(contextData);
	
	/* Call the client-specific protocol plug-in GL render actions for each remote client: */
	for(RemoteClientMap::ConstIterator cmIt=remoteClientMap.begin();!cmIt.isFinished();++cmIt)
		{
		/* Process all protocols shared with the remote client: */
		const RemoteClientState* client=cmIt->getDest();
		for(RemoteClientState::RemoteClientProtocolList::const_iterator cpIt=client->protocols.begin();cpIt!=client->protocols.end();++cpIt)
			cpIt->protocol->glRenderAction(cpIt->protocolClientState,contextData);
		}
	}

void CollaborationClient::sound(ALContextData& contextData) const
	{
	/* Call all protocol plug-ins' AL render actions: */
	for(ProtocolList::const_iterator pIt=protocols.begin();pIt!=protocols.end();++pIt)
		(*pIt)->alRenderAction(contextData);
	
	/* Call the client-specific protocol plug-in AL render actions for each remote client: */
	for(RemoteClientMap::ConstIterator cmIt=remoteClientMap.begin();!cmIt.isFinished();++cmIt)
		{
		/* Process all protocols shared with the remote client: */
		const RemoteClientState* client=cmIt->getDest();
		for(RemoteClientState::RemoteClientProtocolList::const_iterator cpIt=client->protocols.begin();cpIt!=client->protocols.end();++cpIt)
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

bool CollaborationClient::receiveServerUpdate(void)
	{
	return false;
	}

bool CollaborationClient::receiveServerUpdate(unsigned int clientID)
	{
	return false;
	}

bool CollaborationClient::handleMessage(Protocol::MessageIdType messageId)
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
