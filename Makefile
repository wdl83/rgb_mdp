OBJ_DIR ?= ${PWD}/obj
export OBJ_DIR

DST_DIR ?= ${PWD}/dst
export DST_DIR

all: rgb_worker.Makefile zmqpp/Makefile
	make -C zmqpp
	mkdir -p ${OBJ_DIR}/zmqpp
	make PREFIX=${OBJ_DIR}/zmqpp install -C zmqpp
	make -f rgb_worker.Makefile

install: rgb_worker.Makefile
	make -C zmqpp
	mkdir -p ${OBJ_DIR}/zmqpp
	make PREFIX=${OBJ_DIR}/zmqpp install -C zmqpp
	make PREFIX=${DST_DIR} install -C zmqpp
	make -f rgb_worker.Makefile install

clean: rgb_worker.Makefile
	make -f rgb_worker.Makefile clean

purge:
	rm $(OBJ_DIR) -rf
