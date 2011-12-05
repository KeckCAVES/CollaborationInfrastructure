/***********************************************************************
CollaborationServer - Class to support collaboration between
applications in spatially distributed (immersive) visualization
environments.
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

#ifndef COLLABORATION_COLLABORATIONSERVER_INCLUDED
#define COLLABORATION_COLLABORATIONSERVER_INCLUDED

#include <utility>
#include <string>
#include <vector>
#include <Misc/ConfigurationFile.h>
#include <Plugins/ObjectLoader.h>
#include <Threads/Thread.h>
#include <Threads/Mutex.h>
#include <Comm/ListeningTCPSocket.h>
#include <Comm/NetPipe.h>
#include <Vrui/Geometry.h>
#include <Collaboration/ProtocolServer.h>
#include <Collaboration/CollaborationProtocol.h>

namespace Collaboration {

class CollaborationServer:private CollaborationProtocol
	{
	/* Embedded classes: */
	public:
	class Configuration // Class to configure a collaboration server
		{
		friend class CollaborationServer;
		
		/* Elements: */
		private:
		Misc::ConfigurationFile configFile; // The collaboration infrastructure's configuration file
		Misc::ConfigurationFileSection cfg; // The server's configuration section
		
		/* Constructors and destructors: */
		public:
		Configuration(void); // Creates a configuration by reading the collaboration infrastructure's configuration file
		
		/* Methods: */
		void setListenPortId(int newListenPortId); // Overrides the default server listening port ID
		double getTickTime(void); // Returns server loop's tick time in seconds
		};
	
	private:
	typedef std::vector<ProtocolServer*> ProtocolList; // Type for lists of server protocol plug-ins
	typedef ProtocolServer::ClientState ProtocolClientState; // Type for protocol-specific client states
	
	struct ClientConnection // Structure containing the current state of a client connection
		{
		/* Embedded classes: */
		public:
		struct ProtocolListEntry // Structure for entries in a client's negotiated protocol list
			{
			/* Elements: */
			public:
			unsigned int index; // Index of protocol in server's main list
			unsigned int clientIndex; // Index of protocol in client's proposed list
			ProtocolServer* protocol; // Pointer to protocol plug-in object
			ProtocolClientState* protocolClientState; // Pointer to protocol's state object for this client
			
			/* Constructors and destructors: */
			ProtocolListEntry(unsigned int sIndex,unsigned int sClientIndex,ProtocolServer* sProtocol,ProtocolClientState* sProtocolClientState)
				:index(sIndex),clientIndex(sClientIndex),protocol(sProtocol),protocolClientState(sProtocolClientState)
				{
				}
			
			/* Methods: */
			static bool comp(const ProtocolListEntry& ple1,const ProtocolListEntry& ple2) // Comparison operator for std::sort
				{
				return ple1.index<ple2.index;
				}
			};
		
		typedef std::vector<ProtocolListEntry> ClientProtocolList; // Type for lists of negotiated protocols
		
		/* Elements: */
		public:
		Threads::Mutex mutex; // Mutex protecting the client connection state structure
		unsigned int clientID; // Server-wide unique client ID
		Threads::Mutex pipeMutex; // Mutex protecting the client communication pipe
		Comm::NetPipePtr pipe; // Communication pipe connecting to the client
		std::string clientHostname; // Hostname of connected client
		int clientPortId; // Port ID of connected client
		ClientProtocolList protocols; // List of protocol plug-ins negotiated with this client sorted in order of ascending index
		Threads::Thread communicationThread; // Thread receiving messages from the connected client
		ClientState state; // Transient client state
		unsigned int stateUpdateMask; // Update mask for the transient client state
		
		/* Constructors and destructors: */
		ClientConnection(unsigned int sClientID,Comm::NetPipePtr sPipe);
		~ClientConnection(void);
		
		/* Methods: */
		bool negotiateProtocols(CollaborationServer& server); // Finds the common subset of protocol plug-ins registered on the client and server; returns false if any protocol rejects the client
		void sendClientConnectProtocols(ClientConnection* dest,Comm::NetPipe& destPipe); // Lets all protocol plug-ins shared by the two clients write their CLIENT_CONNECT message payloads
		};
	
	typedef std::vector<ClientConnection*> ClientList; // Type for lists of client connection state structures
	
	struct ClientListAction // Structure to hold recent changes to the client list
		{
		/* Embedded classes: */
		public:
		enum Action // Enumerated type for client list actions
			{
			ADD_CLIENT,REMOVE_CLIENT
			};
		
		/* Elements: */
		Action action; // Which action was taken
		unsigned int clientID; // ID of the client that was added or removed
		ClientConnection* client; // Pointer to client connection state structure of an added client
		
		/* Constructors and destructors: */
		ClientListAction(Action sAction,unsigned int sClientID,ClientConnection* sClient)
			:action(sAction),clientID(sClientID),client(sClient)
			{
			}
		};
	
	typedef std::vector<ClientListAction> ActionList; // Type for lists of client list actions
	
	/* Elements: */
	private:
	Configuration* configuration; // Pointer to the server's configuration object
	ProtocolServerLoader protocolLoader; // Object loader to dynamically load protocol plug-ins requested by clients
	Comm::ListeningTCPSocket listenSocket; // Socket receiving connection requests from clients
	Threads::Thread listenThread; // Thread receiving connection request messages
	Threads::Mutex protocolListMutex; // Mutex protecting the protocol list
	ProtocolList protocols; // List of protocols currently registered with the server
	std::vector<ProtocolServer*> messageTable; // Table mapping from message IDs to the protocol engines handling them
	Threads::Mutex clientListMutex; // Mutex protecting the client state list
	ClientList clientList; // The list containing the states of all currently connected clients
	ActionList actionList; // List of recent client state list actions
	unsigned int nextClientID; // Unique identification numbers assigned to clients in order of connection
	
	/* Private methods: */
	void* listenThreadMethod(void); // Method for thread receiving connection request messages
	void* clientCommunicationThreadMethod(ClientConnection* client); // Method for thread receiving messages from connected clients
	
	/* Constructors and destructors: */
	public:
	CollaborationServer(Configuration* sConfiguration =0); // Creates a server object with the given configuration
	virtual ~CollaborationServer(void);
	
	/* Methods: */
	int getListenPortId(void) const // Returns the port ID the collaboration server listens on
		{
		return listenSocket.getPortId();
		};
	virtual void registerProtocol(ProtocolServer* newProtocol); // Registers a new protocol plug-in with the server; server inherits objects
	virtual std::pair<ProtocolServer*,int> loadProtocol(std::string protocolName); // Returns a protocol server plug-in for the given protocol, or 0
	virtual void update(void); // Signals the server to send state updates to all connected clients
	
	/*********************************************************************
	Hook methods to layer application-level protocols over the base
	protocol:
	*********************************************************************/
	
	/* Hooks to add payloads to lower-level protocol messages: */
	virtual bool receiveConnectRequest(unsigned int clientID,Comm::NetPipe& pipe); // Hook called when the server receives a client's connection request; serrver rejects the request if the method returns false
	virtual void sendConnectReply(unsigned int clientID,Comm::NetPipe& pipe); // Hook called when the server replies to a client's connection request
	virtual void sendConnectReject(unsigned int clientID,Comm::NetPipe& pipe); // Hook called when the server denies a client's connection request
	virtual void receiveDisconnectRequest(unsigned int clientID,Comm::NetPipe& pipe); // Hook called when the server receives a disconnection request
	virtual void sendDisconnectReply(unsigned int clientID,Comm::NetPipe& pipe); // Hook called when the server sends a disconnect reply to a client
	virtual void receiveClientUpdate(unsigned int clientID,Comm::NetPipe& pipe); // Hook called when the server receives a client's state update packet
	virtual void sendClientConnect(unsigned int sourceClientID,unsigned int destClientID,Comm::NetPipe& pipe); // Hook called when the server sends a connection message for client sourceClient to client destClient
	virtual void sendServerUpdate(unsigned int destClientID,Comm::NetPipe& pipe); // Hook called when the server sends a state update to a client
	virtual void sendServerUpdate(unsigned int sourceClientID,unsigned int destClientID,Comm::NetPipe& pipe); // Hook called when the server sends a state update for client sourceClient to client destClient
	
	/* Hooks to insert processing into the lower-level protocol state machine: */
	virtual bool handleMessage(unsigned int clientID,Comm::NetPipe& pipe,Protocol::MessageIdType messageId); // Hook called when server receives unknown message from client; returns false to signal protocol error
	virtual void connectClient(unsigned int clientID); // Hook called when connection to a new client has been fully established
	virtual void disconnectClient(unsigned int clientID); // Hook called after a client has been disconnected (voluntarily or involuntarily)
	virtual void beforeServerUpdate(unsigned int clientID,Comm::NetPipe& pipe); // Hook called right before the server sends a state update message to the given client
	};

}

#endif
