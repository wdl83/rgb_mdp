all: rgb_worker.Makefile
	make -f rgb_worker.Makefile

clean:
	rm *.o -f
	rm *.elf -f
