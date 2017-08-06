
all:
	$(MAKE) -C IntelPCM
	$(MAKE) -C src
	$(MAKE) -C test

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean
	$(MAKE) -C IntelPCM clean
	@rm -f libthrpool.a  decentallibthrpool.a 

