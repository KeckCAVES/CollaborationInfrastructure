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

#ifndef AGORACLIENT_INCLUDED
#define AGORACLIENT_INCLUDED

#include <string>
#include <Threads/MutexCond.h>
#include <Threads/TripleBuffer.h>
#include <Threads/DropoutBuffer.h>
#include <GL/gl.h>
#include <GL/GLShader.h>
#include <GL/GLObject.h>

#include <Collaboration/ProtocolClient.h>
#include <Collaboration/V4L2VideoDevice.h>
#include <Collaboration/TheoraWrappers.h>

#include <Collaboration/AgoraPipe.h>

/* Forward declarations: */
class GLContextData;
namespace Collaboration {
class SpeexEncoder;
class SpeexDecoder;
}

namespace Collaboration {

class AgoraClient:public ProtocolClient,public AgoraPipe
	{
	/* Embedded classes: */
	protected:
	class RemoteClientState:public ProtocolClient::RemoteClientState,public GLObject
		{
		/* Embedded classes: */
		private:
		struct Frame // Structure for decompressed video frames
			{
			/* Elements: */
			public:
			unsigned char* buffer; // Pointer to allocated color buffer
			
			/* Constructors and destructors: */
			Frame(void)
				:buffer(0)
				{
				}
			private:
			Frame(const Frame& source); // Prohibit copy constructor
			Frame& operator=(const Frame& source); // Prohibit assignment operator
			public:
			~Frame(void)
				{
				delete[] buffer;
				}
			
			/* Methods: */
			void resize(unsigned int newWidth,unsigned int newHeight)
				{
				delete[] buffer;
				buffer=new unsigned char[newWidth*newHeight*3];
				}
			};
		
		struct DataItem:public GLObject::DataItem
			{
			/* Elements: */
			public:
			GLuint videoTextureObjectId; // Texture object ID of video texture
			GLsizei textureSize[2]; // Allocated size of video texture map
			bool shaderSupported; // Flag whether GLSL shaders are supported by OpenGL
			GLShader yuvToRgbShader; // GLSL shader to convert textures in Y'CbCr color space to RGB on-the-fly
			int textureSamplerLoc; // Location of the texture sampler uniform variable
			unsigned int version; // Version number of video frame currently in texture
			
			/* Constructors and destructors: */
			DataItem(void)
				:videoTextureObjectId(0),
				 shaderSupported(GLShader::isSupported()),
				 version(0)
				{
				glGenTextures(1,&videoTextureObjectId);
				}
			virtual ~DataItem(void)
				{
				glDeleteTextures(1,&videoTextureObjectId);
				}
			};
		
		/* Elements: */
		public:
		
		/* Audio decoding state: */
		Threads::DropoutBuffer<char> speexPacketQueue; // Queue for incoming encoded SPEEX packets
		SpeexDecoder* speexDecoder; // SPEEX decoder object
		
		/* Video decoding state: */
		bool hasTheora; // Flag if the server will send video data for this client
		unsigned int frameSize[2]; // Size of decompressed video frames
		Threads::TripleBuffer<OggPacket> oggPacketBuffer; // Buffer for incoming Theora video stream packets
		TheoraDecoder* theoraDecoder; // Theora decoder object
		YpCbCrBuffer* yuvBuffer; // Intermediate buffer to hold decompressed frame in Y'CbCr 4:2:0 format
		Threads::TripleBuffer<Frame> frameBuffer; // Triple buffer for decompressed video frames
		Threads::MutexCond newPacketCond; // Condition variable to signal arrival of a new Theora packet from the server
		Threads::Thread videoDecodingThread; // Thread receiving compressed video data from the source
		unsigned int frameVersion; // Incrementing version number of decompressed frames
		static const char* yuvToRgbVertexShaderSource; // Source code string for YpCbCr->RGB conversion vertex shader
		static const char* yuvToRgbFragmentShaderSource; // Source code string for YpCbCr->RGB conversion fragment shader
		Threads::TripleBuffer<OGTransform> videoTransform; // The remote client's current transformation from video space into local navigation space
		Scalar videoSize[2]; // Width and height of remote video image in virtual video space
		
		/* Private methods: */
		void* videoDecodingThreadMethod(void); // Thread to decode a Theora video stream
		
		/* Constructors and destructors: */
		RemoteClientState(void);
		virtual ~RemoteClientState(void);
		
		/* Methods from GLObject: */
		virtual void initContext(GLContextData& contextData) const;
		
		/* New methods: */
		void display(GLContextData& contextData) const; // Displays the remote client's state
		};
	
	/* Elements: */
	private:
	
	/* Audio encoding state: */
	SpeexEncoder* speexEncoder; // SPEEX audio encoder object to send local audio to all remote clients
	
	/* Video encoding state: */
	bool hasTheora; // Flag if the server thinks this client will send video data
	V4L2VideoDevice* videoDevice; // V4L2 Video device to capture uncompressed video
	TheoraEncoder* theoraEncoder; // Theora video encoder object to compress video
	YpCbCrBuffer* yuvBuffer; // Intermediate buffer to feed a single uncompressed video frame to the Theora encoder
	Threads::TripleBuffer<OggPacket> oggPacketBuffer; // Triple buffer holding ogg packets created by the Theora encoder
	OGTransform videoTransform; // Transformation from video space to Vrui physical space
	Scalar videoSize[2]; // Width and height of video image in video space coordinate units
	
	/* Audio playback state: */
	int playbackNodeIndex; // Index of cluster node which handles sound playback
	std::string playbackPcmDeviceName; // PCM device name for sound playback from remote clients
	size_t jitterBufferSize; // Size of SPEEX packet queue for audio decoding threads
	
	/* Private methods: */
	void videoCaptureCallback(const V4L2VideoDevice::FrameBuffer& frame); // Called when a new frame has arrived from the video capture device
	
	/* Constructors and destructors: */
	public:
	AgoraClient(void); // Creates an Agora client
	virtual ~AgoraClient(void); // Destroys the Agora client
	
	/* Methods from ProtocolClient: */
	virtual const char* getName(void) const;
	virtual unsigned int getNumMessages(void) const;
	virtual void initialize(CollaborationClient& collaborationClient,Misc::ConfigurationFileSection& configFileSection);
	virtual void sendConnectRequest(CollaborationPipe& pipe);
	virtual void receiveConnectReply(CollaborationPipe& pipe);
	virtual void receiveConnectReject(CollaborationPipe& pipe);
	virtual void sendClientUpdate(CollaborationPipe& pipe);
	virtual RemoteClientState* receiveClientConnect(CollaborationPipe& pipe);
	virtual void receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe);
	virtual void frame(ProtocolClient::RemoteClientState* rcs);
	virtual void display(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const;
	};

}

#endif
