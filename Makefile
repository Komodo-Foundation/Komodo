# Komodo — convenience targets; the real logic lives in the scripts.
#
#   make            build ./kmc (incremental; no-op when nothing changed)
#   make fast       build without the fixpoint re-check
#   make test       build, then run the example and sema test suites
#   make install    build, then symlink `komodo` into ~/.local/bin
#   make seed       refresh bootstrap/main.km.asm.gz (commit it with source changes)
#   make clean      remove build products

.PHONY: all fast test install seed clean

all:
	./build-self.sh

fast:
	./build-self.sh --fast

test: all
	./run-tests.sh
	./run-sema-tests.sh

install: all
	./install.sh

seed:
	./build-self.sh --force --update-seed

clean:
	rm -rf .build kmc main.km.asm main.km.stage.asm build.kmc.o
