/***********************************************************************
GrapheinServer - Server object to implement the Graphein shared
annotation protocol.
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

#ifndef COLLABORATION_GRAPHEINSERVER_INCLUDED
#define COLLABORATION_GRAPHEINSERVER_INCLUDED

#include <vector>
#include <IO/VariableMemoryFile.h>
#include <Collaboration/ProtocolServer.h>
#include <Collaboration/GrapheinProtocol.h>

namespace Collaboration {

class GrapheinServer:public ProtocolServer,private GrapheinProtocol
	{
	/* Embedded classes: */
	private:
	typedef IO::VariableMemoryFile MessageBuffer; // Buffer to hold outgoing messages from a client between two updates
	
	class ClientState:public ProtocolServer::ClientState
		{
		friend class GrapheinServer;
		
		/* Elements: */
		private:
		CurveMap curves; // The set of curves currently owned by the client
		MessageBuffer messageBuffer; // Buffer for outgoing messages from this client
		
		/* Constructors and destructors: */
		ClientState(void);
		virtual ~ClientState(void);
		};
	
	/* Constructors and destructors: */
	public:
	GrapheinServer(void); // Creates a Graphein server object
	virtual ~GrapheinServer(void); // Destroys the Graphein server object
	
	/* Methods from ProtocolServer: */
	virtual const char* getName(void) const;
	virtual unsigned int getNumMessages(void) const;
	virtual ProtocolServer::ClientState* receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe);
	virtual void receiveClientUpdate(ProtocolServer::ClientState* cs,Comm::NetPipe& pipe);
	virtual void sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,Comm::NetPipe& pipe);
	virtual void afterServerUpdate(ProtocolServer::ClientState* cs);
	};

}

#endif
