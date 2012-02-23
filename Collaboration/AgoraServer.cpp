/***********************************************************************
AgoraServer - Server object to implement the Agora group audio protocol.
Copyright (c) 2009-2012 Oliver Kreylos

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
#include <Comm/NetPipe.h>
#include <Geometry/GeometryMarshallers.h>

namespace Collaboration {

/*****************************************
Methods of class AgoraServer::ClientState:
*****************************************/

AgoraServer::ClientState::ClientState(void)
	:speexFrameSize(0),
	 speexPacketSize(0),speexPacketBuffer(0,0),
	 theoraHeaders(0)
	{
	}

AgoraServer::ClientState::~ClientState(void)
	{
	delete[] theoraHeaders;
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

ProtocolServer::ClientState* AgoraServer::receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe)
	{
	size_t readMessageLength=0;
	
	/* Receive the client's protocol version: */
	unsigned int clientProtocolVersion=pipe.read<Card>();
	readMessageLength+=sizeof(Card);
	
	/* Check for the correct version number: */
	if(clientProtocolVersion!=protocolVersion)
		return 0;
	
	/* Create a new client state object: */
	ClientState* newClientState=new ClientState;
	
	/* Read the client's mouth position: */
	read(newClientState->mouthPosition,pipe);
	readMessageLength+=sizeof(Point::Scalar)*3;
	
	/* Read the SPEEX frame size, packet size, and packet buffer size: */
	newClientState->speexFrameSize=pipe.read<Card>();
	newClientState->speexPacketSize=pipe.read<Card>();
	size_t speexPacketBufferSize=pipe.read<Card>();
	newClientState->speexPacketBuffer.resize(newClientState->speexPacketSize,speexPacketBufferSize);
	readMessageLength+=sizeof(Card)*3;
	
	/* Read the Theora validity flag: */
	newClientState->hasTheora=pipe.read<Byte>()!=0;
	readMessageLength+=sizeof(Byte);
	
	if(newClientState->hasTheora)
		{
		/* Read the client's video transformation: */
		read(newClientState->videoTransform,pipe);
		readMessageLength+=Misc::Marshaller<ONTransform>::getSize(newClientState->videoTransform);
		pipe.read(newClientState->videoSize,2);
		readMessageLength+=sizeof(Scalar)*2;
		
		/* Read the client's Theora video stream headers: */
		newClientState->theoraHeadersSize=pipe.read<Card>();
		readMessageLength+=sizeof(Card);
		newClientState->theoraHeaders=new Byte[newClientState->theoraHeadersSize];
		pipe.read(newClientState->theoraHeaders,newClientState->theoraHeadersSize);
		readMessageLength+=newClientState->theoraHeadersSize;
		}
	
	/* Check for correctness: */
	if(protocolMessageLength!=readMessageLength)
		{
		/* Fatal error; stop communicating with client entirely: */
		delete newClientState;
		Misc::throwStdErr("AgoraServer::receiveConnectRequest: Protocol error; received %u bytes instead of %u",protocolMessageLength,(unsigned int)readMessageLength);
		}
	
	/* Return the client state object: */
	return newClientState;
	}

void AgoraServer::receiveClientUpdate(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe)
	{
	/* Get a handle on the Agora state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("AgoraServer::receiveClientUpdate: Client state object has mismatching type");
	
	if(myCs->speexFrameSize>0)
		{
		/* Read all SPEEX frames sent by the client: */
		size_t numSpeexFrames=pipe.read<Misc::UInt16>();
		for(size_t i=0;i<numSpeexFrames;++i)
			{
			Byte* speexPacket=myCs->speexPacketBuffer.getWriteSegment();
			pipe.read(speexPacket,myCs->speexPacketSize);
			myCs->speexPacketBuffer.pushSegment();
			}
		}
	
	if(myCs->hasTheora)
		{
		/* Check if the client sent a new video packet: */
		if(pipe.read<Byte>()!=0)
			{
			/* Read a Theora packet from the client: */
			VideoPacket& theoraPacket=myCs->theoraPacketBuffer.startNewValue();
			theoraPacket.read(pipe);
			myCs->theoraPacketBuffer.postNewValue();
			}
		}
	}

void AgoraServer::sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	/* Get a handle on the Agora state object: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	if(mySourceCs==0)
		Misc::throwStdErr("AgoraServer::sendClientConnect: Client state object has mismatching type");
	
	/* Send the client's mouth position: */
	write(mySourceCs->mouthPosition,pipe);
	
	/* Send the client's SPEEX frame size and packet size: */
	pipe.write<Card>(mySourceCs->speexFrameSize);
	pipe.write<Card>(mySourceCs->speexPacketSize);
	
	if(mySourceCs->hasTheora)
		{
		pipe.write<Byte>(1);
		
		/* Write the client's virtual video transformation: */
		write(mySourceCs->videoTransform,pipe);
		pipe.write(mySourceCs->videoSize,2);
		
		/* Write the source client's Theora stream headers: */
		pipe.write<Card>(mySourceCs->theoraHeadersSize);
		pipe.write(mySourceCs->theoraHeaders,mySourceCs->theoraHeadersSize);
		}
	else
		pipe.write<Byte>(0);
	}

void AgoraServer::sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe)
	{
	/* Get a handle on the Agora state object: */
	ClientState* mySourceCs=dynamic_cast<ClientState*>(sourceCs);
	if(mySourceCs==0)
		Misc::throwStdErr("AgoraServer::sendServerUpdate: Client state object has mismatching type");
	
	if(mySourceCs->speexFrameSize>0)
		{
		/* Send all SPEEX packets from the source client's packet buffer to the destination client: */
		pipe.write<Misc::UInt16>(mySourceCs->numSpeexPackets);
		for(size_t i=0;i<mySourceCs->numSpeexPackets;++i)
			{
			const Byte* speexPacket=mySourceCs->speexPacketBuffer.getLockedSegment(i);
			pipe.write(speexPacket,mySourceCs->speexPacketSize);
			}
		}
	
	/* Check if the destination client expects streaming video from the source client: */
	if(mySourceCs->hasTheora)
		{
		/* Check if there is a new video packet for the client: */
		if(mySourceCs->hasTheoraPacket)
			{
			/* Write the Theora packet to the client: */
			pipe.write<Byte>(1);
			mySourceCs->theoraPacketBuffer.getLockedValue().write(pipe);
			}
		else
			pipe.write<Byte>(0);
		}
	}

void AgoraServer::beforeServerUpdate(ProtocolServer::ClientState* cs)
	{
	/* Get a handle on the Agora state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("AgoraServer::beforeServerUpdate: Client state object has mismatching type");
	
	/* Lock the available SPEEX packets: */
	myCs->numSpeexPackets=myCs->speexFrameSize>0?myCs->speexPacketBuffer.lockQueue():0;
	
	/* Check if there is a new Theora packet in the receiving buffer: */
	myCs->hasTheoraPacket=myCs->hasTheora&&myCs->theoraPacketBuffer.lockNewValue();
	}

void AgoraServer::afterServerUpdate(ProtocolServer::ClientState* cs)
	{
	/* Get a handle on the Agora state object: */
	ClientState* myCs=dynamic_cast<ClientState*>(cs);
	if(myCs==0)
		Misc::throwStdErr("AgoraServer::afterServerUpdate: Client state object has mismatching type");
	
	/* Unlock the SPEEX packet buffer: */
	if(myCs->speexFrameSize>0)
		myCs->speexPacketBuffer.unlockQueue();
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
