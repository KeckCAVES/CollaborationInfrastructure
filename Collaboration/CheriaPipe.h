/***********************************************************************
CheriaPipe - Common interface between a Cheria server and a Cheria
client.
Copyright (c) 2010 Oliver Kreylos

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

#ifndef CHERIAPIPE_INCLUDED
#define CHERIAPIPE_INCLUDED

#include <Collaboration/CollaborationPipe.h>

/* Forward declarations: */
namespace Misc {
class ReadBuffer;
class WriteBuffer;
}

namespace Collaboration {

struct CheriaPipe
	{
	/* Embedded classes: */
	protected:
	enum MessageId // Enumerated type for Cheria protocol messages
		{
		CREATE_DEVICE=0,
		DESTROY_DEVICE,
		CREATE_TOOL,
		DESTROY_TOOL,
		DEVICE_STATES,
		MESSAGES_END
		};
	
	typedef CollaborationPipe::MessageIdType MessageIdType; // Type for message IDs
	typedef CollaborationPipe::Scalar Scalar; // Scalar type
	typedef CollaborationPipe::Point Point; // Point type
	typedef CollaborationPipe::Vector Vector; // Vector type
	typedef CollaborationPipe::OGTransform OGTransform; // Orthogonal transformation for input device positions/orientations in navigational space
	
	struct ServerDeviceState // Structure to represent an input device on the Cheria server
		{
		/* Elements: */
		public:
		bool newDevice; // Flag if this device's creation message is still in the client's message buffer
		int trackType; // Device's tracking type
		OGTransform transform; // Device's position and orientation in navigational space
		Vector rayDirection; // Device's preferred ray direction in device space
		int numButtons; // Number of buttons on the device
		unsigned char* buttonStates; // Bit mask of button flags
		int numValuators; // Number of valuators on the device
		Scalar* valuatorStates; // Array of valuator values
		
		/* Constructors and destructors: */
		ServerDeviceState(CollaborationPipe& pipe);
		~ServerDeviceState(void);
		
		/* Methods: */
		template <class PipeParam>
		void writeLayout(PipeParam& pipe) const;
		void read(CollaborationPipe& pipe);
		template <class PipeParam>
		void write(PipeParam& pipe) const;
		};
	
	struct ServerToolState // Structure to represent a tool on the Cheria server
		{
		/* Embedded classes: */
		public:
		struct Slot // Structure for button or valuator slots
			{
			/* Elements: */
			public:
			unsigned int deviceId; // ID of the device assigned to this slot
			int index; // Index of this slot's assigned button or valuator on the device
			};
		
		/* Elements: */
		bool newTool; // Flag if this device's creation message is still in the client's message buffer
		char* className; // Tool's class name
		int numButtonSlots; // Number of button slots in tool's input assignment
		Slot* buttonSlots; // Array of button slot assignments
		int numValuatorSlots; // Number of valuator slots in tool's input assignment
		Slot* valuatorSlots; // Array of valuator slot assignments
		
		/* Constructors and destructors: */
		ServerToolState(CollaborationPipe& pipe); // Creates a tool state by reading from the given pipe
		~ServerToolState(void); // Destroys the tool state
		
		/* Methods: */
		template <class PipeParam>
		void writeLayout(PipeParam& pipe) const; // Writes the tool's state to the given pipe
		};
	
	/* Elements: */
	static const char* protocolName; // Network name of Cheria protocol
	static const unsigned int protocolVersion; // Specific version of protocol implementation
	};

}

#endif
