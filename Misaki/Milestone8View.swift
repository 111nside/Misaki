import SwiftUI

struct Milestone8View: View {
    @State private var output: String?

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 8 · Dynamic ELF Modules") {
                    Button {
                        execute()
                    } label: {
                        Label("Execute ET_DYN module test", systemImage: "shippingbox.and.arrow.backward")
                    }
                    Text("Loads a self-authored ELF64 ET_DYN image, parses its dynamic symbol table, resolves an imported guest function, and applies two RELA relocations before running it.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let output {
                        Text(output)
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Implemented") {
                    Label("PT_LOAD / PT_DYNAMIC + configurable load bias", systemImage: "checkmark.circle")
                    Label("SysV dynamic symbols and DT_NEEDED", systemImage: "checkmark.circle")
                    Label("RELATIVE / GLOB_DAT / JUMP_SLOT (RELA)", systemImage: "checkmark.circle")
                    Label("Final ELF memory protections restored", systemImage: "checkmark.circle")
                }
                Section("Not implemented") {
                    Label("PS4 SELF/PRX/NID loading", systemImage: "xmark.circle")
                    Label("PS4 system firmware, kernel and GPU", systemImage: "xmark.circle")
                    Text("This diagnostic does not boot the genuine PS4 home menu or run commercial games.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Dynamic modules")
        }
    }

    private func execute() {
        do {
            let result = try DemoDynamicRunner.run()
            output = "ET_DYN module: \(result.passed ? "PASS" : "FAIL")\nRAX=\(result.output)\nRelocations=\(result.relocations)\nImports=\(result.imports.count)\nInstructions=\(result.instructions)\nStack restored=\(result.stackRestored)"
        } catch {
            output = "ET_DYN module test failed: \(error.localizedDescription)"
        }
    }
}
