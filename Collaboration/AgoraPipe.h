/***********************************************************************
AgoraPipe - Common interface between an Agora server and an Agora
client.
Copyright (c) 2009-2010 Oliver Kreylos

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

#ifndef AGORAPIPE_INCLUDED
#define AGORAPIPE_INCLUDED

#include <Misc/SizedTypes.h>

#include <Collaboration/CollaborationPipe.h>

namespace Collaboration {

struct AgoraPipe
	{
	/* Embedded classes: */
	protected:
	typedef CollaborationPipe::Scalar Scalar; // Scalar type in virtual video space
	typedef CollaborationPipe::Point Point; // Point type in virtual video space
	typedef CollaborationPipe::OGTransform OGTransform; // Orthogonal transformation in virtual video space
	
	class VideoPacket // Helper class to store and transmit encoded video packets
		{
		/* Elements: */
		private:
		char bos; // Beginning-of-stream flag
		Misc::SInt64 granulePos; // Index of most recent keyframe
		Misc::SInt64 packetNo; // Packet sequence number in video stream
		size_t allocSize; // Size of allocated packet buffer
		unsigned char* data; // Packet data buffer
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
		void read(Collaboration::CollaborationPipe& pipe) // Reads the packet from a pipe
			{
			/* Read the packet header: */
			bos=pipe.read<char>();
			granulePos=pipe.read<Misc::SInt64>();
			packetNo=pipe.read<Misc::SInt64>();
			
			/* Read the data size: */
			dataSize=pipe.read<unsigned int>();
			
			/* Reallocate the data buffer if necessary: */
			if(allocSize<dataSize)
				{
				delete[] data;
				allocSize=dataSize;
				data=new unsigned char[allocSize];
				}
			
			/* Read the packet data: */
			pipe.read(data,dataSize);
			}
		void write(Collaboration::CollaborationPipe& pipe) const // Writes the packet to a pipe
			{
			/* Write the packet header: */
			pipe.write<char>(bos);
			pipe.write<Misc::SInt64>(granulePos);
			pipe.write<Misc::SInt64>(packetNo);
			
			/* Write the data size: */
			pipe.write<unsigned int>((unsigned int)(dataSize));
			
			/* Write the packet data: */
			pipe.write(data,dataSize);
			}
		};
	
	/* Elements: */
	static const char* protocolName; // Network name of Agora protocol
	static const unsigned int numProtocolMessages; // Number of Agora-specific protocol messages
	};

}

#endif
