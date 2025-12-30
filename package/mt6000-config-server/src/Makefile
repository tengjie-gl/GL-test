TARGET = mt6000-config-server

SRCS = main.c server.c config_parser.c ssdp.c
OBJS = $(SRCS:.c=.o)

CFLAGS += -Wall -O2 -pthread
LDFLAGS += -lpthread

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJS) $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@