all: framer pemtest

RM ?= rm -f

framer: frames/frame.c skeinwrap.c frames/pem.c skein/skein.c skein/skein_block.c
	gcc -DFRAMER_TEST -o $@ $^

pemtest: frames/pem.c
	gcc -DTEST_PEM -o $@ $^

clean:
	$(RM) framer pemtest
