/***********************************************************************
TheoraWrappers - Classes wrapping some common ogg/Theora data structures
for easier programming.
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

#ifndef THEORAWRAPPERS_INCLUDED
#define THEORAWRAPPERS_INCLUDED

#include <string.h>
#include <Misc/ThrowStdErr.h>
#include <ogg/ogg.h>
#include <theora/theora.h>
#include <Comm/ClusterPipe.h>

namespace Collaboration {

struct OggPacket:public ogg_packet // Helper structure to manage ogg packets
	{
	/* Elements: */
	private:
	size_t allocSize; // Privately allocated packet size
	
	/* Constructors and destructors: */
	public:
	OggPacket(void) // Creates an empty ogg packet
		:allocSize(0)
		{
		packet=0;
		bytes=0;
		}
	private:
	OggPacket(const OggPacket& source); // Prohibit copy constructor
	OggPacket& operator=(const OggPacket& source); // Prohibit assignment operator
	public:
	~OggPacket(void) // Destroys the ogg packet
		{
		if(allocSize>0)
			delete[] packet;
		}

	/* Methods: */
	void clone(const OggPacket& source) // Sometimes you just have to clone an ogg packet
		{
		b_o_s=source.b_o_s;
		e_o_s=source.e_o_s;
		granulepos=source.granulepos;
		packetno=source.packetno;
		if(allocSize<size_t(source.bytes))
			{
			/* Re-allocate the packet buffer: */
			if(allocSize>0)
				delete[] packet;
			allocSize=source.bytes;
			packet=new unsigned char[allocSize];
			}
		bytes=source.bytes;
		memcpy(packet,source.packet,bytes);
		}
	void read(Comm::ClusterPipe& pipe) // Reads packet from cluster pipe
		{
		b_o_s=pipe.read<char>();
		e_o_s=0;
		granulepos=pipe.read<ogg_int64_t>();
		packetno=pipe.read<ogg_int64_t>();
		bytes=pipe.read<unsigned int>();
		if(allocSize<size_t(bytes))
			{
			/* Re-allocate the packet buffer: */
			if(allocSize>0)
				delete[] packet;
			allocSize=bytes;
			packet=new unsigned char[allocSize];
			}
		pipe.read<unsigned char>(packet,bytes);
		}
	size_t getNetworkSize(void) const // Returns the size of a packet when written to the network
		{
		return sizeof(char)+sizeof(ogg_int64_t)*2+sizeof(unsigned int)+bytes;
		}
	void write(Comm::ClusterPipe& pipe) const // Writes packet to cluster pipe
		{
		pipe.write<char>(b_o_s);
		pipe.write<ogg_int64_t>(granulepos);
		pipe.write<ogg_int64_t>(packetno);
		pipe.write<unsigned int>(bytes);
		pipe.write<unsigned char>(packet,bytes);
		}
	};

struct TheoraComment:public theora_comment // Helper structure to manage codec creation state
	{
	/* Constructors and destructors: */
	public:
	TheoraComment(void) // Creates an uninitialized comment object
		{
		theora_comment_init(this);
		}
	private:
	TheoraComment(const TheoraComment& source); // Prohibit copy constructor
	TheoraComment& operator=(const TheoraComment& source); // Prohibit assignment operator
	public:
	~TheoraComment(void) // Destroys the comment object
		{
		theora_comment_clear(this);
		}
	};

struct TheoraInfo:public theora_info // Helper structure to manage codec creation state
	{
	/* Constructors and destructors: */
	public:
	TheoraInfo(void) // Creates an uninitialized info object
		{
		theora_info_init(this);
		}
	private:
	TheoraInfo(const TheoraInfo& source); // Prohibit copy constructor
	TheoraInfo& operator=(const TheoraInfo& source); // Prohibit assignment operator
	public:
	~TheoraInfo(void) // Destroys the info object
		{
		theora_info_clear(this);
		}
	
	/* Methods: */
	void readStreamHeaders(Comm::ClusterPipe& pipe) // Initializes info structure by reading header packets from cluster pipe
		{
		/* Encoder sent three packets: headers, comments, tables: */
		OggPacket headerPackets[3];
		for(int i=0;i<3;++i)
			headerPackets[i].read(pipe);
		
		TheoraComment decoderComment;
		for(int i=0;i<3;++i)
			{
			int result;
			if((result=theora_decode_header(this,&decoderComment,&headerPackets[i]))!=0)
				Misc::throwStdErr("TheoraInfo::readStreamHeaders: Error %d while reading header packet",result);
			}
		}
	};

