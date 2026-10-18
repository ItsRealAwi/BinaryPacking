from pathlib import Path

from setuptools import Extension, setup
from setuptools.command.build_py import build_py

here = Path(__file__).parent


class BuildPy(build_py):
    """The package lives in the project root, so keep build files out of it."""

    def find_package_modules(self, package, package_dir):
        return [m for m in super().find_package_modules(package, package_dir) if m[1] != "setup"]


setup(
    name="BinaryPacking",
    version="1.0.2",
    cmdclass={"build_py": BuildPy},
    description="Fast C extension for packing and unpacking numeric values as binary data.",
    long_description=(here / "README.md").read_text(encoding="utf-8"),
    long_description_content_type="text/markdown",
    author="ItsRealAwi",
    license="MIT",
    license_files=["LICENSE"],
    url="https://github.com/ItsRealAwi/BinaryPacking",
    python_requires=">=3.10,<3.17",
    packages=["BinaryPacking"],
    package_dir={"BinaryPacking": "."},
    package_data={"BinaryPacking": ["Packing.pyi", "py.typed"]},
    include_package_data=True,
    ext_modules=[
        Extension(
            "BinaryPacking.Packing",
            sources=["Packing.c"],
            define_macros=[("Py_LIMITED_API", "0x030A0000")],
            py_limited_api=True,
        )
    ],
    options={"bdist_wheel": {"py_limited_api": "cp310"}},
    zip_safe=False,
    keywords=["binary", "pack", "unpack", "struct", "serialization"],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: C",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
        "Programming Language :: Python :: 3.13",
        "Programming Language :: Python :: 3.14",
        "Programming Language :: Python :: 3.15",
        "Programming Language :: Python :: 3.16",
        "Programming Language :: Python :: Implementation :: CPython",
        "Typing :: Typed",
    ],
)
