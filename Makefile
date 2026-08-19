CC       ?= cc
CSTD     := -std=c11
WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
OPT      ?= -O2
CPPFLAGS += -Isrc -Ithird_party
# -MMD -MP emits .d files so a header change rebuilds every .c that includes it.
# Without this, editing a header silently links mismatched objects.
DEPFLAGS := -MMD -MP
CFLAGS   += $(CSTD) $(WARN) $(OPT)
LDLIBS   := -lm

BIN      := ascii-art
BUILD    := build
SRC      := $(wildcard src/*.c)
OBJ      := $(patsubst src/%.c,$(BUILD)/%.o,$(SRC))
LIBOBJ   := $(filter-out $(BUILD)/main.o,$(OBJ))
TESTBIN  := $(BUILD)/test_ascii

SAMPLE   ?= images/owl.png

.PHONY: all clean test examples svelte preview bull site

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# stb_image is third-party: compile it without our pedantic warning set so the
# vendored header does not drown out warnings in our own code.
$(BUILD)/image.o: src/image.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(DEPFLAGS) $(CSTD) $(OPT) -Wall -c -o $@ $<

$(BUILD)/%.o: src/%.c | $(BUILD)
	$(CC) $(CPPFLAGS) $(DEPFLAGS) $(CFLAGS) -c -o $@ $<

$(BUILD):
	mkdir -p $(BUILD)

test: $(TESTBIN)
	./$(TESTBIN)

$(TESTBIN): tests/test_ascii.c $(LIBOBJ) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Regenerates the checked-in sample outputs. The sample owl sits on a white
# background, so --invert gives it the dark-ink-on-light-page orientation.
examples: $(BIN)
	mkdir -p examples
	./$(BIN) $(SAMPLE) --mode mono  --format txt  --width 180 --charset simple \
		--invert --threshold 8 -o examples/sample.txt
	./$(BIN) $(SAMPLE) --mode gray  --format html --width 260 --charset full \
		--invert --contrast 1.2 --brightness -5 --threshold 8 --gray-levels 16 \
		-o examples/sample-gray.html
	./$(BIN) $(SAMPLE) --mode color --format html --width 260 --charset full \
		--saturation 0.65 --contrast 1.15 --threshold 8 --color-step 32 \
		-o examples/sample-color.html

# A .svelte component you can drop straight into a project. --charset safe is
# the full ramp minus { and }, which Svelte would parse as expression
# delimiters -- with them the component is a compile error.
svelte: $(BIN)
	mkdir -p examples
	./$(BIN) $(SAMPLE) --mode gray --format html --width 200 --charset safe \
		--invert --contrast 1.2 --threshold 8 -o examples/AsciiArt.svelte
	@echo "wrote examples/AsciiArt.svelte"

# Render the bull example and a browser-openable preview of it.
BULL_ARGS := --mode gray --width 300 --charset-custom '8096453271:. ' \
	--levels 20,195 --gamma 1.25 --threshold 10 --gray-levels 64

# The whiter-horned source has a brighter highlight tail (p98 209 vs 182), so it
# needs a higher white point or the horns clip to a flat white mass.
HORN_ARGS := --mode gray --width 300 --charset-custom '8096453271:. ' \
	--levels 20,230 --gamma 1.25 --threshold 10 --gray-levels 64

bull: $(BIN)
	mkdir -p examples
	./$(BIN) images/bull.jpg $(BULL_ARGS) --format html -o examples/BullAscii.svelte
	OUT=examples/bull-preview.html NO_OPEN=1 ./preview.sh images/bull.jpg $(BULL_ARGS)
	./$(BIN) images/bull.jpg $(subst --width 300,--width 240,$(BULL_ARGS)) --format html -o examples/BullCoarse.svelte
	OUT=examples/bull-coarse-preview.html NO_OPEN=1 ./preview.sh images/bull.jpg $(subst --width 300,--width 240,$(BULL_ARGS))
	./$(BIN) images/bull-whiter-horns.jpg $(HORN_ARGS) --format html -o examples/BullWhiterHorns.svelte
	OUT=examples/bull-whiter-horns-preview.html NO_OPEN=1 ./preview.sh images/bull-whiter-horns.jpg $(HORN_ARGS)
	@echo "components: examples/BullAscii.svelte examples/BullWhiterHorns.svelte"

# make preview IMG=path/to.jpg  -- renders with the bull settings and opens it
IMG ?= images/bull.jpg
preview: $(BIN)
	./preview.sh $(IMG) $(BULL_ARGS)

# Regenerate the bull component in the sibling website repo. The component is
# self-contained (own scoped <style>), so the site needs no global ascii CSS.
SITE ?= ../site/web/src/lib/components/AsciiBull.svelte
SITE_ARGS := --mode gray --width 220 --charset-custom '8096453271:. ' \
	--levels 20,195 --gamma 1.25 --threshold 10 --gray-levels 64

# The three below head sections of the dashboard, and share one cell size so a
# digit is the same size across the page: a section box measures ~950px, a digit
# is 0.6021em wide, and the bull sets them at ~9.55px, which is
# 950 / (9.55 x 0.6021) = 166 columns. That is the number to keep -- each
# --width is only what that crop has to be sampled at to land on it, and a
# re-crop means finding it again. Count the columns in the generated component,
# not in a --format txt render: the two renderers do not crop to the same box.

# The containers section. Same charset as the bull, two departures its source
# forces: it is a 4:1 photo, so it is sampled far wider than the others to reach
# the 166, and its white hull clips to a flat mass at the bull's white point, so
# that goes up to 235.
SHIP ?= ../site/web/src/lib/components/AsciiShip.svelte
SHIP_ARGS := --mode gray --width 193 --charset-custom '8096453271:. ' \
	--levels 25,235 --gamma 1.25 --threshold 10 --gray-levels 64

# The uptime section. Same charset and tone as the bull -- its night sky suits
# the bull's black point, and lifting it any further takes the milky way with it.
DISH ?= ../site/web/src/lib/components/AsciiDish.svelte
DISH_ARGS := --mode gray --width 166 --charset-custom '8096453271:. ' \
	--levels 20,195 --gamma 1.25 --threshold 10 --gray-levels 64

site: $(BIN)
	./$(BIN) images/bull.jpg $(SITE_ARGS) --format html -o $(SITE)
	./$(BIN) images/ship.jpg $(SHIP_ARGS) --format html -o $(SHIP)
	./$(BIN) images/sat_dish.jpg $(DISH_ARGS) --format html -o $(DISH)
	@echo "wrote $(SITE) $(SHIP) $(DISH)"

-include $(OBJ:.o=.d)

clean:
	rm -rf $(BUILD) $(BIN)
