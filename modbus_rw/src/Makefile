.SUFFIXES: .tar.gz .c
PKG_RELEASE:=0
PKG_VERSION:=1.0
IDIR=./inc
ODIR=./obj
BIN=./
SRC=./src
TARGET=modbus_rw
CC=gcc
CFLAGS+=-I$(IDIR)  -g -lpthread -lrt
#SDIR=/home/fargo/workspace/git/E220/openwrt/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2
SDIR=/home/mujeeb/Desktop/epack/openwrt/staging_dir/target-mipsel_24kec+dsp_uClibc-0.9.33.2
LDFLAGS+=-L$(SDIR)/usr/lib -L$(SDIR)/usr/lib/mysql 
LIBS+= -lmodbus_database -lmysqlclient -lgeneric_apis -lcurl
#uncomment below line to work in localtime
#DFLAGS+=DM_LOCALTIME

_SRC := $(shell ls $(SRC))

_DEPS := $(shell ls $(IDIR))
DEPS=$(_DEPS:%=$(IDIR)/%)

_OBJS=$(_SRC:%.c=%.o)
OBJS=$(_OBJS:%=$(ODIR)/%)

$(ODIR)/%.o: $(SRC)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(LIBS) $(LDFLAGS) $(DFLAGS)

$(BIN)/$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS) $(LDFLAGS) $(DFLAGS)

clean:
	-rm -r ./*.gz ./$(TARGET)-$(PKG_VERSION).$(PKG_RELEASE) $(BIN)$(TARGET) $(ODIR)/*.o 
