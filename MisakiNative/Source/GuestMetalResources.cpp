#include "../Include/GuestMetalResources.hpp"

#include <limits>
#include <sstream>

namespace misaki {
namespace {
constexpr std::uint64_t fnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t fnvPrime = 1099511628211ULL;
constexpr std::uint64_t descriptorAddress = 0x110000;
constexpr std::uint64_t outputGuestAddress = 0x500000;

void fail(std::string *error, const char *message) {
    if (error) *error = message;
}
void digest32(std::uint64_t &hash, std::uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) {
        hash ^= static_cast<std::uint8_t>(word >> (8 * i));
        hash *= fnvPrime;
    }
}
std::uint64_t wordsHash(const std::array<std::uint32_t, 64> &data) {
    std::uint64_t hash = fnvOffset;
    for (auto word : data) digest32(hash, word);
    return hash;
}
std::uint64_t sourceHash(const std::string &source) {
    std::uint64_t hash = fnvOffset;
    for (unsigned char ch : source) { hash ^= ch; hash *= fnvPrime; }
    return hash;
}
bool validDescriptor(const GCNBufferDescriptor &desc) {
    return desc.supportedLinearMetadata && desc.baseAddress != 0 &&
           (desc.baseAddress & 3) == 0 && desc.strideBytes == 16 &&
           desc.records == 64 && desc.baseAddress <=
                    std::numeric_limits<std::uint64_t>::max() - 1023;
}
bool validProgram(const Resource28Program &program) {
    if (program.instructions.size() != 3) return false;
    for (std::size_t i = 0; i < 3; ++i) {
        const auto &ins = program.instructions[i];
        if (ins.referenceRegister >= 6 || ins.indexOffset < -32 || ins.indexOffset > 32)
            return false;
        if (ins.opcode != static_cast<Resource28Opcode>(i + 1) ||
            (i == 1 && ins.indexOffset != 0)) return false;
    }
    return true;
}
std::optional<std::uint32_t> index(std::uint32_t raw, std::int32_t offset) {
    const std::int64_t value = static_cast<std::int64_t>(raw) + offset;
    if (value < 0 || value >= 64) return std::nullopt;
    return static_cast<std::uint32_t>(value);
}
std::optional<std::uint32_t> readGuestWord(const GuestMemory &memory,
                                           std::uint64_t address) {
    if (address > std::numeric_limits<std::uint64_t>::max() - 3)
        return std::nullopt;
    std::uint32_t out = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto b = memory.read8(address + i);
        if (!b) return std::nullopt;
        out |= std::uint32_t(*b) << (8 * i);
    }
    return out;
}
void put32(std::uint8_t *data, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
std::optional<std::uint32_t> readFromResource(const GuestMemory &memory,
                                              const GCNBufferDescriptor &descriptor,
                                              std::uint32_t indexValue) {
    // Both arithmetic steps are bounded by the validated 64x16 descriptor.
    return readGuestWord(memory, descriptor.baseAddress + indexValue * 16u);
}
std::string metalIndex(std::uint32_t reg, std::int32_t offset) {
    std::string expr = "registers[" + std::to_string(reg) + "]";
    if (offset > 0) expr += " + " + std::to_string(offset) + "u";
    if (offset < 0) expr += " - " + std::to_string(-offset) + "u";
    return expr;
}
void metalCheckedIndex(std::ostringstream &msl, std::uint32_t reg,
                       std::int32_t offset, const char *variable) {
    if (offset < 0)
        msl << "    if (registers[" << reg << "] < " << -offset << "u) return;\n";
    if (offset > 0)
        msl << "    if (registers[" << reg << "] > " << 63 - offset << "u) return;\n";
    msl << "    const uint " << variable << " = " << metalIndex(reg, offset) << ";\n"
           "    if (" << variable << " >= 64u) return;\n";
}
} // namespace

Resource28Program makeResource28Program() {
    return {{{Resource28Opcode::loadU32, 0, 0},
             {Resource28Opcode::addRegister, 2, 0},
             {Resource28Opcode::storeU32, 1, -1}}};
}

