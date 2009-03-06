/***********************************************************************
EmineoPipe - Common interface between an Emineo server and an Emineo
client.
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

#include <Geometry/Vector.h>
#include <Geometry/Rotation.h>

#include <Collaboration/CollaborationPipe.h>

#include <Collaboration/EmineoPipe.h>

namespace Collaboration {

/***********************************
Static elements of class EmineoPipe:
***********************************/

const char* EmineoPipe::protocolName="Emineo"; // How inventive
const unsigned int EmineoPipe::numProtocolMessages=0;

EmineoPipe::OGTransform EmineoPipe::receiveOGTransform(CollaborationPipe& pipe)
	{
	/* Read the transformation's components: */
	OGTransform::Vector translation;
	pipe.read<OGTransform::Scalar>(translation.getComponents(),3);
	OGTransform::Scalar quaternion[4];
	pipe.read<OGTransform::Scalar>(quaternion,4);
	OGTransform::Scalar scaling=pipe.read<OGTransform::Scalar>();
	
	/* Assemble and return the transformation: */
	return OGTransform(translation,OGTransform::Rotation::fromQuaternion(quaternion),scaling);
	}

void EmineoPipe::sendOGTransform(const EmineoPipe::OGTransform& transform,CollaborationPipe& pipe)
	{
	/* Write the transformation's components: */
	pipe.write<OGTransform::Scalar>(transform.getTranslation().getComponents(),3);
	pipe.write<OGTransform::Scalar>(transform.getRotation().getQuaternion(),4);
	pipe.write<OGTransform::Scalar>(transform.getScaling());
	}

}
