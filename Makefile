CC ?= gcc
CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -Werror -O2 -flto
LDFLAGS ?= -s
EXE ?= swosconv.exe

AMIGA_EXE = swosconv
RELEASE_DIR = release
HOST_RELEASE_EXE = swosconv-win64.exe
AMIGA_RELEASE_EXE = swosconv-amigaos68k
SRC = swosconv.c
OBJ = swosconv.o

.PHONY: all clean format check lint test amiga release

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $@

clean:
	$(RM) $(OBJ) $(EXE) $(AMIGA_EXE)

format:
	clang-format -i $(SRC)

check:
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(EXE)

lint:
	clang-tidy $(SRC) -- $(CFLAGS)

test: check
	bash test/run-tests.sh ./$(EXE)

amiga:
	$(MAKE) CC=m68k-amigaos-gcc EXE=$(AMIGA_EXE)

release:
	rm -rf $(RELEASE_DIR)
	mkdir $(RELEASE_DIR)
	$(MAKE) clean
	$(MAKE) EXE=$(RELEASE_DIR)/$(HOST_RELEASE_EXE)
	$(MAKE) clean
	$(MAKE) CC=m68k-amigaos-gcc EXE=$(RELEASE_DIR)/$(AMIGA_RELEASE_EXE)
	ls -lh $(RELEASE_DIR)/
