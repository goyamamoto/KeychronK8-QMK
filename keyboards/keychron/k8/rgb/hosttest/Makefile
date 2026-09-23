# Host unit tests for the K8 US-JIS substitution: compiles the real usjis.c
# against stub QMK headers and checks every report it produces.
QMK   ?= ../../../../..
BOARD  = ..
BUILD  = build

CC      ?= cc
CFLAGS  += -std=gnu11 -O0 -g -Wall -Wextra -Wno-unused-parameter
CFLAGS  += -Istubs -I$(BOARD) -I$(QMK)/quantum

OBJS = $(BUILD)/usjis.o $(BUILD)/stubs.o $(BUILD)/test_usjis.o

test: $(BUILD)/test_usjis
	./$(BUILD)/test_usjis

$(BUILD)/test_usjis: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD)/usjis.o: $(BOARD)/usjis.c $(BOARD)/usjis.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<
$(BUILD)/%.o: %.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)

.PHONY: test clean
