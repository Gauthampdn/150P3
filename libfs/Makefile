# Target library
lib := libfs.a
objects:= fs.o disk.o
CC:= gcc
CFLAGS:= -Wall -Werror -Wextra
STATIC:= ar rcs

all: $(lib) $(objects)

$(lib): $(objects)
	$(STATIC) $@ $^

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(lib) $(objects)