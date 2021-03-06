# Makefile for SGOS2:kernel

# Target machine information
ARCH = i386

ASFLAGS = -g
CCFLAGS = -g -Werror -Wall -Wno-char-subscripts -I../include -Iinclude -fno-builtin -ffreestanding -fleading-underscore
LDFLAGS =
OCFLAGS = -Obinary
ODFLAGS = -S

ifeq ($(OSTYPE), Linux)
# linux env
CCFLAGS += -fno-stack-protector
LDFLAGS += -T$(LDSCRIPT)
else
# windows env
LDFLAGS += -Ttext=0xC0100000 -e_start
endif

AS = as $(ASFLAGS)
CC = gcc $(CCFLAGS)
LD = ld $(LDFLAGS)
OC = objcopy $(OCFLAGS)
OD = objdump $(ODFLAGS)
LD2 = ld2
NM = nm
WF = ../tools/wf/wf

IMAGE = ../sgos2.img

# Kernel dir
DIR_ARCH	= arch
DIR_IPC		= ipc
DIR_MM		= mm
DIR_KD		= kd
DIR_START	= start
DIR_LIB		= lib
DIR_TM		= tm

# All objects, here can be set the first link obj for your kernel
OBJECTS :=

# Platform objects
include arch/$(ARCH)/Makefile

# Kernel objects
OBJECTS += $(patsubst %.c,%.o,  $(wildcard $(DIR_ARCH)/*.c)) \
		   $(patsubst %.c,%.o,  $(wildcard $(DIR_IPC)/*.c)) \
		   $(patsubst %.c,%.o,  $(wildcard $(DIR_MM)/*.c)) \
		   $(patsubst %.c,%.o,  $(wildcard $(DIR_KD)/*.c)) \
		   $(patsubst %.c,%.o,  $(wildcard $(DIR_START)/*.c)) \
		   $(patsubst %.c,%.o,  $(wildcard $(DIR_LIB)/*.c)) \
		   $(patsubst %.c,%.o,  $(wildcard $(DIR_TM)/*.c))

# Target
all: kernel

kernel: $(OBJECTS)
	$(LD) $(OBJECTS) -o$@
	$(NM) $@ > $@.sym
	$(OD) $@ > $@.dmp
	$(OC) $@ $@.bin
	$(WF) $(IMAGE) -src $@.bin -dest sgos/$@.bin

clean:
	$(RM) $(OBJECTS) \
		kernel.dmp \
		kernel \
		kernel.sym \
		kernel.bin

# Standard Procedures
.s.o:
	$(AS) -o $@ $<

.c.o:
	$(CC) -c -o $@ $<
