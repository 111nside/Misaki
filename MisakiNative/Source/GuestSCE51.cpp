#include "../Include/GuestSCE51.hpp"
#include <limits>

namespace misaki { namespace {
std::uint64_t readLE(const std::uint8_t *bytes, std::size_t at, unsigned count) {
    std::uint64_t result = 0;
    for (unsigned i=0;i<count;++i) result |= std::uint64_t(bytes[at+i]) << (8*i);
    return result;
}
bool inRange(std::uint64_t offset, std::uint64_t length, std::size_t size) {
    return offset <= size && length <= size - offset;
}
void failure(std::string *error, const char *why) { if (error) *error = why; }
} // namespace
std::optional<SCE51Report> inspectSCE51(const std::uint8_t *bytes,
                                        std::size_t size, std::string *error) {
    if (error) error->clear();
    const auto reject = [&](const char *why)->std::optional<SCE51Report> {
        failure(error, why); return std::nullopt;
    };
    const auto m = inspectPS4Executable(bytes,size,error);
    if (!m) return std::nullopt;
    if (m->programHeaders == 0 || m->programHeaders > 128 || m->elfHeaderOffset > size ||
        !inRange(m->elfHeaderOffset,64,size)) return reject("Invalid SCE ELF header offset");
    const std::uint64_t off=m->elfHeaderOffset;
    const std::uint64_t phoff=readLE(bytes,static_cast<std::size_t>(off+32),8);
    if (phoff > std::numeric_limits<std::uint64_t>::max()-off ||
        !inRange(off+phoff,std::uint64_t(m->programHeaders)*56,size))
        return reject("ELF program header table exceeds file bounds");
    SCE51Report report;
    report.container=m->kind; report.elfType=m->elfType; report.entry=m->entry;
    report.encryptedSegments=m->encryptedSegments;
    report.compressedSegments=m->compressedSegments;
    bool unsupported=false;
    for (unsigned i=0;i<m->programHeaders;++i) {
        const auto at=static_cast<std::size_t>(off+phoff+i*56u);
        SCE51Segment segment;
        segment.type=static_cast<std::uint32_t>(readLE(bytes,at,4));
        segment.flags=static_cast<std::uint32_t>(readLE(bytes,at+4,4));
        segment.offset=readLE(bytes,at+8,8);
        segment.virtualAddress=readLE(bytes,at+16,8);
        segment.fileBytes=readLE(bytes,at+32,8);
        segment.memoryBytes=readLE(bytes,at+40,8);
        if (m->kind==PS4ContainerKind::sceELF && segment.type==1 &&
            segment.fileBytes>segment.memoryBytes)
            return reject("Unwrapped load segment exceeds memory size");
        if (segment.virtualAddress>UINT64_MAX-segment.memoryBytes)
            return reject("Load segment virtual range overflows");
        if (segment.type==1) {
            // Encrypted SELF wrappers can carry opaque PT_LOAD metadata. Do
            // not impose ordinary ELF loading constraints on protected data.
            if (m->kind==PS4ContainerKind::sceELF &&
                (!segment.memoryBytes || (segment.flags & ~7u) ||
                 (segment.flags&3u)==3u))
                return reject("Unsupported unwrapped load permissions or size");
            ++report.loadSegments;
            if (segment.flags & 1u) ++report.executableSegments;
            if (m->kind==PS4ContainerKind::sceELF &&
                !inRange(segment.offset,segment.fileBytes,size))
                return reject("Unwrapped PT_LOAD outside file");
        } else if (segment.type==2) {
            ++report.dynamicSegments;
            if (m->kind==PS4ContainerKind::sceELF &&
                !inRange(segment.offset,segment.fileBytes,size))
                return reject("Unwrapped PT_DYNAMIC outside file");
        } else if (segment.type==0x61000000u || segment.type==0x61000001u ||
                   segment.type==0x61000002u || segment.type==0x61000010u ||
                   segment.type==0x6fffffffu) {
            ++report.specialSegments;
        } else if (segment.type!=0 && segment.type!=6) {
            unsupported=true;
        }
        report.segments.push_back(segment);
    }
    if (m->encryptedSegments) report.disposition=SCE51Disposition::encryptedPayload;
    else if (m->compressedSegments) report.disposition=SCE51Disposition::compressedPayload;
    else if (unsupported || !report.loadSegments || !report.executableSegments)
        report.disposition=SCE51Disposition::unsupportedLayout;
    else report.disposition=SCE51Disposition::unencryptedFixtureOnly;
    // Even recognized, unencrypted input is not automatically authentic PS4 code.
    report.firmwareExecutable=false;
    return report;
}
} // namespace misaki
