TARGET := mc_demo

INCDIR := /usr/include/ffmpeg
#LIBDIR := /usr/lib64

LIBS := stdc++ avcodec avfilter avformat avresample avutil swscale

# inner macros
CPPFLAGS += -I$(INCDIR)
CFLAGS := -Wall -O -g
CXXFLAGS := $(CFLAGS)
LDFLAGS += $(addprefix -l,$(LIBS))

# common command
RM := rm -f
AR := ar
CC := gcc
LD := ld

#ifeq
#else
#endif

# build target
.PHONY : default clean
default : $(TARGET)

SRCS := MediaCodec.cpp
SRCS += MediaCodecDemo.cpp
OBJS := $(patsubst %.cpp,%.o,$(SRCS))
DEPS := $(patsubst %.o,%.d,$(OBJS))

$(TARGET) : $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@ 

# auto generate depend file between source and object
%.o : %.cpp
	$(CC) $(CPPFLAGS) $(CXXFLAGS) -c $^ -o $@

%.d : %.cpp
	@set -e; rm -f $@; \
		$(CC) -M $(CPPFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\l.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

include $(DEPS)

clean :
	$(RM) $(OBJS) 
	$(RM) $(DEPS)
	$(RM) $(TARGET)
