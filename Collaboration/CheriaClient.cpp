/***********************************************************************
CheriaClient - Client object to implement the Cheria input device
distribution protocol.
Copyright (c) 2010-2013 Oliver Kreylos

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

#define DEBUGGING 0

#include <Collaboration/CheriaClient.h>

#include <stdexcept>
#if DEBUGGING
#include <iostream>
#endif
#include <Misc/ThrowStdErr.h>
#include <IO/FixedMemoryFile.h>
#include <Comm/NetPipe.h>
#include <Vrui/Vrui.h>
#include <Vrui/InputDevice.h>
#include <Vrui/InputGraphManager.h>
#include <Vrui/PointingTool.h>
#include <Collaboration/CollaborationClient.h>

namespace Collaboration {

/*******************************************************************
Methods of class CheriaClient::RemoteClientState::RemoteDeviceState:
*******************************************************************/

CheriaClient::RemoteClientState::RemoteDeviceState::RemoteDeviceState(IO::File& source)
	:DeviceState(source),
	 device(Vrui::getInputDeviceManager()->createInputDevice("CheriaRemoteDevice",trackType,numButtons,numValuators))
	{
	/* Permanently grab the device: */
	Vrui::getInputGraphManager()->grabInputDevice(device,0);
	}

CheriaClient::RemoteClientState::RemoteDeviceState::~RemoteDeviceState(void)
	{
	Vrui::getInputGraphManager()->releaseInputDevice(device,0);
	Vrui::getInputDeviceManager()->destroyInputDevice(device);
	}

/************************************************
Methods of class CheriaClient::RemoteClientState:
************************************************/

CheriaClient::RemoteClientState::RemoteClientState(CheriaClient& sClient)
	:client(sClient),
	 remoteDevices(17),remoteTools(17)
	{
	}

CheriaClient::RemoteClientState::~RemoteClientState(void)
	{
	/* Destroy all remote devices (which automatically destroys all remote tools): */
	client.remoteClientDestroyingDevice=true;
	for(RemoteDeviceMap::Iterator rdIt=remoteDevices.begin();!rdIt.isFinished();++rdIt)
		delete rdIt->getDest();
	client.remoteClientDestroyingDevice=false;
	
	/* Delete any leftover message buffers: */
	{
	Threads::Mutex::Lock messageBufferLock(messageBufferMutex);
	for(std::vector<IncomingMessage*>::iterator mIt=messages.begin();mIt!=messages.end();++mIt)
		delete *mIt;
	}
	}

