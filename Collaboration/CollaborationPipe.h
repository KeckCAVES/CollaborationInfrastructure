/***********************************************************************
CollaborationPipe - Class defining the communication protocol between a
collaboration client and a collaboration server.
Copyright (c) 2007-2010 Oliver Kreylos

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

#ifndef COLLABORATIONPIPE_INCLUDED
#define COLLABORATIONPIPE_INCLUDED

#include <Comm/ClusterPipe.h>
#include <Geometry/Point.h>
#include <Geometry/Vector.h>
#include <Geometry/OrthogonalTransformation.h>

namespace Collaboration {

class CollaborationPipe:public Comm::ClusterPipe
	{
	/* Embedded classes: */
	public:
	typedef unsigned short int MessageIdType; // Network type for protocol messages
	
	enum MessageId // Enumerated type for protocol messages
		{
		CONNECT_REQUEST, // Request to connect to server
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
	
	typedef float Scalar; // Scalar type for transmitted geometric data
	typedef Geometry::Point<Scalar,3> Point; // Type for points
	typedef Geometry::Vector<Scalar,3> Vector; // Type for vectors
	typedef Geometry::OrthogonalTransformation<Scalar,3> OGTransform; // Type for rigid body transformations with uniform scaling
	
	struct ClientState // Transient client state transmitted with every client update and server update
		{
		/* Elements: */
		public:
		
		/* Definition of client's physical environment in client's navigational coordinates: */
		Point center; // Center point of client's environment
		Vector forward,up; // Forward and up vectors of client's environment
		Scalar size; // Size of client's environment
		Scalar inchScale; // Length of one inch in client's navigational coordinates
		
		/* Definition of client's active viewers and input devices in client's navigational coordinates: */
		unsigned int numViewers; // Number of viewers defined by client
		OGTransform* viewerStates; // States of client's viewers
		#ifdef COLLABORATION_SHARE_DEVICES
		unsigned int numInputDevices; // Number of input devices defined by client
		OGTransform* inputDeviceStates; // States of client's input devices
		#endif
		
		/* Constructors and destructors: */
		ClientState(void); // Creates empty client state structure
		private:
		ClientState(const ClientState&); // Prohibit copy constructor
		ClientState& operator=(const ClientState&); // Prohibit assignment operator
		public:
		~ClientState(void);
		
		/* Methods: */
		#ifdef COLLABORATION_SHARE_DEVICES
		ClientState& resize(unsigned int newNumViewers,unsigned int newNumInputDevices); // Re-allocates the viewer and input device state arrays
		#else
		ClientState& resize(unsigned int newNumViewers); // Re-allocates the viewer state array
		#endif
		ClientState& updateFromVrui(void); // Updates the client state structure by reading Vrui's internal data structures
		};
	
	/* Constructors and destructors: */
	public:
	CollaborationPipe(const char* hostName,int portID,Comm::MulticastPipe* sPipe =0); // Creates a collaboration pipe for the given server host name/port ID
	CollaborationPipe(const Comm::TCPSocket* sSocket,Comm::MulticastPipe* sPipe =0); // Creates a collaboration pipe for the given TCP socket
	
	/* Methods: */
	void writeMessage(MessageIdType messageId) // Writes a protocol message to the pipe
		{
		write<MessageIdType>(messageId);
		}
	MessageIdType readMessage(void) // Reads a protocol message from the pipe
		{
		return read<MessageIdType>();
		}
	void writeTrackerState(const OGTransform& trackerState); // Writes a tracker state to the pipe
	OGTransform readTrackerState(void); // Reads a tracker state from the pipe
	void writeClientState(const ClientState& clientState); // Writes transient client state to pipe
	ClientState& readClientState(ClientState& clientState); // Reads transient client state from pipe
	};

}

#endif
