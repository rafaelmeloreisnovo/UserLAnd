.PHONY: all clean test

all clean:
	$(MAKE) -C rafaelia $@

test:
	$(MAKE) -C rafaelia test
