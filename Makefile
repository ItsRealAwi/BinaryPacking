PYTHON ?= python

.PHONY: build wheel sdist dist check upload clean

build:
	$(PYTHON) setup.py build_ext --inplace

wheel:
	$(PYTHON) -m pip wheel . -w dist --no-deps

sdist:
	$(PYTHON) setup.py sdist

dist: clean sdist wheel

check: dist
	$(PYTHON) -m twine check dist/*

upload: check
	$(PYTHON) -m twine upload dist/*

clean:
	$(PYTHON) -c "import shutil,glob;[shutil.rmtree(p,True) for p in ['build','dist']+glob.glob('*.egg-info')]"
