/***********************************************************************
CollaborationClient - Class to support collaboration between
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

#ifndef COLLABORATION_COLLABORATIONCLIENT_INCLUDED
#define COLLABORATION_COLLABORATIONCLIENT_INCLUDED

#include <string>
#include <vector>
#include <Misc/HashTable.h>
#include <Misc/ConfigurationFile.h>
#include <Plugins/ObjectLoader.h>
#include <Threads/Thread.h>
#include <Threads/Mutex.h>
#include <Threads/Spinlock.h>
#include <Threads/TripleBuffer.h>
#include <Comm/NetPipe.h>
#include <GLMotif/ToggleButton.h>
#include <Vrui/Geometry.h>
#include <Vrui/GlyphRenderer.h>
#include <Collaboration/ProtocolClient.h>
#include <Collaboration/CollaborationProtocol.h>

/* Forward declarations: */
class GLContextData;
namespace GLMotif {
class PopupWindow;
class RowColumn;
class TextField;
}
class ALContextData;

namespace Collaboration {

class CollaborationClient:private CollaborationProtocol
	{
	/* Embedded classes: */
	public:
	class Configuration // Class to configure a collaboration server
		{
		friend class CollaborationClient;
		
		/* Elements: */
		private:
		Misc::ConfigurationFile configFile; // The collaboration infrastructure's configuration file
		Misc::ConfigurationFileSection cfg; // The client's configuration section
		
		/* Constructors and destructors: */
		public:
		Configuration(void); // Creates a configuration by reading the collaboration infrastructure's configuration file
		
		/* Methods: */
		void setServer(std::string newServerHostName,int newServerPortId); // Sets to which server to connect
		void setClientName(std::string newClientName); // Sets the client's display name
		};
	
	private:
	typedef std::vector<ProtocolClient*> ProtocolList; // Type for lists of client protocol plug-ins
	typedef ProtocolClient::RemoteClientState ProtocolRemoteClientState; // Type for protocol-specific states of remote clients
	
	struct RemoteClientState // Structure containing persistent state of remote clients
		{
		/* Embedded classes: */
		public:
		struct ProtocolListEntry // Structure for entries in a client's shared protocol list
			{
			/* Elements: */
			public:
			ProtocolClient* protocol; // Pointer to protocol plug-in object
			ProtocolRemoteClientState* protocolClientState; // Pointer to protocol's state object for this remote client
			
			/* Constructors and destructors: */
			ProtocolListEntry(ProtocolClient* sProtocol,ProtocolRemoteClientState* sProtocolClientState)
				:protocol(sProtocol),protocolClientState(sProtocolClientState)
				{
				}
			};
		
		typedef std::vector<ProtocolListEntry> RemoteClientProtocolList; // Type for lists of shared protocols
		
		/* Elements: */
		public:
		unsigned int clientID; // Server-wide unique client ID
		RemoteClientProtocolList protocols; // List of protocols and protocol states shared with this client
		Threads::TripleBuffer<ClientState> state; // Transient client state
		volatile unsigned int updateMask; // Accumulated update mask from recent server updates
		GLMotif::TextField* nameTextField; // Pointer to display name text field for this client
		GLMotif::ToggleButton* followToggle; // Pointer to "follow" toggle button for this client
		GLMotif::ToggleButton* faceToggle; // Pointer to "face" toggle button for this client
		
		/* Constructors and destructors: */
		RemoteClientState(void); // Creates uninitialized remote client state structure
		~RemoteClientState(void);
		};
	
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
		RemoteClientState* client; // Pointer to client state structure of new client
		
		/* Constructors and destructors: */
		ClientListAction(Action sAction,unsigned int sClientID,RemoteClientState* sClient)
			:action(sAction),clientID(sClientID),client(sClient)
			{
			}
		};
	
	typedef std::vector<ClientListAction> ActionList; // Type for lists of client list actions
	typedef Misc::HashTable<unsigned int,RemoteClientState*> RemoteClientMap; // Hash table to map from client IDs to client objects
	typedef Misc::HashTable<ProtocolRemoteClientState*,RemoteClientState*> ProtocolClientMap; // Hash table to map from protocol client state objects to remote client state objects
	
	/* Elements: */
	private:
	Configuration* configuration; // Pointer to the client's configuration object
	ProtocolClientLoader protocolLoader; // Object loader to dynamically load protocol plug-ins from DSOs
	protected:
	Threads::Mutex pipeMutex; // Mutex serializing access to the collaboration pipe
	Comm::NetPipePtr pipe; // Pipe connected to the collaboration server
	private:
	Threads::Thread communicationThread; // Thread handling communication with the collaboration server
	ProtocolList protocols; // List of protocols currently registered with the server
	std::vector<ProtocolClient*> messageTable; // Table mapping from message IDs to the protocol engines handling them
	
	/* Lists keeping track of persistent state of remote clients: */
	Threads::Mutex actionListMutex; // Mutex protecting the client action list
	ActionList actionList; // List of recent client list actions
	Threads::Mutex clientMapMutex; // Mutex protecting the client hash table
	RemoteClientMap remoteClientMap; // Hash table mapping from client IDs to remote client state structures
	ProtocolClientMap protocolClientMap; // Hash table mapping from per-protocol client state structures to remote client state structures
	
	/* Local client state: */
	Threads::Spinlock clientStateMutex; // Mutex protecting the local client state
	ClientState clientState; // Transient state of local client
	unsigned int followClientID; // ID of client whose navigation transformation to follow (0 if disabled)
	unsigned int faceClientID; // ID of client whom to face in a conversation (0 if disabled)
	
