MODULE_DIR ?= $(CURDIR)

MODULES  += $(addprefix $(MODULE_DIR)/,getcontext.o setcontext.o makecontext.o swapcontext.o) 

PTHREAD_GENERATE_MANGLE ?= -n "s/^.*@@@name@@@\([^@]*\)@@@value@@@[^0-9Xxa-fA-F-]*\([0-9Xxa-fA-F-][0-9Xxa-fA-F-]*\).*@@@end@@@.*\$$/\#define \1 \2/p"

$(MODULE_DIR)/makecontext.o : $(MODULE_DIR)/makecontext.c
	@echo "MODULE_DIR='$(MODULE_DIR)'" 
	$(CC) $(CFLAGS) -c $< -o $@

$(MODULE_DIR)/getcontext.o: $(MODULE_DIR)/getcontext.S $(MODULE_DIR)/ucontext_i.h 
	$(CC) $(CFLAGS) -c $< -o $@

$(MODULE_DIR)/setcontext.o: $(MODULE_DIR)/setcontext.S $(MODULE_DIR)/ucontext_i.h
	$(CC) $(CFLAGS) -c $< -o $@

$(MODULE_DIR)/swapcontext.o: $(MODULE_DIR)/swapcontext.S $(MODULE_DIR)/ucontext_i.h
	$(CC) $(CFLAGS) -c $< -o $@

$(MODULE_DIR)/ucontext_i.h: $(MODULE_DIR)/ucontext_i.sym
	awk -f $(MODULE_DIR)/scripts/gen-as-const.awk $< \
		| $(CC) $(CFLAGS) -x c - -S -o - \
			| sed $(PTHREAD_GENERATE_MANGLE) > $@
