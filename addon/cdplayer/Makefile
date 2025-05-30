#
# Makefile
#

CIRCLEHOME = ../../circle

OBJS    = mediaplayer.o

libmediaplayer.a: $(OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(OBJS)

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)
