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

#ifndef COLLABORATION_AGORACLIENT_INCLUDED
#define COLLABORATION_AGORACLIENT_INCLUDED

#include <string>
#include <Threads/MutexCond.h>
#include <Threads/TripleBuffer.h>
#include <Threads/DropoutBuffer.h>
#include <GL/gl.h>
#include <GLMotif/ToggleButton.h>
#include <Sound/Config.h>
#include <Video/Config.h>
#if VIDEO_CONFIG_HAVE_THEORA
#include <Video/TheoraPacket.h>
#include <Video/TheoraFrame.h>
#include <Video/TheoraEncoder.h>
#include <Video/TheoraDecoder.h>
#endif
#include <AL/Config.h>
#include <AL/ALObject.h>
#include <Collaboration/ProtocolClient.h>
#include <Collaboration/AgoraProtocol.h>
#if SOUND_CONFIG_HAVE_SPEEX
#include <Collaboration/SpeexDecoder.h>
#endif

/* Forward declarations: */
namespace Misc {
class CallbackData;
}
class GLContextData;
namespace GLMotif {
class Widget;
class PopupWindow;
class ToggleButton;
class VideoPane;
}
#if VIDEO_CONFIG_HAVE_THEORA
namespace Video {
class FrameBuffer;
class VideoDevice;
class ImageExtractor;
class YpCbCr420Texture;
}
#endif
#if SOUND_CONFIG_HAVE_SPEEX
namespace Collaboration {
class SpeexEncoder;
}
#endif

namespace Collaboration {

class AgoraClient:public ProtocolClient,private AgoraProtocol
	{
	/* Embedded classes: */
	protected:
	class RemoteClientState:public ProtocolClient::RemoteClientState,public ALObject
		{
		/* Embedded classes: */
		private:
		struct ALDataItem:public ALObject::DataItem
			{
			/* Elements: */
			public:
			#if ALSUPPORT_CONFIG_HAVE_OPENAL && SOUND_CONFIG_HAVE_SPEEX
			SpeexDecoder speexDecoder; // SPEEX decoder object
			ALuint source; // Source to play back the remote client's audio transmission
			ALuint* buffers; // Buffers to stream the remote client's audio transmission into the source
			ALuint* freeBuffers; // Stack of free buffers
			size_t numFreeBuffers; // Number of free buffers on the stack
			#endif
			
			/* Constructors and destructors: */
			ALDataItem(size_t sSpeexFrameSize,Threads::DropoutBuffer<char>& sSpeexPacketQueue);
			virtual ~ALDataItem(void);
			};
		
		/* Elements: */
		public:
		
		/* Audio decoding state: */
		size_t remoteSpeexFrameSize; // Frame size of incoming SPEEX packets
		Point mouthPosition; // Position of remote client's mouth in viewer's device space
		float rolloffFactor; // Roll-off factor for attenuation of remote audio sources; 0.0 disables attenuation
		mutable Threads::DropoutBuffer<char> speexPacketQueue; // Queue for incoming encoded SPEEX packets
		Point localMouthPosition; // Position of remote client's mouth in local client's physical space
		
		/* Video decoding state: */
		bool hasTheora; // Flag if the server will send video data for this client
		ONTransform videoTransform; // The remote client's transformation from its video space into its physical space
		Scalar videoSize[2]; // Width and height of remote video image in remote client's physical space
		#if VIDEO_CONFIG_HAVE_THEORA
		Threads::TripleBuffer<Video::TheoraPacket> theoraPacketBuffer; // Buffer for incoming Theora video stream packets
		Threads::MutexCond newPacketCond; // Condition variable to signal arrival of a new Theora packet from the server
		Video::TheoraDecoder theoraDecoder; // Theora decoder object
		Threads::Thread videoDecodingThread; // Thread receiving compressed video data from the source
		Threads::TripleBuffer<Video::TheoraFrame> theoraFrameBuffer; // Triple buffer for decompressed video frames
		Video::YpCbCr420Texture* frameTexture; // Texture to render the remote client's video stream
		#endif
		OGTransform localVideoTransform; // Transformation from remote client's video space into local client's navigational space
		
		/* Private methods: */
		#if VIDEO_CONFIG_HAVE_THEORA
		void* videoDecodingThreadMethod(void); // Thread to decode a Theora video stream
		#endif
		
