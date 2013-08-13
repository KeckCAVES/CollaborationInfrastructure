/***********************************************************************
CheriaClient - Client object to implement the Cheria input device
distribution protocol.
Copyright (c) 2010-2013 Oliver Kreylos

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

#ifndef COLLABORATION_CHERIACLIENT_INCLUDED
#define COLLABORATION_CHERIACLIENT_INCLUDED

#include <Misc/HashTable.h>
#include <IO/VariableMemoryFile.h>
#include <Threads/Mutex.h>
#include <Vrui/GlyphRenderer.h>
#include <Vrui/InputDeviceManager.h>
#include <Vrui/ToolManager.h>
#include <Collaboration/ProtocolClient.h>
#include <Collaboration/CheriaProtocol.h>

/* Forward declarations: */
namespace IO {
class FixedMemoryFile;
}
namespace Vrui {
class PointingTool;
}

namespace Collaboration {

class CheriaClient:public ProtocolClient,private CheriaProtocol
	{
	/* Embedded classes: */
	private:
	typedef IO::FixedMemoryFile IncomingMessage; // Type for buffers storing incoming messages
	typedef IO::VariableMemoryFile OutgoingMessage; // Type for buffers storing outgoing messages
	
	class RemoteClientState:public ProtocolClient::RemoteClientState
		{
		/* Embedded classes: */
		public:
		struct RemoteDeviceState:public DeviceState // Structure to represent remote devices and their states
			{
			/* Elements: */
			public:
			Vrui::InputDevice* device; // Pointer to local input device representing the remote device
			
			/* Constructors and destructors: */
			RemoteDeviceState(IO::File& source); // Reads device state layout from the given source and creates local proxy device
			~RemoteDeviceState(void); // Destroys local proxy device
			};
		
		typedef Misc::HashTable<unsigned int,RemoteDeviceState*> RemoteDeviceMap; // Hash table to map remote device IDs to local input device pointers
		typedef Misc::HashTable<unsigned int,Vrui::PointingTool*> RemoteToolMap; // Hash table to map remote device IDs to local pointing tool pointers
		
		/* Elements: */
		CheriaClient& client; // Cheria client object to which the remote client state belongs
		RemoteDeviceMap remoteDevices; // Map of remote client's device IDs to local input devices
		RemoteToolMap remoteTools; // Map of remote client's tool IDs to local tools
		Threads::Mutex messageBufferMutex; // Mutex serializing access to the message buffer list
		std::vector<IncomingMessage*> messages; // List of buffers retaining server update messages between frame calls
		
		/* Constructors and destructors: */
		RemoteClientState(CheriaClient& sClient);
		virtual ~RemoteClientState(void);
		
		/* Methods: */
		void processMessages(void); // Reads and processes all queued server update messages
		};
	
	struct LocalDeviceState:public DeviceState // Structure to associate button and valuator masks with represented local devices
		{
		/* Elements: */
		public:
		unsigned int deviceId; // The device's local device ID
		Byte* buttonMasks; // Array of mask flags for each of the device's buttons, to disable those not used by local pointing tools
		Byte* valuatorMasks; // Array of mask flags for each of the device's valuators, to disable those not used by local pointing tools
		
		/* Constructors and destructors: */
		LocalDeviceState(unsigned int sDeviceId,const Vrui::InputDevice* device); // Initializes local device state from device's layout
		~LocalDeviceState(void); // Destroys a local device state
		};
	
	typedef Misc::HashTable<Vrui::InputDevice*,LocalDeviceState*> LocalDeviceMap; // Hash table to map input device pointers to local device states
	typedef Misc::HashTable<Vrui::PointingTool*,unsigned int> LocalToolMap; // Hash table to map tool pointers to local tool IDs
	
	/* Elements: */
	private:
	Vrui::Glyph inputDeviceGlyph; // Glyph to render remote input devices
	Threads::Mutex localDevicesMutex; // Mutex serializing access to the local input device and tool maps
	unsigned int nextLocalDeviceId; // Next ID to assign to a local input device
	LocalDeviceMap localDevices; // Hash table of local devices represented by the Cheria client
	unsigned int nextLocalToolId; // Next ID to assign to a local tool
	LocalToolMap localTools; // Hash table of local tools represented by the Cheria client
	OutgoingMessage message; // Buffer to assemble client update messages as devices are created / destroyed
	volatile bool remoteClientCreatingDevice; // Flag if a remote Cheria client is currently creating an input device
	volatile bool remoteClientDestroyingDevice; // Flag if a remote Cheria client is currently destroying an input device
	volatile bool remoteClientCreatingTool; // Flag if a remote Cheria client is currently creating a tool
	volatile bool remoteClientDestroyingTool; // Flag if a remote Cheria client is currently destroying a tool
	
	/* Private methods: */
	void createInputDevice(Vrui::InputDevice* device); // Method to add a newly-created local input device to the local input device set
	void createTool(Vrui::PointingTool* tool); // Method to add a newly-created local pointing tool to the local tool set
	void inputDeviceCreationCallback(Vrui::InputDeviceManager::InputDeviceCreationCallbackData* cbData); // Callback called when a local device is created
	void inputDeviceDestructionCallback(Vrui::InputDeviceManager::InputDeviceDestructionCallbackData* cbData); // Callback called when a local device is destroyed
	void toolCreationCallback(Vrui::ToolManager::ToolCreationCallbackData* cbData); // Callback called when a local tool is created
	void toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData); // Callback called when a local tool is created
	
	/* Constructors and destructors: */
	public:
	CheriaClient(void); // Creates a Cheria client
	virtual ~CheriaClient(void); // Destroys the Cheria client
	
	/* Methods from ProtocolClient: */
	virtual const char* getName(void) const;
	virtual unsigned int getNumMessages(void) const;
	virtual void initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection);
	virtual void sendConnectRequest(Comm::NetPipe& pipe);
	virtual void receiveConnectReply(Comm::NetPipe& pipe);
	virtual void receiveDisconnectReply(Comm::NetPipe& pipe);
	virtual ProtocolClient::RemoteClientState* receiveClientConnect(Comm::NetPipe& pipe);
	virtual bool receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,Comm::NetPipe& pipe);
	virtual void sendClientUpdate(Comm::NetPipe& pipe);
	virtual void frame(void);
	virtual void frame(ProtocolClient::RemoteClientState* rcs);
	};

}

#endif
