/***********************************************************************
AgoraClient - Client object to implement the Agora group audio protocol.
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

#include <Collaboration/AgoraClient.h>

#include <iostream>
#include <Misc/FunctionCalls.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <Geometry/GeometryValueCoders.h>
#include <GL/gl.h>
#include <GL/Extensions/GLARBTextureNonPowerOfTwo.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>
#include <Vrui/Vrui.h>

#include <Collaboration/CollaborationPipe.h>
#include <Collaboration/CollaborationClient.h>
#include <Collaboration/SpeexEncoder.h>
#include <Collaboration/SpeexDecoder.h>
#include <Collaboration/TheoraWrappers.h>

namespace Collaboration {

/*******************************************************
Static elements of class AgoraClient::RemoteClientState:
*******************************************************/

const char* AgoraClient::RemoteClientState::yuvToRgbVertexShaderSource="\
	void main() \n\
		{ \n\
		/* Copy the texture coordinate: */ \n\
		gl_TexCoord[0]=gl_MultiTexCoord0; \n\
		 \n\
		/* Transform the vertex: */ \n\
		gl_Position=ftransform(); \n\
		}\n\
	";

const char* AgoraClient::RemoteClientState::yuvToRgbFragmentShaderSource="\
	uniform sampler2D textureSampler; // Sampler for input Y'CbCr texture \n\
	 \n\
	void main() \n\
		{ \n\
		/* Get the interpolated texture color in Y'CbCr space: */ \n\
		vec4 ypcbcr=texture2D(textureSampler,gl_TexCoord[0].st); \n\
		 \n\
		/* Convert the color to yuv first: */ \n\
		vec3 yuv; \n\
		yuv[0]=(255.0*ypcbcr[0]-16.0)/219.0; \n\
		yuv[1]=(127.0*ypcbcr[1]-(127.0*128.0)/255.0)/112.0; \n\
		yuv[2]=(127.0*ypcbcr[2]-(127.0*128.0)/255.0)/112.0; \n\
		 \n\
		/* Then convert to RGB: */ \n\
		vec4 rgb; \n\
		rgb[0]=yuv[0]+1.402*yuv[2]; \n\
		rgb[1]=yuv[0]-0.344*yuv[1]-0.714*yuv[2]; \n\
		rgb[2]=yuv[0]+1.772*yuv[1]; \n\
		rgb[3]=ypcbcr[3]; \n\
		 \n\
		/* Store the final color: */ \n\
		gl_FragColor=rgb; \n\
		}\n\
	";

/***********************************************
Methods of class AgoraClient::RemoteClientState:
***********************************************/

void* AgoraClient::RemoteClientState::videoDecodingThreadMethod(void)
	{
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	while(true)
		{
		/* Check if there is a new Theora packet in the packet buffer: */
		{
		Threads::MutexCond::Lock newPacketLock(newPacketCond);
		while(!oggPacketBuffer.lockNewValue())
			newPacketCond.wait(newPacketLock);
		}
		
		/* Feed the packet into the video decoder: */
		theora_decode_packetin(theoraDecoder,const_cast<OggPacket*>(&oggPacketBuffer.getLockedValue()));
		
		/* Read any decoded frames from the video decoder: */
		if(theora_decode_YUVout(theoraDecoder,yuvBuffer)==0)
			{
			/* Convert the frame from Y'CbCr 4:2:0 to Y'CbCr 1:1:1: */
			Frame& frame=frameBuffer.startNewValue();
			if(frame.buffer==0)
				frame.resize(frameSize[0],frameSize[1]);
			yuvBuffer->unswizzleYpCbCr111(frame.buffer);
			
			/* Post the new frame: */
			frameBuffer.postNewValue();
			Vrui::requestUpdate();
			}
		}
	
	return 0;
	}

AgoraClient::RemoteClientState::RemoteClientState(void)
	:speexPacketQueue(0,0),
	 speexDecoder(0),
	 hasTheora(false),theoraDecoder(0),yuvBuffer(0)
	{
	}

AgoraClient::RemoteClientState::~RemoteClientState(void)
	{
	if(theoraDecoder!=0)
		{
		/* Stop the video decoding thread: */
		videoDecodingThread.cancel();
		videoDecodingThread.join();
		}
	
	delete speexDecoder;
	delete theoraDecoder;
	delete yuvBuffer;
	}

