# =============================================================================
# Makefile – Memory Leak Detection and Reporting Tool
# =============================================================================
#
# Targets:
#   make          – build the library object, both demos (default)
#   make run-leak – run the intentional-leak demo
#   make run-clean– run the clean (no leak) demo
#   make test     – build and run the unit-test suite
#   make clean    – remove all build artefacts
#
# Compiler settings
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
LDFLAGS = -lpthread

# Directories
SRC_DIR   = src
EX_DIR    = examples
TEST_DIR  = tests
BUILD_DIR = build

# Source / object files
MEMTRACK_SRC = $(SRC_DIR)/memtrack.c
MEMTRACK_OBJ = $(BUILD_DIR)/memtrack.o

# Demo executables
DEMO_LEAK  = $(BUILD_DIR)/demo_leak
DEMO_CLEAN = $(BUILD_DIR)/demo_clean

# Test executable
TEST_BIN = $(BUILD_DIR)/test_memtrack

# =============================================================================
# Default target – build everything
# =============================================================================
.PHONY: all
all: $(BUILD_DIR) $(MEMTRACK_OBJ) $(DEMO_LEAK) $(DEMO_CLEAN)
	@echo ""
	@echo "Build complete.  Artefacts in $(BUILD_DIR)/"
	@echo "  Run:  make run-leak    (intentional leaks demo)"
	@echo "  Run:  make run-clean   (clean memory demo)"
	@echo "  Run:  make test        (unit test suite)"

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# =============================================================================
# Compile memtrack library object
# =============================================================================
$(MEMTRACK_OBJ): $(MEMTRACK_SRC) $(SRC_DIR)/memtrack.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# =============================================================================
# Demo executables
# =============================================================================
$(DEMO_LEAK): $(EX_DIR)/demo_leak.c $(MEMTRACK_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@ $(LDFLAGS)

$(DEMO_CLEAN): $(EX_DIR)/demo_clean.c $(MEMTRACK_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@ $(LDFLAGS)

# =============================================================================
# Test suite
# =============================================================================
$(TEST_BIN): $(TEST_DIR)/test_memtrack.c $(MEMTRACK_OBJ) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) $^ -o $@ $(LDFLAGS)

.PHONY: test
test: $(TEST_BIN)
	@echo ""
	@echo "=== Running unit tests ==="
	@./$(TEST_BIN); EXIT=$$?; \
	 echo ""; \
	 exit $$EXIT

# =============================================================================
# Run targets
# =============================================================================
.PHONY: run-leak
run-leak: $(DEMO_LEAK)
	@echo ""
	@echo "=== Running demo_leak ==="
	@./$(DEMO_LEAK)

.PHONY: run-clean
run-clean: $(DEMO_CLEAN)
	@echo ""
	@echo "=== Running demo_clean ==="
	@./$(DEMO_CLEAN)

# =============================================================================
# Clean
# =============================================================================
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) leak_report.txt
	@echo "Cleaned build artefacts."
