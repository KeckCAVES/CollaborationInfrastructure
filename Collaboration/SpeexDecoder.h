/***********************************************************************
SpeexDecoder - Class encapsulating an audio decoder using the SPEEX
speech codec.
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

#ifndef SPEEXDECODER_INCLUDED
#define SPEEXDECODER_INCLUDED

#include <Threads/Thread.h>
#include <Threads/DropoutBuffer.h>
#include <speex/speex.h>

namespace Collaboration {

class SpeexDecoder
	{
	/* Elements: */
	private:
	void* speexState; // The SPEEX speech decoding object
	Threads::DropoutBuffer<char>& speexPacketQueue; // Reference to queue of encoded SPEEX audio packets ready for pickup
	SpeexBits speexBits; // SPEEX bit unpacking structure
	size_t speexFrameSize; // Number of audio frames contained in an encoded SPEEX audio packet
	Threads::Thread decodingThread; // The decoding thread
	Threads::DropoutBuffer<signed short int> decodedPacketQueue; // Queue of decoded audio packets
	
	/* Private methods: */
	void* decodingThreadMethod(void); // The decoding thread method
	
	/* Constructors and destructors: */
	public:
	SpeexDecoder(size_t sSpeexFrameSize,Threads::DropoutBuffer<char>& sSpeexPacketQueue); // Creates a SPEEX decoder using the given SPEEX parameters and packet delivery queue and condition variable
	~SpeexDecoder(void); // Destroys the SPEEX decoder
	
	/* Methods: */
	Threads::DropoutBuffer<signed short int>& getDecodedPacketQueue(void) // Returns a reference to the decoded packet queue
		{
		return decodedPacketQueue;
		}
	};

}

#endif