void CheriaClient::RemoteClientState::processMessages(void)
	{
	Threads::Mutex::Lock messageBufferLock(messageBufferMutex);
	
	/* Handle all state tracking and device state update messages: */
	for(std::vector<IncomingMessage*>::iterator mIt=messages.begin();mIt!=messages.end();++mIt)
		{
		/* Process all messages in this buffer: */
		IO::File& msg=**mIt;
		while(!msg.eof())
			{
			/* Read the next message: */
			switch(CheriaProtocol::readMessage(msg))
				{
				case CREATE_DEVICE:
					{
					/* Read the new device's ID: */
					unsigned int newDeviceId=msg.read<Card>();
					
					/* Check if the device already exists: */
					if(remoteDevices.isEntry(newDeviceId))
						{
						/* Skip the new device's layout: */
						DeviceState::skipLayout(msg);
						}
					else
						{
						/* Create a new remote device structure: */
						client.remoteClientCreatingDevice=true;
						RemoteDeviceState* newRemoteDevice=new RemoteDeviceState(msg);
						client.remoteClientCreatingDevice=false;
						
						/* Set the new device's glyph: */
						Vrui::Glyph& deviceGlyph=Vrui::getInputGraphManager()->getInputDeviceGlyph(newRemoteDevice->device);
						deviceGlyph=client.inputDeviceGlyph;
						
						#if DEBUGGING
						std::cout<<"Creating remote device "<<newRemoteDevice->device<<" with remote ID "<<newDeviceId<<std::endl;
						#endif
						
						/* Add the new device to the remote device map: */
						remoteDevices[newDeviceId]=newRemoteDevice;
						}
					
					break;
					}
				
				case DESTROY_DEVICE:
					{
					/* Read the device's ID: */
					unsigned int deviceId=msg.read<Card>();
					
					/* Erase the device from the remote device map: */
					RemoteDeviceMap::Iterator rdIt=remoteDevices.findEntry(deviceId);
					if(!rdIt.isFinished())
						{
						/* Destroy the device: */
						client.remoteClientDestroyingDevice=true;
						delete rdIt->getDest();
						client.remoteClientDestroyingDevice=false;
						
						/* Remove the device from the remote device map: */
						remoteDevices.removeEntry(rdIt);
						}
					
					break;
					}
				
				case CREATE_TOOL:
					{
					/* Read the new tool's ID: */
					unsigned int newToolId=msg.read<Card>();
					
					/* Check if the tool already exists: */
					if(remoteTools.isEntry(newToolId))
						{
						/* Skip the new tool's class name and input assignment: */
						ToolState::skip(msg);
						}
					else
						{
						/* Read the tool's class name and input layout and assignment: */
						ToolState ts(msg);
						
						/* Find the tool's factory object: */
						Vrui::ToolFactory* factory=0;
						try
							{
							factory=Vrui::getToolManager()->loadClass(ts.className.c_str());
							
							/* Get the tool factory's layout and create an input assignment: */
							const Vrui::ToolInputLayout& til=factory->getLayout();
							Vrui::ToolInputAssignment tia(til);
							
							/* Read the tool's input assignment and ensure that the layouts match: */
							bool matches=true;
							matches=matches&&(int(ts.numButtonSlots)==til.getNumButtons()||(til.hasOptionalButtons()&&int(ts.numButtonSlots)>til.getNumButtons()));
							matches=matches&&(int(ts.numValuatorSlots)==til.getNumValuators()||(til.hasOptionalValuators()&&int(ts.numValuatorSlots)>til.getNumValuators()));
							
							if(matches)
								{
								/* Assign all button slots: */
								for(unsigned int buttonSlotIndex=0;buttonSlotIndex<ts.numButtonSlots;++buttonSlotIndex)
									{
									/* Assign the slot: */
									Vrui::InputDevice* slotDevice=remoteDevices.getEntry(ts.buttonSlots[buttonSlotIndex].deviceId).getDest()->device;
									if(int(buttonSlotIndex)<til.getNumButtons())
										tia.setButtonSlot(buttonSlotIndex,slotDevice,ts.buttonSlots[buttonSlotIndex].index);
									else
										tia.addButtonSlot(slotDevice,ts.buttonSlots[buttonSlotIndex].index);
									}
								
								/* Assign all valuator slots: */
								for(unsigned int valuatorSlotIndex=0;valuatorSlotIndex<ts.numValuatorSlots;++valuatorSlotIndex)
									{
									/* Assign the slot: */
									Vrui::InputDevice* slotDevice=remoteDevices.getEntry(ts.valuatorSlots[valuatorSlotIndex].deviceId).getDest()->device;
									if(int(valuatorSlotIndex)<til.getNumValuators())
										tia.setValuatorSlot(valuatorSlotIndex,slotDevice,ts.valuatorSlots[valuatorSlotIndex].index);
									else
										tia.addValuatorSlot(slotDevice,ts.valuatorSlots[valuatorSlotIndex].index);
									}
								
								/* Create the tool: */
								client.remoteClientCreatingTool=true;
								Vrui::Tool* newTool=Vrui::getToolManager()->createTool(factory,tia);
								client.remoteClientCreatingTool=false;
								
								/* Check if the new tool is a pointing tool: */
								Vrui::PointingTool* pointingTool=dynamic_cast<Vrui::PointingTool*>(newTool);
								if(pointingTool!=0)
									{
									/* Add the new tool to the remote tool map: */
									remoteTools[newToolId]=pointingTool;
									
									#if DEBUGGING
									std::cout<<"Created tool of class "<<className<<std::endl;
									#endif
									}
								else
									{
									/* Destroy the tool again: */
									client.remoteClientDestroyingTool=true;
									Vrui::getToolManager()->destroyTool(newTool);
									client.remoteClientDestroyingTool=false;
									}
								}
							}
						catch(std::runtime_error err)
							{
							/* Ignore the error, and the tool */
							#if DEBUGGING
							std::cout<<"Tool creation of class "<<className<<" failed due to "<<err.what()<<std::endl;
							#endif
							}
						}
					
					break;
					}
				
				case DESTROY_TOOL:
					{
					/* Read the tool's ID: */
					unsigned int toolId=msg.read<Card>();
					
					/* Erase the tool from the remote tool map: */
					RemoteToolMap::Iterator rtIt=remoteTools.findEntry(toolId);
					if(!rtIt.isFinished()) // The tool might not exist for valid reasons
						{
						/* Destroy the tool: */
						client.remoteClientDestroyingTool=true;
						Vrui::getToolManager()->destroyTool(rtIt->getDest());
						client.remoteClientDestroyingTool=false;
						
						/* Remove the tool from the remote tool map: */
						remoteTools.removeEntry(rtIt);
						}
					
					break;
					}
				
				case DEVICE_STATES:
					{
					/* Read all contained status messages: */
					unsigned int deviceId;
					while((deviceId=msg.read<Card>())!=0)
						{
						/* Update the device state: */
						remoteDevices.getEntry(deviceId).getDest()->read(msg);
						}
					
					break;
					}
				}
			}
		
		/* Destroy the just-read message buffer: */
		delete *mIt;
		}
	
	/* Clear the message buffer list: */
	messages.clear();
	}

