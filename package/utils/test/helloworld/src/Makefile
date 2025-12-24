all: helloworld

helloworld: helloworld.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

clean:
	rm -f helloworld

.PHONY: all clean