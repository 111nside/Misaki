#pragma once
#include "GuestX64Backend.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
namespace misaki {
// A persistent, deterministic multi-process session with an original private
// service ABI. No native PS4 scheduling, syscalls, or firmware is involved.
class PersistentGuest59 {
public:
    PersistentGuest59();
    ~PersistentGuest59();
    PersistentGuest59(const PersistentGuest59&)=delete;
    PersistentGuest59&operator=(const PersistentGuest59&)=delete;
    bool add(std::uint32_t pid,const std::vector<std::uint8_t>&code);
    bool step(std::uint32_t instructionsPerProcess=1);
    bool allExited()const;
    std::uint32_t processes()const;
    std::uint32_t instructions()const{return instructions_;}
    std::uint32_t contextSwitches()const{return switches_;}
    std::uint32_t yields()const;
    std::uint32_t serviceCalls()const;
    std::uint32_t exited()const;
private:
    struct Slot;
    std::vector<std::unique_ptr<Slot>> slots_;
    std::uint32_t instructions_=0,switches_=0;
    bool bad_=false;
};
std::vector<std::uint8_t> makePersistent59Program();
struct Persistent59Report {std::uint32_t processes=0,instructions=0,
  contextSwitches=0,yields=0,calls=0,exited=0,ticks=0;};
std::optional<Persistent59Report> runPersistent59Diagnostic();
} // namespace misaki
