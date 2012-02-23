/***********************************************************************
AgoraProtocol - Class defining the communication protocol between an
Agora server and an Agora client.
Copyright (c) 2009-2012 Oliver Kreylos

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

#ifndef COLLABORATION_AGORAPROTOCOL_INCLUDED
#define COLLABORATION_AGORAPROTOCOL_INCLUDED

#include <Collaboration/Protocol.h>

namespace Collaboration {

class AgoraProtocol:public Protocol
	{
	/* Embedded classes: */
	public:
	class VideoPacket // Helper class to store and transmit encoded video packets
		{
		/* Elements: */
		private:
		Misc::SInt8 bos; // Beginning-of-stream flag
		Misc::SInt64 granulePos; // Index of most recent keyframe
		Misc::SInt64 packetNo; // Packet sequence number in video stream
		size_t allocSize; // Size of allocated packet buffer
		Misc::UInt8* data; // Packet data buffer
		size_t dataSize; // Amount of data in the packet
		
		/* Constructors and destructors: */
		public:
		VideoPacket(void) // Creates empty video packet
			:bos(0),granulePos(0),packetNo(0),
			 allocSize(0),data(0),dataSize(0)
			{
			}
		private:
		VideoPacket(const VideoPacket& source); // Prohibit copy constructor
		VideoPacket& operator=(const VideoPacket& source); // Prohibit assignment operator
		public:
		~VideoPacket(void) // Destroys a video packet
			{
			delete[] data;
			}
		
		/* Methods: */
		void read(IO::File& source) // Reads the packet from a source
			{
			/* Read the packet header: */
			bos=source.read<Misc::SInt8>();
			granulePos=source.read<Misc::SInt64>();
			packetNo=source.read<Misc::SInt64>();
			
			/* Read the data size: */
			dataSize=source.read<Card>();
			
			/* Reallocate the data buffer if necessary: */
			if(allocSize<dataSize)
				{
				delete[] data;
				allocSize=dataSize;
				data=new Misc::UInt8[allocSize];
				}
			
			/* Read the packet data: */
			source.read(data,dataSize);
			}
		void write(IO::File& sink) const // Writes the packet to a sink
			{
			/* Write the packet header: */
			sink.write<Misc::SInt8>(bos);
			sink.write<Misc::SInt64>(granulePos);
			sink.write<Misc::SInt64>(packetNo);
			
			/* Write the data size: */
			sink.write<Card>(Card(dataSize));
			
			/* Write the packet data: */
			sink.write(data,dataSize);
			}
		};
	
	/* Elements: */
	static const char* protocolName; // Network name of Agora protocol
	static const unsigned int protocolVersion; // Specific version of protocol implementation
	};

}

#endif
