GCC = gcc
CFLAGS = -Wall

MAIN = main.c
OUT = teste.out

all: compile exec clean

compile:
	$(GCC) $(CFLAGS) $(MAIN) -o $(OUT)

exec:
	@clear
	$(OUT)
	
clean:
	@rm $(OUT)