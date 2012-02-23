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

#ifndef COLLABORATION_AGORASERVER_INCLUDED
#define COLLABORATION_AGORASERVER_INCLUDED

#include <Threads/TripleBuffer.h>
#include <Threads/DropoutBuffer.h>
#include <Collaboration/ProtocolServer.h>
#include <Collaboration/AgoraProtocol.h>

namespace Collaboration {

class AgoraServer:public ProtocolServer,private AgoraProtocol
	{
	/* Embedded classes: */
	protected:
	class ClientState:public ProtocolServer::ClientState
		{
		friend class AgoraServer;
		
		/* Elements: */
		private:
		Point mouthPosition; // Client's mouth position in its main viewer's device space
		size_t speexFrameSize; // Client's SPEEX frame size
		size_t speexPacketSize; // Client's SPEEX packet size
		Threads::DropoutBuffer<Byte> speexPacketBuffer; // Buffer holding encoded SPEEX audio packets sent by the client
		
		bool hasTheora; // Flag whether the client is streaming video data
		ONTransform videoTransform; // Transformation from client's video space to client's physical space
		Scalar videoSize[2]; // Client's virtual video size in client's physical space
		size_t theoraHeadersSize; // Size of the client's Theora stream header packets
		Byte* theoraHeaders; // A little-endian buffer containing the clients Theora stream header packets
		Threads::TripleBuffer<VideoPacket> theoraPacketBuffer; // Triple buffer containing encoded video frames from the client
		
		size_t numSpeexPackets; // Transient number of SPEEX packets in the packet queue during server updates
		bool hasTheoraPacket; // Transient flag to denote a fresh Theora frame in the packet buffer during server updates
		
		/* Constructors and destructors: */
		public:
		ClientState(void);
		virtual ~ClientState(void);
		};
	
	/* Constructors and destructors: */
	public:
	AgoraServer(void); // Creates an Agora server object
	virtual ~AgoraServer(void); // Destroys the Agora server object
	
	/* Methods from ProtocolServer: */
	virtual const char* getName(void) const;
	virtual ProtocolServer::ClientState* receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe);
	virtual void receiveClientUpdate(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe);
	virtual void sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual void beforeServerUpdate(ProtocolServer::ClientState* cs);
	virtual void afterServerUpdate(ProtocolServer::ClientState* cs);
	};

}

#endif
