/***********************************************************************
AgoraClient - Client object to implement the Agora group audio protocol.
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

#include <Collaboration/AgoraClient.h>

#include <iostream>
#include <Misc/FunctionCalls.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <IO/FixedMemoryFile.h>
#include <IO/VariableMemoryFile.h>
#include <Comm/NetPipe.h>
#include <Cluster/MulticastPipe.h>
#include <Cluster/ThreadSynchronizer.h>
#include <Geometry/GeometryValueCoders.h>
#include <GL/gl.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/Margin.h>
#include <GLMotif/RowColumn.h>
#include <Sound/Config.h>
#include <Video/Config.h>
#if VIDEO_CONFIG_HAVE_THEORA
#include <Video/VideoDevice.h>
#include <Video/ImageExtractor.h>
#include <Video/YpCbCr420Texture.h>
#include <Video/TheoraInfo.h>
#include <Video/TheoraComment.h>
#include <Video/VideoPane.h>
#endif
#include <AL/Config.h>
#include <AL/ALTemplates.h>
#include <AL/ALGeometryWrappers.h>
#include <AL/ALContextData.h>
#include <Vrui/Vrui.h>
#include <Vrui/Viewer.h>
#include <Collaboration/CollaborationClient.h>
#if SOUND_CONFIG_HAVE_SPEEX
#include <Collaboration/SpeexEncoder.h>
#include <Collaboration/SpeexDecoder.h>
#endif

namespace Collaboration {

/***********************************************************
Methods of class AgoraClient::RemoteClientState::ALDataItem:
***********************************************************/

AgoraClient::RemoteClientState::ALDataItem::ALDataItem(size_t sSpeexFrameSize,Threads::DropoutBuffer<char>& sSpeexPacketQueue)
	#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
	:speexDecoder(sSpeexFrameSize,sSpeexPacketQueue),
	 source(0),buffers(0)
	#endif
	{
	#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
	/* Generate the source and the buffers: */
	alGenSources(1,&source);
	size_t numBuffers=speexDecoder.getDecodedPacketQueue().getMaxQueueSize();
	buffers=new ALuint[numBuffers];
	for(size_t i=0;i<numBuffers;++i)
		buffers[i]=0;
	alGenBuffers(numBuffers,buffers);
	
	/* Create the stack of free buffers: */
	freeBuffers=new ALuint[numBuffers];
	numFreeBuffers=numBuffers;
	for(size_t i=0;i<numBuffers;++i)
		freeBuffers[i]=buffers[i];
	#endif
	}

AgoraClient::RemoteClientState::ALDataItem::~ALDataItem(void)
	{
	#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
	/* Destroy the source and buffers: */
	alSourceStop(source);
	alDeleteSources(1,&source);
	size_t numBuffers=speexDecoder.getDecodedPacketQueue().getMaxQueueSize();
	alDeleteBuffers(numBuffers,buffers);
	delete[] freeBuffers;
	delete[] buffers;
	#endif
	}

/***********************************************
Methods of class AgoraClient::RemoteClientState:
***********************************************/

#if VIDEO_CONFIG_HAVE_THEORA

void* AgoraClient::RemoteClientState::videoDecodingThreadMethod(void)
	{
	Threads::Thread::setCancelState(Threads::Thread::CANCEL_ENABLE);
	// Threads::Thread::setCancelType(Threads::Thread::CANCEL_ASYNCHRONOUS);
	
	while(true)
		{
		/* Wait until there is a new Theora packet in the packet buffer: */
		{
		Threads::MutexCond::Lock newPacketLock(newPacketCond);
		while(!theoraPacketBuffer.lockNewValue())
			newPacketCond.wait(newPacketLock);
		}
		
		/* Feed the packet into the video decoder: */
		theoraDecoder.processPacket(theoraPacketBuffer.getLockedValue());
		
		/* Check if the decoder has a frame ready: */
		if(theoraDecoder.isFrameReady())
			{
			/* Get the decoded frame from the decoder: */
			Video::TheoraFrame frame420;
			theoraDecoder.decodeFrame(frame420);
			
			/* Copy the frame into the decoded frame triple buffer: */
			Video::TheoraFrame& frame=theoraFrameBuffer.startNewValue();
			frame.copy(frame420);
			
			/* Post the new frame: */
			theoraFrameBuffer.postNewValue();
			Vrui::requestUpdate();
			}
		}
	
	return 0;
	}

#endif

AgoraClient::RemoteClientState::RemoteClientState(void)
	:remoteSpeexFrameSize(0),
	 rolloffFactor(1.0f),
	 speexPacketQueue(0,0),
	 hasTheora(false)
	 #if VIDEO_CONFIG_HAVE_THEORA
	 ,frameTexture(0)
	 #endif
	{
	for(int i=0;i<2;++i)
		videoSize[i]=Scalar(0);
	}

AgoraClient::RemoteClientState::~RemoteClientState(void)
	{
	#if VIDEO_CONFIG_HAVE_THEORA
	if(theoraDecoder.isValid())
		{
		/* Stop the video decoding thread: */
		videoDecodingThread.cancel();
		videoDecodingThread.join();
		}
	
	delete frameTexture;
	#endif
	}