std::optional<Resource28Reference> evaluateResource28(
    const Resource28Program &program, const GuestMemory &memory,
    const GCNBufferDescriptor &descriptor,
    const std::array<std::uint32_t, 6> &registers,
    const std::array<std::uint32_t, 64> &initial,
    std::string *error) {
    if (error) error->clear();
    if (!validDescriptor(descriptor) || !validProgram(program)) {
        fail(error, "Unsupported linear buffer descriptor or resource IR sequence");
        return std::nullopt;
    }
    const auto r = index(registers[program.instructions[0].referenceRegister],
                         program.instructions[0].indexOffset);
    const auto w = index(registers[program.instructions[2].referenceRegister],
                         program.instructions[2].indexOffset);
    if (!r || !w) {
        fail(error, "Out-of-bounds guest buffer index"); return std::nullopt;
    }
    const auto source = readFromResource(memory, descriptor, *r);
    if (!source) {
        fail(error, "Unreadable guest resource memory"); return std::nullopt;
    }
    Resource28Reference result;
    result.output = initial;
    result.loadedWord = *source;
    result.storedWord = *source + registers[program.instructions[1].referenceRegister];
    result.readIndex = *r;
    result.writeIndex = *w;
    result.output[*w] = result.storedWord;
    return result;
}

std::optional<std::string> translateResource28ToMetal(
    const Resource28Program &program, const GCNBufferDescriptor &descriptor,
    std::string *error) {
    if (error) error->clear();
    if (!validDescriptor(descriptor) || !validProgram(program)) {
        fail(error, "Unsupported resource IR or descriptor for Metal");
        return std::nullopt;
    }
    std::ostringstream msl;
    msl << "#include <metal_stdlib>\n"
           "using namespace metal;\n"
           "// Input is 64 records x 16 bytes, first DWORD of each record.\n"
           "kernel void misaki_resource28(device const uint *registers [[buffer(0)]],\n"
           "    device const uint *guestWords [[buffer(1)]],\n"
           "    device uint *outputWords [[buffer(2)]],\n"
           "    device uint *status [[buffer(3)]],\n"
           "    uint threadID [[thread_position_in_grid]]) {\n"
           "    if (threadID != 0u) return;\n"
           "    status[0] = 1u;\n";
    const auto &load = program.instructions[0];
    const auto &add = program.instructions[1];
    const auto &store = program.instructions[2];
    // Validate *both* indices before touching the output buffer.
    metalCheckedIndex(msl, load.referenceRegister, load.indexOffset, "readIndex");
    metalCheckedIndex(msl, store.referenceRegister, store.indexOffset, "writeIndex");
    msl << "    const uint loaded = guestWords[readIndex * " << descriptor.strideBytes / 4 << "u];\n"
           "    const uint result = loaded + registers[" << add.referenceRegister << "];\n"
           "    outputWords[writeIndex] = result;\n"
           "    status[0] = 0u;\n"
           "}\n";
    const auto text = msl.str();
    if (text.size() + 1 > 4096) {
        fail(error, "Generated Metal resource shader too large");
        return std::nullopt;
    }
    return text;
}

