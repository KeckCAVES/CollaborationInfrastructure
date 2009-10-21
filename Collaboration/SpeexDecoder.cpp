/***********************************************************************
SpeexDecoder - Class encapsulating an audio decoder using the SPEEX
speech codec.
Copyright (c) 2009 Oliver Kreylos

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

#include <Collaboration/SpeexDecoder.h>

#include <Sound/SoundDataFormat.h>

namespace Collaboration {

/*****************************
Methods of class SpeexDecoder:
*****************************/

void* SpeexDecoder::decodingThreadMethod(void)
	{
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	while(true)
		{
		/* Get an encoded SPEEX packet from the queue: */
		const char* speexPacket=speexPacketQueue.popSegment();
		if(speexPacket!=0)
			{
			/* Decode the packet: */
			speex_bits_read_from(&speexBits,const_cast<char*>(speexPacket),speexPacketQueue.getSegmentSize()); // API failure!
			
			/* Decode the sound data: */
			if(speex_decode_int(speexState,&speexBits,playbackBuffer)<0) // This is an untreatable error; need to bail out
				break;
			speex_bits_reset(&speexBits);
			}
		else
			{
			/* Let SPEEX generate filler sound: */
			speex_decode_int(speexState,0,playbackBuffer);
			}
		
		/* Write the decoded frame to the sound device: */
		try
			{
			write(playbackBuffer,speexFrameSize);
			}
		catch(Sound::ALSAPCMDevice::UnderrunError err) // This should really never happen, but it does
			{
			/* Restart the PCM device: */
			prepare();
			setStartThreshold(speexFrameSize*(speexPacketQueue.getMaxQueueSize()-1));
			}
		}
	
	return 0;
	}

SpeexDecoder::SpeexDecoder(const char* playbackPCMDeviceName,size_t sSpeexFrameSize,Threads::DropoutBuffer<char>& sSpeexPacketQueue)
	:Sound::ALSAPCMDevice(playbackPCMDeviceName,false),
	 speexState(0),
	 speexPacketQueue(sSpeexPacketQueue),
	 speexFrameSize(sSpeexFrameSize),playbackBuffer(0)
	{
	bool speexBitsInitialized=false;
	try
		{
		/* Set the PCM device's sound format for SPEEX wideband decoding: */
		Sound::SoundDataFormat format;
		format.setStandardSampleFormat(16,true,Sound::SoundDataFormat::DontCare);
		format.samplesPerFrame=1;
		format.framesPerSecond=16000;
		setSoundDataFormat(format);
		
		/* Initialize the SPEEX decoder: */
		speexState=speex_decoder_init(&speex_wb_mode);
		spx_int32_t enhancement=0;
		speex_decoder_ctl(speexState,SPEEX_SET_ENH,&enhancement);
		spx_int32_t speexSamplingRate=format.framesPerSecond;
		speex_decoder_ctl(speexState,SPEEX_SET_SAMPLING_RATE,&speexSamplingRate);
		
		/* Initialize the SPEEX bit unpacker: */
		speex_bits_init(&speexBits);
		speexBitsInitialized=true;
		
		/* Allocate the raw audio playback buffer: */
		playbackBuffer=new signed short int[speexFrameSize];
		
		/* Set the playback device's fragment size: */
		setBufferSize(speexFrameSize*speexPacketQueue.getMaxQueueSize(),speexFrameSize);
		
		/* Prepare the device for playback: */
		prepare();
		setStartThreshold(speexFrameSize*(speexPacketQueue.getMaxQueueSize()-1));
		
		/* Start the audio decoding thread: */
		decodingThread.start(this,&SpeexDecoder::decodingThreadMethod);
		}
	catch(...)
		{
		/* Clean up and re-throw: */
		delete[] playbackBuffer;
		if(speexBitsInitialized)
			speex_bits_destroy(&speexBits);
		if(speexState!=0)
			speex_decoder_destroy(speexState);
		throw;
		}
	}

SpeexDecoder::~SpeexDecoder(void)
	{
	/* Stop the audio decoding thread: */
	decodingThread.cancel();
	decodingThread.join();
	
	/* Release all allocated resources: */
	delete[] playbackBuffer;
	speex_bits_destroy(&speexBits);
	speex_decoder_destroy(speexState);
	}

}
