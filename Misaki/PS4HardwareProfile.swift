import Foundation

/// Describes the original PS4 hardware we are targeting. It does NOT emulate it.
enum PS4HardwareProfile {
    static let model = "PlayStation 4 (CUH-1000)"
    static let cpu = "AMD Jaguar, 8 x86-64 cores"
    static let gpu = "AMD GCN, 18 compute units"
    static let systemRAM = "8 GB GDDR5"
    static let graphicsBackend = "Metal (planned)"
    static let firstMajorGoal = "Boot the genuine PS4 home menu"
}
