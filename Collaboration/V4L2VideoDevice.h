/***********************************************************************
V4L2VideoDevice - Wrapper class around video devices as represented by
the Video for Linux version 2 (V4L2) library.
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

#ifndef V4L2VIDEODEVICE_INCLUDED
#define V4L2VIDEODEVICE_INCLUDED

#include <Threads/Thread.h>

/* Forward declarations: */
namespace Misc {
template <class ParameterParam>
class FunctionCall;
class CallbackData;
class ConfigurationFileSection;
}
namespace GLMotif {
class WidgetManager;
class Widget;
}

namespace Collaboration {

class V4L2VideoDevice
	{
	/* Embedded classes: */
	public:
	struct VideoFormat // Structure to report current or available video formats
		{
		/* Elements: */
		public:
		unsigned int pixelFormat; // Pixel format identifier (a fourCC value)
		unsigned int size[2]; // Width and height of video frames in pixels
		size_t lineSize; // Size of a single line of video in bytes (undefined for compressed formats)
		size_t frameSize; // Size of an entire video frame in bytes (maximum value for compressed formats)
		unsigned int frameIntervalCounter,frameIntervalDenominator; // (Expected) video frame interval (inverse rate) as rational number
		
		/* Methods: */
		void setPixelFormat(const char* fourCC) // Sets the format's pixel format to the given fourCC code
			{
			pixelFormat=0U;
			for(int i=0;i<4;++i)
				pixelFormat=(pixelFormat<<8)|(unsigned int)fourCC[3-i];
			}
		char* getFourCC(char fourCCBuffer[5]) // Writes format's pixel format into the given buffer as a NUL-terminated fourCC code
			{
			unsigned int pf=pixelFormat;
			for(int i=0;i<4;++i,pf>>=8)
				fourCCBuffer[i]=char(pf&0xff);
			fourCCBuffer[4]='\0';
			return fourCCBuffer;
			}
		};
	
	struct FrameBuffer // Structure to retain state of allocated frame buffers
		{
		/* Elements: */
		public:
		unsigned int index; // Index to identify memory-mapped buffers
		unsigned char* start; // Pointer to start of buffer in application address space
		size_t size; // Size of buffer in bytes
		unsigned int sequence; // Sequence number of frame
		size_t used; // Actual amount of data in the frame
		
		/* Constructors and destructors: */
		FrameBuffer(void) // Creates an empty, unallocated frame buffer
			:index(~0U),start(0),size(0)
			{
			}
		};
	
	typedef Misc::FunctionCall<const FrameBuffer&> StreamingCallback; // Function call type for streaming capture callback
	
	/* Elements: */
	private:
	int videoFd; // File handle of the V4L2 video device
	bool canRead; // Flag if video device supports read() I/O
	bool canStream; // Flag if video device supports streaming (buffer passing) I/O
	bool frameBuffersMemoryMapped; // Flag if the frame buffer set is memory-mapped from device space
	unsigned int numFrameBuffers; // Number of currently allocated frame buffers
	FrameBuffer* frameBuffers; // Array of currently allocated frame buffers
	Threads::Thread streamingThread; // Background streaming capture thread
	StreamingCallback* streamingCallback; // Function called when a frame buffer becomes ready in streaming capture mode
	
	/* Private methods: */
	void setControl(unsigned int controlId,const char* controlTag,const Misc::ConfigurationFileSection& cfg); // Sets a video device control according to the given tag in the given configuration file section
	void integerControlChangedCallback(Misc::CallbackData* cbData);
	void booleanControlChangedCallback(Misc::CallbackData* cbData);
	void menuControlChangedCallback(Misc::CallbackData* cbData);
	void* streamingThreadMethod(void); // The background streaming capture thread method
	
	/* Constructors and destructors: */
	public:
	V4L2VideoDevice(const char* videoDeviceName); // Opens the given V4L2 video device (/dev/videoXX) as a video source
	private:
	V4L2VideoDevice(const V4L2VideoDevice& source); // Prohibit copy constructor
	V4L2VideoDevice& operator=(const V4L2VideoDevice& source); // Prohibit assignment operator
	public:
	~V4L2VideoDevice(void); // Closes the video device
	
	/* Methods: */
	VideoFormat getVideoFormat(void) const; // Returns the video device's current video format
	VideoFormat& setVideoFormat(VideoFormat& newFormat); // Sets the video device's video format to the most closely matching format; changes the given format structure to the actually set format
	void configure(const Misc::ConfigurationFileSection& cfg); // Configures the video device from the given configuration file section
	GLMotif::Widget* createControlPanel(GLMotif::WidgetManager* widgetManager); // Creates a GLMotif control panel to adjust all exposed video device controls
	
	/* Streaming capture interface methods: */
	unsigned int allocateFrameBuffers(unsigned int requestedNumFrameBuffers); // Allocates the given number of streaming frame buffers; returns actual number of buffers allocated by device
	void startStreaming(void); // Starts streaming video capture using a previously allocated set of frame buffers
	void startStreaming(StreamingCallback* newStreamingCallback); // Starts streaming video capture using a previously allocated set of frame buffers; calls callback from separate thread whenever a new frame buffer becomes reader
	const FrameBuffer& getNextFrame(void); // Returns the next frame buffer captured from the video device; blocks if no frames are ready
	void enqueueFrame(const FrameBuffer& frame); // Returns the given frame buffer to the capturing queue after the caller is done with it
	void stopStreaming(void); // Stops streaming video capture
	void releaseFrameBuffers(void); // Releases all previously allocated frame buffers
	};

}

#endif
