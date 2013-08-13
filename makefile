########################################################################
# Makefile for Vrui collaboration infrastructure.
# Copyright (c) 2009-2013 Oliver Kreylos
#
# This file is part of the WhyTools Build Environment.
# 
# The WhyTools Build Environment is free software; you can redistribute
# it and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
# 
# The WhyTools Build Environment is distributed in the hope that it will
# be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
# of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with the WhyTools Build Environment; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
# 02111-1307 USA
########################################################################

# Directory containing the Vrui build system. The directory below
# matches the default Vrui installation; if Vrui's installation
# directory was changed during Vrui's installation, the directory below
# must be adapted.
VRUI_MAKEDIR := $(HOME)/Vrui-3.0/share/make
ifdef DEBUG
  VRUI_MAKEDIR := $(VRUI_MAKEDIR)/debug
endif

# Root directory for protocol plugins underneath Vrui's library
# directory:
COLLABORATIONPLUGINSDIREXT = CollaborationPlugins

# Uncomment the following line to receive status messages from the
# protocol engine:
# CFLAGS += -DVERBOSE

########################################################################
# Everything below here should not have to be changed
########################################################################

# Define the root of the toolkit source tree
PACKAGEROOT := $(shell pwd)

# Specify version of created dynamic shared libraries
COLLABORATION_VERSION = 2006
MAJORLIBVERSION = 2
MINORLIBVERSION = 6
COLLABORATION_NAME := Collaboration-$(MAJORLIBVERSION).$(MINORLIBVERSION)

# Include definitions for the system environment and system-provided
# packages
include $(VRUI_MAKEDIR)/SystemDefinitions
include $(VRUI_MAKEDIR)/Packages.System
include $(VRUI_MAKEDIR)/Configuration.Vrui
include $(VRUI_MAKEDIR)/Packages.Vrui
include $(PACKAGEROOT)/BuildRoot/Packages.Collaboration

# Override the include file and library search directories:
EXTRACINCLUDEFLAGS += -I$(PACKAGEROOT)
EXTRALINKDIRFLAGS += -L$(PACKAGEROOT)/$(MYLIBEXT)

# Set destination directory for libraries and plugins:
LIBDESTDIR := $(PACKAGEROOT)/$(MYLIBEXT)
VISLETDESTDIR := $(LIBDESTDIR)/Vislets
PLUGINDESTDIR := $(LIBDESTDIR)/Plugins

########################################################################
# Specify additional compiler and linker flags
########################################################################

# Set flags to distinguish between static and shared libraries
ifdef STATIC_LINK
  LIBRARYNAME = $(LIBDESTDIR)/$(1).$(LDEXT).a
  OBJDIREXT = Static
else
  CFLAGS += $(CDSOFLAGS)
  LIBRARYNAME=$(LIBDESTDIR)/$(call FULLDSONAME,$(1))
endif
VISLETNAME = $(VISLETDESTDIR)/lib$(1).$(PLUGINFILEEXT)
PLUGINNAME = $(PLUGINDESTDIR)/lib$(1).$(PLUGINFILEEXT)

########################################################################
# List packages used by this project
# (Supported packages can be found in
# $(VRUI_MAKEDIR)/BuildRoot/Packages)
########################################################################

PACKAGES = 

########################################################################
# Specify all final targets
########################################################################

LIBRARIES = 
PLUGINS = 
VISLETS = 
EXECUTABLES = 

#
# The server- and client-side libraries:
#

LIBRARY_NAMES = libCollaborationServer \
                libCollaborationClient

LIBRARIES += $(LIBRARY_NAMES:%=$(call LIBRARYNAME,%))

#
# The protocol plugins:
#

SERVERPLUGIN_NAMES = FooServer \
                     CheriaServer \
                     GrapheinServer \
                     AgoraServer

SERVERPLUGINS = $(SERVERPLUGIN_NAMES:%=$(call PLUGINNAME,%))

PLUGINS += $(SERVERPLUGINS)

CLIENTPLUGIN_NAMES = FooClient \
                     CheriaClient \
                     GrapheinClient \
                     AgoraClient

CLIENTPLUGINS = $(CLIENTPLUGIN_NAMES:%=$(call PLUGINNAME,%))

