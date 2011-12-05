/***********************************************************************
FooServer - Dummy protocol plug-in class to stress-test the plug-in
mechanism.
Copyright (c) 2009-2011 Oliver Kreylos

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

#ifndef COLLABORATION_FOOSERVER_INCLUDED
#define COLLABORATION_FOOSERVER_INCLUDED

#include <Collaboration/ProtocolServer.h>
#include <Collaboration/FooProtocol.h>

namespace Collaboration {

class FooServer:public ProtocolServer,private FooProtocol
	{
	/* Embedded classes: */
	protected:
	class ClientState:public ProtocolServer::ClientState // Class representing server-side state of a connected client
		{
		friend class FooServer;
		
		/* Elements: */
		private:
		unsigned int bracketLevel; // Counter to check proper nesting of before/after server update calls
		
		/* Constructors and destructors: */
		public:
		ClientState(void);
		virtual ~ClientState(void);
		};
	
	/* Elements: */
	protected:
	unsigned int bracketLevel; // Counter to check proper nesting of before/after server update calls
	
	/* Constructors and destructors: */
	public:
	FooServer(void);
	virtual ~FooServer(void);
	
	/* Methods from ProtocolServer: */
	virtual const char* getName(void) const;
	virtual unsigned int getNumMessages(void) const;
	virtual ProtocolServer::ClientState* receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe);
	virtual void sendConnectReply(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe);
	virtual void sendConnectReject(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe);
	virtual void receiveDisconnectRequest(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe);
	virtual void sendDisconnectReply(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe);
	virtual void receiveClientUpdate(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe);
	virtual void sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual bool handleMessage(ProtocolServer::ClientState* cs,unsigned int messageId,Comm::NetPipe& pipe);
	virtual void connectClient(ProtocolServer::ClientState* cs);
	virtual void disconnectClient(ProtocolServer::ClientState* cs);
	virtual void beforeServerUpdate(void);
	virtual void beforeServerUpdate(ProtocolServer::ClientState* cs);
	virtual void beforeServerUpdate(ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual void afterServerUpdate(ProtocolServer::ClientState* cs);
	virtual void afterServerUpdate(void);
	};

}

#endif
