#Makefile

CC = gcc
RM = rm -rf
INSTALL = /usr/bin/install

ifndef NAME
	NAME = ocpjbod
endif

CFLAGS = -std=gnu99 -O2 -DNAME=\"$(NAME)\"

ifdef UTIL_VERSION
CFLAGS += -DUTIL_VERSION=\"$(UTIL_VERSION)\"
endif

LDFLAGS = -lsgutils2 -lcurl -ljson-c -lswitchtec

BIN = $(NAME)

OBJS = array_device_slot.o  common.o  cooling.o  enclosure_info.o  expander.o  ocpjbod.o  jbod_interface.o  options.o  scsi_buffer.o  sensors.o  ses.o  led.o json.o drive_control.o jbof_interface.o

BINDIR=/usr/bin

#================================================================
# Suffix rules
#================================================================

.SUFFIXES :
.SUFFIXES : .c .o

.c.o :
	$(CC) -c $(CFLAGS) $<

$(BIN) : $(OBJS)
	$(CC) $(LDFLAGS) -o $(BIN) $(OBJS)

all : $(BIN)

install : all
	$(INSTALL) -D -m 755 $(BIN) $(DESTDIR)/$(BINDIR)/$(NAME)

clean :
	$(RM) *.o $(BIN)