PLUGINS += $(CLIENTPLUGINS)

#
# The vislet plugins:
#

VISLET_NAMES = CollaborationClient

VISLETS += $(VISLET_NAMES:%=$(call VISLETNAME,%))

#
# The collaboration server test program:
#

EXECUTABLES += $(EXEDIR)/CollaborationServer

#
# The collaboration client test program:
#

EXECUTABLES += $(EXEDIR)/CollaborationClientTest

# Set the name of the make configuration file:
MAKECONFIGFILE = share/Configuration.Collaboration

ALL = $(LIBRARIES) $(EXECUTABLES) $(PLUGINS) $(VISLETS) $(MAKECONFIGFILE)

.PHONY: all
all: config $(ALL)

# Make all server components depend on collaboration server library:
$(SERVERPLUGINS) $(EXEDIR)/CollaborationServer: $(call LIBRARYNAME,libCollaborationServer)

# Make all client components depend on collaboration client library:
$(CLIENTPLUGINS) $(VISLETS) $(EXEDIR)/CollaborationClientTest: $(call LIBRARYNAME,libCollaborationClient)

########################################################################
# Pseudo-target to print configuration options
########################################################################

.PHONY: config
config: Configure-End

.PHONY: Configure-Begin
Configure-Begin:
	@echo "---- Configured Vrui collaboration infrastructure options: ----"
	@echo "Installation directory: $(VRUI_PACKAGEROOT)"
	@echo "Protocol plug-in directory: $(PLUGININSTALLDIR)/$(COLLABORATIONPLUGINSDIREXT)"
	@echo "Vislet plug-in directory: $(PLUGININSTALLDIR)/$(VRVISLETSDIREXT)"
ifneq ($(SYSTEM_HAVE_SPEEX),0)
	@echo "Audio transmission in Agora plug-in enabled"
else
	@echo "Audio transmission in Agora plug-in disabled"
endif
ifneq ($(SYSTEM_HAVE_THEORA),0)
	@echo "Video transmission in Agora plug-in enabled"
else
	@echo "Video transmission in Agora plug-in disabled"
endif
ifeq ($(SYSTEM),LINUX)
	@echo "Video capture in Agora plug-in enabled"
else
	@echo "Video capture in Agora plug-in disabled"
endif

.PHONY: Configure-End
Configure-End: Configure-Begin
Configure-End:
	@echo "--------"

