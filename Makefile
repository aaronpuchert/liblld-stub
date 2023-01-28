LLVM_VERSION=15

all: liblld-stub.a liblld-stub.so

liblld-stub.a:
	$(CXX) -c -fvisibility-inlines-hidden $(CXXFLAGS) -o DriverStub.o DriverStub.cpp
	ar -crs $@ DriverStub.o

liblld-stub.so:
	$(CXX) -fPIC -fvisibility-inlines-hidden $(CXXFLAGS) -shared -lLLVM -Wl,-soname,liblld-stub.so.$(LLVM_VERSION) $(LDFLAGS) -o $@.$(LLVM_VERSION) DriverStub.cpp
	ln -s $@.$(LLVM_VERSION) $@

clean:
	-rm DriverStub.o liblld-stub.a liblld-stub.so liblld-stub.so.$(LLVM_VERSION)

.PHONY:	all clean