struct TheoraEncoder:public theora_state // Helper structure to manage theora encoders
	{
	/* Constructors and destructors: */
	public:
	TheoraEncoder(TheoraInfo& encoderInfo) // Creates an encoder object
		{
		int result=theora_encode_init(this,&encoderInfo);
		if(result!=0)
			Misc::throwStdErr("TheoraEncoder::TheoraEncoder: Error %d while initializing encoder object");
		}
	private:
	TheoraEncoder(const TheoraEncoder& source); // Prohibit copy constructor
	TheoraEncoder& operator=(const TheoraEncoder& source); // Prohibit assignment operator
	public:
	~TheoraEncoder(void) // Destroys the encoder object
		{
		theora_clear(this);
		}
	};

struct TheoraDecoder:public theora_state // Helper structure to manage theora decoders
	{
	/* Constructors and destructors: */
	public:
	TheoraDecoder(TheoraInfo& decoderInfo) // Creates a decoder object
		{
		int result=theora_decode_init(this,&decoderInfo);
		if(result!=0)
			Misc::throwStdErr("TheoraDecoder::TheoraDecoder: Error %d while initializing decoder object");
		}
	private:
	TheoraDecoder(const TheoraDecoder& source); // Prohibit copy constructor
	TheoraDecoder& operator=(const TheoraDecoder& source); // Prohibit assignment operator
	public:
	~TheoraDecoder(void) // Destroys the decoder object
		{
		theora_clear(this);
		}
	};

struct YpCbCrBuffer:public yuv_buffer // Helper structure to manage decompressed video frames in Y'CbCr 4:2:0 format
	{
	/* Elements: */
	private:
	bool memoryIsMine;
	
	/* Constructors and destructors: */
	public:
	YpCbCrBuffer(void) // Creates uninitialized buffer that will be initialized by the Theora decoder
		:memoryIsMine(false)
		{
		}
	YpCbCrBuffer(unsigned int frameWidth,unsigned int frameHeight) // Creates a 4:2:0 frame buffer for the given frame size
		:memoryIsMine(true)
		{
		/* Set up an interleaved frame buffer: */
		y_width=frameWidth;
		y_height=frameHeight;
		y_stride=(frameWidth*3)/2;
		y=new unsigned char[frameHeight*(frameWidth*3)/2];
		uv_width=frameWidth/2;
		uv_height=frameHeight/2;
		uv_stride=frameWidth*3;
		u=y+frameWidth;
		v=y+(frameWidth*5)/2;
		}
	~YpCbCrBuffer(void) // Destroys the frame buffer
		{
		if(memoryIsMine)
			delete[] y;
		}
	
	/* Methods: */
	void swizzleYpCbCr422(const unsigned char* frame) // Swizzles from 4:2:2 interleaved frame
		{
		unsigned char* yRowPtr=y;
		unsigned char* cbRowPtr=u;
		unsigned char* crRowPtr=v;
		const unsigned char* framePtr=frame;
		for(int y=0;y<y_height;y+=2)
			{
			/* Process an even row by keeping its Cb values: */
			unsigned char* yPtr=yRowPtr;
			unsigned char* cbPtr=cbRowPtr;
			for(int x=0;x<y_width;x+=2)
				{
				*(yPtr++)=*(framePtr++);
				*(cbPtr++)=*(framePtr++);
				*(yPtr++)=*(framePtr++);
				++framePtr;
				}
			yRowPtr+=y_stride;
			cbRowPtr+=uv_stride;
			
			/* Process an odd row by keeping its Cr values: */
			yPtr=yRowPtr;
			unsigned char* crPtr=crRowPtr;
			for(int x=0;x<y_width;x+=2)
				{
				*(yPtr++)=*(framePtr++);
				++framePtr;
				*(yPtr++)=*(framePtr++);
				*(crPtr++)=*(framePtr++);
				}
			yRowPtr+=y_stride;
			crRowPtr+=uv_stride;
			}
		}
	void unswizzleYpCbCr111(unsigned char* frame) // Unswizzles to a 1:1:1 interleaved frame
		{
		unsigned char* yRowPtr=y;
		unsigned char* cbRowPtr=u;
		unsigned char* crRowPtr=v;
		unsigned char* framePtr=frame;
		for(int y=0;y<y_height;y+=2)
			{
			/* Process two rows identically: */
			for(int i=0;i<2;++i)
				{
				unsigned char* yPtr=yRowPtr;
				unsigned char* cbPtr=cbRowPtr;
				unsigned char* crPtr=crRowPtr;
				
				/* Unswizzle a row: */
				for(int x=0;x<y_width;x+=2)
					{
					*(framePtr++)=*(yPtr++);
					*(framePtr++)=*cbPtr;
					*(framePtr++)=*crPtr;
					*(framePtr++)=*(yPtr++);
					*(framePtr++)=*(cbPtr++);
					*(framePtr++)=*(crPtr++);
					}
				
				yRowPtr+=y_stride;
				}
			
			cbRowPtr+=uv_stride;
			crRowPtr+=uv_stride;
			}
		}
	};

}

#endif