void AgoraClient::RemoteClientState::initContext(ALContextData& contextData) const
	{
	#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
	if(remoteSpeexFrameSize>0)
		{
		#ifdef VERBOSE
		std::cout<<"AgoraClient::RemoteClientState::initContext: Initializing audio playback"<<std::endl;
		#endif
		
		/* Create and store a data item: */
		ALDataItem* dataItem=new ALDataItem(remoteSpeexFrameSize,speexPacketQueue);
		contextData.addDataItem(this,dataItem);
		
		/* Initialize the source: */
		alSourceLooping(dataItem->source,AL_FALSE);
		alSourcePitch(dataItem->source,1.0f);
		alSourceGain(dataItem->source,1.0f);
		alSourceRolloffFactor(dataItem->source,rolloffFactor);
		}
	#endif
	}

void AgoraClient::RemoteClientState::glRenderAction(GLContextData& contextData) const
	{
	glPushMatrix();
	glMultMatrix(localVideoTransform);
	
	#if VIDEO_CONFIG_HAVE_THEORA
	
	/* Install the remote client's video texture: */
	GLfloat tc[2];
	frameTexture->install(contextData,tc);
	
	/* Draw the texture: */
	glBegin(GL_QUADS);
	glTexCoord2f(0.0f,tc[1]);
	glVertex3f( videoSize[0],0.0f,-videoSize[1]);
	glTexCoord2f(tc[0],tc[1]);
	glVertex3f(-videoSize[0],0.0f,-videoSize[1]);
	glTexCoord2f(tc[0],0.0f);
	glVertex3f(-videoSize[0],0.0f, videoSize[1]);
	glTexCoord2f(0.0f,0.0f);
	glVertex3f( videoSize[0],0.0f, videoSize[1]);
	glEnd();
	
	/* Uninstall the video texture: */
	frameTexture->uninstall(contextData);
	
	#else
	
	/* Draw the pretend texture: */
	glBegin(GL_QUADS);
	glVertex3f( videoSize[0],0.0f,-videoSize[1]);
	glVertex3f(-videoSize[0],0.0f,-videoSize[1]);
	glVertex3f(-videoSize[0],0.0f, videoSize[1]);
	glVertex3f( videoSize[0],0.0f, videoSize[1]);
	glEnd();
	
	#endif
	
	/* Draw the video frame's back side: */
	glBegin(GL_QUADS);
	glNormal3f(0.0f,-1.0f,0.0f);
	glVertex3f(-videoSize[0],0.0f,-videoSize[1]);
	glVertex3f( videoSize[0],0.0f,-videoSize[1]);
	glVertex3f( videoSize[0],0.0f, videoSize[1]);
	glVertex3f(-videoSize[0],0.0f, videoSize[1]);
	glEnd();
	
	glPopMatrix();
	}

void AgoraClient::RemoteClientState::alRenderAction(ALContextData& contextData) const
	{
	#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
	/* Get the data item: */
	ALDataItem* dataItem=contextData.retrieveDataItem<ALDataItem>(this);
	
	/* Put all processed source buffers back into the free buffer stack: */
	ALint numProcessedBuffers;
	alGetSourcei(dataItem->source,AL_BUFFERS_PROCESSED,&numProcessedBuffers);
	if(numProcessedBuffers>0)
		{
		alSourceUnqueueBuffers(dataItem->source,numProcessedBuffers,dataItem->freeBuffers+dataItem->numFreeBuffers);
		dataItem->numFreeBuffers+=numProcessedBuffers;
		}
	
	/* Lock all ready buffers in the SPEEX decoder's output queue: */
	Threads::DropoutBuffer<signed short int>& queue=dataItem->speexDecoder.getDecodedPacketQueue();
	size_t numSegments=queue.lockQueue();
	
	/* Queue all ready buffers on the source: */
	for(size_t i=0;i<numSegments&&dataItem->numFreeBuffers>0;++i)
		{
		/* Fill the next free buffer: */
		--dataItem->numFreeBuffers;
		alBufferData(dataItem->freeBuffers[dataItem->numFreeBuffers],AL_FORMAT_MONO16,queue.getLockedSegment(i),remoteSpeexFrameSize*sizeof(signed short int),16000);
		
		/* Enqueue the buffer: */
		alSourceQueueBuffers(dataItem->source,1,&dataItem->freeBuffers[dataItem->numFreeBuffers]);
		}
	
	/* Remove all locked segments from the SPEEX decoder's output queue: */
	queue.unlockQueue();
	
	/* Check for underflow condition: */
	ALint sourceState;
	alGetSourcei(dataItem->source,AL_SOURCE_STATE,&sourceState);
	if(sourceState!=AL_PLAYING&&dataItem->numFreeBuffers<=queue.getMaxQueueSize()/2)
		{
		/* (Re-)start the source: */
		alSourcePlay(dataItem->source);
		}
	
	/* Set the source position: */
	alSourcePosition(dataItem->source,localMouthPosition);
	
	/* Set the source attenuation factors in physical coordinates: */
	alSourceReferenceDistance(dataItem->source,Vrui::getMeterFactor()*Vrui::Scalar(2));
	#endif
	}

/****************************
Methods of class AgoraClient:
****************************/

