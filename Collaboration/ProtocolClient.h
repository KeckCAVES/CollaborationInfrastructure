/***********************************************************************
ProtocolClient - Abstract base class for the client-side components of
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

#ifndef COLLABORATION_PROTOCOLCLIENT_INCLUDED
#define COLLABORATION_PROTOCOLCLIENT_INCLUDED

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
class GLContextData;
namespace GLMotif {
class RowColumn;
}
class ALContextData;
namespace Collaboration {
class CollaborationClient;
}

namespace Collaboration {

class ProtocolClient
	{
	friend class CollaborationClient;
	
	/* Embedded classes: */
	protected:
	class RemoteClientState // Class representing client-side state of a remote client
		{
		/* Constructors and destructors: */
		public:
		RemoteClientState(void);
		virtual ~RemoteClientState(void);
		};
	
	/* Elements: */
	protected:
	CollaborationClient* client; // Pointer to the main client object
	private:
	unsigned int messageIdBase; // Base value for message IDs reserved for this protocol
	
	/* Constructors and destructors: */
	public:
	ProtocolClient(void);
	virtual ~ProtocolClient(void);
	
	/* Methods: */
	unsigned int getMessageIdBase(void) const // Returns the first message ID assigned to this protocol
		{
		return messageIdBase;
		}
	virtual const char* getName(void) const =0; // Returns the protocol's name; must be unique and match exactly the name returned by the server engine
	virtual unsigned int getNumMessages(void) const; // Returns the number of protocol messages used by this protocol
	virtual void initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection); // Called when the protocol client is registered with a collaboration client
	virtual bool haveSettingsDialog(void) const; // Method to inquire whether the protocol wants to add user interface elements to the collaboration client's settings dialog; called after receiveConnectReply
	virtual void buildSettingsDialog(GLMotif::RowColumn* settingsDialog); // Method to add user interface elements to the collaboration client's settings dialog; called after haveSettingsDialog returns true
	
	/***********************************
	Client protocol engine hook methods:
	***********************************/
	
	/* Hooks to add payloads to lower-level protocol messages: */
	
	/* Called from client's API: */
	virtual void sendConnectRequest(Comm::NetPipe& pipe); // Hook called when the client sends a connection request message to the server
	virtual void receiveConnectReply(Comm::NetPipe& pipe); // Hook called when the client receives a positive connection reply
	virtual void receiveConnectReject(Comm::NetPipe& pipe); // Hook called when the client receives a negative connection reply
	virtual void sendDisconnectRequest(Comm::NetPipe& pipe); // Hook called when the client sends a disconnection request message to the server
	
	/* Called from client's communication background thread: */
	virtual void receiveDisconnectReply(Comm::NetPipe& pipe); // Hook called when the client receives a disconnection reply message from the server
	virtual RemoteClientState* receiveClientConnect(Comm::NetPipe& pipe); // Hook called when the client receives a connection message for a new remote client
	virtual bool receiveServerUpdate(Comm::NetPipe& pipe); // Hook called when the client receives a state update packet from the server; returns true if application state changed
	virtual bool receiveServerUpdate(RemoteClientState* rcs,Comm::NetPipe& pipe); // Hook called when the client receives a state update packet for the given remote client from the server; returns true if application state changed
	virtual void sendClientUpdate(Comm::NetPipe& pipe); // Hook called when the client sends a client state update packet
	
	/* Hooks to insert processing into the lower-level client protocol state machine: */
	
	/* Called from client's API: */
	virtual void rejectedByServer(void); // Hook called when the protocol is rejected during connection initiation, i.e., if the server doesn't support it
	virtual void connectClient(RemoteClientState* rcs); // Hook called when connection to a new remote client has been fully established
	virtual void disconnectClient(RemoteClientState* rcs); // Hook called right before a client is completely disconnected
	virtual void frame(void); // Hook called during client's frame method
	virtual void frame(RemoteClientState* rcs); // Hook called for each remote client sharing this protocol, during client's frame method
	virtual void glRenderAction(GLContextData& contextData) const; // Hook called during client's display method
	virtual void glRenderAction(const RemoteClientState* rcs,GLContextData& contextData) const; // Hook called for each remote client sharing this protocol, during client's display method
	virtual void alRenderAction(ALContextData& contextData) const; // Hook called during client's sound method
	virtual void alRenderAction(const RemoteClientState* rcs,ALContextData& contextData) const; // Hook called for each remote client sharing this protocol, during client's sound method
	
	/* Called from client's communication background thread: */
	virtual void beforeClientUpdate(Comm::NetPipe& pipe); // Hook called right before the client sends a client update packet
	virtual bool handleMessage(unsigned int messageId,Comm::NetPipe& pipe); // Hook called when the client receives unknown message from server; returns false to signal protocol error
	};

/*****************
Type declarations:
*****************/

typedef Plugins::ObjectLoader<ProtocolClient> ProtocolClientLoader;

}

#endif
