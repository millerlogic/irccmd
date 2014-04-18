# Makefile based on Lua 5.2's Makefile

# Configurable settings

PLAT= none

# End of settings

PLATS= generic linux macosx

all: $(PLAT)

$(PLATS) clean:
	cd src && $(MAKE) $@

none:
	@echo "Please do 'make PLATFORM' where PLATFORM is one of these:"
	@echo "    $(PLATS)"

.PHONY: all $(PLATS) clean none