void AgoraClient::videoCaptureCallback(const Video::FrameBuffer* frame)
	{
	#if VIDEO_CONFIG_HAVE_THEORA
	
	/* Start a new frame in the Theora frame buffer: */
	Video::TheoraFrame& theoraFrame=theoraFrameBuffer.startNewValue();
	
	/* Extract a Y'CbCr 4:2:0 image from the raw video frame: */
	void* base[3];
	unsigned int stride[3];
	for(int i=0;i<3;++i)
		{
		base[i]=theoraFrame.planes[i].data+theoraFrame.offsets[i];
		stride[i]=theoraFrame.planes[i].stride;
		}
	videoExtractor->extractYpCbCr420(frame,base[0],stride[0],base[1],stride[1],base[2],stride[2]);
	
	/* Post the new uncompressed frame: */
	theoraFrameBuffer.postNewValue();
	if(localVideoWindowShown)
		Vrui::requestUpdate();
	
	/* Feed the new image to the Theora encoder: */
	theoraEncoder.encodeFrame(theoraFrame);
	
	/* Grab the encoded Theora packet: */
	Video::TheoraPacket packet;
	if(theoraEncoder.emitPacket(packet))
		{
		/* Copy the new packet into the delivery buffer: */
		Video::TheoraPacket& buffer=theoraPacketBuffer.startNewValue();
		buffer=packet;
		theoraPacketBuffer.postNewValue();
		}
	
	#endif
	}

void AgoraClient::showVideoDeviceSettingsCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	if(cbData->set)
		{
		/* Pop up the video device settings dialog: */
		Vrui::popupPrimaryWidget(videoDeviceSettings);
		}
	else
		{
		/* Pop down the video device settings dialog: */
		Vrui::popdownPrimaryWidget(videoDeviceSettings);
		}
	}

void AgoraClient::pauseAudioCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	/* Pause or unpause audio transmission: */
	pauseAudio=cbData->set;
	}

void AgoraClient::pauseVideoCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	/* Pause or unpause video transmission: */
	pauseVideo=cbData->set;
	}

void AgoraClient::showLocalVideoWindowCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
	if(cbData->set)
		{
		/* Pop up the local video window: */
		Vrui::popupPrimaryWidget(localVideoWindow);
		}
	else
		{
		/* Pop down the local video window: */
		Vrui::popdownPrimaryWidget(localVideoWindow);
		}
	localVideoWindowShown=cbData->set;
	}

void AgoraClient::videoDeviceSettingsCloseCallback(Misc::CallbackData* cbData)
	{
	if(showVideoDeviceSettingsToggle!=0)
		showVideoDeviceSettingsToggle->setToggle(false);
	}

void AgoraClient::localVideoWindowCloseCallback(Misc::CallbackData* cbData)
	{
	if(showLocalVideoWindowToggle!=0)
		showLocalVideoWindowToggle->setToggle(false);
	localVideoWindowShown=false;
	}

AgoraClient::AgoraClient(void)
	:mouthPosition(Vrui::getMainViewer()->getDeviceEyePosition(Vrui::Viewer::MONO)),
	 #if SOUND_CONFIG_HAVE_SPEEX
	 speexEncoder(0),
	 #endif
	 haveAudio(false),pauseAudio(false),
	 jitterBufferSize(6),rolloffFactor(1.0f),
	 hasTheora(false),
	 videoDevice(0),videoExtractor(0),
	 videoDeviceSettings(0),
	 showVideoDeviceSettingsToggle(0),showLocalVideoWindowToggle(0),
	 localVideoWindow(0),videoPane(0),
	 haveVideo(false),localVideoWindowShown(false),pauseVideo(false)
	{
	}

AgoraClient::~AgoraClient(void)
	{
	#if SOUND_CONFIG_HAVE_SPEEX
	delete speexEncoder;
	#endif
	delete videoDeviceSettings;
	delete localVideoWindow;
	if(videoDevice!=0)
		{
		/* Stop streaming: */
		videoDevice->stopStreaming();
		videoDevice->releaseFrameBuffers();
		
		/* Release all resources: */
		delete videoExtractor;
		delete videoDevice;
		}
	}

const char* AgoraClient::getName(void) const
	{
	return protocolName;
	}

