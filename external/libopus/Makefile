###########################################################################
#
# Copyright 2018 Samsung Electronics All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
# either express or implied. See the License for the specific
# language governing permissions and limitations under the License.
#
###########################################################################
############################################################################
# external/libopus/Makefile
#
#   Copyright (C) 2012 Gregory Nutt. All rights reserved.
#   Author: Gregory Nutt <gnutt@nuttx.org>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name NuttX nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

-include $(TOPDIR)/.config
-include $(TOPDIR)/Make.defs

DEPPATH	= --dep-path . --dep-path celt --dep-path silk --dep-path silk/fixed --dep-path src
VPATH	= :celt :silk :silk/fixed :src

ASRCS	=
CSRCS	=


CSRCS += $(notdir $(wildcard celt/*.c))
CSRCS += $(notdir $(wildcard silk/*.c))
CSRCS += $(notdir $(wildcard silk/fixed/*.c))
CSRCS += analysis.c \
	 mlp.c \
	 mlp_data.c \
	 opus.c \
	 opus_decoder.c \
	 opus_encoder.c \
	 repacketizer.c \
	 opus_multistream.c \
	 opus_multistream_decoder.c \
	 opus_multistream_encoder.c

ADD_CFLAGS := -DNULL=0 -DSOCKLEN_T=socklen_t -DLOCALE_NOT_USED \
          -D_LARGEFILE_SOURCE=1 -D_FILE_OFFSET_BITS=64 \
          -Drestrict='' -D__EMX__ -DOPUS_BUILD \
    	  -DHAVE_LRINT -DHAVE_LRINTF -O1 -fno-math-errno
ADD_CFLAGS += -DFIXED_POINT -DDISABLE_FLOAT_API
ADD_CFLAGS += -DUSE_SHAREDPTR

CFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus
CFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/include
CFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/celt
CFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/silk
CFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/silk/fixed

CXXFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus
CXXFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/include
CXXFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/celt
CXXFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/silk
CXXFLAGS += -I$(TOPDIR)/$(EXTDIR)/libopus/silk/fixed

CFLAGS += $(ADD_CFLAGS)

CXXFLAGS += $(ADD_CFLAGS)

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))

SRCS = $(ASRCS) $(CSRCS)
OBJS = $(AOBJS) $(COBJS)

ifeq ($(CONFIG_WINDOWS_NATIVE),y)
  BIN = ..\libexternal$(LIBEXT)
else
ifeq ($(WINTOOL),y)
  BIN = ..\\libexternal$(LIBEXT)
else
  BIN = ../libexternal$(LIBEXT)
endif
endif

all: .built
.PHONY: .depend depend clean distclean

$(AOBJS): %$(OBJEXT): %.S
	$(call ASSEMBLE, $<, $@)

# Use CXX compiler for .c to support smart pointer (std::shared_ptr)
$(COBJS): %$(OBJEXT): %.c
	$(call COMPILEXX, $<, $@)

.built: $(OBJS)
	$(call ARCHIVE, $(BIN), $(OBJS))
	$(Q) touch .built

.depend: Makefile $(SRCS)
	$(Q) $(MKDEP) $(DEPPATH) "$(CXX)" -- $(CXXFLAGS) -- $(SRCS) >Make.dep
	$(Q) touch $@

depend: .depend

clean:
	$(call DELFILE, .built)
	$(call CLEAN)

distclean: clean
	$(call DELFILE, Make.dep)
	$(call DELFILE, .depend)

-include Make.dep