void AgoraClient::RemoteClientState::initContext(GLContextData& contextData) const
	{
	if(theoraDecoder!=0)
		{
		/* Create and store a data item: */
		DataItem* dataItem=new DataItem;
		contextData.addDataItem(this,dataItem);
		
		/* Initialize the texture object: */
		glBindTexture(GL_TEXTURE_2D,dataItem->videoTextureObjectId);
		
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		
		/* Calculate the texture size: */
		if(GLARBTextureNonPowerOfTwo::isSupported())
			{
			/* Use the original texture size: */
			for(int i=0;i<2;++i)
				dataItem->textureSize[i]=frameSize[i];
			}
		else
			{
			/* Pad texture size to next power of two: */
			for(int i=0;i<2;++i)
				for(dataItem->textureSize[i]=1;dataItem->textureSize[i]<GLsizei(frameSize[i]);dataItem->textureSize[i]<<=1)
					;
			
			/* Upload a dummy texture right away: */
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,dataItem->textureSize[0],dataItem->textureSize[1],0,GL_RGB,GL_UNSIGNED_BYTE,0);
			}
		
		glBindTexture(GL_TEXTURE_2D,0);
		
		if(dataItem->shaderSupported)
			{
			/* Build the Y'CbCr -> RGB conversion shader: */
			dataItem->yuvToRgbShader.compileVertexShaderFromString(yuvToRgbVertexShaderSource);
			dataItem->yuvToRgbShader.compileFragmentShaderFromString(yuvToRgbFragmentShaderSource);
			dataItem->yuvToRgbShader.linkShader();
			
			/* Get the shader's texture sampler uniform location: */
			dataItem->textureSamplerLoc=dataItem->yuvToRgbShader.getUniformLocation("textureSampler");
			}
		}
	}

void AgoraClient::RemoteClientState::display(GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Set up OpenGL: */
	glPushAttrib(GL_TEXTURE_BIT);
	glBindTexture(GL_TEXTURE_2D,dataItem->videoTextureObjectId);
	
	/* Check if the texture object is current: */
	if(dataItem->version!=frameVersion)
		{
		/* Upload the new video frame into the texture object: */
		glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
		glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,0);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
		glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
		glPixelStorei(GL_UNPACK_SKIP_IMAGES,0);
		glPixelStorei(GL_UNPACK_ALIGNMENT,1);
		if(GLsizei(frameSize[0])==dataItem->textureSize[0]&&GLsizei(frameSize[1])==dataItem->textureSize[1])
			glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,frameSize[0],frameSize[1],0,GL_RGB,GL_UNSIGNED_BYTE,frameBuffer.getLockedValue().buffer);
		else
			glTexSubImage2D(GL_TEXTURE_2D,0,0,0,frameSize[0],frameSize[1],GL_RGB,GL_UNSIGNED_BYTE,frameBuffer.getLockedValue().buffer);
		
		dataItem->version=frameVersion;
		}
	
	if(dataItem->shaderSupported)
		{
		/* Install the Y'CbCr->RGB shader: */
		dataItem->yuvToRgbShader.useProgram();
		glUniform1iARB(dataItem->textureSamplerLoc,0);
		}
	else
		{
		glEnable(GL_TEXTURE_2D);
		glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
		}
	
	glPushMatrix();
	glMultMatrix(videoTransform.getLockedValue());
	
	/* Draw the texture: */
	float t0=float(frameSize[0])/float(dataItem->textureSize[0]);
	float t1=float(frameSize[1])/float(dataItem->textureSize[1]);
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f,t1);
	glVertex3f( videoSize[0],0.0f,-videoSize[1]);
	glTexCoord2f(t0,t1);
	glVertex3f(-videoSize[0],0.0f,-videoSize[1]);
	glTexCoord2f(t0,0.0f);
	glVertex3f(-videoSize[0],0.0f, videoSize[1]);
	glTexCoord2f(0.0f,0.0f);
	glVertex3f( videoSize[0],0.0f, videoSize[1]);
	glEnd();
	
	glPopMatrix();
	
	if(dataItem->shaderSupported)
		GLShader::disablePrograms();
	
	/* Reset OpenGL state: */
	glBindTexture(GL_TEXTURE_2D,0);
	glPopAttrib();
	}

/****************************
Methods of class AgoraClient:
****************************/

