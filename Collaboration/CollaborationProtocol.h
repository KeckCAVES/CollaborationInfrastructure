/***********************************************************************
CollaborationProtocol - Class defining the communication protocol
between a collaboration client and a collaboration server.
Copyright (c) 2007-2011 Oliver Kreylos

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

#ifndef COLLABORATION_COLLABORATIONPROTOCOL_INCLUDED
#define COLLABORATION_COLLABORATIONPROTOCOL_INCLUDED

#include <string>
#include <Geometry/Plane.h>
#include <Collaboration/Protocol.h>

/* Forward declarations: */
namespace IO {
class File;
}

namespace Collaboration {

class CollaborationProtocol:public Protocol
	{
	/* Embedded classes: */
	public:
	enum MessageId // Enumerated type for protocol messages
		{
		CONNECT_REQUEST=0, // Request to connect to server
		CONNECT_REPLY, // Positive connect reply
		CONNECT_REJECT, // Negative connect reply
		DISCONNECT_REQUEST, // Polite request to disconnect from server
		DISCONNECT_REPLY, // Disconnect reply from server to cleanly shut down listening threads
		CLIENT_UPDATE, // Updates the connected client's state on the server side
		CLIENT_CONNECT, // Notifies connected clients that a new client has connected to the server
		CLIENT_DISCONNECT, // Notifies connected clients that another client has disconnected from the server
		SERVER_UPDATE, // Sends current state of all other connected clients to a connected client
		MESSAGES_END // First message ID that can be used by a higher-level protocol
		};
	
	typedef Geometry::Plane<Scalar,3> Plane; // Data type for plane equations
	
	struct ClientState // State of a client's environment synchronized between the server and all connected clients
		{
		/* Embedded classes: */
		public:
		enum UpdateMask // Enumerated type to denote which parts of a client's environment definition have changed
			{
			NO_CHANGE=0x0,     // No change in client state
			ENVIRONMENT=0x1,   // Any part of the physical-space environment definition changed
			CLIENTNAME=0x2,    // Client's display name changed
			NUM_VIEWERS=0x4,   // The number of viewers changed
			VIEWER=0x8,        // Any viewer changed position and/or orientation
			NAVTRANSFORM=0x10, // The navigation transformation changed
			FULL_UPDATE=0x1f   // Full initialization
			};
		
		/* Elements: */
		unsigned int updateMask; // Cumulative update mask of this client state
		
		/* Definition of client's physical environment in client's physical coordinate system: */
		Scalar inchFactor; // Length of one inch in client's physical coordinate units
		Point displayCenter; // Center point of client's environment
		Scalar displaySize; // Size of client's environment in client's physical coordinate units
		Vector forward,up; // Forward and up vectors of client's environment
		Plane floorPlane; // Plane equation of client's environment
		
		/* Client's display name: */
		std::string clientName;
		
		/* Definition of client's active viewers in client's physical coordinate system: */
		unsigned int numViewers; // Number of viewers defined by client
		ONTransform* viewerStates; // Positions and orientations of client's viewers
		
		/* Client's current navigation transformation: */
		OGTransform navTransform;
		
		/* Constructors and destructors: */
		ClientState(void); // Creates empty client state structure
		private:
		ClientState(const ClientState&); // Prohibit copy constructor
		public:
		ClientState& operator=(const ClientState&); // Assignment operator
		~ClientState(void);
		
		/* Methods: */
		bool resize(unsigned int newNumViewers); // Re-allocates the viewer state array; returns true if size changed
		};
	
	/* Methods: */
	static void readClientState(ClientState& clientState,IO::File& source); // Reads client state update from the given source
	static void writeClientState(unsigned int updateMask,const ClientState& clientState,IO::File& sink); // Writes client state update to the given sink using the specific state update mask
	};

}

#endif
