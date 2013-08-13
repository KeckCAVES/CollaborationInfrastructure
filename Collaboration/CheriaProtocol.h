/***********************************************************************
CheriaProtocol - Class defining the communication protocol between a
Cheria server and a Cheria client.
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

#ifndef COLLABORATION_CHERIAPROTOCOL_INCLUDED
#define COLLABORATION_CHERIAPROTOCOL_INCLUDED

#include <string>
#include <Collaboration/Protocol.h>

namespace Collaboration {

class CheriaProtocol:public Protocol
	{
	/* Embedded classes: */
	public:
	enum MessageId // Enumerated type for Cheria protocol messages
		{
		CREATE_DEVICE=0,
		DESTROY_DEVICE,
		CREATE_TOOL,
		DESTROY_TOOL,
		DEVICE_STATES,
		MESSAGES_END
		};
	
	struct DeviceState // Structure to exchange input device data between server and clients
		{
		/* Embedded classes: */
		public:
		enum UpdateMask // Enumerated type to denote which parts of a device state have changed
			{
			NO_CHANGE=0x0,    // No change in device state
			RAYDIRECTION=0x1, // Device's ray direction in device coordinates changed
			TRANSFORM=0x2,    // Device's position and orientation in environment's physical space changed
			VELOCITY=0x4,     // Device's linear or angular velocities changed
			BUTTON=0x8,       // Any button changed state
			VALUATOR=0x10,    // Any valuator changed state
			FULL_UPDATE=0x1f  // Full initialization
			};
		
		/* Elements: */
		public:
		int trackType; // Device's tracking type
		unsigned int numButtons; // Number of buttons on the device
		unsigned int numValuators; // Number of valuators on the device
		unsigned int updateMask; // Cumulative update mask of this device state
		Vector rayDirection; // Device's preferred ray direction in device space
		Scalar rayStart; // Start parameter of device's ray
		ONTransform transform; // Device's position and orientation in client's physical space
		Vector linearVelocity,angularVelocity; // Device's linear and angular velocities in client's physical space
		Byte* buttonStates; // Bit array of button flags
		Scalar* valuatorStates; // Array of valuator values
		
		/* Constructors and destructors: */
		DeviceState(int sTrackType,unsigned int sNumButtons,unsigned int sNumValuators); // Creates device state with given layout
		DeviceState(IO::File& source); // Creates device state with layout read from file
		private:
		DeviceState(const DeviceState& source); // Prohibit copy constructor
		DeviceState& operator=(const DeviceState& source); // Prohibit assignment operator
		public:
		~DeviceState(void);
		
		/* Methods: */
		static void skipLayout(IO::File& source); // Skips a device layout transmitted on the given source
		void writeLayout(IO::File& sink) const; // Writes device's layout to the given sink
		void read(IO::File& source); // Reads device's state from the given source
		void write(unsigned int writeUpdateMask,IO::File& sink) const; // Writes device's state to the given sink
		};
	
	struct ToolState // Structure to exchange tool data between server and clients
		{
		/* Embedded classes: */
		public:
		struct Slot // Structure for button or valuator slots
			{
			/* Elements: */
			public:
			unsigned int deviceId; // ID of the device assigned to this slot
			unsigned int index; // Index of this slot's assigned button or valuator on the device
			};
		
		/* Elements: */
		std::string className; // Tool's class name
		unsigned int numButtonSlots; // Number of button slots in tool's input assignment
		Slot* buttonSlots; // Array of button slot assignments
		unsigned int numValuatorSlots; // Number of valuator slots in tool's input assignment
		Slot* valuatorSlots; // Array of valuator slot assignments
		
		/* Constructors and destructors: */
		ToolState(const char* sClassName,unsigned int sNumButtonSlots,unsigned int sNumValuatorSlots); // Creates tool state with given class and input layout
		ToolState(IO::File& source); // Creates tool state by reading class name and input layout and assignment from the given source
		private:
		ToolState(const ToolState& source); // Prohibit copy constructor
		ToolState& operator=(const ToolState& source); // Prohibit assignment operator
		public:
		~ToolState(void); // Destroys the tool state
		
		/* Methods: */
		static void skip(IO::File& source); // Skips tool's class name and input layout and assignment transmitted on the given source
		void write(IO::File& sink) const; // Writes tool's class name and input layout and assignment to the given sink
		};
	
	/* Elements: */
	static const char* protocolName; // Network name of Cheria protocol
	static const unsigned int protocolVersion; // Specific version of protocol implementation
	};

}

#endif
