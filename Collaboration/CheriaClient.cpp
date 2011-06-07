/***********************************************************************
CheriaClient - Client object to implement the Cheria input device
distribution protocol.
Copyright (c) 2010 Oliver Kreylos

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

#define DEBUGGING 1

#include <Collaboration/CheriaClient.h>

#include <stdexcept>
#if DEBUGGING
#include <iostream>
#endif
#include <Misc/ThrowStdErr.h>
#include <Misc/ReadBuffer.h>
#include <Misc/StringMarshaller.h>
#include <Geometry/OrthogonalTransformation.h>
#include <Vrui/Vrui.h>
#include <Vrui/InputDevice.h>
#include <Vrui/InputGraphManager.h>
#include <Vrui/PointingTool.h>

namespace Collaboration {

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
	{
	/* Lock the client's remote device state: */
	Threads::Mutex::Lock remoteDevicesLock(client.remoteDevicesMutex);
	
	/* Destroy all remote devices (which automatically destroys all remote tools): */
	for(RemoteDeviceMap::Iterator rdIt=remoteDevices.begin();!rdIt.isFinished();++rdIt)
		{
		/* Destroy the device: */
		client.remoteClientDestroyingDevice=true;
		Vrui::getInputGraphManager()->releaseInputDevice(rdIt->getDest(),0);
		Vrui::getInputDeviceManager()->destroyInputDevice(rdIt->getDest());
		client.remoteClientDestroyingDevice=false;
		}
	}
	
	/* Delete any leftover message buffers: */
	{
	Threads::Mutex::Lock messageBufferLock(messageBufferMutex);
	for(std::vector<Misc::ReadBuffer*>::iterator mbIt=messageBuffers.begin();mbIt!=messageBuffers.end();++mbIt)
		delete *mbIt;
	}
	}

