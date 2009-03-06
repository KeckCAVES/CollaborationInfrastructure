/***********************************************************************
EmineoClient - Client object to implement the Emineo 3D video tele-
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

#ifndef EMINEOCLIENT_INCLUDED
#define EMINEOCLIENT_INCLUDED

#include <string>
#include <Threads/Thread.h>
#include <Threads/TripleBuffer.h>
#include <Comm/MulticastPipe.h>
#include <Geometry/OrthogonalTransformation.h>

#include <Collaboration/ProtocolClient.h>

#include <Collaboration/EmineoPipe.h>

/* Forward declarations: */
namespace emineo {
class FacadeGroup;
class Renderer;
}

namespace Collaboration {

class EmineoClient:public ProtocolClient,public EmineoPipe
	{
	/* Embedded classes: */
	protected:
	class RemoteClientState:public ProtocolClient::RemoteClientState
		{
		/* Elements: */
		public:
		std::string gatewayHostname;
		int gatewayPort;
		bool hasSource; // Flag if the remote client was supposed to have a source, even if the connection actually failed
		Threads::TripleBuffer<OGTransform> cameraTransform; // The current 3D video camera transformation of the remote client
		
		/* Elements from actual Emineo 3D video rendering engine: */
		emineo::FacadeGroup* facades; // A collection of 3D camera images provided by the remote client's 3D video gateway
		emineo::Renderer* renderer; // A renderer for the facade group
		Threads::Thread initializeEmineoThread; // A background thread to initialize the remote client's Emineo renderer without delay
		
		/* Constructors and destructors: */
		RemoteClientState(const std::string& sGatewayHostname,int sGatewayPort,Comm::MulticastPipe* mcPipe); // Connects a TCP pipe to the remote client's 3D video gateway
		virtual ~RemoteClientState(void); // Shuts down the 3D video pipe
		
		/* Methods: */
		void* initializeEmineoThreadMethod(void); // Background thread method to initialize the Emineo renderer for this remote client
		};
	
	/* Elements: */
	private:
	std::string gatewayHostname; // Hostname of client's 3D video gateway
	int gatewayPort; // Port number of same
	bool hasSource; // Flag whether this client has a 3D video source
	OGTransform cameraTransformation; // Transformation from camera space to client's Vrui physical coordinates
	
	/* Constructors and destructors: */
	public:
	EmineoClient(void); // Creates an Emineo client with settings read from an Emineo.cfg configuration file
	virtual ~EmineoClient(void); // Destroys the Emineo client
	
	/* Methods from ProtocolClient: */
	virtual const char* getName(void) const;
	virtual void sendConnectRequest(CollaborationPipe& pipe);
	virtual void sendClientUpdate(CollaborationPipe& pipe);
	virtual RemoteClientState* receiveClientConnect(CollaborationPipe& pipe);
	virtual void receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe);
	virtual void frame(ProtocolClient::RemoteClientState* rcs);
	virtual void display(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const;
	};

}

#endif
