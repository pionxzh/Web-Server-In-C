all:	
	gcc -o fork fork.c
	gcc -o select select.c
clean:
	rm -i fork select