/***********************************************
Methods of class CheriaClient::LocalDeviceState:
***********************************************/

CheriaClient::LocalDeviceState::LocalDeviceState(unsigned int sDeviceId,const Vrui::InputDevice* device)
	:DeviceState(device->getTrackType(),device->getNumButtons(),device->getNumValuators()),
	 deviceId(sDeviceId),
	 buttonMasks(numButtons>0?new Byte[(numButtons+7)/8]:0),valuatorMasks(numValuators>0?new Byte[(numValuators+7)/8]:0)
	{
	/* Initialize the mask arrays to false: */
	for(unsigned int i=0;i<(numButtons+7)/8;++i)
		buttonMasks[i]=0x0U;
	for(unsigned int i=0;i<(numValuators+7)/8;++i)
		valuatorMasks[i]=0x0U;
	}

CheriaClient::LocalDeviceState::~LocalDeviceState(void)
	{
	delete[] buttonMasks;
	delete[] valuatorMasks;
	}

/*****************************
Methods of class CheriaClient:
*****************************/

void CheriaClient::createInputDevice(Vrui::InputDevice* device)
	{
	#if DEBUGGING
	std::cout<<"Creating local input device "<<device<<" with local ID "<<nextLocalDeviceId<<std::endl;
	#endif
	
	/* Create a local device state structure for the new device: */
	LocalDeviceState* lds=new LocalDeviceState(nextLocalDeviceId,device);
	
	/* Add the new input device to the local device map: */
	localDevices[device]=lds;
	
	/**************************************************************
	Write an input device creation message into the message buffer:
	**************************************************************/
	
	/* Write the message ID: */
	writeMessage(CREATE_DEVICE,message);
	
	/* Write the new input device's ID: */
	message.write<Card>(nextLocalDeviceId);
	
	/* Write the new input device's layout: */
	lds->writeLayout(message);
	
	if(++nextLocalDeviceId==0)
		++nextLocalDeviceId;
	}

