########################################################################
# Makefile for Vrui collaboration infrastructure.
# Copyright (c) 2009-2011 Oliver Kreylos
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

# Define the root of the toolkit source tree
PACKAGEROOT := $(shell pwd)

# Vrui package root directory.
# NOTE: This is not the Vrui installation directory, like in most other
# makefiles for Vrui-based applications, but the directory in which Vrui
# itself was built. This is because the collaboration infrastructure is
# an extension of Vrui itself.
VRUIPACKAGEROOT := $(HOME)/src/Vrui-2.1-001

# Include definitions for the system environment
include $(VRUIPACKAGEROOT)/BuildRoot/SystemDefinitions
include $(VRUIPACKAGEROOT)/BuildRoot/Packages

# Root directory for the software installation
# NOTE: This must be the same directory into which Vrui itself was
# installed.
INSTALLDIR = $(HOME)/Vrui-2.1

# Root directory for vislet plugins underneath Vrui's library directory.
# This needs to be changed if Vrui's vislet directory was changed in
# Vrui's makefile.
VISLETPLUGINEXT = VRVislets

# Root directory for protocol plugins underneath Vrui's library
# directory:
PROTOCOLPLUGINEXT = CollaborationPlugins

# Uncomment the following line to receive status messages from the
# protocol engine:
# CFLAGS += -DVERBOSE

# Set this to 1 if Vrui executables and shared libraries shall contain
# links to any shared libraries they link to. This will relieve a user
# from having to set up a dynamic library search path.
USE_RPATH = 1

# This setting must match the same setting in Vrui's makefile.
GLSUPPORT_USE_TLS = 0

########################################################################
# Everything below here should not have to be changed
########################################################################

# Specify version of created dynamic shared libraries
MAJORLIBVERSION = 1
MINORLIBVERSION = 6

# Specify default optimization/debug level
ifdef DEBUG
  DEFAULTDEBUGLEVEL = 3
  DEFAULTOPTLEVEL = 0
else
  DEFAULTDEBUGLEVEL = 0
  DEFAULTOPTLEVEL = 3
endif

# Set destination directory for libraries and plugins:
ifdef DEBUG
  LIBDESTDIR = $(PACKAGEROOT)/$(LIBEXT)/debug
else
  LIBDESTDIR = $(PACKAGEROOT)/$(LIBEXT)
endif
VISLETDESTDIR = $(LIBDESTDIR)/$(VISLETPLUGINEXT)
PLUGINDESTDIR = $(LIBDESTDIR)/$(PROTOCOLPLUGINEXT)

# Directories for installation components
HEADERINSTALLDIR = $(INSTALLDIR)/$(INCLUDEEXT)
ifdef DEBUG
  LIBINSTALLDIR = $(INSTALLDIR)/$(LIBEXT)/debug
  EXECUTABLEINSTALLDIR = $(INSTALLDIR)/$(BINEXT)/debug
else
  LIBINSTALLDIR = $(INSTALLDIR)/$(LIBEXT)
  EXECUTABLEINSTALLDIR = $(INSTALLDIR)/$(BINEXT)
endif
VISLETINSTALLDIR = $(LIBINSTALLDIR)/$(VISLETPLUGINEXT)
PLUGININSTALLDIR = $(LIBINSTALLDIR)/$(PROTOCOLPLUGINEXT)
ETCINSTALLDIR = $(INSTALLDIR)/etc
SHAREINSTALLDIR = $(INSTALLDIR)/share

########################################################################
# Definitions for required software packages
########################################################################

# The SPEEX speech coding library:
SPEEX_BASEDIR = /usr
SPEEX_DEPENDS = 
SPEEX_INCLUDE = 
SPEEX_LIBDIR  = 
SPEEX_LIBS    = -lspeex

# The ogg multimedia container library:
OGG_BASEDIR = /usr
OGG_DEPENDS = 
OGG_INCLUDE = 
OGG_LIBDIR  = 
OGG_LIBS    = -logg

