# from skbuild import setup
import setuptools
import skbuild
import sys
import os
# https://scikit-build.readthedocs.io/en/latest/usage.html

build_native_pass = "build" in sys.argv

cmake_process_manifest_hook = None
cmake_source_dir = "."

if not build_native_pass:
    # sys.argv.append("--skip-cmake")
    cmake_process_manifest_hook = lambda a: []
    cmake_source_dir = None

os.environ['TEMP'] = 'build'
os.environ['TMP'] = 'build'

setuptools.setup(
    cmake_install_dir='build',
    name="dgfx",
    version="1.0",
    packages=['dgfxpy'],
    cmake_source_dir=cmake_source_dir,
    cmake_process_manifest_hook=cmake_process_manifest_hook,
)