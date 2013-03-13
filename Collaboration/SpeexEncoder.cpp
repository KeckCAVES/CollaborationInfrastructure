/***********************************************************************
SpeexEncoder - Class encapsulating an audio encoder using the SPEEX
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

#include <Collaboration/SpeexEncoder.h>

// DEBUGGING
#include <iostream>
#include <Sound/SoundDataFormat.h>

namespace Collaboration {

/*****************************
Methods of class SpeexEncoder:
*****************************/

void* SpeexEncoder::encodingThreadMethod(void)
	{
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	while(true)
		{
		/* Read raw audio data from the recording PCM device: */
		size_t numFrames=read(recordingBuffer,speexFrameSize);
		if(numFrames!=speexFrameSize) // Untreatable error; need to bail out
			{
			// DEBUGGING
			std::cerr<<"SpeexEncoder: Error while reading from sound device; received "<<numFrames<<" frames instead of "<<speexFrameSize<<std::endl;
			break;
			}
		
		/* Encode the raw audio data into the SPEEX bit packer: */
		if(speex_encode_int(speexState,recordingBuffer,&speexBits)<0)
			break; // Untreatable error; need to bail out
		
		/* Write packed bits into the SPEEX packet queue: */
		char* speexPacket=speexPacketQueue.getWriteSegment();
		speex_bits_write(&speexBits,speexPacket,speexPacketSize);
		speexPacketQueue.pushSegment();
		speex_bits_reset(&speexBits);
		}
	
	return 0;
	}

SpeexEncoder::SpeexEncoder(const char* recordingPCMDeviceName,size_t sPacketQueueSize)
	:Sound::ALSAPCMDevice(recordingPCMDeviceName,true),
	 speexState(0),
	 speexFrameSize(0),recordingBuffer(0),
	 speexPacketSize(0),speexPacketQueue(0,0)
	{
	bool speexBitsInitialized=false;
	try
		{
		/* Set the PCM device's sound format for SPEEX wideband encoding: */
		Sound::SoundDataFormat format;
		format.setStandardSampleFormat(16,true,Sound::SoundDataFormat::DontCare);
		format.samplesPerFrame=1;
		format.framesPerSecond=16000;
		setSoundDataFormat(format);
		
		/* Initialize the SPEEX encoder: */
		speexState=speex_encoder_init(&speex_wb_mode);
		spx_int32_t speexQuality=5;
		speex_encoder_ctl(speexState,SPEEX_SET_QUALITY,&speexQuality);
		spx_int32_t speexComplexity=3;
		speex_encoder_ctl(speexState,SPEEX_SET_COMPLEXITY,&speexComplexity);
		spx_int32_t speexSamplingRate=format.framesPerSecond;
		speex_encoder_ctl(speexState,SPEEX_SET_SAMPLING_RATE,&speexSamplingRate);
		spx_int32_t speexFrameSizeInt32;
		speex_encoder_ctl(speexState,SPEEX_GET_FRAME_SIZE,&speexFrameSizeInt32);
		speexFrameSize=speexFrameSizeInt32;
		
		/* Allocate the raw audio recording buffer: */
		recordingBuffer=new signed short int[speexFrameSize];
		
		/* Set the recording device's fragment size: */
		if(sPacketQueueSize<4)
			sPacketQueueSize=4;
		setBufferSize(speexFrameSize*sPacketQueueSize,speexFrameSize);
		
		/* Initialize the SPEEX bit packer: */
		speex_bits_init(&speexBits);
		speexBitsInitialized=true;
		
		/* Allocate the SPEEX packet queue: */
		speexPacketSize=42; // Hard-coded, because I see no way to query it
		speexPacketQueue.resize(speexPacketSize,sPacketQueueSize);
		
		/* Start the recording PCM device: */
		prepare();
		start();
		
		/* Start the audio encoding thread: */
		encodingThread.start(this,&SpeexEncoder::encodingThreadMethod);
		}
	catch(...)
		{
		/* Clean up and re-throw: */
		if(speexBitsInitialized)
			speex_bits_destroy(&speexBits);
		delete[] recordingBuffer;
		if(speexState!=0)
			speex_encoder_destroy(speexState);
		throw;
		}
	}

SpeexEncoder::~SpeexEncoder(void)
	{
	/* Stop the audio encoding thread: */
	encodingThread.cancel();
	encodingThread.join();
	
	/* Release all allocated resources: */
	speex_bits_destroy(&speexBits);
	delete[] recordingBuffer;
	speex_encoder_destroy(speexState);
	}

}
