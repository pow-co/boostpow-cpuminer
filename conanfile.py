from conans import ConanFile, CMake


class BoostMinerConan(ConanFile):
    name = "BoostMiner"
    version = "0.2.4"
    license = "Proprietary"
    author = "Proof of Work Company"
    url = "https://github.com/ProofOfWorkCompany/BoostMiner"
    description = "Worker for Mining Boost Puzzles on Bitcoin"
    topics = ("bitcoin", "mining", "cpu", "sha256", "proofofwork", "boost")
    settings = "os", "compiler", "build_type", "arch"   
    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}
    generators = "cmake"
    exports_sources = "src/*"
    requires = "gigamonkey/v0.0.5@proofofwork/stable", "gtest/1.10.0"

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        self.copy("BoostMiner", dst="bin", keep_path=False)