void AgoraClient::videoCaptureCallback(const V4L2VideoDevice::FrameBuffer& frame)
	{
	/* Swizzle the new frame into the encoder input buffer (ouch), and reduce pixel format to Y'CbCr 4:2:0: */
	yuvBuffer->swizzleYpCbCr422(frame.start);
	
	/* Feed the new frame to the video encoder: */
	if(theora_encode_YUVin(theoraEncoder,yuvBuffer)==0)
		{
		/* Grab the encoded frame from the video encoder: */
		OggPacket encoderPacket;
		if(theora_encode_packetout(theoraEncoder,0,&encoderPacket)==1)
			{
			/* Copy the new packet into the delivery buffer: */
			OggPacket& packet=oggPacketBuffer.startNewValue();
			packet.clone(encoderPacket);
			oggPacketBuffer.postNewValue();
			}
		}
	}

AgoraClient::AgoraClient(void)
	:speexEncoder(0),
	 hasTheora(false),videoDevice(0),theoraEncoder(0),yuvBuffer(0),
	 playbackNodeIndex(0),playbackPcmDeviceName("default"),
	 jitterBufferSize(2)
	{
	}

AgoraClient::~AgoraClient(void)
	{
	delete speexEncoder;
	if(videoDevice!=0)
		{
		/* Stop streaming: */
		videoDevice->stopStreaming();
		videoDevice->releaseFrameBuffers();
		
		/* Release all resources: */
		delete videoDevice;
		delete theoraEncoder;
		delete yuvBuffer;
		}
	}

const char* AgoraClient::getName(void) const
	{
	return protocolName;
	}

unsigned int AgoraClient::getNumMessages(void) const
	{
	return numProtocolMessages;
	}

void AgoraClient::initialize(CollaborationClient& collaborationClient,Misc::ConfigurationFileSection& configFileSection)
	{
	/************************************
	Initialize audio and video recording:
	************************************/
	
	if(Vrui::isMaster()&&configFileSection.retrieveValue<bool>("./enableRecording",true))
		{
		try
			{
			/* Create a SPEEX encoder: */
			std::string recordingPcmDeviceName=configFileSection.retrieveValue<std::string>("./recordingPcmDeviceName","default");
			size_t sendQueueSize=configFileSection.retrieveValue<unsigned int>("./sendQueueSize",8);
			speexEncoder=new SpeexEncoder(recordingPcmDeviceName.c_str(),sendQueueSize);
			}
		catch(std::runtime_error err)
			{
			/* Disable audio recording on errors: */
			std::cerr<<"AgoraClient::AgoraClient: Disabling sound recording due to exception "<<err.what()<<std::endl;
			delete speexEncoder;
			speexEncoder=0;
			}
		}
	
	if(Vrui::isMaster()&&configFileSection.retrieveValue<bool>("./enableCapture",true))
		{
		try
			{
			/* Create a video capture device: */
			std::string captureVideoDeviceName=configFileSection.retrieveValue<std::string>("./captureVideoDeviceName","/dev/video");
			videoDevice=new V4L2VideoDevice(captureVideoDeviceName.c_str());
			
			/* Configure the video device's video format and device settings: */
			std::cout<<"AgoraClient::initialize: Configuring video capture device"<<std::endl;
			videoDevice->configure(configFileSection);
			
			/* Retrieve the video device's video format: */
			std::cout<<"AgoraClient::initialize: Retrieving video format"<<std::endl;
			V4L2VideoDevice::VideoFormat videoFormat=videoDevice->getVideoFormat();
			
			/* Read Theora encoder settings: */
			int theoraBitrate=configFileSection.retrieveValue<int>("./theoraBitrate",0);
			int theoraQuality=configFileSection.retrieveValue<int>("./theoraQuality",32);
			if(theoraQuality<0)
				theoraQuality=0;
			else if(theoraQuality>63)
				theoraQuality=63;
			int theoraGopSize=configFileSection.retrieveValue<int>("./theoraGopSize",50);
			
			/* Create a Theora encoder object: */
			std::cout<<"AgoraClient::initialize: Creating Theora video encoder"<<std::endl;
			{
			TheoraInfo encoderInfo;
			encoderInfo.width=videoFormat.size[0];
			encoderInfo.height=videoFormat.size[1];
			encoderInfo.frame_width=videoFormat.size[0]&~0xf;
			encoderInfo.frame_height=videoFormat.size[1]&~0xf;
			encoderInfo.offset_x=((encoderInfo.width-encoderInfo.frame_width)/2)&~0x1;
			encoderInfo.offset_y=((encoderInfo.height-encoderInfo.frame_height)/2)&~0x1;
			encoderInfo.fps_numerator=videoFormat.frameIntervalDenominator;
			encoderInfo.fps_denominator=videoFormat.frameIntervalCounter;
			encoderInfo.aspect_numerator=1;
			encoderInfo.aspect_denominator=1;
			encoderInfo.colorspace=OC_CS_UNSPECIFIED;
			encoderInfo.target_bitrate=theoraBitrate;
			encoderInfo.keyframe_frequency=theoraGopSize;
			encoderInfo.keyframe_frequency_force=theoraGopSize;
			encoderInfo.keyframe_mindistance=theoraGopSize/3;
			encoderInfo.quality=theoraQuality;
			encoderInfo.quick_p=1;
			encoderInfo.dropframes_p=0;
			encoderInfo.keyframe_auto_p=1;
			encoderInfo.keyframe_data_target_bitrate=(encoderInfo.target_bitrate*5)/4;
			encoderInfo.keyframe_auto_threshold=80;
			encoderInfo.noise_sensitivity=1;
			encoderInfo.sharpness=0;
			encoderInfo.pixelformat=OC_PF_420;
			theoraEncoder=new TheoraEncoder(encoderInfo);
			}
			
			/* Create an intermediate buffer to feed frames to the Theora encoder: */
			yuvBuffer=new YpCbCrBuffer(videoFormat.size[0],videoFormat.size[1]);
			
			/* Read the video transformation and video image size: */
			videoTransform=configFileSection.retrieveValue<OGTransform>("./virtualVideoTransform",OGTransform::identity);
			if(videoFormat.size[0]>=videoFormat.size[1])
				{
				videoSize[0]=Scalar(videoFormat.size[0])/Scalar(videoFormat.size[1]);
				videoSize[1]=Scalar(1);
				}
			else
				{
				videoSize[0]=Scalar(1);
				videoSize[1]=Scalar(videoFormat.size[1])/Scalar(videoFormat.size[0]);
				}
			videoSize[0]=configFileSection.retrieveValue<Scalar>("./virtualVideoWidth",videoSize[0]);
			videoSize[1]=configFileSection.retrieveValue<Scalar>("./virtualVideoHeight",videoSize[1]);
			}
		catch(std::runtime_error err)
			{
			/* Diable video capture on errors: */
			std::cerr<<"AgoraClient::AgoraClient: Disabling video capture due to exception "<<err.what()<<std::endl;
			
			/* Clean up: */
			delete yuvBuffer;
			yuvBuffer=0;
			delete theoraEncoder;
			theoraEncoder=0;
			delete videoDevice;
			videoDevice=0;
			}
		}
	
	playbackNodeIndex=configFileSection.retrieveValue<int>("./playbackNodeIndex",playbackNodeIndex);
	playbackPcmDeviceName=configFileSection.retrieveValue<std::string>("./playbackPcmDeviceName",playbackPcmDeviceName);
	jitterBufferSize=configFileSection.retrieveValue<unsigned int>("./jitterBufferSize",jitterBufferSize);
	}