		/* Constructors and destructors: */
		RemoteClientState(void);
		virtual ~RemoteClientState(void);
		
		/* Methods from ALObject: */
		virtual void initContext(ALContextData& contextData) const;
		
		/* New methods: */
		void glRenderAction(GLContextData& contextData) const; // Displays the remote client's state
		void alRenderAction(ALContextData& contextData) const; // Displays the remote client's sound state
		};
	
	/* Elements: */
	private:
	
	/* Audio encoding state: */
	Point mouthPosition; // Client's mouth position in main viewer's device space
	#if SOUND_CONFIG_HAVE_SPEEX
	SpeexEncoder* speexEncoder; // SPEEX audio encoder object to send local audio to all remote clients
	#endif
	bool haveAudio; // Flag whether the client actually has an audio capture device
	bool pauseAudio; // Flag to temporarily pause audio transmission
	
	/* Audio playback state: */
	size_t jitterBufferSize; // Size of SPEEX packet queue for audio decoding threads
	float rolloffFactor; // Roll-off factor for attenuation of remote audio sources; 0.0 disables attenuation
	
	/* Video encoding state: */
	bool hasTheora; // Flag if the server thinks this client will send video data
	Video::VideoDevice* videoDevice; // V4L2 Video device to capture uncompressed video
	Video::ImageExtractor* videoExtractor; // Extractor to convert raw video frames to Y'CbCr 4:2:0
	GLMotif::Widget* videoDeviceSettings; // Settings dialog for the video device
	GLMotif::ToggleButton* showVideoDeviceSettingsToggle; // Toggle button to show the video device settings dialog
	GLMotif::ToggleButton* showLocalVideoWindowToggle; // Toggle button to show the local video feed
	GLMotif::PopupWindow* localVideoWindow; // Dialog window to show the local video feed
	GLMotif::VideoPane* videoPane; // Widget displaying the video stream
	#if VIDEO_CONFIG_HAVE_THEORA
	Video::TheoraEncoder theoraEncoder; // Theora video encoder object to compress video
	#endif
	ONTransform videoTransform; // Transformation from video space to Vrui physical space
	Scalar videoSize[2]; // Width and height of video image in video space coordinate units
	#if VIDEO_CONFIG_HAVE_THEORA
	Threads::TripleBuffer<Video::TheoraFrame> theoraFrameBuffer; // Intermediate buffer to feed a single uncompressed video frame to the Theora encoder
	Threads::TripleBuffer<Video::TheoraPacket> theoraPacketBuffer; // Triple buffer holding packets created by the Theora encoder
	#endif
	bool haveVideo; // Flag whether the client actually has a video capture device
	bool localVideoWindowShown; // Flag whether the local video window is currently popped up
	bool pauseVideo; // Flag to temporarily pause video transmission
	
	/* Private methods: */
	void videoCaptureCallback(const Video::FrameBuffer* frame); // Called when a new frame has arrived from the video capture device
	void showVideoDeviceSettingsCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void pauseAudioCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void pauseVideoCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void showLocalVideoWindowCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void videoDeviceSettingsCloseCallback(Misc::CallbackData* cbdata);
	void localVideoWindowCloseCallback(Misc::CallbackData* cbdata);
	
	/* Constructors and destructors: */
	public:
	AgoraClient(void); // Creates an Agora client
	virtual ~AgoraClient(void); // Destroys the Agora client
	
	/* Methods from ProtocolClient: */
	virtual const char* getName(void) const;
	virtual void initialize(CollaborationClient* sClient,Misc::ConfigurationFileSection& configFileSection);
	virtual bool haveSettingsDialog(void) const;
	virtual void buildSettingsDialog(GLMotif::RowColumn* settingsDialog);
	virtual void sendConnectRequest(Comm::NetPipe& pipe);
	virtual void receiveConnectReply(Comm::NetPipe& pipe);
	virtual void receiveConnectReject(Comm::NetPipe& pipe);
	virtual RemoteClientState* receiveClientConnect(Comm::NetPipe& pipe);
	virtual bool receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,Comm::NetPipe& pipe);
	virtual void sendClientUpdate(Comm::NetPipe& pipe);
	virtual void frame(void);
	virtual void frame(ProtocolClient::RemoteClientState* rcs);
	virtual void glRenderAction(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const;
	virtual void alRenderAction(const ProtocolClient::RemoteClientState* rcs,ALContextData& contextData) const;
	};

}

#endif