$(wildcard *.cpp Collaboration/*.cpp): config

########################################################################
# Specify other actions to be performed on a `make clean'
########################################################################

.PHONY: extraclean
extraclean:
	-rm -f $(LIBRARY_NAMES:%=$(LIBDESTDIR)/$(call DSONAME,%))
	-rm -f $(LIBRARY_NAMES:%=$(LIBDESTDIR)/$(call LINKDSONAME,%))
	-rm -f $(VISLET_NAMES:%=$(call VISLETNAME,%))
	-rm -f $(PLUGIN_NAMES:%=$(call PLUGINNAME,%))

.PHONY: extrasqueakyclean
extrasqueakyclean:
	-rm -f $(ALL)
	-rm -rf $(PACKAGEROOT)/$(LIBEXT)

# Include basic makefile
include $(VRUI_MAKEDIR)/BasicMakefile

########################################################################
# Specify build rules for dynamic shared objects
########################################################################

#
# The Vrui collaboration infrastructure:
#

LIBCOLLABORATION_HEADERS = Collaboration/Protocol.h \
                           Collaboration/ProtocolServer.h \
                           Collaboration/ProtocolClient.h \
                           Collaboration/CollaborationProtocol.h \
                           Collaboration/CollaborationServer.h \
                           Collaboration/CollaborationClient.h

#
# The Vrui collaboration infrastructure (server side):
#

LIBCOLLABORATIONSERVER_SOURCES = Collaboration/CollaborationProtocol.cpp \
                                 Collaboration/ProtocolServer.cpp \
                                 Collaboration/CollaborationServer.cpp

$(OBJDIR)/Collaboration/CollaborationServer.o: CFLAGS += -DCOLLABORATION_PLUGINDSONAMETEMPLATE='"$(PLUGININSTALLDIR)/$(COLLABORATIONPLUGINSDIREXT)/lib%s.$(PLUGINFILEEXT)"'
$(OBJDIR)/Collaboration/CollaborationServer.o: CFLAGS += -DCOLLABORATION_CONFIGFILENAME='"$(ETCINSTALLDIR)/Collaboration.cfg"'

$(call LIBRARYNAME,libCollaborationServer): PACKAGES += $(MYCOLLABORATIONSERVER_DEPENDS)
$(call LIBRARYNAME,libCollaborationServer): EXTRACINCLUDEFLAGS += $(MYCOLLABORATIONSERVER_INCLUDE)
$(call LIBRARYNAME,libCollaborationServer): CFLAGS += $(MYCOLLABORATIONSERVER_CFLAGS)
$(call LIBRARYNAME,libCollaborationServer): $(LIBCOLLABORATIONSERVER_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: libCollaborationServer
libCollaborationServer: $(call LIBRARYNAME,libCollaborationServer)

#
# The Vrui collaboration infrastructure (client side):
#

LIBCOLLABORATIONCLIENT_SOURCES = Collaboration/CollaborationProtocol.cpp \
                                 Collaboration/ProtocolClient.cpp \
                                 Collaboration/CollaborationClient.cpp

$(OBJDIR)/Collaboration/CollaborationClient.o: CFLAGS += -DCOLLABORATION_PLUGINDSONAMETEMPLATE='"$(PLUGININSTALLDIR)/$(COLLABORATIONPLUGINSDIREXT)/lib%s.$(PLUGINFILEEXT)"'
$(OBJDIR)/Collaboration/CollaborationClient.o: CFLAGS += -DCOLLABORATION_CONFIGFILENAME='"$(ETCINSTALLDIR)/Collaboration.cfg"'

$(call LIBRARYNAME,libCollaborationClient): PACKAGES += $(MYCOLLABORATIONCLIENT_DEPENDS)
$(call LIBRARYNAME,libCollaborationClient): EXTRACINCLUDEFLAGS += $(MYCOLLABORATIONCLIENT_INCLUDE)
$(call LIBRARYNAME,libCollaborationClient): CFLAGS += $(MYCOLLABORATIONCLIENT_CFLAGS)
$(call LIBRARYNAME,libCollaborationClient): $(LIBCOLLABORATIONCLIENT_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: libCollaborationClient
libCollaborationClient: $(call LIBRARYNAME,libCollaborationClient)

#
# The collaboration server test program:
#

$(EXEDIR)/CollaborationServer: PACKAGES += MYCOLLABORATIONSERVER MYMISC
$(EXEDIR)/CollaborationServer: $(OBJDIR)/CollaborationServerMain.o
.PHONY: CollaborationServer
CollaborationServer: $(EXEDIR)/CollaborationServer

#
# The collaboration client test program:
#

$(EXEDIR)/CollaborationClientTest: PACKAGES += MYCOLLABORATIONCLIENT MYVRUI MYGLMOTIF MYMISC
$(EXEDIR)/CollaborationClientTest: $(OBJDIR)/CollaborationClientTest.o
.PHONY: CollaborationClientTest
CollaborationClientTest: $(EXEDIR)/CollaborationClientTest

#
# The collaboration protocol plugins:
#

# Implicit rule for creating protocol plug-ins:
$(call PLUGINNAME,%): CFLAGS += $(CPLUGINFLAGS)
$(call PLUGINNAME,%): $(OBJDIR)/Collaboration/%.o
	@mkdir -p $(PLUGINDESTDIR)
ifdef SHOWCOMMAND
	$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $(filter %.o,$^) $(LINKDIRFLAGS) $(LINKLIBFLAGS)
else
	@echo Linking $@...
	@$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $(filter %.o,$^) $(LINKDIRFLAGS) $(LINKLIBFLAGS)
endif

# Dependencies for protocol plug-ins:
$(SERVERPLUGIN_NAMES:%=$(call PLUGINNAME,%)): PACKAGES += MYCOLLABORATIONSERVER
$(CLIENTPLUGIN_NAMES:%=$(call PLUGINNAME,%)): PACKAGES += MYCOLLABORATIONCLIENT

$(call PLUGINNAME,CheriaServer): $(OBJDIR)/Collaboration/CheriaProtocol.o \
                                 $(OBJDIR)/Collaboration/CheriaServer.o

$(call PLUGINNAME,CheriaClient): $(OBJDIR)/Collaboration/CheriaProtocol.o \
                                 $(OBJDIR)/Collaboration/CheriaClient.o

$(call PLUGINNAME,GrapheinServer): PACKAGES += MYGLWRAPPERS
$(call PLUGINNAME,GrapheinServer): $(OBJDIR)/Collaboration/GrapheinProtocol.o \
                                   $(OBJDIR)/Collaboration/GrapheinServer.o

$(call PLUGINNAME,GrapheinClient): $(OBJDIR)/Collaboration/GrapheinProtocol.o \
                                   $(OBJDIR)/Collaboration/GrapheinClient.o

$(call PLUGINNAME,AgoraServer): $(OBJDIR)/Collaboration/AgoraProtocol.o \
                                $(OBJDIR)/Collaboration/AgoraServer.o

$(call PLUGINNAME,AgoraClient): PACKAGES += MYALSUPPORT MYVIDEO MYSOUND
ifneq ($(SYSTEM_HAVE_SPEEX),0)
  $(call PLUGINNAME,AgoraClient): PACKAGES += SPEEX
  $(call PLUGINNAME,AgoraClient): $(OBJDIR)/Collaboration/SpeexEncoder.o \
                                  $(OBJDIR)/Collaboration/SpeexDecoder.o \
                                  $(OBJDIR)/Collaboration/AgoraProtocol.o \
                                  $(OBJDIR)/Collaboration/AgoraClient.o
else
  $(call PLUGINNAME,AgoraClient): $(OBJDIR)/Collaboration/AgoraProtocol.o \
                                  $(OBJDIR)/Collaboration/AgoraClient.o
endif

# Mark all protocol plugin object files as intermediate:
.SECONDARY: $(SERVERPLUGIN_NAMES:%=$(OBJDIR)/Collaboration/%.o)
.SECONDARY: $(CLIENTPLUGIN_NAMES:%=$(OBJDIR)/Collaboration/%.o)

#
# The collaboration client vislet:
#

# Implicit rule for creating vislet plug-ins:
$(call VISLETNAME,%): PACKAGES += MYCOLLABORATIONCLIENT
$(call VISLETNAME,%): CFLAGS += $(CPLUGINFLAGS)
$(call VISLETNAME,%): $(OBJDIR)/%Vislet.o
	@mkdir -p $(VISLETDESTDIR)
ifdef SHOWCOMMAND
	$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $(filter %.o,$^) $(LINKDIRFLAGS) $(LINKLIBFLAGS)
else
	@echo Linking $@...
	@$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $(filter %.o,$^) $(LINKDIRFLAGS) $(LINKLIBFLAGS)
endif

$(call VISLETNAME,CollaborationClient): $(OBJDIR)/CollaborationClientVislet.o

# Mark all vislet plugin object files as intermediate:
.SECONDARY: $(VISLET_NAMES:%=$(OBJDIR)/%Vislet.o)

########################################################################
# Specify installation rules for header files, libraries, executables,
# configuration files, and shared files.
########################################################################

# Pseudo-target to dump Collaboration Infrastructure configuration settings
$(MAKECONFIGFILE): config
	@echo Creating configuration makefile fragment...
	@echo '# Makefile fragment for Collaboration configuration options' > $(MAKECONFIGFILE)
	@echo '# Autogenerated by Collaboration installation on $(shell date)' >> $(MAKECONFIGFILE)
	@echo >> $(MAKECONFIGFILE)
	@echo '# Version information:'>> $(MAKECONFIGFILE)
	@echo 'COLLABORATION_VERSION = $(COLLABORATION_VERSION)' >> $(MAKECONFIGFILE)
	@echo 'COLLABORATION_NAME = $(COLLABORATION_NAME)' >> $(MAKECONFIGFILE)
	@echo >> $(MAKECONFIGFILE)
	@echo '# Installation directories:'>> $(MAKECONFIGFILE)
	@echo 'COLLABORATIONPLUGINSDIREXT = $(COLLABORATIONPLUGINSDIREXT)' >> $(MAKECONFIGFILE)

ifdef INSTALLPREFIX
  HEADERINSTALLDIR := $(INSTALLPREFIX)/$(HEADERINSTALLDIR)
  LIBINSTALLDIR := $(INSTALLPREFIX)/$(LIBINSTALLDIR)
  PLUGININSTALLDIR := $(INSTALLPREFIX)/$(PLUGININSTALLDIR)
  EXECUTABLEINSTALLDIR := $(INSTALLPREFIX)/$(EXECUTABLEINSTALLDIR)
  ETCINSTALLDIR := $(INSTALLPREFIX)/$(ETCINSTALLDIR)
  SHAREINSTALLDIR := $(INSTALLPREFIX)/$(SHAREINSTALLDIR)
  MAKEINSTALLDIR := $(INSTALLPREFIX)/$(MAKEINSTALLDIR)
endif

install: all
# Install all header files in HEADERINSTALLDIR:
	@echo Installing header files...
	@install -d $(HEADERINSTALLDIR)/Collaboration
	@install -m u=rw,go=r $(LIBCOLLABORATION_HEADERS) $(HEADERINSTALLDIR)/Collaboration
# Install all library files in LIBINSTALLDIR:
	@echo Installing libraries...
	@install -d $(LIBINSTALLDIR)
	@install $(LIBRARIES) $(LIBINSTALLDIR)
	@echo Configuring run-time linker...
	$(foreach LIBNAME,$(LIBRARY_NAMES),$(call CREATE_SYMLINK,$(LIBNAME)))
# Install all protocol plugins in PLUGININSTALLDIR/COLLABORATIONPLUGINSDIREXT:
	@echo Installing protocol plugins...
	@install -d $(PLUGININSTALLDIR)/$(COLLABORATIONPLUGINSDIREXT)
	@install $(PLUGINS) $(PLUGININSTALLDIR)/$(COLLABORATIONPLUGINSDIREXT)
# Install all vislet plugins in PLUGININSTALLDIR/VRVISLETSDIREXT:
	@echo Installing vislet plugins...
	@install -d $(PLUGININSTALLDIR)/$(VRVISLETSDIREXT)
	@install $(VISLETS) $(PLUGININSTALLDIR)/$(VRVISLETSDIREXT)
# Install all binaries in EXECUTABLEINSTALLDIR:
	@echo Installing executables...
	@install -d $(EXECUTABLEINSTALLDIR)
	@install $(EXECUTABLES) $(EXECUTABLEINSTALLDIR)
# Install all configuration files in ETCINSTALLDIR:
	@echo Installing configuration files...
	@install -d $(ETCINSTALLDIR)
	@install -m u=rw,go=r share/Collaboration.cfg $(ETCINSTALLDIR)
# Install the package and configuration files in MAKEINSTALLDIR:
	@echo Installing makefile fragments...
	@install -d $(MAKEINSTALLDIR)
	@install -m u=rw,go=r BuildRoot/Packages.Collaboration $(MAKEINSTALLDIR)
	@install -m u=rw,go=r $(MAKECONFIGFILE) $(MAKEINSTALLDIR)

uninstall:
	@echo Removing header files...
	@rm -rf $(HEADERINSTALLDIR)/Collaboration
	@echo Removing libraries...
	@rm -f $(LIBRARIES:$(LIBDESTDIR)/%=$(LIBINSTALLDIR)/%)
	$(foreach LIBNAME,$(LIBRARY_NAMES),$(call DESTROY_SYMLINK,$(LIBNAME)))
	@echo Removing protocol plugins...
	@rm -rf $(PLUGININSTALLDIR)/$(COLLABORATIONPLUGINSDIREXT)
	@echo Removing vislet plugins...
	@rm -f $(VISLETS:$(VISLETDESTDIR)/%=$(PLUGININSTALLDIR)/$(VRVISLETSDIREXT)/%)
	@echo Removing executables...
	@rm -f $(EXECUTABLES:$(EXEDIR)/%=$(EXECUTABLEINSTALLDIR)/%)
	@echo Removing configuration files...
	@rm -f $(ETCINSTALLDIR)/Collaboration.cfg
	@echo Removing makefile fragments...
	@rm -f $(MAKEINSTALLDIR)/Packages.Collaboration
	@rm -f $(MAKEINSTALLDIR)/Configuration.Collaboration