# The Theora video codec library:
THEORA_BASEDIR = /usr
THEORA_DEPENDS = OGG
THEORA_INCLUDE = 
THEORA_LIBDIR  = 
THEORA_LIBS    = -ltheora

# Package definition for Vrui collaboration infrastructure:
MYCOLLABORATION_BASEDIR     = $(PACKAGEROOT)
MYCOLLABORATION_DEPENDS     = MYVRUI
MYCOLLABORATION_INCLUDE     = -I$(MYCOLLABORATION_BASEDIR)
MYCOLLABORATION_LIBDIR      = -L$(MYCOLLABORATION_BASEDIR)/$(MYLIBEXT)
MYCOLLABORATION_LIBS        = -lCollaboration.$(LDEXT)

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
# (Supported packages can be found in ./BuildRoot/Packages)
########################################################################

PACKAGES = 

########################################################################
# Specify all final targets
########################################################################

LIBRARIES = 
VISLETS = 
PLUGINS = 
EXECUTABLES = 

#
# The only installed library:
#

LIBRARY_NAMES = libCollaboration

LIBRARIES += $(LIBRARY_NAMES:%=$(call LIBRARYNAME,%))

#
# The protocol plugins:
#

PLUGIN_NAMES = FooServer \
               FooClient \
               CheriaServer \
               CheriaClient \
               GrapheinServer \
               GrapheinClient \
               AgoraServer \
               AgoraClient

PLUGINS += $(PLUGIN_NAMES:%=$(call PLUGINNAME,%))

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

ALL = $(LIBRARIES) $(PLUGINS) $(VISLETS) $(EXECUTABLES)

.PHONY: all
all: config $(ALL)

# Make all plugins, vislets, and executables depend on collaboration library:
$(PLUGINS) $(VISLETS) $(EXECUTABLES): $(call LIBRARYNAME,libCollaboration)

########################################################################
# Pseudo-target to print configuration options
########################################################################

.PHONY: config
config:
	@echo "---- Configured Vrui collaboration infrastructure options: ----"
	@echo "Vrui package root directory: $(VRUIPACKAGEROOT)"
ifneq ($(GLSUPPORT_USE_TLS),0)
	@echo "Multithreaded rendering enabled"
else
	@echo "Multithreaded rendering disabled"
endif
	@echo "Installation directory: $(INSTALLDIR)"
	@echo "Vislet plug-in directory: $(VISLETINSTALLDIR)"
	@echo "Protocol plug-in directory: $(PLUGININSTALLDIR)"
	@echo "--------"

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
include $(VRUIPACKAGEROOT)/BuildRoot/BasicMakefile

########################################################################
# Specify build rules for dynamic shared objects
########################################################################

#
# Function to get the build dependencies of a package
#

DEPENDENCIES = $(patsubst -l%.$(LDEXT),$(call LIBRARYNAME,lib%),$(foreach PACKAGENAME,$(filter MY%,$($(1)_DEPENDS)),$($(PACKAGENAME)_LIBS)))

#
# The Vrui collaboration infrastructure
#

COLLABORATION_HEADERS = Collaboration/CollaborationPipe.h \
                        Collaboration/CollaborationServer.h \
                        Collaboration/CollaborationClient.h \
                        Collaboration/ProtocolServer.h \
                        Collaboration/ProtocolClient.h

COLLABORATION_SOURCES = Collaboration/CollaborationPipe.cpp \
                        Collaboration/CollaborationServer.cpp \
                        Collaboration/CollaborationClient.cpp \
                        Collaboration/ProtocolServer.cpp \
                        Collaboration/ProtocolClient.cpp

$(OBJDIR)/Collaboration/CollaborationServer.o: CFLAGS += -DCOLLABORATION_PLUGINDSONAMETEMPLATE='"$(PLUGININSTALLDIR)/lib%s.$(PLUGINFILEEXT)"'
$(OBJDIR)/Collaboration/CollaborationServer.o: CFLAGS += -DCOLLABORATION_CONFIGFILENAME='"$(ETCINSTALLDIR)/Collaboration.cfg"'
$(OBJDIR)/Collaboration/CollaborationClient.o: CFLAGS += -DCOLLABORATION_PLUGINDSONAMETEMPLATE='"$(PLUGININSTALLDIR)/lib%s.$(PLUGINFILEEXT)"'
$(OBJDIR)/Collaboration/CollaborationClient.o: CFLAGS += -DCOLLABORATION_CONFIGFILENAME='"$(ETCINSTALLDIR)/Collaboration.cfg"'

