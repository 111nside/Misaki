import UniformTypeIdentifiers

// Explicitly declare the file extensions accepted by the iOS Files picker.
// These are imported for inspection only; importing does not execute them.
extension UTType {
    static let misakiSPRX = UTType(filenameExtension: "sprx", conformingTo: .data)!
    static let misakiSELF = UTType(filenameExtension: "self", conformingTo: .data)!
    static let misakiELF = UTType(filenameExtension: "elf", conformingTo: .data)!
}