void CheriaClient::RemoteClientState::processMessages(void)
	{
	/* Lock the client's remote device state and this remote client's message buffer list: */
	Threads::Mutex::Lock remoteDevicesLock(client.remoteDevicesMutex);
	Threads::Mutex::Lock messageBufferLock(messageBufferMutex);
	
	/* Handle all state tracking messages and only the final device state dump: */
	for(std::vector<Misc::ReadBuffer*>::iterator mbIt=messageBuffers.begin();mbIt!=messageBuffers.end();++mbIt)
		{
		/* Process all messages in this buffer: */
		Misc::ReadBuffer& pipe=**mbIt;
		bool goOn=true;
		while(goOn)
			{
			/* Read the next message: */
			MessageIdType message=pipe.read<MessageIdType>();
			switch(message)
				{
				case CREATE_DEVICE:
					{
					/* Read the new device's ID: */
					unsigned int newDeviceId=pipe.read<unsigned int>();
					
					/* Read the new device's layout: */
					int trackType=pipe.read<int>();
					int numButtons=pipe.read<int>();
					int numValuators=pipe.read<int>();
					
					/* Create a new input device: */
					client.remoteClientCreatingDevice=true;
					Vrui::InputDevice* newDevice=Vrui::getInputDeviceManager()->createInputDevice("CheriaRemoteDevice",trackType,numButtons,numValuators);
					Vrui::getInputGraphManager()->grabInputDevice(newDevice,0);
					client.remoteClientCreatingDevice=false;
					
					/* Set the new device's glyph: */
					Vrui::Glyph& deviceGlyph=Vrui::getInputGraphManager()->getInputDeviceGlyph(newDevice);
					deviceGlyph=client.inputDeviceGlyph;
					
					#if DEBUGGING
					std::cout<<"Creating remote device "<<newDevice<<" with remote ID "<<newDeviceId<<std::endl;
					#endif
					
					/* Add the new device to the remote device map: */
					remoteDevices.setEntry(RemoteDeviceMap::Entry(newDeviceId,newDevice));
					
					break;
					}
				
				case DESTROY_DEVICE:
					{
					/* Read the device's ID: */
					unsigned int deviceId=pipe.read<unsigned int>();
					
					/* Erase the device from the remote device map: */
					RemoteDeviceMap::Iterator rdIt=remoteDevices.findEntry(deviceId);
					if(!rdIt.isFinished())
						{
						/* Destroy the device: */
						client.remoteClientDestroyingDevice=true;
						Vrui::getInputGraphManager()->releaseInputDevice(rdIt->getDest(),0);
						Vrui::getInputDeviceManager()->destroyInputDevice(rdIt->getDest());
						client.remoteClientDestroyingDevice=false;
						
						/* Remove the device from the remote device map: */
						remoteDevices.removeEntry(rdIt);
						}
					
					break;
					}
				
				case CREATE_TOOL:
					{
					/* Read the new tool's ID: */
					unsigned int newToolId=pipe.read<unsigned int>();
					
					/* Read the tool's class name: */
					char* className=Misc::readCString(pipe);
					
					/* Find the tool's factory object: */
					Vrui::ToolFactory* factory=0;
					try
						{
						factory=Vrui::getToolManager()->loadClass(className);
						
						/* Get the tool factory's layout and create an input assignment: */
						const Vrui::ToolInputLayout& til=factory->getLayout();
						Vrui::ToolInputAssignment tia(til);
						
						/* Read the tool's input assignment and ensure that the layouts match: */
						bool matches=true;
						
						/* Read all button slots: */
						int numButtonSlots=pipe.read<int>();
						matches=matches&&(numButtonSlots==til.getNumButtons()||(til.hasOptionalButtons()&&numButtonSlots>til.getNumButtons()));
						for(int buttonSlotIndex=0;buttonSlotIndex<numButtonSlots;++buttonSlotIndex)
							{
							/* Read the slot's device ID and device slot index: */
							unsigned int deviceId=pipe.read<unsigned int>();
							Vrui::InputDevice* device=remoteDevices.getEntry(deviceId).getDest();
							int slotIndex=pipe.read<int>();
							
							if(matches)
								{
								/* Assign the slot: */
								if(buttonSlotIndex<til.getNumButtons())
									tia.setButtonSlot(buttonSlotIndex,device,slotIndex);
								else
									tia.addButtonSlot(device,slotIndex);
								}
							}
						
						/* Read all valuator slots: */
						int numValuatorSlots=pipe.read<int>();
						matches=matches&&(numValuatorSlots==til.getNumValuators()||(til.hasOptionalValuators()&&numValuatorSlots>til.getNumValuators()));
						for(int valuatorSlotIndex=0;valuatorSlotIndex<numValuatorSlots;++valuatorSlotIndex)
							{
							/* Read the slot's device ID and device slot index: */
							unsigned int deviceId=pipe.read<unsigned int>();
							Vrui::InputDevice* device=remoteDevices.getEntry(deviceId).getDest();
							int slotIndex=pipe.read<int>();
							
							if(matches)
								{
								/* Assign the slot: */
								if(valuatorSlotIndex<til.getNumValuators())
									tia.setValuatorSlot(valuatorSlotIndex,device,slotIndex);
								else
									tia.addValuatorSlot(device,slotIndex);
								}
							}
						
						if(matches)
							{
							/* Create the tool: */
							client.remoteClientCreatingTool=true;
							Vrui::Tool* newTool=Vrui::getToolManager()->createTool(factory,tia);
							client.remoteClientCreatingTool=false;
							
							/* Check if the new tool is a pointing tool: */
							Vrui::PointingTool* pointingTool=dynamic_cast<Vrui::PointingTool*>(newTool);
							if(pointingTool!=0)
								{
								/* Add the new tool to the remote tool map: */
								remoteTools.setEntry(RemoteToolMap::Entry(newToolId,pointingTool));
								
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
					
					/* Clean up: */
					delete[] className;
					
					break;
					}
				
				case DESTROY_TOOL:
					{
					/* Read the tool's ID: */
					unsigned int toolId=pipe.read<unsigned int>();
					
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
					/* Check if this is the last queued message buffer: */
					std::vector<Misc::ReadBuffer*>::iterator next=mbIt;
					++next;
					if(next==messageBuffers.end())
						{
						/* Get the navigation transformation: */
						OGTransform nav=Vrui::getNavigationTransformation();
						
						/* Read status messages for each device: */
						for(size_t i=0;i<remoteDevices.getNumEntries();++i)
							{
							/* Read the device ID: */
							unsigned int deviceId=pipe.read<unsigned int>();
							
							/* Get the device: */
							Vrui::InputDevice* device=remoteDevices.getEntry(deviceId).getDest();
							
							/* Read the device's position and orientation: */
							Vector translation;
							pipe.read<Scalar>(translation.getComponents(),3);
							Scalar quaternion[4];
							pipe.read<Scalar>(quaternion,4);
							Scalar scaling=pipe.read<Scalar>();
							OGTransform transform=nav;
							transform*=OGTransform(translation,OGTransform::Rotation::fromQuaternion(quaternion),scaling);
							device->setTransformation(Vrui::TrackerState(transform.getTranslation(),transform.getRotation()));
							
							/* Read the device's ray direction: */
							Vector rayDirection;
							pipe.read<Scalar>(rayDirection.getComponents(),3);
							device->setDeviceRayDirection(rayDirection);
							
							/* Read the device's button states: */
							unsigned char bitMask=0x0;
							int numBits=0;
							for(int i=0;i<device->getNumButtons();++i)
								{
								if(numBits==0)
									{
									pipe.read<unsigned char>(bitMask);
									numBits=8;
									}
								device->setButtonState(i,bitMask&0x1);
								bitMask>>=1;
								--numBits;
								}
							
							/* Read the device's valuator states: */
							for(int i=0;i<device->getNumValuators();++i)
								device->setValuator(i,pipe.read<Scalar>());
							}
						}
					
					/* This is the last message: */
					goOn=false;
					break;
					}
				}
			}
		
		/* Destroy the just read message buffer: */
		delete *mbIt;
		}
	
	/* Clear the message buffer list: */
	messageBuffers.clear();
	}

/***********************************************
Methods of class CheriaClient::LocalDeviceState:
***********************************************/

CheriaClient::LocalDeviceState::LocalDeviceState(unsigned int sDeviceId,const Vrui::InputDevice* device)
	:deviceId(sDeviceId),
	 buttonMasks(new bool[device->getNumButtons()]),valuatorMasks(new bool[device->getNumValuators()])
	{
	/* Initialize the mask arrays to false: */
	for(int i=0;i<device->getNumButtons();++i)
		buttonMasks[i]=false;
	for(int i=0;i<device->getNumValuators();++i)
		valuatorMasks[i]=false;
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
	localDevices.setEntry(LocalDeviceMap::Entry(device,lds));
	
	/**************************************************************
	Write an input device creation message into the message buffer:
	**************************************************************/
	
	/* Write the message ID: */
	messageBuffer.write<MessageIdType>(CREATE_DEVICE);
	
	/* Write the new input device's ID: */
	messageBuffer.write<unsigned int>(nextLocalDeviceId);
	
	/* Write the new input device's layout: */
	messageBuffer.write<int>(device->getTrackType());
	messageBuffer.write<int>(device->getNumButtons());
	messageBuffer.write<int>(device->getNumValuators());
	
	++nextLocalDeviceId;
	}

void CheriaClient::createTool(Vrui::PointingTool* tool)
	{
	#if DEBUGGING
	std::cout<<"Creating local tool "<<tool<<" with local ID "<<nextLocalToolId<<std::endl;
	#endif
	
	/* Add the new tool to the local tool map: */
	localTools.setEntry(LocalToolMap::Entry(tool,nextLocalToolId));
	
	/*****************************************************
	Write a tool creation message into the message buffer:
	*****************************************************/
	
	/* Write the message ID: */
	messageBuffer.write<MessageIdType>(CREATE_TOOL);
	
	/* Write the new tool's ID: */
	messageBuffer.write<unsigned int>(nextLocalToolId);
	
	/* Write the tool's class name: */
	Misc::writeCString(tool->getFactory()->getClassName(),messageBuffer);
	
	const Vrui::ToolInputAssignment& tia=tool->getInputAssignment();
	
	/* Write and enable the tool's button slots: */
	messageBuffer.write<int>(tia.getNumButtonSlots());
	for(int buttonSlotIndex=0;buttonSlotIndex<tia.getNumButtonSlots();++buttonSlotIndex)
		{
		/* Get the slot's local device state structure: */
		LocalDeviceState* lds=localDevices.getEntry(tia.getButtonSlot(buttonSlotIndex).device).getDest();
		
		/* Write the device's ID: */
		messageBuffer.write<unsigned int>(lds->deviceId);
		
		/* Write the slot's button index: */
		int index=tia.getButtonSlot(buttonSlotIndex).index;
		messageBuffer.write<int>(index);
		
		/* Enable the slot's button on the device: */
		lds->buttonMasks[index]=true;
		}
	
	/* Write and enable the tool's valuator slots: */
	messageBuffer.write<int>(tia.getNumValuatorSlots());
	for(int valuatorSlotIndex=0;valuatorSlotIndex<tia.getNumValuatorSlots();++valuatorSlotIndex)
		{
		/* Get the slot's local device state structure: */
		LocalDeviceState* lds=localDevices.getEntry(tia.getValuatorSlot(valuatorSlotIndex).device).getDest();
		
		/* Write the device's ID: */
		messageBuffer.write<unsigned int>(lds->deviceId);
		
		/* Write the slot's valuator index: */
		int index=tia.getValuatorSlot(valuatorSlotIndex).index;
		messageBuffer.write<int>(index);
		
		/* Enable the slot's valuator on the device: */
		lds->valuatorMasks[index]=true;
		}
	
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
			messageBuffer.write<MessageIdType>(DESTROY_DEVICE);
			messageBuffer.write<unsigned int>(ldIt->getDest()->deviceId);
			
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
	if(!remoteClientDestroyingTool)
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
					lds->buttonMasks[tia.getButtonSlot(buttonSlotIndex).index]=false;
					}
				
				/* Disable the tool's valuator slots: */
				for(int valuatorSlotIndex=0;valuatorSlotIndex<tia.getNumValuatorSlots();++valuatorSlotIndex)
					{
					/* Get the slot's local device state structure: */
					LocalDeviceState* lds=localDevices.getEntry(tia.getValuatorSlot(valuatorSlotIndex).device).getDest();
					
					/* Disable the slot's valuator on the device: */
					lds->valuatorMasks[tia.getValuatorSlot(valuatorSlotIndex).index]=false;
					}
				
				/* Send a tool destruction message: */
				messageBuffer.write<MessageIdType>(DESTROY_TOOL);
				messageBuffer.write<unsigned int>(ltIt->getDest());
				
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

void CheriaClient::initialize(CollaborationClient& collaborationClient,Misc::ConfigurationFileSection& configFileSection)
	{
	/* Initialize and configure the remote input device glyph: */
	inputDeviceGlyph.enable(Vrui::Glyph::CONE,GLMaterial(GLMaterial::Color(0.5f,0.5f,0.5f),GLMaterial::Color(0.5f,0.5f,0.5f),25.0f));
	inputDeviceGlyph.configure(configFileSection,"remoteInputDeviceGlyphType","remoteInputDeviceGlyphMaterial");
	}

void CheriaClient::sendConnectRequest(CollaborationPipe& pipe)
	{
	/* Send the length of the following message: */
	pipe.write<unsigned int>(sizeof(unsigned int));
	
	/* Send the client's protocol version: */
	pipe.write<unsigned int>(protocolVersion);
	}

void CheriaClient::receiveConnectReply(CollaborationPipe& pipe)
	{
	/* Set the message buffer's endianness swapping behavior to that of the pipe: */
	messageBuffer.setEndianness(pipe.mustSwapOnWrite());
	
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

void CheriaClient::receiveDisconnectReply(CollaborationPipe& pipe)
	{
	/* Unregister callbacks with the input device and tool managers: */
	Vrui::getInputDeviceManager()->getInputDeviceCreationCallbacks().remove(this,&CheriaClient::inputDeviceCreationCallback);
	Vrui::getInputDeviceManager()->getInputDeviceDestructionCallbacks().remove(this,&CheriaClient::inputDeviceDestructionCallback);
	Vrui::getToolManager()->getToolCreationCallbacks().remove(this,&CheriaClient::toolCreationCallback);
	Vrui::getToolManager()->getToolDestructionCallbacks().remove(this,&CheriaClient::toolDestructionCallback);
	}

void CheriaClient::sendClientUpdate(CollaborationPipe& pipe)
	{
	{
	Threads::Mutex::Lock localDevicesLock(localDevicesMutex);
	
	/* Send accumulated state tracking messages all at once and then clear the message buffer: */
	messageBuffer.writeToSink(pipe);
	messageBuffer.clear();
	
	/* Get the inverse navigation transformation: */
	OGTransform invNav=Vrui::getInverseNavigationTransformation();
	
	/* Send the current state of all represented devices: */
	pipe.writeMessage(DEVICE_STATES);
	for(LocalDeviceMap::Iterator ldIt=localDevices.begin();!ldIt.isFinished();++ldIt)
		{
		/* Get the device: */
		Vrui::InputDevice* device=ldIt->getSource();
		
		/* Get the device's local state structure: */
		LocalDeviceState* lds=ldIt->getDest();
		
		/* Write the device's local ID: */
		pipe.write<unsigned int>(lds->deviceId);
		
		/* Calculate the device's position and orientation in navigational space: */
		OGTransform deviceTransform=invNav;
		deviceTransform*=device->getTransformation();
		
		/* Write the device's position and orientation: */
		pipe.write<Scalar>(deviceTransform.getTranslation().getComponents(),3);
		pipe.write<Scalar>(deviceTransform.getRotation().getQuaternion(),4);
		pipe.write<Scalar>(deviceTransform.getScaling());
		
		/* Write the device's ray direction: */
		Vector rayDirection=Vector(device->getDeviceRayDirection());
		pipe.write<Scalar>(rayDirection.getComponents(),3);
		
		/* Write the device's button states: */
		unsigned char bitMask=0x0;
		int bit=0x1;
		for(int i=0;i<device->getNumButtons();++i)
			{
			/* Add the button state if the button is enabled: */
			if(lds->buttonMasks[i]&&device->getButtonState(i))
				bitMask|=bit;
			bit<<=1;
			if(bit==0x100)
				{
				pipe.write<unsigned char>(bitMask);
				bitMask=0x0;
				bit=0x1;
				}
			}
		if(bit!=0x1)
			pipe.write<unsigned char>(bitMask);
		
		/* Write the device's valuator states: */
		for(int i=0;i<device->getNumValuators();++i)
			{
			/* Write the valuator's value if it is enabled, or zero otherwise: */
			pipe.write<Scalar>(lds->valuatorMasks[i]?device->getValuator(i):Scalar(0));
			}
		}
	
	}
	}

ProtocolClient::RemoteClientState* CheriaClient::receiveClientConnect(CollaborationPipe& pipe)
	{
	/* Create a new remote client state object: */
	RemoteClientState* newClientState=new RemoteClientState(*this);
	
	/* Read the size of the following message: */
	unsigned int messageSize=pipe.read<unsigned int>();
	
	#if DEBUGGING
	std::cout<<"Received client connect message of size "<<messageSize<<std::endl;
	#endif
	
	/* Read the entire message into a new read buffer that has the same endianness as the pipe's read end: */
	Misc::ReadBuffer* buffer=new Misc::ReadBuffer(messageSize,pipe.mustSwapOnRead());
	buffer->readFromSource(pipe);
	
	/* Store the new buffer in the client's message list: */
	newClientState->messageBuffers.push_back(buffer);
	
	return newClientState;
	}

void CheriaClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe)
	{
	/* Get a handle on the remote client state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("CheriaClient::receiveServerUpdate: Mismatching remote client state object type");
	
	/* Read the size of the following message: */
	unsigned int messageSize=pipe.read<unsigned int>();
	
	/* Read the entire message into a new read buffer that has the same endianness as the pipe's read end: */
	Misc::ReadBuffer* buffer=new Misc::ReadBuffer(messageSize,pipe.mustSwapOnRead());
	buffer->readFromSource(pipe);
	
	/* Store the new buffer in the client's message list: */
	{
	Threads::Mutex::Lock messageBufferLock(myRcs->messageBufferMutex);
	myRcs->messageBuffers.push_back(buffer);
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