void AgoraClient::initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection)
	{
	/* Call the base class method: */
	ProtocolClient::initialize(sClient,configFileSection);
	
	/* Synchronize thread IDs between here and end of method body: */
	Cluster::ThreadSynchronizer threadSynchronizer(Vrui::getMainPipe());
	
	/**************************
	Initialize audio recording:
	**************************/
	
	#if SOUND_CONFIG_HAVE_SPEEX
	
	if(Vrui::isMaster()&&configFileSection.retrieveValue<bool>("./enableRecording",true))
		{
		try
			{
			/* Configure the mouth position: */
			mouthPosition=configFileSection.retrieveValue<Point>("./mouthPosition",mouthPosition);
			
			/* Create a SPEEX encoder: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::initialize: Creating audio encoder"<<std::endl;
			#endif
			std::string recordingPcmDeviceName=configFileSection.retrieveValue<std::string>("./recordingPcmDeviceName","default");
			size_t sendQueueSize=configFileSection.retrieveValue<unsigned int>("./sendQueueSize",8);
			speexEncoder=new SpeexEncoder(recordingPcmDeviceName.c_str(),sendQueueSize);
			
			/* Remember that the client has audio: */
			haveAudio=true;
			}
		catch(std::runtime_error err)
			{
			/* Disable audio recording on errors: */
			std::cerr<<"AgoraClient::AgoraClient: Disabling sound recording due to exception "<<err.what()<<std::endl;
			delete speexEncoder;
			speexEncoder=0;
			}
		}
	
	#endif
	
	/*************************
	Initialize audio playback:
	*************************/
	
	/* Get the playback jitter buffer size: */
	jitterBufferSize=configFileSection.retrieveValue<unsigned int>("./jitterBufferSize",(unsigned int)jitterBufferSize);
	
	/* Get the rolloff factor for remote sound sources: */
	rolloffFactor=configFileSection.retrieveValue<float>("./rolloffFactor",rolloffFactor);
	
	/**************************
	Initialize video recording:
	**************************/
	
	#if VIDEO_CONFIG_HAVE_THEORA
	
	if(Vrui::isMaster()&&configFileSection.retrieveValue<bool>("./enableCapture",true))
		{
		try
			{
			/* Get the list of video devices available on the system: */
			std::vector<Video::VideoDevice::DeviceIdPtr> videoDevices=Video::VideoDevice::getVideoDevices();
			if(videoDevices.empty())
				Misc::throwStdErr("No video capture devices found");
			
			/* Read the name of the requested video device: */
			std::string videoDeviceName=configFileSection.retrieveValue<std::string>("./captureVideoDeviceName",videoDevices[0]->getName());
			
			/* Find the requested video device in the list: */
			if(videoDeviceName=="default")
				videoDevice=Video::VideoDevice::createVideoDevice(videoDevices[0]);
			else
				{
				for(std::vector<Video::VideoDevice::DeviceIdPtr>::iterator vdIt=videoDevices.begin();vdIt!=videoDevices.end();++vdIt)
					if((*vdIt)->getName()==videoDeviceName)
						videoDevice=Video::VideoDevice::createVideoDevice(*vdIt);
				}
			if(videoDevice==0)
				Misc::throwStdErr("Video capture device \"%s\" not found",videoDeviceName.c_str());
			
			/* Configure the video device's video format and device settings: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::initialize: Configuring video capture device"<<std::endl;
			#endif
			videoDevice->configure(configFileSection);
			
			/* Retrieve the video device's video format: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::initialize: Retrieving video format"<<std::endl;
			#endif
			Video::VideoDataFormat videoFormat=videoDevice->getVideoFormat();
			#ifdef VERBOSE
			std::cout<<"AgoraClient::initialize: Selected video format is "<<videoFormat.size[0]<<'x'<<videoFormat.size[1]<<'@'<<double(videoFormat.frameIntervalDenominator)/double(videoFormat.frameIntervalCounter)<<"Hz"<<std::endl;
			#endif
			
			/* Get the video device's image extractor: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::initialize: Creating video image extractor"<<std::endl;
			#endif
			videoExtractor=videoDevice->createImageExtractor();
			
			/* Create the Theora video encoder: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::initialize: Creating Theora video encoder"<<std::endl;
			#endif
			{
			Video::TheoraInfo theoraInfo;
			theoraInfo.setImageSize(videoFormat.size);
			theoraInfo.colorspace=TH_CS_UNSPECIFIED;
			theoraInfo.pixel_fmt=TH_PF_420;
			theoraInfo.target_bitrate=configFileSection.retrieveValue<int>("./theoraBitrate",0);
			theoraInfo.setQuality(configFileSection.retrieveValue<int>("./theoraQuality",32));
			theoraInfo.setGopSize(configFileSection.retrieveValue<int>("./theoraGopSize",theoraInfo.getGopSize()));
			theoraInfo.fps_numerator=videoFormat.frameIntervalDenominator;
			theoraInfo.fps_denominator=videoFormat.frameIntervalCounter;
			theoraInfo.aspect_numerator=1;
			theoraInfo.aspect_denominator=1;
			theoraEncoder.init(theoraInfo);
			
			/* Set the encoder to maximum speed: */
			theoraEncoder.setSpeedLevel(theoraEncoder.getMaxSpeedLevel());
			
			/* Create an intermediate buffer to feed frames to the Theora encoder: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::initialize: Creating Theora video encoding buffer"<<std::endl;
			#endif
			for(int i=0;i<3;++i)
				theoraFrameBuffer.getBuffer(i).init420(theoraInfo);
			}
			
			/* Read the video transformation and video image size: */
			videoTransform=configFileSection.retrieveValue<ONTransform>("./virtualVideoTransform",ONTransform::identity);
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
			
			/* Remember that the client has video: */
			haveVideo=true;
			}
		catch(std::runtime_error err)
			{
			/* Disable video capture on errors: */
			std::cerr<<"AgoraClient::AgoraClient: Disabling video capture due to exception "<<err.what()<<std::endl;
			
			/* Clean up: */
			for(int i=0;i<3;++i)
				theoraFrameBuffer.getBuffer(i).release();
			theoraEncoder.release();
			delete videoExtractor;
			videoExtractor=0;
			delete videoDevice;
			videoDevice=0;
			}
		}
	
	#endif
	
	#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
	
	/* Request sound processing: */
	Vrui::requestSound();
	
	#endif
	
	if(Vrui::getMainPipe()!=0)
		{
		/* Share the audio and video flags with the slaves: */
		if(Vrui::isMaster())
			{
			Vrui::getMainPipe()->write<Byte>(haveAudio?1:0);
			Vrui::getMainPipe()->write<Byte>(haveVideo?1:0);
			Vrui::getMainPipe()->flush();
			}
		else
			{
			haveAudio=Vrui::getMainPipe()->read<Byte>()!=0;
			haveVideo=Vrui::getMainPipe()->read<Byte>()!=0;
			}
		}
	}