void CheriaClient::createTool(Vrui::PointingTool* tool)
	{
	#if DEBUGGING
	std::cout<<"Creating local tool "<<tool<<" with local ID "<<nextLocalToolId<<std::endl;
	#endif
	
	/* Add the new tool to the local tool map: */
	localTools[tool]=nextLocalToolId;
	
	/*****************************************************
	Write a tool creation message into the message buffer:
	*****************************************************/
	
	/* Write the message ID: */
	writeMessage(CREATE_TOOL,message);
	
	/* Write the new tool's ID: */
	message.write<Card>(nextLocalToolId);
	
	/* Create a tool state structure: */
	const Vrui::ToolInputAssignment& tia=tool->getInputAssignment();
	ToolState ts(tool->getFactory()->getClassName(),tia.getNumButtonSlots(),tia.getNumValuatorSlots());
	
	/* Fill in the tool's button slot assignment: */
	for(unsigned int buttonSlotIndex=0;buttonSlotIndex<ts.numButtonSlots;++buttonSlotIndex)
		{
		/* Get the slot's local device state structure: */
		LocalDeviceState* lds=localDevices.getEntry(tia.getButtonSlot(buttonSlotIndex).device).getDest();
		ts.buttonSlots[buttonSlotIndex].deviceId=lds->deviceId;
		unsigned int index=tia.getButtonSlot(buttonSlotIndex).index;
		ts.buttonSlots[buttonSlotIndex].index=index;
		
		/* Enable the slot's button on the device for further updates: */
		lds->buttonMasks[index/8]|=0x1U<<(index%8);
		}
	
	/* Fill in the tool's valuator slot assignment: */
	for(unsigned int valuatorSlotIndex=0;valuatorSlotIndex<ts.numValuatorSlots;++valuatorSlotIndex)
		{
		/* Get the slot's local device state structure: */
		LocalDeviceState* lds=localDevices.getEntry(tia.getValuatorSlot(valuatorSlotIndex).device).getDest();
		ts.valuatorSlots[valuatorSlotIndex].deviceId=lds->deviceId;
		unsigned int index=tia.getValuatorSlot(valuatorSlotIndex).index;
		ts.valuatorSlots[valuatorSlotIndex].index=index;
		
		/* Enable the slot's valuator on the device for further updates: */
		lds->valuatorMasks[index/8]|=0x1U<<(index%8);
		}
	
	/* Write the tool state structure into the message buffer: */
	ts.write(message);
	
	if(++nextLocalToolId==0)
		++nextLocalToolId;
	}

void CheriaClient::inputDeviceCreationCallback(Vrui::InputDeviceManager::InputDeviceCreationCallbackData* cbData)
	{
	/* Ignore devices being created by remote Cheria clients: */
	if(!remoteClientCreatingDevice)
		{
		Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
		
		/* Add a representation for the new input device: */
		createInputDevice(cbData->inputDevice);
		}
	}

void CheriaClient::inputDeviceDestructionCallback(Vrui::InputDeviceManager::InputDeviceDestructionCallbackData* cbData)
	{
	/* Ignore devices being destroyed by remote Cheria clients: */
	if(!remoteClientDestroyingDevice)
		{
		Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
		
		/* Remove the input device from the local device map: */
		LocalDeviceMap::Iterator ldIt=localDevices.findEntry(cbData->inputDevice);
		if(!ldIt.isFinished())
			{
			/* Send a device destruction message: */
			writeMessage(DESTROY_DEVICE,message);
			message.write<Card>(ldIt->getDest()->deviceId);
			
			/* Destroy the device's local device state and remove the device: */
			delete ldIt->getDest();
			localDevices.removeEntry(ldIt);
			}
		}
	}

void CheriaClient::toolCreationCallback(Vrui::ToolManager::ToolCreationCallbackData* cbData)
	{
	/* Ignore tools being created by remote Cheria clients: */
	if(!remoteClientCreatingTool)
		{
		/* Check if the tool is a pointing tool: */
		Vrui::PointingTool* pointingTool=dynamic_cast<Vrui::PointingTool*>(cbData->tool);
		if(pointingTool!=0)
			{
			Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
			
			/* Add the tool to the representation: */
			createTool(pointingTool);
			}
		}
	}

