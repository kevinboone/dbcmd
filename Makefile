NAME    := dbcmd
VERSION := 0.0.4
CC      :=  gcc 
LIBS    := -lm -lcurl ${EXTRA_LIBS} 
TARGET	:= $(NAME) 
SOURCES := $(shell find src/ -type f -name *.c)
OBJECTS := $(patsubst src/%,build/%,$(SOURCES:.c=.o))
DEPS	:= $(OBJECTS:.o=.deps)
DESTDIR := /usr
MANDIR  := $(DESTDIR)/share/man
BINDIR  := $(DESTDIR)/bin
SHARE   := /usr/share/$(TARGET)
CFLAGS  := -fpie -fpic -Wall -DNAME=\"$(NAME)\" -DVERSION=\"$(VERSION)\" -g -I include ${EXTRA_CFLAGS}
LDFLAGS := -pie  ${EXTRA_LDFLAGS}

all: $(TARGET)

$(TARGET): $(OBJECTS) 
	$(CC) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS) 

build/%.o: src/%.c
	@mkdir -p build/
	$(CC) $(CFLAGS) -MD -MF $(@:.o=.deps) -c -o $@ $<

clean:
	@echo "  Cleaning..."; $(RM) -r build/ $(TARGET) 

install: $(TARGET)
	mkdir -p $(DESTDIR) $(BINDIR) $(MANDIR)
	install -s -m 755 $(TARGET) ${BINDIR}
	mkdir -p $(MANDIR)/man1
	cp -p man1/* ${MANDIR}/man1/

-include $(DEPS)

.PHONY: clean