void AgoraClient::sendConnectRequest(CollaborationPipe& pipe)
	{
	/* Prepare the three stream header packets required by the server: */
	OggPacket headerPackets[3];
	bool theoraValid=theoraEncoder!=0;
	if(theoraValid)
		{
		/* Retrieve the three stream header packets from the Theora encoder: */
		std::cout<<"AgoraClient::sendConnectRequest: Encoding stream header packets"<<std::endl;
		OggPacket tempPacket;
		int result1=theora_encode_header(theoraEncoder,&tempPacket);
		headerPackets[0].clone(tempPacket);
		TheoraComment comment;
		int result2=theora_encode_comment(&comment,&tempPacket);
		headerPackets[1].clone(tempPacket);
		int result3=theora_encode_tables(theoraEncoder,&tempPacket);
		headerPackets[2].clone(tempPacket);
		theoraValid=result1==0&&result2==0&&result3==0;
		if(!theoraValid)
			{
			delete videoDevice;
			videoDevice=0;
			delete theoraEncoder;
			theoraEncoder=0;
			delete yuvBuffer;
			yuvBuffer=0;
			}
		}
	
	/* Send the length of the following message: */
	unsigned int messageLength=sizeof(unsigned int)*3;
	messageLength+=sizeof(char);
	if(theoraValid)
		messageLength+=headerPackets[0].getNetworkSize()+headerPackets[1].getNetworkSize()+headerPackets[2].getNetworkSize()+sizeof(Scalar)*2;
	pipe.write<unsigned int>(messageLength);
	
	std::cout<<"AgoraClient::sendConnectRequest: Sending "<<messageLength<<" bytes"<<std::endl;
	
	/* Write the SPEEX frame and packet size: */
	pipe.write<unsigned int>(speexEncoder!=0?speexEncoder->getFrameSize():0);
	pipe.write<unsigned int>(speexEncoder!=0?speexEncoder->getPacketQueue().getSegmentSize():0);
	pipe.write<unsigned int>(speexEncoder!=0?speexEncoder->getPacketQueue().getMaxQueueSize():0);
	
	/* Write the Theora validity flag: */
	pipe.write<char>(theoraValid?1:0);
	
	if(theoraValid)
		{
		/* Write the Theora stream headers: */
		for(int i=0;i<3;++i)
			headerPackets[i].write(pipe);
		
		/* Write the virtual video image size: */
		for(int i=0;i<2;++i)
			pipe.write<Scalar>(videoSize[i]);
		
		/* From now on, the server expects us to send Theora video stream packets: */
		hasTheora=true;
		}
	}