void CheriaClient::toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData)
	{
	/* Ignore tools being destroyed by remote Cheria clients: */
	if(!(remoteClientDestroyingDevice||remoteClientDestroyingTool))
		{
		/* Check if the tool is a pointing tool: */
		Vrui::PointingTool* pointingTool=dynamic_cast<Vrui::PointingTool*>(cbData->tool);
		if(pointingTool!=0)
			{
			Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
			
			LocalToolMap::Iterator ltIt=localTools.findEntry(pointingTool);
			if(!ltIt.isFinished())
				{
				/**************************************************
				Disable all buttons and valuators used by the tool:
				**************************************************/
				
				const Vrui::ToolInputAssignment& tia=pointingTool->getInputAssignment();
				
				/* Disable the tool's button slots: */
				for(int buttonSlotIndex=0;buttonSlotIndex<tia.getNumButtonSlots();++buttonSlotIndex)
					{
					/* Get the slot's local device state structure: */
					LocalDeviceState* lds=localDevices.getEntry(tia.getButtonSlot(buttonSlotIndex).device).getDest();
					
					/* Disable the slot's button on the device: */
					unsigned int index=tia.getButtonSlot(buttonSlotIndex).index;
					lds->buttonMasks[index/8]&=~(0x1U<<(index%8));
					}
				
				/* Disable the tool's valuator slots: */
				for(int valuatorSlotIndex=0;valuatorSlotIndex<tia.getNumValuatorSlots();++valuatorSlotIndex)
					{
					/* Get the slot's local device state structure: */
					LocalDeviceState* lds=localDevices.getEntry(tia.getValuatorSlot(valuatorSlotIndex).device).getDest();
					
					/* Disable the slot's valuator on the device: */
					unsigned int index=tia.getValuatorSlot(valuatorSlotIndex).index;
					lds->valuatorMasks[index/8]&=~(0x1U<<(index%8));
					}
				
				/* Send a tool destruction message: */
				writeMessage(DESTROY_TOOL,message);
				message.write<Card>(ltIt->getDest());
				
				/* Remove the tool: */
				localTools.removeEntry(ltIt);
				}
			}
		}
	}

CheriaClient::CheriaClient(void)
	:nextLocalDeviceId(1),localDevices(17),
	 nextLocalToolId(1),localTools(17),
	 remoteClientCreatingDevice(false),remoteClientDestroyingDevice(false),
	 remoteClientCreatingTool(false),remoteClientDestroyingTool(false)
	{
	}

CheriaClient::~CheriaClient(void)
	{
	/* Delete local device states of all remaining represented input devices: */
	for(LocalDeviceMap::Iterator ldIt=localDevices.begin();!ldIt.isFinished();++ldIt)
		delete ldIt->getDest();
	
	/* Unregister callbacks with the input device and tool managers: */
	Vrui::getInputDeviceManager()->getInputDeviceCreationCallbacks().remove(this,&CheriaClient::inputDeviceCreationCallback);
	Vrui::getInputDeviceManager()->getInputDeviceDestructionCallbacks().remove(this,&CheriaClient::inputDeviceDestructionCallback);
	Vrui::getToolManager()->getToolCreationCallbacks().remove(this,&CheriaClient::toolCreationCallback);
	Vrui::getToolManager()->getToolDestructionCallbacks().remove(this,&CheriaClient::toolDestructionCallback);
	}

const char* CheriaClient::getName(void) const
	{
	return protocolName;
	}

unsigned int CheriaClient::getNumMessages(void) const
	{
	return MESSAGES_END;
	}

void CheriaClient::initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection)
	{
	/* Call the base class method: */
	ProtocolClient::initialize(sClient,configFileSection);
	
	/* Initialize and configure the remote input device glyph: */
	inputDeviceGlyph.enable(Vrui::Glyph::CONE,GLMaterial(GLMaterial::Color(0.5f,0.5f,0.5f),GLMaterial::Color(0.5f,0.5f,0.5f),25.0f));
	inputDeviceGlyph.configure(configFileSection,"remoteInputDeviceGlyphType","remoteInputDeviceGlyphMaterial");
	}

