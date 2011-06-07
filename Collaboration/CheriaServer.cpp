/***********************************************************************
CheriaServer - Server object to implement the Cheria input device
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

#include <Collaboration/CheriaServer.h>

#if DEBUGGING
#include <iostream>
#endif
#include <Misc/ThrowStdErr.h>

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
	/* Delete all server device states: */
	for(ClientDeviceMap::Iterator cdIt=clientDevices.begin();!cdIt.isFinished();++cdIt)
		delete cdIt->getDest();
	
	/* Delete all server tool states: */
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

ProtocolServer::ClientState* CheriaServer::receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe)
	{
	/* Check the protocol message length: */
	if(protocolMessageLength!=sizeof(unsigned int))
		{
		/* Fatal error; stop communicating with client entirely: */
		Misc::throwStdErr("CheriaServer::receiveConnectRequest: Protocol error; received %u bytes instead of %u",(unsigned int)protocolMessageLength,(unsigned int)sizeof(unsigned int));
		}
	
	/* Read the client's protocol version: */
	unsigned int clientProtocolVersion=pipe.read<unsigned int>();
	
	/* Check for the correct version number: */
	if(clientProtocolVersion==protocolVersion)
		{
		/* Create the new client state object and set its message buffer's endianness: */
		ClientState* result=new ClientState;
		result->messageBuffer.setEndianness(pipe.mustSwapOnWrite());
		
		return result;
		}
	else
		return 0;
	}

void CheriaServer::receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Cheria state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("CheriaServer::receiveClientUpdate: Client state object has mismatching type");
	
	/* Read all messages from the pipe: */
	bool goOn=true;
	while(goOn)
		{
		/* Read the next message: */
		MessageIdType message=pipe.readMessage();
		
		switch(message)
			{
			case CREATE_DEVICE:
				{
				/* Read the new device's ID: */
				unsigned int newDeviceId=pipe.read<unsigned int>();
				
				#if DEBUGGING
				std::cout<<"CREATE_DEVICE "<<newDeviceId<<std::endl;
				#endif
				
				/* Create the new device: */
				ServerDeviceState* newDevice=new ServerDeviceState(pipe);
				
				/* Store the new device in the client's device map: */
				myCs->clientDevices.setEntry(ClientDeviceMap::Entry(newDeviceId,newDevice));
				
				/* Append a creation message to the client's outgoing buffer: */
				myCs->messageBuffer.write<MessageIdType>(CREATE_DEVICE);
				myCs->messageBuffer.write<unsigned int>(newDeviceId);
				newDevice->writeLayout(myCs->messageBuffer);
				
				break;
				}
			
			case DESTROY_DEVICE:
				{
				/* Read the device's ID: */
				unsigned int deviceId=pipe.read<unsigned int>();
				
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
				myCs->messageBuffer.write<MessageIdType>(DESTROY_DEVICE);
				myCs->messageBuffer.write<unsigned int>(deviceId);
				
				break;
				}
			
			case CREATE_TOOL:
				{
				/* Read the new tool's ID: */
				unsigned int newToolId=pipe.read<unsigned int>();
				
				#if DEBUGGING
				std::cout<<"CREATE_TOOL "<<newToolId<<std::endl;
				#endif
				
				/* Create the new tool: */
				ServerToolState* newTool=new ServerToolState(pipe);
				
				/* Store the new tool in the client's tool map: */
				myCs->clientTools.setEntry(ClientToolMap::Entry(newToolId,newTool));
				
				/* Append the message to the client's outgoing buffer: */
				myCs->messageBuffer.write<MessageIdType>(CREATE_TOOL);
				myCs->messageBuffer.write<unsigned int>(newToolId);
				newTool->writeLayout(myCs->messageBuffer);
				
				break;
				}
			
			case DESTROY_TOOL:
				{
				/* Read the tool's ID: */
				unsigned int toolId=pipe.read<unsigned int>();
				
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
				myCs->messageBuffer.write<MessageIdType>(DESTROY_TOOL);
				myCs->messageBuffer.write<unsigned int>(toolId);
				
				break;
				}
			
			case DEVICE_STATES:
				{
				/* Read device states for all devices currently managed by the client: */
				for(size_t i=0;i<myCs->clientDevices.getNumEntries();++i)
					{
					unsigned int deviceId=pipe.read<unsigned int>();
					
					ServerDeviceState* device=myCs->clientDevices.getEntry(deviceId).getDest();
					device->read(pipe);
					}
				
				/* This is the last message: */
				goOn=false;
				break;
				}
			}
		}
	}

void CheriaServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	/* Get handles on the Cheria state objects: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	ClientState* myDestCs=dynamic_cast<ClientState*>(destCs);
	if(mySourceCs==0||myDestCs==0)
		Misc::throwStdErr("CheriaServer::sendClientConnect: Client state object has mismatching type");
	
	/* Create a temporary message buffer with the same endianness as the pipe's write end: */
	Misc::WriteBuffer buffer(pipe.mustSwapOnWrite());
	
	/*********************************************************************
	Assemble the update message in the temporary buffer:
	*********************************************************************/
	
	/* Send creation messages for the source client's "old" devices to the destination client: */
	for(ClientDeviceMap::Iterator cdIt=mySourceCs->clientDevices.begin();!cdIt.isFinished();++cdIt)
		if(!cdIt->getDest()->newDevice)
			{
			buffer.write<MessageIdType>(CREATE_DEVICE);
			buffer.write<unsigned int>(cdIt->getSource());
			cdIt->getDest()->writeLayout(buffer);
			}
	
	/* Send creation messages for the source client's "old" tools to the destination client: */
	for(ClientToolMap::Iterator ctIt=mySourceCs->clientTools.begin();!ctIt.isFinished();++ctIt)
		if(!ctIt->getDest()->newTool)
			{
			buffer.write<MessageIdType>(CREATE_TOOL);
			buffer.write<unsigned int>(ctIt->getSource());
			ctIt->getDest()->writeLayout(buffer);
			}
	
	/* Send the current states of the source client's "old" managed input devices: */
	buffer.write<MessageIdType>(DEVICE_STATES);
	for(ClientDeviceMap::Iterator cdIt=mySourceCs->clientDevices.begin();!cdIt.isFinished();++cdIt)
		if(!cdIt->getDest()->newDevice)
			{
			/* Send a device state message: */
			buffer.write<unsigned int>(cdIt->getSource());
			cdIt->getDest()->write(buffer);
			}
	
	/*********************************************************************
	Send the assembled message to the client in one go:
	*********************************************************************/
	
	#if DEBUGGING
	std::cout<<"Writing client connect message of size "<<buffer.getDataSize()<<std::endl;
	#endif
	
	/* Write the message's total size first: */
	pipe.write<unsigned int>(buffer.getDataSize());
	
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
	myCs->messageBuffer.write<MessageIdType>(DEVICE_STATES);
	for(ClientDeviceMap::Iterator cdIt=myCs->clientDevices.begin();!cdIt.isFinished();++cdIt)
		{
		/* Send a device state message: */
		myCs->messageBuffer.write<unsigned int>(cdIt->getSource());
		cdIt->getDest()->write(myCs->messageBuffer);
		}
	}

void CheriaServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
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
	pipe.write<unsigned int>(mySourceCs->messageBuffer.getDataSize());
	
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
	
	/* Mark the client's devices as old: */
	for(ClientDeviceMap::Iterator cdIt=myCs->clientDevices.begin();!cdIt.isFinished();++cdIt)
		cdIt->getDest()->newDevice=false;
	
	/* Mark the client's tools as old: */
	for(ClientToolMap::Iterator ctIt=myCs->clientTools.begin();!ctIt.isFinished();++ctIt)
		ctIt->getDest()->newTool=false;
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