void AgoraClient::receiveConnectReply(CollaborationPipe& pipe)
	{
	/* The server has accepted the Agora protocol connection; start recording audio and video: */
	if(videoDevice!=0)
		{
		try
			{
			/* Start capturing and encoding video: */
			std::cout<<"AgoraClient::receiveConnectReply: Starting video capture"<<std::endl;
			videoDevice->allocateFrameBuffers(5);
			videoDevice->startStreaming(new Misc::VoidMethodCall<const V4L2VideoDevice::FrameBuffer&,AgoraClient>(this,&AgoraClient::videoCaptureCallback));
			}
		catch(std::runtime_error err)
			{
			/* Print error message and disable video capture: */
			std::cerr<<"AgoraClient::receiveConnectReply: Disabling video capture due to exception "<<err.what()<<std::endl;
			delete videoDevice;
			videoDevice=0;
			delete theoraEncoder;
			theoraEncoder=0;
			delete yuvBuffer;
			yuvBuffer=0;
			}
		}
	}

void AgoraClient::receiveConnectReject(CollaborationPipe& pipe)
	{
	/* The server has rejected the Agora protocol connection; release all resources: */
	delete videoDevice;
	videoDevice=0;
	delete theoraEncoder;
	theoraEncoder=0;
	delete yuvBuffer;
	yuvBuffer=0;
	}

void AgoraClient::sendClientUpdate(CollaborationPipe& pipe)
	{
	if(Vrui::isMaster())
		{
		if(speexEncoder!=0)
			{
			/* Check if there is data in the SPEEX packet buffer: */
			size_t numSpeexPackets=speexEncoder->getPacketQueue().getQueueSize();
			
			/* Send all packets from the SPEEX queue: */
			pipe.write<unsigned short>(numSpeexPackets);
			for(size_t i=0;i<numSpeexPackets;++i)
				{
				const char* speexPacket=speexEncoder->getPacketQueue().popSegment();
				pipe.write<char>(speexPacket,speexEncoder->getPacketQueue().getSegmentSize());
				}
			}
		
		/* Check if the server expects a video capture state update: */
		if(hasTheora)
			{
			/* Check if there is a new video packet in the buffer: */
			if(theoraEncoder!=0&&oggPacketBuffer.lockNewValue())
				{
				/* Send the packet: */
				pipe.write<char>(1);
				oggPacketBuffer.getLockedValue().write(pipe);
				}
			else
				{
				/* Tell server there is no packet: */
				pipe.write<char>(0);
				}
			
			/* Send the current virtual video transformation: */
			OGTransform video=OGTransform(Vrui::getInverseNavigationTransformation());
			video*=videoTransform;
			pipe.writeTrackerState(video);
			}
		}
	}

