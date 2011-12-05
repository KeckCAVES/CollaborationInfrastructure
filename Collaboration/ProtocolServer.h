/***********************************************************************
ProtocolServer - Abstract base class for the server-side components of
collaboration protocols that can be added to the base protocol
implemented by CollaborationClient and CollaborationServer, to simplify
creating complex higher-level protocols.
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

#ifndef COLLABORATION_PROTOCOLSERVER_INCLUDED
#define COLLABORATION_PROTOCOLSERVER_INCLUDED

/* Forward declarations: */
namespace Misc {
class ConfigurationFileSection;
}
namespace Plugins {
template <class ManagedClassParam>
class ObjectLoader;
}
namespace Comm {
class NetPipe;
}
namespace Collaboration {
class CollaborationServer;
}

namespace Collaboration {

class ProtocolServer
	{
	friend class CollaborationServer;
	
	/* Embedded classes: */
	protected:
	class ClientState // Class representing server-side state of a connected client
		{
		/* Constructors and destructors: */
		public:
		ClientState(void);
		virtual ~ClientState(void);
		};
	
	/* Elements: */
	protected:
	CollaborationServer* server; // Pointer to the server object
	unsigned int messageIdBase; // Base value for message IDs reserved for this protocol
	
	/* Constructors and destructors: */
	public:
	ProtocolServer(void);
	virtual ~ProtocolServer(void);
	
	/* Methods: */
	unsigned int getMessageIdBase(void) const // Returns the first message ID assigned to this protocol
		{
		return messageIdBase;
		}
	virtual const char* getName(void) const =0; // Returns the protocol's (hopefully unique) name
	virtual unsigned int getNumMessages(void) const; // Returns the number of protocol messages used by this protocol
	virtual void initialize(CollaborationServer* sServer,Misc::ConfigurationFileSection& configFileSection); // Called when the protocol server is registered with a collaboration server
	
	/***********************************
	Server protocol engine hook methods:
	***********************************/
	
	/* Hooks to add payloads to lower-level protocol messages: */
	virtual ClientState* receiveConnectRequest(unsigned int protocolMessageLength,Comm::NetPipe& pipe); // Hook called when the server receives a client's connection request; serrver rejects the request if the method returns 0
	virtual void sendConnectReply(ClientState* cs,Comm::NetPipe& pipe); // Hook called when the server replies to a client's connection request
	virtual void sendConnectReject(ClientState* cs,Comm::NetPipe& pipe); // Hook called when the server denies a client's connection request
	virtual void receiveDisconnectRequest(ClientState* cs,Comm::NetPipe& pipe); // Hook called when the server receives a disconnection request
	virtual void sendDisconnectReply(ClientState* cs,Comm::NetPipe& pipe); // Hook called when the server sends a disconnect reply to a client
	virtual void receiveClientUpdate(ClientState* cs,Comm::NetPipe& pipe); // Hook called when the server receives a client's state update packet
	virtual void sendClientConnect(ClientState* sourceCs,ClientState* destCs,Comm::NetPipe& pipe); // Hook called when the server sends a connection message for client sourceClient to client destClient
	virtual void sendServerUpdate(ClientState* destCs,Comm::NetPipe& pipe); // Hook called when the server sends a state update to a client
	virtual void sendServerUpdate(ClientState* sourceCs,ClientState* destCs,Comm::NetPipe& pipe); // Hook called when the server sends a state update for client sourceClient to client destClient
	
	/* Hooks to insert processing into the lower-level protocol state machine: */
	virtual bool handleMessage(ClientState* cs,unsigned int messageId,Comm::NetPipe& pipe); // Hook called when server receives unknown message from client; returns false to signal protocol error
	virtual void connectClient(ClientState* cs); // Hook called when connection to a new client has been fully established
	virtual void disconnectClient(ClientState* cs); // Hook called after a client has been disconnected (voluntarily or involuntarily)
	virtual void beforeServerUpdate(void); // Hook called before the server sends state update messages
	virtual void beforeServerUpdate(ClientState* cs); // Hook called for each client before the server sends state update messages
	virtual void beforeServerUpdate(ClientState* destCs,Comm::NetPipe& pipe); // Hook called right before the server sends a state update message to the given client
	virtual void afterServerUpdate(ClientState* cs); // Hook called for each client after the server sent state update messages
	virtual void afterServerUpdate(void); // Hook called after the server sent state update messages
	};

/*****************
Type declarations:
*****************/

typedef Plugins::ObjectLoader<ProtocolServer> ProtocolServerLoader;

}

#endif