void CheriaClient::sendConnectRequest(Comm::NetPipe& pipe)
	{
	/* Send the length of the following message: */
	pipe.write<Card>(sizeof(Card));
	
	/* Send the client's protocol version: */
	pipe.write<Card>(protocolVersion);
	}

void CheriaClient::receiveConnectReply(Comm::NetPipe& pipe)
	{
	/* Set the message buffer's endianness swapping behavior to that of the pipe: */
	message.setSwapOnWrite(pipe.mustSwapOnWrite());
	
	Vrui::InputDeviceManager* idm=Vrui::getInputDeviceManager();
	Vrui::ToolManager* tm=Vrui::getToolManager();
	
	{
	Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
	
	/* Add all existing devices to the local device map: */
	int numDevices=idm->getNumInputDevices();
	for(int i=0;i<numDevices;++i)
		{
		/* Add the input device to the representation: */
		createInputDevice(idm->getInputDevice(i));
		}
	
	/* Add all existing pointing tools to the local tool map: */
	for(Vrui::ToolManager::ToolList::iterator tIt=tm->beginTools();tIt!=tm->endTools();++tIt)
		{
		/* Check if the tool is a pointing tool: */
		Vrui::PointingTool* pointingTool=dynamic_cast<Vrui::PointingTool*>(*tIt);
		if(pointingTool!=0)
			{
			/* Add the tool to the representation: */
			createTool(pointingTool);
			}
		}
	}
	
	/* Register callbacks with the input device and tool managers: */
	idm->getInputDeviceCreationCallbacks().add(this,&CheriaClient::inputDeviceCreationCallback);
	idm->getInputDeviceDestructionCallbacks().add(this,&CheriaClient::inputDeviceDestructionCallback);
	tm->getToolCreationCallbacks().add(this,&CheriaClient::toolCreationCallback);
	tm->getToolDestructionCallbacks().add(this,&CheriaClient::toolDestructionCallback);
	}

void CheriaClient::receiveDisconnectReply(Comm::NetPipe& pipe)
	{
	/* Unregister callbacks with the input device and tool managers: */
	Vrui::getInputDeviceManager()->getInputDeviceCreationCallbacks().remove(this,&CheriaClient::inputDeviceCreationCallback);
	Vrui::getInputDeviceManager()->getInputDeviceDestructionCallbacks().remove(this,&CheriaClient::inputDeviceDestructionCallback);
	Vrui::getToolManager()->getToolCreationCallbacks().remove(this,&CheriaClient::toolCreationCallback);
	Vrui::getToolManager()->getToolDestructionCallbacks().remove(this,&CheriaClient::toolDestructionCallback);
	}

ProtocolClient::RemoteClientState* CheriaClient::receiveClientConnect(Comm::NetPipe& pipe)
	{
	/* Create a new remote client state object: */
	RemoteClientState* newClientState=new RemoteClientState(*this);
	
	/* Read the size of the following message: */
	unsigned int messageSize=pipe.read<Card>();
	
	#if DEBUGGING
	std::cout<<"Received client connect message of size "<<messageSize<<std::endl;
	#endif
	
	/* Read the entire message into a new read buffer that has the same endianness as the pipe's read end: */
	IncomingMessage* msg=new IncomingMessage(messageSize);
	msg->setSwapOnRead(pipe.mustSwapOnRead());
	pipe.readRaw(msg->getMemory(),messageSize);
	
	/* Store the new buffer in the client's message list: */
	newClientState->messages.push_back(msg);
	
	return newClientState;
	}

bool CheriaClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,Comm::NetPipe& pipe)
	{
	/* Get a handle on the remote client state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("CheriaClient::receiveServerUpdate: Mismatching remote client state object type");
	
	/* Read the size of the following message: */
	unsigned int messageSize=pipe.read<Card>();
	
	if(messageSize>0)
		{
		/* Read the entire message into a new read buffer that has the same endianness as the pipe's read end: */
		IncomingMessage* msg=new IncomingMessage(messageSize);
		msg->setSwapOnRead(pipe.mustSwapOnRead());
		pipe.readRaw(msg->getMemory(),messageSize);
		
		/* Store the new buffer in the client's message list: */
		{
		Threads::Mutex::Lock messageBufferLock(myRcs->messageBufferMutex);
		myRcs->messages.push_back(msg);
		}
		}
	
	return messageSize>sizeof(MessageIdType)+sizeof(Card);
	}

