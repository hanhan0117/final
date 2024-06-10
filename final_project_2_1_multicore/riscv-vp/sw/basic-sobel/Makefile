OBJECTS  = main.o bootstrap.o
CFLAGS   = -march=rv32ima -mabi=ilp32
LDFLAGS  = -nostartfiles -Wl,--no-relax

#VP       = $(VPPATH)/basic-acc
VP_FLAGS = --intercept-syscalls --error-on-zero-traphandler=true 

include ../Makefile.common
