VERSION := $(shell awk -F'"' '/define VERSION/{print $$2}' SmoothSkip.h)
SOURCES := $(shell git ls-files | grep -vE 'ignore|Makefile' | tr "\n" " ")
ARCHIVE := "dist/SmoothSkip-$(VERSION).zip"

package:
	mkdir -p dist
	rm -f $(ARCHIVE)
	7z a $(ARCHIVE) $(SOURCES) Release/*/*.dll

sum:
	sha256sum Release/*bit/*.dll