$(call LIBRARYNAME,libCollaboration): PACKAGES += $(MYCOLLABORATION_DEPENDS)
$(call LIBRARYNAME,libCollaboration): EXTRACINCLUDEFLAGS += $(MYCOLLABORATION_INCLUDE)
$(call LIBRARYNAME,libCollaboration): $(COLLABORATION_SOURCES:%.cpp=$(OBJDIR)/%.o)
.PHONY: libCollaboration
libCollaboration: $(call LIBRARYNAME,libCollaboration)

#
# The collaboration protocol plugins:
#

# Implicit rule for creating protocol plug-ins:
$(call PLUGINNAME,%): PACKAGES += MYCOLLABORATION
$(call PLUGINNAME,%): CFLAGS += $(CPLUGINFLAGS)
$(call PLUGINNAME,%): $(OBJDIR)/Collaboration/%.o
	@mkdir -p $(PLUGINDESTDIR)
ifdef SHOWCOMMAND
	$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $^ $(LINKDIRFLAGS) $(LINKLIBFLAGS)
else
	@echo Linking $@...
	@$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $^ $(LINKDIRFLAGS) $(LINKLIBFLAGS)
endif

$(call PLUGINNAME,CheriaServer): $(OBJDIR)/Collaboration/CheriaPipe.o \
                                 $(OBJDIR)/Collaboration/CheriaServer.o

$(call PLUGINNAME,CheriaClient): $(OBJDIR)/Collaboration/CheriaPipe.o \
                                 $(OBJDIR)/Collaboration/CheriaClient.o

$(call PLUGINNAME,GrapheinServer): $(OBJDIR)/Collaboration/GrapheinPipe.o \
                                   $(OBJDIR)/Collaboration/GrapheinServer.o

$(call PLUGINNAME,GrapheinClient): $(OBJDIR)/Collaboration/GrapheinPipe.o \
                                   $(OBJDIR)/Collaboration/GrapheinClient.o

$(call PLUGINNAME,AgoraServer): $(OBJDIR)/Collaboration/AgoraPipe.o \
                                $(OBJDIR)/Collaboration/AgoraServer.o

$(call PLUGINNAME,AgoraClient): PACKAGES += THEORA OGG SPEEX
$(call PLUGINNAME,AgoraClient): $(OBJDIR)/Collaboration/SpeexEncoder.o \
                                $(OBJDIR)/Collaboration/SpeexDecoder.o \
                                $(OBJDIR)/Collaboration/AgoraPipe.o \
                                $(OBJDIR)/Collaboration/AgoraClient.o

# Mark all protocol plugin object files as intermediate:
.SECONDARY: $(PLUGIN_NAMES:%=$(OBJDIR)/Collaboration/%.o)

#
# The collaboration client vislet:
#

# Implicit rule for creating vislet plug-ins:
$(call VISLETNAME,%): PACKAGES += MYCOLLABORATION
$(call VISLETNAME,%): CFLAGS += $(CPLUGINFLAGS)
$(call VISLETNAME,%): $(OBJDIR)/%Vislet.o
	@mkdir -p $(VISLETDESTDIR)
ifdef SHOWCOMMAND
	$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $^ $(LINKDIRFLAGS) $(LINKLIBFLAGS)
else
	@echo Linking $@...
	@$(CCOMP) $(PLUGINLINKFLAGS) -o $@ $^ $(LINKDIRFLAGS) $(LINKLIBFLAGS)
endif

$(call VISLETNAME,CollaborationClient): $(OBJDIR)/CollaborationClientVislet.o

