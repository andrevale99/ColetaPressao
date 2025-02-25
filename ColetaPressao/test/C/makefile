GCC = gcc
CFLAGS = -Wall

MAIN = CPTerminal.c
OUT = teste_cpt.out

all: compile exec clean

compile:
	$(GCC) $(CFLAGS) $(MAIN) -o $(OUT)

exec:
	@clear
	$(OUT)
	
clean:
	@rm $(OUT)