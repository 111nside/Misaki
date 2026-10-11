#include "GuestVM48.hpp"
#include <cassert>
#include <iostream>
using namespace misaki;
int main(){
    GuestVM48 vm;
    assert(vm.map(0x10000,4096,permission::read|permission::write));
    assert(vm.map(0x20000,8192,permission::read|permission::write));
    assert(vm.mappedBytes()==12288 && vm.regions()==2);
    const std::uint8_t value[]={4,5,6,7};
    assert(vm.write(0x10030,value,sizeof(value)));
    assert(vm.write(0x20fff,value,sizeof(value)));
    assert(vm.read(0x10030)==4 && vm.read(0x21002)==7);
    assert(!vm.map(0x10000,4096,permission::read|permission::write));
    assert(!vm.map(0x20000,4096,permission::read|permission::write));
    assert(!vm.map(0x30000,4096,permission::read|permission::write|permission::execute));
    assert(!vm.map(0x31000,512,permission::read));
    assert(!vm.protect(0x10000,4096,permission::read|permission::write|permission::execute));
    assert(vm.protect(0x10000,4096,permission::read|permission::execute));
    assert(!vm.write(0x10030,value,sizeof(value)) && vm.snapshot().isExecutable(0x10030));
    assert(vm.unmap(0x10000,4096));
    assert(!vm.read(0x10030) && vm.read(0x20fff)==4);
    assert(!vm.unmap(0x10000,4096));
    assert(vm.mappedBytes()==8192 && vm.regions()==1);
    GuestVM48 other=vm;
    assert(other.unmap(0x20000,8192));
    assert(vm.read(0x20fff)==4 && !other.read(0x20fff));
    assert(vm.protect(0x20000,8192,permission::read));
    assert(!vm.write(0x21000,value,1));
    assert(!vm.map(UINT64_MAX-4095,4096,permission::read));
    std::cout<<"PASS: compatibility 48 transactional guest VM map/protect/unmap\n";
}
