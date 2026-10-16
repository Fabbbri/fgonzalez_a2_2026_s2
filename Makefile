# Las mediciones se ejecutan en serie, incluso con make -j.
.DEFAULT_GOAL := help
.NOTPARALLEL:
PYTHON ?= python3
PERF ?= perf
REPETITIONS ?= 3
MODELS ?= sequential fine coarse smt cmp
PROBLEMS ?= nbody ray dl sentinel
THREADS ?= auto
SMT_THREADS ?= auto
SCOPE ?= process
EVENTS ?= task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses,stalled-cycles-frontend,stalled-cycles-backend
NBODY_ARGS ?= --bodies-per-galaxy 500 --steps 1000 --dt 0.002 --seed 42
RAY_ARGS ?= --width 640 --height 480 --samples 8 --bounces 12 --tile-size 32
SENTINEL_ARGS ?= --width 24444 --height 1499 --tile-width 32 --tile-height 32 --allow-nodata
SLC_A ?= sentinel1-SLC/data/coregistered/P09-R2-2023-2024/A.tif
SLC_B ?= sentinel1-SLC/data/coregistered/P09-R2-2023-2024/B.tif
DL_EPOCHS ?= 1000
DL_SAMPLES ?= 100
DL_SEED ?= 1
DL_QUANTUM_US ?= 100
BLOCK_INTERVAL_MS ?= 10
BLOCK_DURATION_MS ?= 1
NBODY_QUANTUM ?= 1
RAY_QUANTUM ?= 64
SENTINEL_QUANTUM ?= 2
export PERF REPETITIONS MODELS PROBLEMS THREADS SMT_THREADS SCOPE EVENTS
export NBODY_ARGS RAY_ARGS SENTINEL_ARGS SLC_A SLC_B
export DL_EPOCHS DL_SAMPLES DL_SEED DL_QUANTUM_US
export BLOCK_INTERVAL_MS BLOCK_DURATION_MS NBODY_QUANTUM RAY_QUANTUM SENTINEL_QUANTUM

.PHONY: help build build-nbody build-ray build-dl build-sentinel perf perf-nbody perf-ray perf-dl perf-sentinel perf-report test-perf
help:
	@echo 'make perf                  Compila y mide los cuatro problemas y cinco modelos'
	@echo 'make perf-nbody            Mide un problema (también ray, dl, sentinel)'
	@echo 'make perf-report           Regenera perf/reporte.md y gráficas SVG desde CSV'
	@echo 'make test-perf             Pruebas del perfilador con contadores simulados'
	@echo 'Opciones: REPETITIONS=3 THREADS=4 SMT_THREADS=12 SCOPE=process|cpu'
	@echo 'Ver perf/README.md para eventos, cargas y rutas Sentinel.'
build: build-nbody build-ray build-dl build-sentinel
build-nbody:
	+$(MAKE) -C n-body-sim build
build-ray:
	+$(MAKE) -C 'ray tracing' WITH_SDL=0 build
build-dl:
	+$(MAKE) -C DL/src build
build-sentinel:
	+$(MAKE) -C sentinel1-SLC all
perf:
	$(PYTHON) scripts/perf_profile.py --check
	@for problem in $(PROBLEMS); do $(MAKE) perf-$$problem || exit $$?; done
perf-nbody: build-nbody
	$(PYTHON) scripts/perf_profile.py --problem nbody
perf-ray: build-ray
	$(PYTHON) scripts/perf_profile.py --problem ray
perf-dl: build-dl
	$(PYTHON) scripts/perf_profile.py --problem dl
perf-sentinel: build-sentinel
	$(PYTHON) scripts/perf_profile.py --problem sentinel
perf-report:
	$(PYTHON) scripts/perf_profile.py --report
test-perf:
	$(PYTHON) -m unittest discover -s scripts/tests -p 'test_perf*.py'
