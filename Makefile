PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include

.PHONY: install uninstall

install:
	@echo "Installing lazy headers to $(INCLUDEDIR)/lazy..."
	install -d $(INCLUDEDIR)/lazy/
	cp -r include/* $(INCLUDEDIR)/lazy/
	@echo "Done. You can now use: #include <lazy/lazy.hpp>"

uninstall:
	@echo "Removing lazy headers from $(INCLUDEDIR)..."
	rm -rf $(INCLUDEDIR)/lazy
	@echo "Done."