std::optional<MetalResource28Plan> runMetalResource28Diagnostic() {
    try {
        const auto prior = runMetalIRDiagnostic();
        const auto descriptorDiagnostic = runGCNShaderDiagnostic();
        if (!prior || !descriptorDiagnostic ||
            !validDescriptor(descriptorDiagnostic->buffer) ||
            prior->expected != std::array<std::uint32_t, 6>{
                7, 7, 14, 0x40000000u, 0x40800000u, 0x40800000u})
            return std::nullopt;
        const auto descBytes = makeGCNBufferFixture();
        const auto &descriptor = descriptorDiagnostic->buffer;
        const auto program = makeResource28Program();
        auto shader = translateResource28ToMetal(program, descriptor);
        if (!shader) return std::nullopt;

        MetalResource28Plan result;
        result.irChecksum = prior->irChecksum;
        result.registers = prior->expected;
        result.descriptorBase = descriptor.baseAddress;
        result.descriptorStride = descriptor.strideBytes;
        result.descriptorRecords = descriptor.records;
        result.resourceMSL = std::move(*shader);
        result.sourceHash = sourceHash(result.resourceMSL);
        for (std::size_t i = 0; i < 64; ++i) {
            // Each record has a 32-bit payload followed by twelve zero bytes.
            put32(result.guestBytes.data() + i * 16, static_cast<std::uint32_t>(i * 3 + 10));
            result.outputInitial[i] = 0xA0000000u + static_cast<std::uint32_t>(i);
        }
        GuestMemory guest;
        std::array<std::uint8_t, 16> encodedDescriptor{};
        for (unsigned i = 0; i < 4; ++i)
            put32(encodedDescriptor.data() + i * 4, descBytes[i]);
        if (!guest.map(descriptorAddress, encodedDescriptor.size(),
                       permission::read | permission::write) ||
            !guest.writeBytes(descriptorAddress, encodedDescriptor.data(), encodedDescriptor.size()) ||
            !guest.protect(descriptorAddress, encodedDescriptor.size(), permission::read) ||
            !guest.map(descriptor.baseAddress, result.guestBytes.size(),
                       permission::read | permission::write) ||
            !guest.writeBytes(descriptor.baseAddress, result.guestBytes.data(),
                              result.guestBytes.size()) ||
            !guest.protect(descriptor.baseAddress, result.guestBytes.size(), permission::read) ||
            !guest.map(outputGuestAddress, result.outputInitial.size() * 4,
                       permission::read | permission::write))
            return std::nullopt;
        result.guestInputReadOnly = !guest.writeBytes(descriptor.baseAddress,
                                                      result.guestBytes.data(), 1) &&
                                    !guest.writeBytes(descriptorAddress,
                                                      encodedDescriptor.data(), 1);
        const auto redecoded = decodeGuestGCNBuffer(guest, descriptorAddress);
        if (!result.guestInputReadOnly || !redecoded ||
            redecoded->baseAddress != descriptor.baseAddress ||
            redecoded->strideBytes != descriptor.strideBytes ||
            redecoded->records != descriptor.records) return std::nullopt;
        const auto evaluated = evaluateResource28(program, guest, *redecoded,
                                                   result.registers, result.outputInitial);
        if (!evaluated) return std::nullopt;
        result.outputExpected = evaluated->output;
        result.readIndex = evaluated->readIndex;
        result.writeIndex = evaluated->writeIndex;
        result.loadedWord = evaluated->loadedWord;
        result.storedWord = evaluated->storedWord;
        result.initialHash = wordsHash(result.outputInitial);
        result.expectedHash = wordsHash(result.outputExpected);
        // Exercise writable, isolated guest output and verify exact readback.
        std::array<std::uint8_t, 256> initialOutputBytes{};
        for (std::size_t i = 0; i < 64; ++i)
            put32(initialOutputBytes.data() + i * 4, result.outputInitial[i]);
        std::uint8_t stored[4];
        put32(stored, result.storedWord);
        if (!guest.writeBytes(outputGuestAddress, initialOutputBytes.data(),
                              initialOutputBytes.size()) ||
            !guest.writeBytes(outputGuestAddress + result.writeIndex * 4u,
                              stored, sizeof(stored))) return std::nullopt;
        result.guestOutputWritable = true;
        for (std::size_t i = 0; i < 64; ++i) {
            auto actual = readGuestWord(guest, outputGuestAddress + i * 4u);
            if (!actual || *actual != result.outputExpected[i])
                result.guestOutputWritable = false;
        }
        if (!result.guestOutputWritable || result.readIndex != 7 ||
            result.writeIndex != 6 || result.loadedWord != 31 ||
            result.storedWord != 45) return std::nullopt;
        return result;
    } catch (...) { return std::nullopt; }
}
} // namespace misaki
