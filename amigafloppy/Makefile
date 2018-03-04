PREFIX = /usr/local

src = $(wildcard src/*.c) $(wildcard src/unix/*.c)
obj = $(src:.c=.o)
dep = $(obj:.o=.d)
bin = amigafloppy

CFLAGS = -pedantic -Wall -g -Isrc

$(bin): $(obj)
	$(CC) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.d: %.c
	@$(CPP) $(CFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	rm -f $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