void CheriaClient::sendClientUpdate(Comm::NetPipe& pipe)
	{
	Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
	
	/* Send accumulated state tracking messages all at once and then clear the message buffer: */
	message.writeToSink(pipe);
	message.clear();
	
	/* Send the current state of all local input devices: */
	writeMessage(DEVICE_STATES,pipe);
	for(LocalDeviceMap::Iterator ldIt=localDevices.begin();!ldIt.isFinished();++ldIt)
		{
		/* Get the device's local state structure: */
		LocalDeviceState* lds=ldIt->getDest();
		
		/* Check if the device has changed since the last update: */
		if(lds->updateMask!=DeviceState::NO_CHANGE)
			{
			/* Write the device's local ID: */
			pipe.write<Card>(lds->deviceId);
			
			/* Write the device's state update: */
			lds->write(lds->updateMask,pipe);
			
			/* Reset the device's update mask: */
			lds->updateMask=DeviceState::NO_CHANGE;
			}
		}
	
	/* Terminate the update packet with a zero device ID: */
	pipe.write<Card>(0);
	}

void CheriaClient::frame(void)
	{
	Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
	
	/* Update the states of all local input devices: */
	for(LocalDeviceMap::Iterator ldIt=localDevices.begin();!ldIt.isFinished();++ldIt)
		{
		Vrui::InputDevice* device=ldIt->getSource();
		LocalDeviceState& lds=*(ldIt->getDest());
		
		/* Update the device's ray direction: */
		Vector rayDirection(device->getDeviceRayDirection()); // Conversion to lower precision
		Scalar rayStart=Scalar(device->getDeviceRayStart()); // Conversion to lower precision
		if(lds.rayDirection!=rayDirection||lds.rayStart!=rayStart)
			{
			lds.updateMask|=DeviceState::RAYDIRECTION;
			lds.rayDirection=rayDirection;
			lds.rayStart=rayStart;
			}
		
		/* Update the device's position and orientation: */
		ONTransform transform(device->getTransformation()); // Conversion to lower precision
		if(lds.transform!=transform)
			lds.updateMask|=DeviceState::TRANSFORM;
		lds.transform=transform;
		
		/* Update the device's velocities: */
		Vector linearVelocity(device->getLinearVelocity()); // Conversion to lower precision
		Vector angularVelocity(device->getAngularVelocity()); // Conversion to lower precision
		if(lds.linearVelocity!=linearVelocity||lds.angularVelocity!=angularVelocity)
			{
			lds.updateMask|=DeviceState::VELOCITY;
			lds.linearVelocity=linearVelocity;
			lds.angularVelocity=angularVelocity;
			}
		
		/* Update the device's button states: */
		bool buttonChanged=false;
		unsigned int index=0;
		unsigned int mask=0x1U;
		for(unsigned int buttonIndex=0;buttonIndex<lds.numButtons;++buttonIndex)
			{
			/* Update the button state: */
			unsigned int oldButtonState=lds.buttonStates[index]&mask;
			unsigned int newButtonState=lds.buttonMasks[index]&mask;
			if(!device->getButtonState(buttonIndex))
				newButtonState=0x0U;
			buttonChanged=buttonChanged||newButtonState!=oldButtonState;
			if(newButtonState)
				lds.buttonStates[index]|=mask;
			else
				lds.buttonStates[index]&=~mask;
			
			/* Go to the next button and button mask bit: */
			mask<<=1;
			if(mask==0x100U)
				{
				++index;
				mask=0x1U;
				}
			}
		if(buttonChanged)
			lds.updateMask|=DeviceState::BUTTON;
		
		/* Update the device's valuator states: */
		bool valuatorChanged=false;
		index=0;
		mask=0x1U;
		for(unsigned int valuatorIndex=0;valuatorIndex<lds.numValuators;++valuatorIndex)
			{
			/* Update the valuator state: */
			Scalar newValue(device->getValuator(valuatorIndex)); // Conversion to lower precision
			if((lds.valuatorMasks[index]&mask)==0)
				newValue=Scalar(0);
			valuatorChanged=valuatorChanged||lds.valuatorStates[valuatorIndex]!=newValue;
			lds.valuatorStates[valuatorIndex]=newValue;
			
			/* Go to the next valuator mask bit: */
			mask<<=1;
			if(mask==0x100U)
				{
				++index;
				mask=0x1U;
				}
			}
		if(valuatorChanged)
			lds.updateMask|=DeviceState::VALUATOR;
		}
	}

void CheriaClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	/* Get a handle on the remote client state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("CheriaClient::frame: Mismatching remote client state object type");
	
	/* Process the remote client's queued server update messages: */
	myRcs->processMessages();
	
	/* Calculate the transformation from the remote client's physical space into the local client's physical space: */
	Vrui::NavTransform remoteNav=Vrui::NavTransform(client->getClientState(rcs).getLockedValue().navTransform);
	remoteNav.doInvert();
	remoteNav.leftMultiply(Vrui::getNavigationTransformation());
	
	/* Update the states of all remote input devices: */
	for(RemoteClientState::RemoteDeviceMap::Iterator rdIt=myRcs->remoteDevices.begin();!rdIt.isFinished();++rdIt)
		{
		RemoteClientState::RemoteDeviceState& rds=*(rdIt->getDest());
		
		/* Update the device ray direction: */
		if(rds.updateMask&DeviceState::RAYDIRECTION)
			rds.device->setDeviceRay(rds.rayDirection,rds.rayStart);
		
		/* Always update the device transformation as either navigation transformation might have changed: */
		Vrui::NavTransform deviceTransform=Vrui::NavTransform(rds.transform);
		deviceTransform.leftMultiply(remoteNav);
		deviceTransform.renormalize();
		rds.device->setTransformation(Vrui::TrackerState(deviceTransform.getTranslation(),deviceTransform.getRotation()));
		
		/* Always update the device velocities as either navigation transformation might have changed: */
		rds.device->setLinearVelocity(remoteNav.transform(rds.linearVelocity));
		rds.device->setAngularVelocity(remoteNav.transform(rds.angularVelocity));
		
		/* Update all button states: */
		if(rds.updateMask&DeviceState::BUTTON)
			{
			unsigned int index=0;
			unsigned int mask=0x1U;
			for(unsigned int buttonIndex=0;buttonIndex<rds.numButtons;++buttonIndex)
				{
				rds.device->setButtonState(buttonIndex,(rds.buttonStates[index]&mask)!=0x0U);
				mask<<=1;
				if(mask==0x100U)
					{
					++index;
					mask=0x1U;
					}
				}
			}
		
		/* Update all valuator states: */
		if(rds.updateMask&DeviceState::VALUATOR)
			{
			for(unsigned int valuatorIndex=0;valuatorIndex<rds.numValuators;++valuatorIndex)
				rds.device->setValuator(valuatorIndex,rds.valuatorStates[valuatorIndex]);
			}
		
		/* Reset the device's update mask: */
		rds.updateMask=DeviceState::NO_CHANGE;
		}
	
	/* Update the states of all remote pointing tools: */
	Scalar scaleFactor=remoteNav.getScaling();
	for(RemoteClientState::RemoteToolMap::Iterator rtIt=myRcs->remoteTools.begin();!rtIt.isFinished();++rtIt)
		{
		/* Set the pointing tool's scaling factor: */
		rtIt->getDest()->setScaleFactor(scaleFactor);
		}
	}

}

/****************
DSO entry points:
****************/

extern "C" {

Collaboration::ProtocolClient* createObject(Collaboration::ProtocolClientLoader& objectLoader)
	{
	return new Collaboration::CheriaClient;
	}

void destroyObject(Collaboration::ProtocolClient* object)
	{
	delete object;
	}

}
