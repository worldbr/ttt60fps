TARGET = ttt_camera_dist03
OBJS = src/main.o
CFLAGS = -O2 -G0 -Wall -Wextra -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector
LDFLAGS = -nostartfiles
BUILD_PRX = 1
PRX_EXPORTS = exports.exp
LIBS = -lpspuser
PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
