/***********************************************************************
FooCrapSender - Helper functions to send random messages over a
collaboration pipe to stress-test the plug-in handling mechanism.
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

#ifndef FOOCRAPSENDER_INCLUDED
#define FOOCRAPSENDER_INCLUDED

#include <Misc/ThrowStdErr.h>
#include <Math/Random.h>

namespace Collaboration {

namespace {

/****************
Helper functions:
****************/

void sendRandomCrap(CollaborationPipe& pipe)
	{
	unsigned int messageSize=Math::randUniformCO(0,64)+32;
	pipe.write<unsigned int>(messageSize);
	unsigned int sumTotal=0;
	for(unsigned int i=0;i<messageSize;++i)
		{
		unsigned char value=(unsigned char)(Math::randUniformCO(0,256));
		pipe.write<unsigned char>(value);
		sumTotal+=value;
		}
	pipe.write<unsigned int>(sumTotal);
	}

void receiveRandomCrap(CollaborationPipe& pipe)
	{
	unsigned int messageSize=pipe.read<unsigned int>();
	unsigned int sumTotal=0;
	for(unsigned int i=0;i<messageSize;++i)
		{
		unsigned char value=pipe.read<unsigned char>();
		sumTotal+=value;
		}
	unsigned int check=pipe.read<unsigned int>();
	if(check!=sumTotal)
		Misc::throwStdErr("FooClient::Fatal frotocol failure");
	}

}

}

#endif
