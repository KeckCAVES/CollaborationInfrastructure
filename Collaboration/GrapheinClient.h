/***********************************************************************
GrapheinClient - Client object to implement the Graphein shared
annotation protocol.
Copyright (c) 2009-2010 Oliver Kreylos

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

#ifndef GRAPHEINCLIENT_INCLUDED
#define GRAPHEINCLIENT_INCLUDED

#include <vector>
#include <Threads/Mutex.h>
#include <GLMotif/NewButton.h>
#include <GLMotif/Slider.h>
#include <Vrui/UtilityTool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/ToolManager.h>
#include <Collaboration/ProtocolClient.h>

#include <Collaboration/GrapheinPipe.h>

/* Forward declarations: */
class GLContextData;
namespace GLMotif {
class PopupWindow;
class RowColumn;
class TextField;
}

namespace Collaboration {

class GrapheinClient:public ProtocolClient,public GrapheinPipe
	{
	/* Embedded classes: */
	protected:
	class RemoteClientState:public ProtocolClient::RemoteClientState
		{
		/* Elements: */
		public:
		mutable Threads::Mutex curveMutex; // Mutex protecting the curve hash table
		CurveHasher curves; // Set of curves owned by the remote client
		
		/* Constructors and destructors: */
		RemoteClientState(void);
		virtual ~RemoteClientState(void);
		
		/* Methods: */
		void glRenderAction(GLContextData& contextData) const; // Displays the remote client's state
		};
	
	class GrapheinTool; // Forward declaration
	typedef Vrui::GenericToolFactory<GrapheinTool> GrapheinToolFactory; // Graphein tool class uses the generic factory class
	
	class GrapheinTool:public Vrui::UtilityTool // Class for Graphein annotation tools
		{
		friend class Vrui::GenericToolFactory<GrapheinTool>;
		friend class GrapheinClient;
		
		/* Elements: */
		private:
		static GrapheinToolFactory* factory; // Pointer to the factory object for this class
		static const Curve::Color curveColors[8]; // Standard line color palette
		GrapheinClient* client; // Pointer to the Graphein client object owning the tool
		GLMotif::PopupWindow* controlDialogPopup;
		GLMotif::TextField* lineWidthValue;
		GLMotif::RowColumn* colorBox;
		GLfloat newLineWidth; // Line width for new curves
		Curve::Color newColor; // Color for new curves
		bool active; // Flag whether the tool is currently creating a curve
		unsigned int currentCurveId; // ID number of the currently sketched curve
		Point lastPoint; // The last point appended to the curve
		Point currentPoint; // The current dragging point
		
		/* Constructors and destructors: */
		public:
		GrapheinTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment);
		virtual ~GrapheinTool(void);
		
		/* Methods from Vrui::Tool: */
		virtual const Vrui::ToolFactory* getFactory(void) const
			{
			return factory;
			}
		virtual void buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData);
		virtual void frame(void);
		virtual void display(GLContextData& contextData) const;
		
		/* New methods: */
		void lineWidthSliderCallback(GLMotif::Slider::ValueChangedCallbackData* cbData);
		void colorButtonSelectCallback(GLMotif::NewButton::SelectCallbackData* cbData);
		void deleteCurvesCallback(Misc::CallbackData* cbData);
		};
	
	friend class GrapheinTool;
	
	/* Elements: */
	private:
	Threads::Mutex curveMutex; // Mutex protecting the curve hash table
	unsigned int nextCurveId; // ID number for the next created curve
	CurveHasher curves; // Set of curves owned by the client
	CurveActionList actions; // Queue of pending curve actions
	
	/* Constructors and destructors: */
	public:
	GrapheinClient(void); // Creates a Graphein client
	virtual ~GrapheinClient(void); // Destroys the Graphein client
	
	/* Methods from ProtocolClient: */
	virtual const char* getName(void) const;
	virtual unsigned int getNumMessages(void) const;
	virtual void initialize(CollaborationClient& collaborationClient,Misc::ConfigurationFileSection& configFileSection);
	virtual void sendConnectRequest(CollaborationPipe& pipe);
	virtual void receiveConnectReply(CollaborationPipe& pipe);
	virtual void receiveDisconnectReply(CollaborationPipe& pipe);
	virtual void sendClientUpdate(CollaborationPipe& pipe);
	virtual ProtocolClient::RemoteClientState* receiveClientConnect(CollaborationPipe& pipe);
	virtual void receiveServerUpdate(ProtocolClient::RemoteClientState* rcs,CollaborationPipe& pipe);
	virtual void frame(ProtocolClient::RemoteClientState* rcs);
	virtual void glRenderAction(GLContextData& contextData) const;
	virtual void glRenderAction(const ProtocolClient::RemoteClientState* rcs,GLContextData& contextData) const;
	
	/* New methods: */
	void toolCreationCallback(Vrui::ToolManager::ToolCreationCallbackData* cbData);
	};

}

#endif