AgoraClient::RemoteClientState* AgoraClient::receiveClientConnect(CollaborationPipe& pipe)
	{
	/* Create a new remote client state object: */
	RemoteClientState* newClientState=new RemoteClientState;
	
	/* Read the remote client's audio encoding state: */
	size_t remoteSpeexFrameSize=pipe.read<unsigned int>();
	size_t remoteSpeexPacketSize=pipe.read<unsigned int>();
	newClientState->speexPacketQueue.resize(remoteSpeexPacketSize,jitterBufferSize);
	if(remoteSpeexFrameSize>0&&Vrui::getNodeIndex()==playbackNodeIndex)
		{
		/* Create a SPEEX decoder object: */
		try
			{
			/* Create a SPEEX decoder: */
			newClientState->speexDecoder=new SpeexDecoder(playbackPcmDeviceName.c_str(),remoteSpeexFrameSize,newClientState->speexPacketQueue);
			}
		catch(std::runtime_error err)
			{
			/* Disable audio playback on any errors: */
			std::cerr<<"AgoraClient::receiveClientConnect: Disabling audio playback for remote client due to exception "<<err.what()<<std::endl;
			newClientState->speexDecoder=0; // Just to be safe
			}
		}
	
	/* Read the remote client's video encoding state: */
	newClientState->hasTheora=pipe.read<char>()!=0;
	if(newClientState->hasTheora)
		{
		try
			{
			/* Create an encoder info structure by reading the remote client's Theora stream header packets: */
			{
			std::cout<<"Reading remote client's video stream headers"<<std::endl;
			TheoraInfo decoderInfo;
			decoderInfo.readStreamHeaders(pipe);
			
			/* Remember the video stream's frame size: */
			newClientState->frameSize[0]=decoderInfo.width;
			newClientState->frameSize[1]=decoderInfo.height;
			
			/* Create a video decoder: */
			std::cout<<"Creating video decoder"<<std::endl;
			newClientState->theoraDecoder=new TheoraDecoder(decoderInfo);
			}
			
			/* Initialize a buffer for uncompressed video frames: */
			newClientState->yuvBuffer=new YpCbCrBuffer;
			
			/* Start the video decoding thread: */
			newClientState->videoDecodingThread.start(newClientState,&AgoraClient::RemoteClientState::videoDecodingThreadMethod);
			}
		catch(std::runtime_error err)
			{
			/* Disable video playback on any errors: */
			std::cerr<<"AgoraClient::receiveClientConnect: Disabling video playback for remote client due to exception "<<err.what()<<std::endl;
			delete newClientState->yuvBuffer;
			newClientState->yuvBuffer=0;
			delete newClientState->theoraDecoder;
			newClientState->theoraDecoder=0;
			}
		
		/* Read the remote client's virtual video size: */
		for(int i=0;i<2;++i)
			newClientState->videoSize[i]=pipe.read<Scalar>();
		}
	
	/* Return the new client state object: */
	return newClientState;
	}

void AgoraClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe)
	{
	/* Get a handle on the Agora state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("AgoraClient::receiveServerUpdate: Remote client state object has mismatching type");
	
	if(myRcs->speexPacketQueue.getSegmentSize()!=0)
		{
		/* Receive a number of SPEEX audio packets from the server and shove them into the remote client's decoding queue: */
		size_t numSpeexPackets=pipe.read<unsigned short>();
		for(size_t i=0;i<numSpeexPackets;++i)
			{
			char* speexPacket=myRcs->speexPacketQueue.getWriteSegment();
			pipe.read<char>(speexPacket,myRcs->speexPacketQueue.getSegmentSize());
			myRcs->speexPacketQueue.pushSegment();
			}
		}
	
	/* Check if the server sent a video state update: */
	if(myRcs->hasTheora)
		{
		/* Check for a new Theora packet from the server: */
		if(pipe.read<char>()!=0)
			{
			/* Push a new Theora packet onto the decoder queue: */
			myRcs->oggPacketBuffer.startNewValue().read(pipe);
			myRcs->oggPacketBuffer.postNewValue();
			
			/* Wake up the video decoding thread, just in case: */
			myRcs->newPacketCond.signal();
			}
		
		/* Read a new video transformation from the server: */
		myRcs->videoTransform.postNewValue(pipe.readTrackerState());
		}
	}

void AgoraClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	/* Get a handle on the Agora state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("AgoraClient::frame: Remote client state object has mismatching type");
	
	/* Lock any new frames from the video decoding thread: */
	if(myRcs->frameBuffer.lockNewValue())
		{
		/* Update the frame version: */
		++myRcs->frameVersion;
		}
	
	/* Lock the most recent video transformation: */
	myRcs->videoTransform.lockNewValue();
	}

void AgoraClient::display(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const
	{
	/* Get a handle on the Agora state object: */
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("AgoraClient::display: Remote client state object has mismatching type");
	
	if(myRcs->theoraDecoder!=0)
		myRcs->display(contextData);
	}

}

/****************
DSO entry points:
****************/

extern "C" {

Collaboration::ProtocolClient* createObject(Collaboration::ProtocolClientLoader& objectLoader)
	{
	return new Collaboration::AgoraClient;
	}

void destroyObject(Collaboration::ProtocolClient* object)
	{
	delete object;
	}

}
