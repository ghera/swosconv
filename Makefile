CC ?= gcc
CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -Werror
LDFLAGS ?=
EXE ?= swosconv.exe
SRC = swosconv.c
OBJ = swosconv.o

.PHONY: all clean format check lint test

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) -o $@

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c $(SRC) -o $@

clean:
	$(RM) $(OBJ) $(EXE)

format:
	clang-format -i $(SRC)

check:
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) -o $(EXE)

lint:
	clang-tidy $(SRC) -- $(CFLAGS)

test: check
	bash test/run-tests.sh ./$(EXE)
