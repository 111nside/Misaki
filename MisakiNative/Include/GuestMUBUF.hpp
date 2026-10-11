#pragma once

#include "GuestMetalResources.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace misaki {

// GCN 1.0/1.1 (Southern Islands / Sea Islands) MUBUF 64-bit encoding.
// Hardware bit fields and load/store opcodes follow AMD's published ISA.
// Only untyped BUFFER_LOAD_DWORD (0x0c) and BUFFER_STORE_DWORD (0x1c)
// are recognized here. No GCN wavefront execution or PS4 shader support.
enum class MUBUFOp : std::uint32_t { loadDword = 12, storeDword = 28 };

struct MUBUFInstruction {
    MUBUFOp opcode = MUBUFOp::loadDword;
    std::uint32_t offsetBytes = 0;
    std::uint32_t vaddr = 0;
    std::uint32_t vdata = 0;
    std::uint32_t srsrc = 0;   // actual first SGPR of 4-register resource tuple
    std::uint32_t soffset = 0; // encoded SGPR or inline operand
    bool offen = false;
    bool idxen = false;
    bool glc = false;
    bool addr64 = false;
    bool lds = false;
    bool slc = false;
    bool tfe = false;
    std::uint32_t wordOffset = 0;
};

struct MUBUFTrace {
    std::vector<MUBUFInstruction> instructions;
    std::uint32_t loads = 0;
    std::uint32_t stores = 0;
    std::uint64_t checksum = 0;
};

// Decodes an exact-length MUBUF-only sequence. All failures are atomic.
// Does not read resource memory or act upon any guest operation.
std::optional<MUBUFTrace> decodeMUBUFWords(const std::uint32_t *words,
                                            std::size_t wordCount,
                                            std::string *error = nullptr);
std::optional<MUBUFTrace> decodeGuestMUBUF(const GuestMemory &guest,
                                            std::uint64_t guestAddress,
                                            std::size_t wordCount,
                                            std::string *error = nullptr);

// Restricted, explicitly defined single-lane execution contract:
// - IDXEN=1, OFFEN=0, 0 immediate bytes, zero inline SOFFSET (128).
// - VADDR=v4, VDATA=v8; resource registers s[0..3] / s[4..7].
// - First 32-bit DWORD of the selected record, native unsigned bits.
// - Input descriptor stride=16, output stride=4; 64 records each.
// The ALU's first output register is explicitly staged into guest v4;
// it is NOT automatic interoperability of full GCN SGPR/VGPR state.
struct MUBUF29Reference {
    std::array<std::uint32_t, 64> output{};
    std::uint32_t index = 0;
    std::uint32_t loaded = 0;
    std::uint32_t stored = 0;
};
std::optional<MUBUF29Reference> executeMUBUF29(
    const MUBUFTrace &trace, GuestMemory &memory,
    std::uint64_t inputDescriptorAddress, std::uint64_t outputDescriptorAddress,
    std::uint32_t vaddrIndex, const std::array<std::uint32_t, 64> &initial,
    std::string *error = nullptr);

std::optional<std::string> translateMUBUF29ToMetal(
    const MUBUFTrace &trace, const GCNBufferDescriptor &input,
    const GCNBufferDescriptor &output, std::string *error = nullptr);

struct MUBUF29Plan {
    MUBUFTrace decoded;
    std::string metalSource;
    std::array<std::uint8_t, 1024> inputBytes{};
    std::array<std::uint32_t, 64> initial{};
    std::array<std::uint32_t, 64> expected{};
    std::array<std::uint32_t, 6> aluRegisters{};
    std::uint32_t recordIndex = 0;
    std::uint32_t loaded = 0;
    std::uint32_t stored = 0;
    std::uint64_t inputBase = 0;
    std::uint64_t outputBase = 0;
    std::uint64_t shaderSourceHash = 0;
    std::uint64_t resultHash = 0;
    bool codeReadOnly = false;
    bool descriptorsReadOnly = false;
    bool sourceReadOnly = false;
    bool outputWritable = false;
};

std::vector<std::uint32_t> makeMUBUF29Fixture();
std::optional<MUBUF29Plan> runMUBUF29Diagnostic();

} // namespace misaki
