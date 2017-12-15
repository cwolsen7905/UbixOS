all: universe

universe:
	(cd src; make all)

clean:
	(cd src;make clean)