/***********************************************************************
EmineoServer - Server object to implement the Emineo 3D video tele-
immersion protocol.
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

#ifndef EMINEOSERVER_INCLUDED
#define EMINEOSERVER_INCLUDED

#include <string>
#include <Comm/TCPPipe.h>

#include <Collaboration/ProtocolServer.h>

#include <Collaboration/EmineoPipe.h>

namespace Collaboration {

class EmineoServer:public ProtocolServer,public EmineoPipe
	{
	/* Embedded classes: */
	protected:
	class ClientState:public ProtocolServer::ClientState
		{
		friend class EmineoServer;
		
		/* Elements: */
		private:
		std::string gatewayHostname;
		int gatewayPort;
		bool hasSource; // Flag whether the client has a video source
		OGTransform cameraTransform; // The client's current transformation from 3D video coordinates to navigational coordinates
		
		/* Constructors and destructors: */
		public:
		ClientState(const std::string& sGatewayHostname,int sGatewayPort);
		virtual ~ClientState(void);
		};
	
	/* Constructors and destructors: */
	public:
	EmineoServer(void); // Creates an Emineo server object
	virtual ~EmineoServer(void); // Destroys the Emineo server object
	
	/* Methods from ProtocolServer: */
	virtual const char* getName(void) const;
	virtual ProtocolServer::ClientState* receiveConnectRequest(unsigned int protocolMessageLength,CollaborationPipe& pipe);
	virtual void receiveClientUpdate(ProtocolServer::ClientState* cs,CollaborationPipe& pipe);
	virtual void sendClientConnect(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	virtual void sendServerUpdate(ProtocolServer::ClientState* sourceCs,ProtocolServer::ClientState* destCs,CollaborationPipe& pipe);
	};

}

#endif
