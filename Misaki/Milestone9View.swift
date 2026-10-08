import SwiftUI

/// Diagnostic surface for Misaki's portable C++ engine.
/// Does not invoke Sony firmware or execute game binaries.
struct Milestone9View: View {
    @State private var reportText: String?
    @State private var executionText: String?
    @State private var backendText: String?

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 9 · Native C++ core") {
                    LabeledContent("Engine", value: NativeCoreAPI.version)
                    Button {
                        runNativeDiagnostic()
                    } label: {
                        Label("Run native core diagnostic", systemImage: "cpu")
                    }
                    if let reportText {
                        Text(reportText)
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Milestone 10 · Native guest execution") {
                    Button {
                        runGuestELF()
                    } label: {
                        Label("Execute native guest ELF", systemImage: "play.rectangle")
                    }
                    Text("Loads a self-authored ELF64 executable, links a guest function, and runs CALL/MOV/ADD/RET/HLT through the original C++ interpreter.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let executionText {
                        Text(executionText)
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Milestone 11 · Portable x86-64 backend") {
                    Button {
                        runX64Backend()
                    } label: {
                        Label("Execute expanded CPU backend", systemImage: "cpu.fill")
                    }
                    Text("Runs a separate non-JIT C++ backend with 16 guest registers, stack frames, signed multiplication, conditional branches and a linked guest function. Self-authored test bytes only.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let backendText {
                        Text(backendText)
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Native engine") {
                    Label("C++17 core with C / Swift bridge", systemImage: "checkmark.circle")
                    Label("Modular instruction execution interface", systemImage: "checkmark.circle")
                    Label("Integer, register, memory and branch subset", systemImage: "checkmark.circle")
                    Label("Native regression suite (CMake / CTest)", systemImage: "checkmark.circle")
                }
                Section("Not yet implemented") {
                    Label("Full x86-64, SSE/AVX and optimized ARM64 translation", systemImage: "xmark.circle")
                    Label("PS4 operating system and firmware", systemImage: "xmark.circle")
                    Label("PS4 GCN-to-Metal graphics", systemImage: "xmark.circle")
                    Text("Passing a native CPU diagnostic does not mean PS4 firmware can boot.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Native core")
        }
    }

    private func runX64Backend() {
        let result = NativeCoreAPI.runX64BackendDiagnostic()
        backendText = "Expanded CPU backend: \(result.passed ? "PASS" : "FAIL")\nRAX=\(result.rax)\nInstructions=\(result.instructions)\nImports=\(result.imports)\nStack restored=\(result.stackRestored)\nImport read-only=\(result.importReadOnly)\nStatus=\(result.status)"
    }

    private func runGuestELF() {
        let result = NativeCoreAPI.runCPUExecutionDiagnostic()
        executionText = "Native guest CPU: \(result.passed ? "PASS" : "FAIL")\nRAX=\(result.rax)\nInstructions=\(result.instructions)\nImports=\(result.imports)\nStack restored=\(result.stackRestored)\nRead-only import=\(result.importReadOnly)\nStatus=\(result.status)"
    }

    private func runNativeDiagnostic() {
        let result = NativeCoreAPI.runDiagnostic()
        reportText = "Native core: \(result.passed ? "PASS" : "FAIL")\nELF type=\(result.elfType)\nModules=\(result.registeredModules)\nImports=\(result.resolvedImports)\nGuest pointer=0x\(String(result.linkedGuestAddress, radix: 16))\nRead-only=\(result.importReadOnly)\nStatus=\(result.status)"
    }
}
