from conans import ConanFile, CMake


class CoLibConan(ConanFile):
    name = "co_lib"
    version = "0.1"
    license = "MIT"
    author = "Dmitry Khominich khdmitryi@gmail.com"
    description = "experimental asynchronous C++20 framework that feels like std library"
    topics = ("c++20", "coroutines", "asynchronous programming")
    generators = "cmake"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "src/*", "CMakeLists.txt", "tests/*", "cmake/*"
    requires = "libuv/1.40.0", "boost/1.75.0"
    build_requires = "catch2/2.13.4"
    default_options = {
        "boost:header_only": True,
    }

    def package(self):
        self.copy("*.hpp", dst="include", src="src")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)

    def build(self): # this is not building a library, just tests
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()

    def package_info(self):
        self.cpp_info.libs = ["co_lib"]
