/***********************************************************************
AgoraServer - Server object to implement the Agora group audio protocol.
Copyright (c) 2009 Oliver Kreylos

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

#include <Collaboration/AgoraServer.h>

#include <iostream>
#include <Misc/ThrowStdErr.h>

#include <Collaboration/CollaborationPipe.h>

namespace Collaboration {

/*****************************************
Methods of class AgoraServer::ClientState:
*****************************************/

AgoraServer::ClientState::ClientState(void)
	:speexFrameSize(0),
	 speexPacketSize(0),speexPacketBuffer(0,0)
	{
	}

AgoraServer::ClientState::~ClientState(void)
	{
	}

/****************************
Methods of class AgoraServer:
****************************/

AgoraServer::AgoraServer(void)
	{
	}

AgoraServer::~AgoraServer(void)
	{
	}

const char* AgoraServer::getName(void) const
	{
	return protocolName;
	}

ProtocolServer::ClientState* AgoraServer::receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe)
	{
	/* Create a new client state object: */
	ClientState* newClientState=new ClientState;
	
	/* Read the SPEEX frame size, packet size, and packet buffer size: */
	newClientState->speexFrameSize=pipe.read<unsigned int>();
	newClientState->speexPacketSize=pipe.read<unsigned int>();
	size_t speexPacketBufferSize=pipe.read<unsigned int>();
	newClientState->speexPacketBuffer.resize(newClientState->speexPacketSize,speexPacketBufferSize);
	size_t readMessageLength=sizeof(unsigned int)*3;
	
	/* Read the Theora validity flag: */
	newClientState->hasTheora=pipe.read<char>()!=0;
	readMessageLength+=sizeof(char);
	
	if(newClientState->hasTheora)
		{
		/* Read the client's Theora video stream headers: */
		for(int i=0;i<3;++i)
			{
			newClientState->theoraStreamHeaders[i].read(pipe);
			readMessageLength+=newClientState->theoraStreamHeaders[i].getNetworkSize();
			}
		
		/* Read the client's virtual video size: */
		for(int i=0;i<2;++i)
			newClientState->videoSize[i]=pipe.read<Scalar>();
		readMessageLength+=sizeof(Scalar)*2;
		}
	
	/* Check for correctness: */
	if(protocolMessageLength!=readMessageLength)
		{
		/* Must be a protocol error; return failure: */
		delete newClientState;
		return 0;
		}
	
	/* Return the client state object: */
	return newClientState;
	}

void AgoraServer::receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Agora state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("AgoraServer::receiveClientUpdate: Client state object has mismatching type");
	
	if(myCs->speexFrameSize>0)
		{
		/* Read all SPEEX frames sent by the client: */
		size_t numSpeexFrames=pipe.read<unsigned short>();
		for(size_t i=0;i<numSpeexFrames;++i)
			{
			char* speexPacket=myCs->speexPacketBuffer.getWriteSegment();
			pipe.read<char>(speexPacket,myCs->speexPacketSize);
			myCs->speexPacketBuffer.pushSegment();
			}
		}
	
	if(myCs->hasTheora)
		{
		/* Check if the client sent a new video packet: */
		if(pipe.read<char>()!=0)
			{
			/* Read a Theora packet from the client: */
			OggPacket& theoraPacket=myCs->theoraPacketBuffer.startNewValue();
			theoraPacket.read(pipe);
			myCs->theoraPacketBuffer.postNewValue();
			}
		
		/* Read the client's new video transformation: */
		myCs->videoTransform=pipe.readTrackerState();
		}
	}

void AgoraServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Agora state object: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	if(mySourceCs==0)
		Misc::throwStdErr("AgoraServer::sendClientConnect: Client state object has mismatching type");
	
	/* Send the client's SPEEX frame size and packet size: */
	pipe.write<unsigned int>(mySourceCs->speexFrameSize);
	pipe.write<unsigned int>(mySourceCs->speexPacketSize);
	
	if(mySourceCs->hasTheora)
		{
		pipe.write<char>(1);
		
		/* Write the source client's Theora stream headers: */
		for(int i=0;i<3;++i)
			mySourceCs->theoraStreamHeaders[i].write(pipe);
		
		/* Write the client's virtual video size: */
		for(int i=0;i<2;++i)
			pipe.write<Scalar>(mySourceCs->videoSize[i]);
		}
	else
		pipe.write<char>(0);
	}

void AgoraServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Agora state object: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	if(mySourceCs==0)
		Misc::throwStdErr("AgoraServer::sendServerUpdate: Client state object has mismatching type");
	
	if(mySourceCs->speexFrameSize>0)
		{
		/* Send all SPEEX packets from the source client's packet buffer to the destination client: */
		size_t numSpeexPackets=mySourceCs->speexPacketBuffer.getQueueSize();
		
		/* Send all packets from the SPEEX queue: */
		pipe.write<unsigned short>(numSpeexPackets);
		for(size_t i=0;i<numSpeexPackets;++i)
			{
			const char* speexPacket=mySourceCs->speexPacketBuffer.popSegment();
			pipe.write<char>(speexPacket,mySourceCs->speexPacketSize);
			}
		}
	
	/* Check if the destination client expects streaming video from the source client: */
	if(mySourceCs->hasTheora)
		{
		/* Check if there is a new video packet for the client: */
		if(mySourceCs->theoraPacketBuffer.lockNewValue())
			{
			/* Write the Theora packet to the client: */
			pipe.write<char>(1);
			mySourceCs->theoraPacketBuffer.getLockedValue().write(pipe);
			}
		else
			pipe.write<char>(0);
		
		/* Write the source client's new video transformation: */
		pipe.writeTrackerState(mySourceCs->videoTransform);
		}
	}

}

/****************
DSO entry points:
****************/

extern "C" {

Collaboration::ProtocolServer* createObject(Collaboration::ProtocolServerLoader& objectLoader)
	{
	return new Collaboration::AgoraServer;
	}

void destroyObject(Collaboration::ProtocolServer* object)
	{
	delete object;
	}

}