bool AgoraClient::haveSettingsDialog(void) const
	{
	return haveAudio||haveVideo;
	}

void AgoraClient::buildSettingsDialog(GLMotif::RowColumn* settingsDialog)
	{
	if(haveVideo)
		{
		/* Create a toggle to show the video source settings dialog: */
		GLMotif::Margin* showVideoDeviceSettingsMargin=new GLMotif::Margin("ShowVideoDeviceSettingsMargin",settingsDialog,false);
		showVideoDeviceSettingsMargin->setAlignment(GLMotif::Alignment::LEFT);
		
		showVideoDeviceSettingsToggle=new GLMotif::ToggleButton("ShowVideoDeviceSettingsToggle",showVideoDeviceSettingsMargin,"Show Video Device Settings");
		showVideoDeviceSettingsToggle->setBorderWidth(0.0f);
		showVideoDeviceSettingsToggle->setHAlignment(GLFont::Left);
		showVideoDeviceSettingsToggle->setToggle(false);
		showVideoDeviceSettingsToggle->getValueChangedCallbacks().add(this,&AgoraClient::showVideoDeviceSettingsCallback);
		
		showVideoDeviceSettingsMargin->manageChild();
		
		/* Create a toggle to show the local video feed: */
		GLMotif::Margin* showLocalVideoWindowMargin=new GLMotif::Margin("ShowLocalVideoWindowMargin",settingsDialog,false);
		showLocalVideoWindowMargin->setAlignment(GLMotif::Alignment::LEFT);
		
		showLocalVideoWindowToggle=new GLMotif::ToggleButton("ShowLocalVideoWindowToggle",showLocalVideoWindowMargin,"Show Local Video Feed");
		showLocalVideoWindowToggle->setBorderWidth(0.0f);
		showLocalVideoWindowToggle->setHAlignment(GLFont::Left);
		showLocalVideoWindowToggle->setToggle(false);
		showLocalVideoWindowToggle->getValueChangedCallbacks().add(this,&AgoraClient::showLocalVideoWindowCallback);
		
		showLocalVideoWindowMargin->manageChild();
		}
	
	if(haveAudio||haveVideo)
		{
		/* Create toggle buttons to pause audio and video transmission, respectively: */
		GLMotif::Margin* pauseTogglesMargin=new GLMotif::Margin("PauseTogglesMargin",settingsDialog,false);
		pauseTogglesMargin->setAlignment(GLMotif::Alignment::LEFT);
		
		GLMotif::RowColumn* pauseTogglesBox=new GLMotif::RowColumn("PauseTogglesBox",pauseTogglesMargin,false);
		pauseTogglesBox->setOrientation(GLMotif::RowColumn::HORIZONTAL);
		pauseTogglesBox->setPacking(GLMotif::RowColumn::PACK_TIGHT);
		pauseTogglesBox->setNumMinorWidgets(1);
		
		if(haveAudio)
			{
			GLMotif::ToggleButton* pauseAudioToggle=new GLMotif::ToggleButton("PauseAudioToggle",pauseTogglesBox,"Pause Audio");
			pauseAudioToggle->setBorderWidth(0.0f);
			pauseAudioToggle->setHAlignment(GLFont::Left);
			pauseAudioToggle->setToggle(false);
			pauseAudioToggle->getValueChangedCallbacks().add(this,&AgoraClient::pauseAudioCallback);
			}
		
		if(haveVideo)
			{
			GLMotif::ToggleButton* pauseVideoToggle=new GLMotif::ToggleButton("PauseVideoToggle",pauseTogglesBox,"Pause Video");
			pauseVideoToggle->setBorderWidth(0.0f);
			pauseVideoToggle->setHAlignment(GLFont::Left);
			pauseVideoToggle->setToggle(false);
			pauseVideoToggle->getValueChangedCallbacks().add(this,&AgoraClient::pauseVideoCallback);
			}
		
		pauseTogglesBox->manageChild();
		
		pauseTogglesMargin->manageChild();
		}
	}

