TARGET = ttt_fps_teste05
OBJS = src/main.o
CFLAGS = -O2 -G0 -Wall -Wextra -std=gnu11
LDFLAGS = -nostartfiles
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
LIBS = -lpspuser
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
