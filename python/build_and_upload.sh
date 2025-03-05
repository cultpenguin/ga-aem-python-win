#!/bin/bash

# Make sure you have the required tools
pip install --upgrade pip
pip install --upgrade build setuptools wheel twine

# Build your package
#make clean
#make all
mkdir -p gatdaem1d
# Copy DLL files instead of .so
cp ./gatdaem1d/gatdaem1d.dll ./gatdaem1d/
cp ./gatdaem1d/libfftw3-3.dll ./gatdaem1d/
cp ./gatdaem1d/libfftw3f-3.dll ./gatdaem1d/


# Build distribution packages using build module (PEP 517)
python -m build

# Upload to TestPyPI first to verify
python -m twine upload --repository testpypi dist/*

echo "If the TestPyPI upload was successful, you can install and test with:"
echo "pip install --index-url https://test.pypi.org/simple/ ga-aem-forward-win

echo "When ready for the real release, run:"
echo "python -m twine upload dist/*"

