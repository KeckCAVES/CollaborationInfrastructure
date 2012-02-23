/***********************************************************************
AgoraProtocol - Class defining the communication protocol between an
Agora server and an Agora client.
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

#include <Collaboration/AgoraProtocol.h>

namespace Collaboration {

/**************************************
Static elements of class AgoraProtocol:
**************************************/

const char* AgoraProtocol::protocolName="Agora"; // How inventive
const unsigned int AgoraProtocol::protocolVersion=(2U<<16)+0U; // Version 2.0

}
