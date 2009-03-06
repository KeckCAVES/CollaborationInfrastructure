/***********************************************************************
FooServer - Dummy protocol plug-in class to stress-test the plug-in
mechanism.
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

#ifndef FOOSERVER_INCLUDED
#define FOOSERVER_INCLUDED

#include <Collaboration/ProtocolServer.h>

namespace Collaboration {

class FooServer:public ProtocolServer
	{
	/* Embedded classes: */
	protected:
	class ClientState:public ProtocolServer::ClientState // Class representing server-side state of a connected client
		{
		/* Constructors and destructors: */
		public:
		ClientState(void);
		virtual ~ClientState(void);
		};
	
	/* Constructors and destructors: */
	public:
	FooServer(void);
	virtual ~FooServer(void);
	
	/* Methods from ProtocolServer: */
	virtual const char* getName(void) const;
	virtual unsigned int getNumMessages(void) const;
	virtual ProtocolServer::ClientState* receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe);
	virtual void sendConnectReply(ProtocolServer::ClientState* cs,CollaborationPipe& pipe);
	virtual void sendConnectReject(ProtocolServer::ClientState* cs,CollaborationPipe& pipe);
	virtual void receiveDisconnectRequest(ProtocolServer::ClientState* cs,CollaborationPipe& pipe);
	virtual void sendDisconnectReply(ProtocolServer::ClientState* cs,CollaborationPipe& pipe);
	virtual void receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe);
	virtual void sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual void sendClientDisconnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual bool handleMessage(ProtocolServer::ClientState* cs,unsigned int messageId,CollaborationPipe& pipe);
	virtual void connectClient(ProtocolServer::ClientState* cs);
	virtual void disconnectClient(ProtocolServer::ClientState* cs);
	virtual void beforeServerUpdate(ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual void afterServerUpdate(ProtocolServer::ClientState* cs);
	};

}

#endif