	/* User interface: */
	GLMotif::PopupWindow* clientDialogPopup; // Dialog window showing the state of the collaboration client
	GLMotif::ToggleButton* showSettingsToggle; // Toggle button to show/hide the client settings dialog
	GLMotif::RowColumn* clientListRowColumn; // RowColumn widget containing the connected client list
	GLMotif::PopupWindow* settingsDialogPopup; // Dialog window to configure the collaboration client at runtime
	
	/* Rendering flags: */
	Vrui::Glyph viewerGlyph; // Glyph to render a remote viewer
	bool fixGlyphScaling; // Always keep displayed glyphs at their configured size, even when navigation scaling is different
	bool renderRemoteEnvironments; // Render the orientations and sizes of the environments of remote clients
	
	/* Private methods: */
	void createClientDialog(void);
	void createSettingsDialog(void);
	void showSettingsDialog(void);
	void showSettingsToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void followClient(const ClientState& cs) const; // Updates navigation transformation to follow the given client
	void faceClient(const ClientState& cs) const; // Updates navigation transformation to face the given client
	void followClientToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData,const unsigned int& clientID);
	void faceClientToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData,const unsigned int& clientID);
	void fixGlyphScalingToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void renderRemoteEnvironmentsToggleValueChangedCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void settingsDialogCloseCallback(Misc::CallbackData* cbData);
	void* communicationThreadMethod(void); // Method for thread receiving messages from the collaboration server
	void* serverUpdateThreadMethod(void); // Method for thread sending client state updates to the collaboration server
	void updateClientState(void); // Updates the local client state from current Vrui state
	
	/* Constructors and destructors: */
	public:
	CollaborationClient(Configuration* sConfiguration =0); // Opens a connection to a collaboration server using settings from the configuration object
	private:
	CollaborationClient(const CollaborationClient& source); // Prohibit copy constructor
	CollaborationClient& operator=(const CollaborationClient& source); // Prohibit assignment operator
	public:
	virtual ~CollaborationClient(void); // Disconnects from the collaboration server
	
	/* Methods: */
	void setClientName(std::string newClientName); // Changes the client's name seen by other clients
	virtual void registerProtocol(ProtocolClient* newProtocol); // Registers a new protocol with the client; must be called before connect()
	Comm::NetPipe& getPipe(void) // Returns the client's collaboration pipe
		{
		return *pipe;
		}
	virtual void connect(void); // Runs the connection initiation protocol; throws exception if fails
	ProtocolClient* getProtocol(const char* protocolName); // Returns a pointer to a protocol client; returns 0 if protocol does not exist
	const Threads::TripleBuffer<ClientState>& getClientState(unsigned int clientID) const // Returns the client state of the client with the given ID
		{
		return remoteClientMap.getEntry(clientID).getDest()->state;
		}
	const Threads::TripleBuffer<ClientState>& getClientState(ProtocolRemoteClientState* prcs) const // Returns the client state of the client who owns the given protocol client state
		{
		return protocolClientMap.getEntry(prcs).getDest()->state;
		}
	Vrui::Glyph& getViewerGlyph(void) // Returns the glyph used to display remote viewers
		{
		return viewerGlyph;
		}
	bool getFixGlyphScaling(void) const // Returns the fixed glyph scaling flag
		{
		return fixGlyphScaling;
		}
	void setFixGlyphScaling(bool enable); // Sets the fixed glyph scaling flag
	void setRenderRemoteEnvironments(bool enable); // Sets the remote environment rendering flag
	void showDialog(void); // Shows the collaboration client dialog
	void hideDialog(void); // Hides the collaboration client dialog
	GLMotif::PopupWindow* getDialog(void) // Returns the collaboration client dialog
		{
		return clientDialogPopup;
		}
	virtual void frame(void); // Same as frame method of Vrui applications and vislets
	virtual void display(GLContextData& contextData) const; // Same as display method of Vrui applications and vislets; needs to be called in navigation coordinates
	virtual void sound(ALContextData& contextData) const; // Same as sound method of Vrui applications and vislets; needs to be called in navigation coordinates
	
	/*********************************************************************
	Hook methods to layer application-level protocols over the base
	protocol:
	*********************************************************************/
	
	/* Hooks to add payloads to lower-level protocol messages: */
	virtual void sendConnectRequest(void); // Hook called when the client sends a connection request message to the server
	virtual void receiveConnectReply(void); // Hook called when the client receives a positive connection reply
	virtual void receiveConnectReject(void); // Hook called when the client receives a negative connection reply
	virtual void sendDisconnectRequest(void); // Hook called when the client sends a disconnection request message to the server
	virtual void receiveDisconnectReply(void); // Hook called when the client receives a disconnection reply message from the server
	virtual void sendClientUpdate(void); // Hook called when the client sends a client state update packet
	virtual void receiveClientConnect(unsigned int clientID); // Hook called when the client receives a connection message for the given remote client
	virtual bool receiveServerUpdate(void); // Hook called when the client receives a state update packet from the server; returns true if application state changed
	virtual bool receiveServerUpdate(unsigned int clientID); // Hook called when the client receives a state update packet for the given remote client from the server; returns true if application state changed
	
	/* Hooks to insert processing into the lower-level protocol state machine: */
	virtual bool handleMessage(MessageIdType messageId); // Hook called when the client receives unknown message from server; returns false to signal protocol error
	virtual void beforeClientUpdate(void); // Hook called right before the client sends a client update packet
	virtual void disconnectClient(unsigned int clientID); // Hook called when a remote client gets disconnected from the server
	};

}

#endif
