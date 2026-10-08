.DEFAULT_GOAL := test-amiga-arm
.PHONY: setup-amiga amiga-doctor amiga-build amiga-emulator amiga-sim amiga-fsuae amiga-hardware test-amiga amiga-arm-build amiga-zz9000-build amiga-arm-demo test-amiga-arm
setup-amiga:
	python3 scripts/amiga.py setup

amiga-doctor:
	python3 scripts/amiga.py doctor

amiga-build:
	python3 scripts/amiga.py build

amiga-emulator:
	python3 scripts/amiga.py emulator

amiga-sim amiga-fsuae amiga-hardware:
	.tools/amiga-venv/bin/python scripts/amiga.py serve --profile $(patsubst amiga-%,%,$(subst amiga-sim,amiga-simulator,$@))

test-amiga:
	.tools/amiga-venv/bin/python tests/amiga/smoke_mcp.py

amiga-arm-build:
	python3 scripts/build_arm_debug.py

amiga-zz9000-build:
	python3 scripts/build_zz9000_debug.py $(ZZ9000_BUILD_ARGS)

amiga-arm-demo:
	.tools/amiga-venv/bin/python scripts/arm_debug_demo.py

test-amiga-arm:
	.tools/amiga-venv/bin/python -m unittest discover -s tests/amiga/arm_debug -v

.PHONY: amiga-fractal-build test-amiga-fractal
amiga-fractal-build:
	python3 scripts/build_zz9000_debug.py --fractal $(ZZ9000_BUILD_ARGS)

test-amiga-fractal:
	.tools/amiga-venv/bin/python -m unittest discover -s tests/amiga/fractal -v