void AgoraClient::sendConnectRequest(Comm::NetPipe& pipe)
	{
	/* Bail out if not on the master node: */
	if(!Vrui::isMaster())
		return;
	
	/* Calculate the length of the following message: */
	unsigned int messageLength=sizeof(Card); // Protocol version number
	messageLength+=sizeof(Point::Scalar)*3; // Mouth position
	messageLength+=sizeof(Card)*3; // SPEEX format parameters
	messageLength+=sizeof(Byte); // Video streaming flag
	
	#if VIDEO_CONFIG_HAVE_THEORA
	
	/* Write the Theora encoder's stream header packets into a little-endian temporary buffer: */
	IO::VariableMemoryFile theoraHeaders;
	theoraHeaders.setEndianness(Misc::LittleEndian);
	if(haveVideo)
		{
		/* Write the headers: */
		Video::TheoraComment comments;
		theoraEncoder.writeHeaders(comments,theoraHeaders);
		
		/* Increase the message size: */
		messageLength+=Misc::Marshaller<ONTransform>::getSize(videoTransform); // Virtual video transformation
		messageLength+=sizeof(Scalar)*2; // Virtual video size
		messageLength+=sizeof(Card); // Length of header packets
		messageLength+=theoraHeaders.getDataSize(); // Header packets
		}
	
	#endif
	
	/* Send the length of the following message: */
	#ifdef VERBOSE
	std::cout<<"AgoraClient::sendConnectRequest: Sending "<<messageLength<<" bytes"<<std::endl;
	#endif
	pipe.write<Card>(messageLength);
	
	/* Write the protocol version number: */
	pipe.write<Card>(protocolVersion);
	
	/* Write the mouth position: */
	write(mouthPosition,pipe);
	
	#if SOUND_CONFIG_HAVE_SPEEX
	
	/* Write the SPEEX frame and packet size: */
	pipe.write<Card>(speexEncoder!=0?speexEncoder->getFrameSize():0);
	pipe.write<Card>(speexEncoder!=0?speexEncoder->getPacketQueue().getSegmentSize():0);
	pipe.write<Card>(speexEncoder!=0?speexEncoder->getPacketQueue().getMaxQueueSize():0);
	
	#else
	
	/* Tell the server we don't have audio recording: */
	pipe.write<Card>(0);
	pipe.write<Card>(0);
	pipe.write<Card>(0);
	
	#endif
	
	#if VIDEO_CONFIG_HAVE_THEORA
	
	if(theoraEncoder.isValid())
		{
		/* Write the Theora validity flag: */
		pipe.write<Byte>(1);
		
		/* From now on, the server expects us to send Theora video stream packets: */
		hasTheora=true;
		
		/* Write the virtual video transformation: */
		write(videoTransform,pipe);
		pipe.write(videoSize,2);
		
		/* Write the size of the Theora stream headers: */
		pipe.write<Card>(theoraHeaders.getDataSize());
		
		/* Write the Theora stream headers: */
		theoraHeaders.writeToSink(pipe);
		}
	else
		{
		/* Write the Theora validity flag: */
		pipe.write<Byte>(0);
		}
	
	#else
	
	/* Write the Theora validity flag: */
	pipe.write<Byte>(0);
	
	#endif
	}

void AgoraClient::receiveConnectReply(Comm::NetPipe& pipe)
	{
	/* Synchronize thread IDs between here and end of method body: */
	Cluster::ThreadSynchronizer threadSynchronizer(Vrui::getMainPipe());
	
	#if VIDEO_CONFIG_HAVE_THEORA
	
	/* The server has accepted the Agora protocol connection; start recording video: */
	if(videoDevice!=0)
		{
		try
			{
			/* Create the video device's settings dialog: */
			videoDeviceSettings=videoDevice->createControlPanel(Vrui::getWidgetManager());
			GLMotif::PopupWindow* vdsPopup=dynamic_cast<GLMotif::PopupWindow*>(videoDeviceSettings);
			if(vdsPopup!=0)
				{
				/* Add a close button to the device's settings dialog: */
				vdsPopup->setCloseButton(true);
				vdsPopup->getCloseCallbacks().add(this,&AgoraClient::videoDeviceSettingsCloseCallback);
				}
			
			/* Create the local video window: */
			localVideoWindow=new GLMotif::PopupWindow("AgoraClientLocalVideoWindow",Vrui::getWidgetManager(),"Local Video Feed");
			localVideoWindow->setCloseButton(true);
			localVideoWindow->getCloseCallbacks().add(this,&AgoraClient::localVideoWindowCloseCallback);
			
			/* Create the video display pane: */
			videoPane=new GLMotif::VideoPane("VideoPane",localVideoWindow);
			Video::VideoDataFormat format=videoDevice->getVideoFormat();
			videoPane->getTexture().setFrameSize(format.size[0],format.size[1]);
			float videoRes=float(Vrui::getInchFactor())/300.0f; // Initial video resolution is 300dpi
			videoPane->setPreferredSize(GLMotif::Vector(float(format.size[0])*videoRes,float(format.size[1])*videoRes,0.0f));
			
			/* Start capturing and encoding video: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::receiveConnectReply: Starting video capture"<<std::endl;
			#endif
			videoDevice->allocateFrameBuffers(5);
			videoDevice->startStreaming(Misc::createFunctionCall(this,&AgoraClient::videoCaptureCallback));
			}
		catch(std::runtime_error err)
			{
			/* Print error message and disable video capture: */
			std::cerr<<"AgoraClient::receiveConnectReply: Disabling video capture due to exception "<<err.what()<<std::endl;
			for(int i=0;i<3;++i)
				theoraFrameBuffer.getBuffer(i).release();
			theoraEncoder.release();
			delete videoExtractor;
			videoExtractor=0;
			delete videoDevice;
			videoDevice=0;
			delete videoDeviceSettings;
			videoDeviceSettings=0;
			delete localVideoWindow;
			localVideoWindow=0;
			}
		}
	
	#endif
	}

void AgoraClient::receiveConnectReject(Comm::NetPipe& pipe)
	{
	/* The server has rejected the Agora protocol connection; release all resources: */
	#if SOUND_CONFIG_HAVE_SPEEX
	delete speexEncoder;
	speexEncoder=0;
	#endif
	haveAudio=false;
	
	#if VIDEO_CONFIG_HAVE_THEORA
	
	for(int i=0;i<3;++i)
		theoraFrameBuffer.getBuffer(i).release();
	theoraEncoder.release();
	delete videoExtractor;
	videoExtractor=0;
	delete videoDevice;
	videoDevice=0;
	
	#endif
	haveVideo=false;
	}

