/***********************************************************************
Protocol - Class defining the basic communication protocol inside the
Vrui collaboration infrastructure.
Copyright (c) 2007-2011 Oliver Kreylos

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

#ifndef COLLABORATION_PROTOCOL_INCLUDED
#define COLLABORATION_PROTOCOL_INCLUDED

#include <Misc/SizedTypes.h>
#include <Misc/StandardMarshallers.h>
#include <IO/File.h>
#include <Geometry/Point.h>
#include <Geometry/Vector.h>
#include <Geometry/Rotation.h>
#include <Geometry/OrthonormalTransformation.h>
#include <Geometry/OrthogonalTransformation.h>
#include <Geometry/GeometryMarshallers.h>

namespace Collaboration {

class Protocol
	{
	/* Embedded classes: */
	public:
	typedef Misc::UInt16 MessageIdType; // Network type for protocol messages
	typedef Misc::UInt8 Byte; // Type for raw bytes
	typedef Misc::UInt32 Card; // Type for transmitted cardinal numbers
	typedef Misc::Float32 Scalar; // Scalar type for transmitted geometric data
	typedef Geometry::Point<Scalar,3> Point; // Type for points
	typedef Geometry::Vector<Scalar,3> Vector; // Type for vectors
	typedef Geometry::Rotation<Scalar,3> Rotation; // Type for rotations
	typedef Geometry::OrthonormalTransformation<Scalar,3> ONTransform; // Type for rigid body transformations
	typedef Geometry::OrthogonalTransformation<Scalar,3> OGTransform; // Type for rigid body transformations with uniform scaling
	
	/* Methods: */
	static MessageIdType readMessage(IO::File& source) // Reads a protocol message from the given source
		{
		return source.read<MessageIdType>();
		}
	static void writeMessage(MessageIdType messageId,IO::File& sink) // Writes a protocol message to the given sink
		{
		sink.write<MessageIdType>(messageId);
		}
	template <class ValueParam>
	static ValueParam read(IO::File& source) // Reads a value from the given source
		{
		return Misc::Marshaller<ValueParam>::read(source);
		}
	template <class ValueParam>
	static void read(ValueParam& value,IO::File& source) // Ditto, by reference
		{
		value=Misc::Marshaller<ValueParam>::read(source);
		}
	template <class ValueParam>
	static void write(const ValueParam& value,IO::File& sink) // Writes a value to the given sink
		{
		Misc::Marshaller<ValueParam>::write(value,sink);
		}
	};

}

#endif
