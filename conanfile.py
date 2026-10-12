from conan import ConanFile
from conan.tools.cmake import cmake_layout


class NmosInterlinkConan(ConanFile):
    """Conan 2 recipe for the NMOS Interlink build dependencies."""

    name = "nmos-interlink"
    version = "0.0.0"
    package_type = "application"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"

    def requirements(self):
        self.requires("boost/1.91.0")
        # Provisional baseline for the first networking-capable build phase.
        self.requires("openssl/3.5.2")

    def layout(self):
        cmake_layout(self)
