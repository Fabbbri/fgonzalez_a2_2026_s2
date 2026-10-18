CXX = g++
CXXFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c++17 -pthread
EIGEN ?= -I/usr/include/eigen3
PYTHON ?= python3
TARGETS = main fino grueso smt cmp
HEADERS = mnist.h checksum.h runtime.h update_pool.h
.DEFAULT_GOAL := all
REPETITIONS ?= 200
EPOCHS ?= 1000
SAMPLES ?= 100
SEED ?= 1
QUANTUM_US ?= 100
BLOCK_INTERVAL_MS ?= 10
BLOCK_DURATION_MS ?= 1
BENCHMARK_MODELS ?= sequential fine-grained coarse-grained smt cmp
THREAD_COUNTS ?= auto
CMP_THREAD_COUNTS ?= auto
SMT_THREAD_COUNTS ?= auto
SMT_STATES ?= on,off
SMT ?= keep
BENCHMARK_MAKE ?= $(MAKE)
BENCHMARK_ARGS ?=
BASELINE_CSV ?=
RESULTS_DIR ?= resultdl
CSV ?= $(RESULTS_DIR)/*.csv
PLOT_DIR ?= $(RESULTS_DIR)/plots/comparison
MPLCONFIGDIR ?= /tmp/dl-matplotlib
export MPLCONFIGDIR

all: main fino grueso smt.bin cmp
build: all

main fino grueso cmp: %: %.cpp lector.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EIGEN) $< lector.cpp $(LDFLAGS) $(LDLIBS) -o $@

smt.bin: smt.cpp lector.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(EIGEN) $< lector.cpp $(LDFLAGS) $(LDLIBS) -o $@
	ln -sf smt.bin smt

smt:
	@set -eu; \
	case '$(SMT)' in \
	    keep) exit 0 ;; \
	    on|off) ;; \
	    *) echo 'SMT debe ser keep, on u off' >&2; exit 1 ;; \
	esac; \
	control=/sys/devices/system/cpu/smt/control; \
	active=/sys/devices/system/cpu/smt/active; \
	if [ ! -r "$$control" ] || [ ! -r "$$active" ]; then \
	    echo 'Este sistema no expone el control SMT de Linux' >&2; exit 1; \
	fi; \
	current=$$(cat "$$control"); \
	if [ "$$current" != '$(SMT)' ]; then \
	    case "$$current" in \
	        forceoff|notsupported|notimplemented) \
	            echo "No se puede cambiar SMT: $$current" >&2; exit 1 ;; \
	    esac; \
	    printf '%s\n' '$(SMT)' | sudo tee "$$control" >/dev/null; \
	fi; \
	state=$$(cat "$$active"); \
	expected=0; [ '$(SMT)' != on ] || expected=1; \
	if [ "$$state" != "$$expected" ]; then \
	    echo "No se alcanzó SMT=$(SMT): smt/active=$$state" >&2; exit 1; \
	fi; \
	echo "SMT=$(SMT), active=$$state (estado global; permanece después de ejecutar)"


benchmark: all smt
	BENCHMARK_COMPILER="$(CXX)" BENCHMARK_FLAGS="$(CPPFLAGS) $(CXXFLAGS) $(EIGEN) $(LDFLAGS)" \
	$(PYTHON) scripts/benchmark.py ./main --models $(BENCHMARK_MODELS) \
		--results-dir "$(RESULTS_DIR)" --repetitions "$(REPETITIONS)" \
		--epochs "$(EPOCHS)" --samples "$(SAMPLES)" --seed "$(SEED)" \
		--quantum-us "$(QUANTUM_US)" --block-interval-ms "$(BLOCK_INTERVAL_MS)" --block-duration-ms "$(BLOCK_DURATION_MS)" \
		--thread-counts "$(THREAD_COUNTS)" --cmp-thread-counts "$(CMP_THREAD_COUNTS)" --smt-thread-counts "$(SMT_THREAD_COUNTS)" \
		--smt-states "$(SMT_STATES)" --make-command "$(BENCHMARK_MAKE)" \
		$(if $(BASELINE_CSV),--baseline-csv "$(BASELINE_CSV)") $(BENCHMARK_ARGS)

rebase:
	$(PYTHON) scripts/rebase_reference.py --results-dir "$(RESULTS_DIR)" \
		$(if $(BASELINE_CSV),--baseline-csv "$(BASELINE_CSV)")

plots:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)"

amdahl:
	$(PYTHON) scripts/plot_metrics.py "$(CSV)" --output-dir "$(PLOT_DIR)" --amdahl-only

test: all
	$(PYTHON) -m unittest discover -s tests -p 'test_*.py'

clean:
	rm -f main fino grueso smt smt.bin cmp

.PHONY: all build smt benchmark rebase plots amdahl test clean
