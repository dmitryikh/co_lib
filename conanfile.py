from conans import ConanFile, CMake


class CoLibConan(ConanFile):
    name = "co_lib"
    version = "0.1"
    license = "MIT"
    author = "Dmitry Khominich khdmitryi@gmail.com"
    description = "experimental asynchronous C++20 framework that feels like std library"
    topics = ("<Put some tag here>", "<here>", "<and here>")
    generators = "cmake"
    exports_sources = "include/*", "CMakeLists.txt", "test/*", "cmake/*"
    requires = "libuv/1.40.0", "boost/1.75.0"
    build_requires = "catch2/2.13.4"

    def package(self):
        self.copy("*.hpp")

    def build(self): # this is not building a library, just tests
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.test()

    def package_id(self):
        self.info.header_only()
