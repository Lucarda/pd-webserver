# library name
lib.name = webserver

cflags += -I ./include -DNO_SSL 
ldlibs += -lm -lpthread

DEFS ?=  



webserver.class.sources = \
	embedded_c.c \
	./src/civetweb.c \
	pd-webserver.c \





datafiles = \
	webserver-help.pd \
	LICENSE.md \
	README.md \
	CHANGELOG.txt \

datadirs = \
	./example \



define forWindows
  cflags += 
  ldlibs = -lws2_32 
  datafiles += scripts/localdeps.win.sh scripts/windep.sh
endef

# -static 




# include Makefile.pdlibbuilder
# (for real-world projects see the "Project Management" section
# in tips-tricks.md)
PDLIBBUILDER_DIR=./pd-lib-builder
include $(PDLIBBUILDER_DIR)/Makefile.pdlibbuilder

localdep_windows: install
	cd "${installpath}"; ./windep.sh webserver.dll
	