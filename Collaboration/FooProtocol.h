/***********************************************************************
FooProtocol - Class defining the communication protocol between a Foo
server and a Foo client.
Copyright (c) 2009-2011 Oliver Kreylos

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

#ifndef COLLABORATION_FOOPROTOCOL_INCLUDED
#define COLLABORATION_FOOPROTOCOL_INCLUDED

#define DUMP_PROTOCOL 1

#if DUMP_PROTOCOL
#include <iostream>
#endif
#include <Misc/ThrowStdErr.h>
#include <Math/Random.h>
#include <Comm/NetPipe.h>
#include <Collaboration/Protocol.h>

namespace Collaboration {

class FooProtocol:public Protocol
	{
	/* Embedded classes: */
	public:
	enum MessageId // Enumerated type for Foo protocol messages
		{
		CRAP=0,
		MESSAGES_END
		};
	
	/* Methods: */
	static void sendRandomCrap(Comm::NetPipe& pipe)
		{
		unsigned int messageSize=Math::randUniformCO(0,64)+32;
		pipe.write<Card>(messageSize);
		unsigned int sumTotal=0;
		for(unsigned int i=0;i<messageSize;++i)
			{
			Byte value=Byte(Math::randUniformCO(0,256));
			pipe.write<Byte>(value);
			sumTotal+=value;
			}
		pipe.write<Card>(sumTotal);
		#if DUMP_PROTOCOL
		std::cout<<"Sent "<<messageSize<<" bytes with checksum "<<sumTotal<<std::endl;
		#endif
		}
	static void receiveRandomCrap(Comm::NetPipe& pipe)
		{
		unsigned int messageSize=pipe.read<Card>();
		unsigned int sumTotal=0;
		for(unsigned int i=0;i<messageSize;++i)
			{
			Byte value=pipe.read<Byte>();
			sumTotal+=value;
			}
		unsigned int check=pipe.read<Card>();
		if(check!=sumTotal)
			Misc::throwStdErr("FooClient::Fatal frotocol failure");
		#if DUMP_PROTOCOL
		std::cout<<"Received "<<messageSize<<" bytes with checksum "<<sumTotal<<std::endl;
		#endif
		}
	};

}

#endif
