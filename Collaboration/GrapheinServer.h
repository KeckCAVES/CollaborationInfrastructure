/***********************************************************************
GrapheinServer - Server object to implement the Graphein shared
annotation protocol.
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

#ifndef GRAPHEINSERVER_INCLUDED
#define GRAPHEINSERVER_INCLUDED

#include <vector>
#include <Collaboration/ProtocolServer.h>

#include <Collaboration/GrapheinPipe.h>

namespace Collaboration {

class GrapheinServer:public ProtocolServer,public GrapheinPipe
	{
	/* Embedded classes: */
	protected:
	class ClientState:public ProtocolServer::ClientState
		{
		friend class GrapheinServer;
		
		/* Elements: */
		private:
		CurveHasher curves; // The set of curves currently owned by the client
		CurveActionList actions; // The queue of pending curve actions
		
		/* Constructors and destructors: */
		ClientState(void);
		virtual ~ClientState(void);
		};
	
	/* Constructors and destructors: */
	public:
	GrapheinServer(void); // Creates an Graphein server object
	virtual ~GrapheinServer(void); // Destroys the Graphein server object
	
	/* Methods from ProtocolServer: */
	virtual const char* getName(void) const;
	virtual ProtocolServer::ClientState* receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe);
	virtual void receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe);
	virtual void sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual void afterServerUpdate(ProtocolServer::ClientState* cs);
	};

}

#endif