# Mark all vislet plugin object files as intermediate:
.SECONDARY: $(VISLET_NAMES:%=$(OBJDIR)/%Vislet.o)

#
# The collaboration server test program:
#

$(EXEDIR)/CollaborationServer: PACKAGES += MYCOLLABORATION
$(EXEDIR)/CollaborationServer: $(OBJDIR)/CollaborationServerMain.o
.PHONY: CollaborationServer
CollaborationServer: $(EXEDIR)/CollaborationServer

#
# The collaboration client test program:
#

$(EXEDIR)/CollaborationClientTest: PACKAGES += MYCOLLABORATION
$(EXEDIR)/CollaborationClientTest: $(OBJDIR)/CollaborationClientTest.o
.PHONY: CollaborationClientTest
CollaborationClientTest: $(EXEDIR)/CollaborationClientTest

########################################################################
# Specify installation rules for header files, libraries, executables,
# configuration files, and shared files.
########################################################################

# Sequence to create symlinks for dynamic libraries:
# First argument: library name
define CREATE_SYMLINK
@-rm -f $(LIBINSTALLDIR)/$(call DSONAME,$(1)) $(LIBINSTALLDIR)/$(call LINKDSONAME,$(1))
@cd $(LIBINSTALLDIR) ; ln -s $(call FULLDSONAME,$(1)) $(call DSONAME,$(1))
@cd $(LIBINSTALLDIR) ; ln -s $(call FULLDSONAME,$(1)) $(call LINKDSONAME,$(1))

endef

install: all
# Install all header files in HEADERINSTALLDIR:
	@echo Installing header files...
	@install -d $(HEADERINSTALLDIR)/Collaboration
	@install -m u=rw,go=r $(COLLABORATION_HEADERS) $(HEADERINSTALLDIR)/Collaboration
# Install all library files in LIBINSTALLDIR:
	@echo Installing libraries...
	@install -d $(LIBINSTALLDIR)
	@install $(LIBRARIES) $(LIBINSTALLDIR)
	@echo Configuring run-time linker...
	$(foreach LIBNAME,$(LIBRARY_NAMES),$(call CREATE_SYMLINK,$(LIBNAME)))
# Install all protocol plugins in PLUGININSTALLDIR:
	@echo Installing protocol plugins...
	@install -d $(PLUGININSTALLDIR)
	@install $(PLUGINS) $(PLUGININSTALLDIR)
# Install all vislet plugins in VISLETINSTALLDIR:
	@echo Installing vislet plugins...
	@install -d $(VISLETINSTALLDIR)
	@install $(VISLETS) $(VISLETINSTALLDIR)
# Install all binaries in EXECUTABLEINSTALLDIR:
	@echo Installing executables...
	@install -d $(EXECUTABLEINSTALLDIR)
	@install $(EXECUTABLES) $(EXECUTABLEINSTALLDIR)
# Install all configuration files in ETCINSTALLDIR:
	@echo Installing configuration files...
	@install -d $(ETCINSTALLDIR)
	@install -m u=rw,go=r share/Collaboration.cfg $(ETCINSTALLDIR)

# Sequence to destroy symlinks for dynamic libraries:
# First argument: library name
define DESTROY_SYMLINK
@-rm -f $(LIBINSTALLDIR)/$(call DSONAME,$(1)) $(LIBINSTALLDIR)/$(call LINKDSONAME,$(1))

endef

uninstall:
	@echo Removing header files...
	@rm -rf $(HEADERINSTALLDIR)/Collaboration
	@echo Removing libraries...
	@rm -f $(LIBRARIES)
	$(foreach LIBNAME,$(LIBRARY_NAMES),$(call DESTROY_SYMLINK,$(LIBNAME)))
	@echo Removing protocol plugins...
	@rm -rf $(PLUGININSTALLDIR)
	@echo Removing vislet plugins...
	@rm -f $(VISLETS)
	@echo Removing executables...
	@rm -f $(EXECUTABLES)
	@echo Removing configuration files...
	@rm -f $(ETCINSTALLDIR)/Collaboration.cfg
