from conan import ConanFile
from conan.tools.cmake import cmake_layout


class NmosInterlinkConan(ConanFile):
    """Optional Conan 2 recipe; dependencies are intentionally deferred."""

    name = "nmos-interlink"
    version = "0.0.0"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)
