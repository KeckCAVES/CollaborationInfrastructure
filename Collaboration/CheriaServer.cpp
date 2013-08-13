/***********************************************************************
CheriaServer - Server object to implement the Cheria input device
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

#include <Collaboration/CheriaServer.h>

#if DEBUGGING
#include <iostream>
#endif
#include <Misc/ThrowStdErr.h>
#include <Comm/NetPipe.h>

namespace Collaboration {

/******************************************
Methods of class CheriaServer::ClientState:
******************************************/

CheriaServer::ClientState::ClientState(void)
	:clientDevices(17),clientTools(17)
	{
	}

CheriaServer::ClientState::~ClientState(void)
	{
	/* Delete all device states: */
	for(ClientDeviceMap::Iterator cdIt=clientDevices.begin();!cdIt.isFinished();++cdIt)
		delete cdIt->getDest();
	
	/* Delete all tool states: */
	for(ClientToolMap::Iterator ctIt=clientTools.begin();!ctIt.isFinished();++ctIt)
		delete ctIt->getDest();
	}

/*****************************
Methods of class CheriaServer:
*****************************/

CheriaServer::CheriaServer(void)
	{
	}

CheriaServer::~CheriaServer(void)
	{
	}

const char* CheriaServer::getName(void) const
	{
	return protocolName;
	}

unsigned int CheriaServer::getNumMessages(void) const
	{
	return MESSAGES_END;
	}

ProtocolServer::ClientState* CheriaServer::receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe)
	{
	#if DEBUGGING
	std::cout<<"CheriaServer::receiveConnectRequest"<<std::endl<<std::flush;
	#endif
	
	/* Check the protocol message length: */
	if(protocolMessageLength!=sizeof(Card))
		{
		/* Fatal error; stop communicating with client entirely: */
		Misc::throwStdErr("CheriaServer::receiveConnectRequest: Protocol error; received %u bytes instead of %u",protocolMessageLength,(unsigned int)sizeof(Card));
		}
	
	/* Read the client's protocol version: */
	unsigned int clientProtocolVersion=pipe.read<Card>();
	
	/* Check for the correct version number: */
	if(clientProtocolVersion==protocolVersion)
		{
		/* Create the new client state object and set its message buffer's endianness: */
		ClientState* result=new ClientState;
		result->messageBuffer.setSwapOnWrite(pipe.mustSwapOnWrite());
		
		return result;
		}
	else
		return 0;
	}

void CheriaServer::receiveClientUpdate(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	/* Get a handle on the Cheria state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("CheriaServer::receiveClientUpdate: Client state object has mismatching type");
	
	/* Read all messages from the pipe: */
	bool goOn=true;
	while(goOn)
		{
		/* Read and handle the next message: */
		switch(readMessage(pipe))
			{
			case CREATE_DEVICE:
				{
				/* Read the new device's ID: */
				unsigned int newDeviceId=pipe.read<Card>();
				
				#if DEBUGGING
				std::cout<<"CREATE_DEVICE "<<newDeviceId<<"..."<<std::flush;
				#endif
				
				/* Create the new device: */
				DeviceState* newDevice=new DeviceState(pipe);
				
				/* Store the new device in the client's device map: */
				myCs->clientDevices[newDeviceId]=newDevice;
				
				/* Append a creation message to the client's outgoing buffer: */
				writeMessage(CREATE_DEVICE,myCs->messageBuffer);
				myCs->messageBuffer.write<Card>(newDeviceId);
				newDevice->writeLayout(myCs->messageBuffer);
				
				#if DEBUGGING
				std::cout<<" "<<newDevice->numButtons<<", "<<newDevice->numValuators<<std::endl<<std::flush;
				#endif
				
				break;
				}
			
			case DESTROY_DEVICE:
				{
				/* Read the device's ID: */
				unsigned int deviceId=pipe.read<Card>();
				
				#if DEBUGGING
				std::cout<<"DESTROY_DEVICE "<<deviceId<<std::endl;
				#endif
				
				/* Erase the device from the client's device map: */
				ClientDeviceMap::Iterator cdIt=myCs->clientDevices.findEntry(deviceId);
				if(!cdIt.isFinished())
					{
					/* Delete the device: */
					delete cdIt->getDest();
					myCs->clientDevices.removeEntry(cdIt);
					}
				
				/* Append the message to the client's outgoing buffer: */
				writeMessage(DESTROY_DEVICE,myCs->messageBuffer);
				myCs->messageBuffer.write<Card>(deviceId);
				
				break;
				}
			
			case CREATE_TOOL:
				{
				/* Read the new tool's ID: */
				unsigned int newToolId=pipe.read<Card>();
				
				#if DEBUGGING
				std::cout<<"CREATE_TOOL "<<newToolId<<"..."<<std::flush;
				#endif
				
				/* Create the new tool: */
				ToolState* newTool=new ToolState(pipe);
				
				/* Store the new tool in the client's tool map: */
				myCs->clientTools[newToolId]=newTool;
				
				/* Append the message to the client's outgoing buffer: */
				writeMessage(CREATE_TOOL,myCs->messageBuffer);
				myCs->messageBuffer.write<Card>(newToolId);
				newTool->write(myCs->messageBuffer);
				
				#if DEBUGGING
				std::cout<<" "<<newTool->numButtonSlots<<", "<<newTool->numValuatorSlots<<std::endl<<std::flush;
				#endif
				
				break;
				}
			
			case DESTROY_TOOL:
				{
				/* Read the tool's ID: */
				unsigned int toolId=pipe.read<Card>();
				
				#if DEBUGGING
				std::cout<<"DESTROY_TOOL "<<toolId<<std::endl;
				#endif
				
				/* Erase the tool from the client's tool map: */
				ClientToolMap::Iterator ctIt=myCs->clientTools.findEntry(toolId);
				if(!ctIt.isFinished())
					{
					/* Delete the tool: */
					delete ctIt->getDest();
					myCs->clientTools.removeEntry(ctIt);
					}
				
				/* Append the message to the client's outgoing buffer: */
				writeMessage(DESTROY_TOOL,myCs->messageBuffer);
				myCs->messageBuffer.write<Card>(toolId);
				
				break;
				}
			
			case DEVICE_STATES:
				{
				/* Read all contained status messages: */
				unsigned int deviceId;
				while((deviceId=pipe.read<Card>())!=0)
					{
					/* Update the device state: */
					myCs->clientDevices.getEntry(deviceId).getDest()->read(pipe);
					}
				
				/* This is the last message: */
				goOn=false;
				break;
				}
			
			default:
				Misc::throwStdErr("CheriaServer::receiveClientUpdate: received unknown message");
			}
		}
	}

void CheriaServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	/* Get handles on the Cheria state objects: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("CheriaServer::sendClientConnect: Client state object has mismatching type");
	
	#if DEBUGGING
	std::cout<<"CheriaServer::sendClientConnect..."<<std::flush;
	#endif
	
	/* Create a temporary message buffer with the same endianness as the pipe's write end: */
	MessageBuffer buffer;
	buffer.setSwapOnWrite(pipe.mustSwapOnWrite());
	
	/*********************************************************************
	Assemble the update message in the temporary buffer:
	*********************************************************************/
	
	/* Send creation messages for the source client's devices to the destination client: */
	for(ClientDeviceMap::Iterator cdIt=mySourceCs->clientDevices.begin();!cdIt.isFinished();++cdIt)
		{
		writeMessage(CREATE_DEVICE,buffer);
		buffer.write<Card>(cdIt->getSource());
		cdIt->getDest()->writeLayout(buffer);
		}
	
	/* Send creation messages for the source client's tools to the destination client: */
	for(ClientToolMap::Iterator ctIt=mySourceCs->clientTools.begin();!ctIt.isFinished();++ctIt)
		{
		writeMessage(CREATE_TOOL,buffer);
		buffer.write<Card>(ctIt->getSource());
		ctIt->getDest()->write(buffer);
		}
	
	/* Send the current states of the source client's devices: */
	writeMessage(DEVICE_STATES,buffer);
	for(ClientDeviceMap::Iterator cdIt=mySourceCs->clientDevices.begin();!cdIt.isFinished();++cdIt)
		{
		/* Send a device state message: */
		buffer.write<Card>(cdIt->getSource());
		cdIt->getDest()->write(DeviceState::FULL_UPDATE,buffer);
		}
	buffer.write<Card>(0);
	
	/*********************************************************************
	Send the assembled message to the client in one go:
	*********************************************************************/
	
	#if DEBUGGING
	std::cout<<" message size "<<buffer.getDataSize()<<std::endl;
	#endif
	
	/* Write the message's total size first: */
	pipe.write<Card>(buffer.getDataSize());
	
	/* Write the message itself: */
	buffer.writeToSink(pipe);
	}

void CheriaServer::beforeServerUpdate(ProtocolServer::ClientState* cs)
	{
	/* Get a handle on the Cheria state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("CheriaServer::beforeServerUpdate: Client state object has mismatching type");
	
	/* Send the current states of the source client's managed input devices: */
	writeMessage(DEVICE_STATES,myCs->messageBuffer);
	for(ClientDeviceMap::Iterator cdIt=myCs->clientDevices.begin();!cdIt.isFinished();++cdIt)
		{
		if(cdIt->getDest()->updateMask!=DeviceState::NO_CHANGE)
			{
			/* Send a device state message: */
			myCs->messageBuffer.write<Card>(cdIt->getSource());
			cdIt->getDest()->write(cdIt->getDest()->updateMask,myCs->messageBuffer);
			
			/* Reset the device's update mask: */
			cdIt->getDest()->updateMask=DeviceState::NO_CHANGE;
			}
		}
	
	/* Terminate the device state update message: */
	myCs->messageBuffer.write<Card>(0);
	}

void CheriaServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	/* Get handles on the Cheria state objects: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("CheriaServer::sendServerUpdate: Client state object has mismatching type");
	
	/*********************************************************************
	Send the source client's accumulated state tracking messages to the
	destination client:
	*********************************************************************/
	
	/* Send the total size of the message first: */
	pipe.write<Card>(mySourceCs->messageBuffer.getDataSize());
	
	/* Write the message itself: */
	mySourceCs->messageBuffer.writeToSink(pipe);
	}

void CheriaServer::afterServerUpdate(ProtocolServer::ClientState* cs)
	{
	/* Get a handle on the Cheria state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("CheriaServer::afterServerUpdate: Client state object has mismatching type");
	
	/* Clear the client's message buffer: */
	myCs->messageBuffer.clear();
	}

}

/****************
DSO entry points:
****************/

extern "C" {

Collaboration::ProtocolServer* createObject(Collaboration::ProtocolServerLoader& objectLoader)
	{
	return new Collaboration::CheriaServer;
	}

void destroyObject(Collaboration::ProtocolServer* object)
	{
	delete object;
	}

}
