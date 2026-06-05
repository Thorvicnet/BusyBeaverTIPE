all:
	gcc -Wall -Wextra -O2 -o main src/main.c src/beaver.c src/drift.c src/formula.c src/encoding.c src/tape.c

clean:
	rm -f main main.log