AgoraClient::RemoteClientState* AgoraClient::receiveClientConnect(Comm::NetPipe& pipe)
	{
	/* Create a new remote client state object: */
	RemoteClientState* newClientState=new RemoteClientState;
	
	/* Read the remote client's audio encoding state: */
	read(newClientState->mouthPosition,pipe);
	newClientState->remoteSpeexFrameSize=pipe.read<Card>();
	size_t remoteSpeexPacketSize=pipe.read<Card>();
	newClientState->speexPacketQueue.resize(remoteSpeexPacketSize,jitterBufferSize);
	#ifdef VERBOSE
	if(newClientState->remoteSpeexFrameSize>0)
		std::cout<<"AgoraClient::receiveClientConnect: Enabling audio for remote client"<<std::endl;
	#endif
	
	/* Initialize the remote client's audio playback state: */
	newClientState->rolloffFactor=rolloffFactor;
	
	/* Read the remote client's video encoding state: */
	newClientState->hasTheora=pipe.read<Byte>()!=0;
	if(newClientState->hasTheora)
		{
		/* Read the remote client's video transformation: */
		read(newClientState->videoTransform,pipe);
		pipe.read(newClientState->videoSize,2);
		
		try
			{
			#if VIDEO_CONFIG_HAVE_THEORA
			
			/* Read the remote client's Theora stream header packets: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::receiveClientConnect: Reading remote client's video stream headers"<<std::endl;
			#endif
			unsigned int headerSize=pipe.read<Card>();
			IO::FixedMemoryFile theoraHeaders(headerSize);
			theoraHeaders.setEndianness(Misc::LittleEndian);
			pipe.readRaw(theoraHeaders.getMemory(),headerSize);
			
			/* Initialize the Theora decoder by processing all stream header packets: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::receiveClientConnect: Initializing video decoder"<<std::endl;
			#endif
			{
			/* Create the setup structures: */
			Video::TheoraInfo theoraInfo;
			Video::TheoraComment theoraComments;
			Video::TheoraDecoder::Setup theoraSetup;
			
			/* Process all header packets: */
			while(!theoraHeaders.eof())
				{
				/* Read and process the next packet: */
				Video::TheoraPacket packet;
				packet.read(theoraHeaders);
				Video::TheoraDecoder::processHeader(packet,theoraInfo,theoraComments,theoraSetup);
				}
			
			/* Initialize the decoder: */
			newClientState->theoraDecoder.init(theoraInfo,theoraSetup);
			
			/* Initialize the triple buffer of decoded frames: */
			for(int i=0;i<3;++i)
				newClientState->theoraFrameBuffer.getBuffer(i).init420(theoraInfo);
			
			/* Initialize the video frame texture: */
			newClientState->frameTexture=new Video::YpCbCr420Texture;
			newClientState->frameTexture->setFrameSize(theoraInfo.pic_width,theoraInfo.pic_height);
			}
			
			/* Start the video decoding thread: */
			newClientState->videoDecodingThread.start(newClientState,&AgoraClient::RemoteClientState::videoDecodingThreadMethod);
			
			#else
			
			/* Skip the remote client's Theora stream header packets: */
			#ifdef VERBOSE
			std::cout<<"AgoraClient::receiveClientConnect: Skipping remote client's video stream headers"<<std::endl;
			#endif
			unsigned int headerSize=pipe.read<Card>();
			pipe.skip<Byte>(headerSize);
			
			#endif
			}
		catch(std::runtime_error err)
			{
			#if VIDEO_CONFIG_HAVE_THEORA
			
			/* Disable video playback on any errors: */
			std::cerr<<"AgoraClient::receiveClientConnect: Disabling video playback for remote client due to exception "<<err.what()<<std::endl;
			newClientState->theoraDecoder.release();
			
			#endif
			}
		}
	
	/* Return the new client state object: */
	return newClientState;
	}

bool AgoraClient::receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,Comm::NetPipe& pipe)
	{
	bool result=false;
	
	/* Get a handle on the Agora state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("AgoraClient::receiveServerUpdate: Remote client state object has mismatching type");
	
	if(myRcs->remoteSpeexFrameSize>0)
		{
		/* Receive a number of SPEEX audio packets from the server and shove them into the remote client's decoding queue: */
		size_t numSpeexPackets=pipe.read<Misc::UInt16>();
		for(size_t i=0;i<numSpeexPackets;++i)
			{
			char* speexPacket=myRcs->speexPacketQueue.getWriteSegment();
			pipe.read(speexPacket,myRcs->speexPacketQueue.getSegmentSize());
			myRcs->speexPacketQueue.pushSegment();
			}
		
		result=true;
		}
	
	/* Check if the server sent a video state update: */
	if(myRcs->hasTheora)
		{
		/* Check for a new Theora packet from the server: */
		if(pipe.read<Byte>()!=0)
			{
			#if VIDEO_CONFIG_HAVE_THEORA
			
			/* Push a new Theora packet onto the decoder queue: */
			myRcs->theoraPacketBuffer.startNewValue().read(pipe);
			myRcs->theoraPacketBuffer.postNewValue();
			
			/* Wake up the video decoding thread, just in case: */
			myRcs->newPacketCond.signal();
			
			#else
			
			/* Skip the new Theora packet: */
			VideoPacket packet;
			packet.read(pipe);
			
			#endif
			}
		
		result=true;
		}
	
	return result;
	}

void AgoraClient::sendClientUpdate(Comm::NetPipe& pipe)
	{
	/* Bail out if on a slave node: */
	if(!Vrui::isMaster())
		return;
	
	#if SOUND_CONFIG_HAVE_SPEEX
	
	if(speexEncoder!=0)
		{
		/* Check if there is data in the SPEEX packet buffer: */
		size_t numSpeexPackets=speexEncoder->getPacketQueue().getQueueSize();
		
		if(pauseAudio)
			{
			/* Discard all SPEEX packets: */
			pipe.write<Misc::UInt16>(0);
			for(size_t i=0;i<numSpeexPackets;++i)
				speexEncoder->getPacketQueue().popSegment();
			}
		else
			{
			/* Send all packets from the SPEEX queue: */
			pipe.write<Misc::UInt16>(numSpeexPackets);
			for(size_t i=0;i<numSpeexPackets;++i)
				{
				const char* speexPacket=speexEncoder->getPacketQueue().popSegment();
				pipe.write(speexPacket,speexEncoder->getPacketQueue().getSegmentSize());
				}
			}
		}
	
	#endif
	
	#if VIDEO_CONFIG_HAVE_THEORA
	
	/* Check if the server expects a video capture state update: */
	if(hasTheora)
		{
		/* Check if there is a new video packet in the buffer: */
		if(theoraEncoder.isValid()&&!pauseVideo&&theoraPacketBuffer.lockNewValue())
			{
			/* Send the packet: */
			pipe.write<Byte>(1);
			theoraPacketBuffer.getLockedValue().write(pipe);
			}
		else
			{
			/* Tell server there is no packet: */
			pipe.write<Byte>(0);
			}
		}
	
	#endif
	}

void AgoraClient::frame(void)
	{
	#if VIDEO_CONFIG_HAVE_THEORA
	
	if(localVideoWindowShown)
		{
		/* Lock the most recent uncompressed local video frame: */
		if(theoraFrameBuffer.lockNewValue())
			{
			Video::TheoraFrame& theoraFrame=theoraFrameBuffer.getLockedValue();
			void* base[3];
			unsigned int stride[3];
			for(int i=0;i<3;++i)
				{
				base[i]=theoraFrame.planes[i].data+theoraFrame.offsets[i];
				stride[i]=theoraFrame.planes[i].stride;
				}
			videoPane->getTexture().setFrame(base[0],stride[0],base[1],stride[1],base[2],stride[2]);
			}
		}
	
	#endif
	}

void AgoraClient::frame(ProtocolClient::RemoteClientState* rcs)
	{
	/* Get a handle on the Agora state object: */
	RemoteClientState* myRcs=dynamic_cast<RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("AgoraClient::frame: Remote client state object has mismatching type");
	
	/* Get the remote client's current client state: */
	const CollaborationProtocol::ClientState& cs=client->getClientState(rcs).getLockedValue();
	
	if(myRcs->remoteSpeexFrameSize!=0)
		{
		/* Update the remote client's local mouth position: */
		Vrui::Point mouthPos=Vrui::Point(cs.navTransform.inverseTransform(cs.viewerStates[0].transform(mouthPosition)));
		myRcs->localMouthPosition=Point(Vrui::getNavigationTransformation().transform(mouthPos));
		}
	
	if(myRcs->hasTheora)
		{
		#if VIDEO_CONFIG_HAVE_THEORA
		
		/* Lock any new frames from the video decoding thread: */
		if(myRcs->theoraFrameBuffer.lockNewValue())
			{
			/* Send the new frame to the frame texture: */
			const Video::TheoraFrame& frame=myRcs->theoraFrameBuffer.getLockedValue();
			const void* planes[3];
			unsigned int strides[3];
			for(int i=0;i<3;++i)
				{
				planes[i]=frame.planes[i].data+frame.offsets[i];
				strides[i]=frame.planes[i].stride;
				}
			myRcs->frameTexture->setFrame(planes[0],strides[0],planes[1],strides[1],planes[2],strides[2]);
			}
		
		#endif
		
		/* Update the remote client's local video transformation: */
		myRcs->localVideoTransform=cs.navTransform;
		myRcs->localVideoTransform.doInvert();
		myRcs->localVideoTransform*=OGTransform(myRcs->videoTransform);
		}
	}

void AgoraClient::glRenderAction(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const
	{
	/* Get a handle on the Agora state object: */
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("AgoraClient::glRenderAction: Remote client state object has mismatching type");
	
	if(myRcs->hasTheora)
		myRcs->glRenderAction(contextData);
	}

void AgoraClient::alRenderAction(const ProtocolClient::RemoteClientState* rcs,ALContextData& contextData) const
	{
	#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
	/* Get a handle on the Agora state object: */
	const RemoteClientState* myRcs=dynamic_cast<const RemoteClientState*>(rcs);
	if(myRcs==0)
		Misc::throwStdErr("AgoraClient::alRenderAction: Remote client state object has mismatching type");
	
	if(myRcs->remoteSpeexFrameSize!=0)
		myRcs->alRenderAction(contextData);
	#endif
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
