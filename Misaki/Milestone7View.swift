import SwiftUI

struct Milestone7View: View {
    @State private var output: String?

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 7 · Guest Linking") {
                    Button {
                        runLinkedDemo()
                    } label: {
                        Label("Execute linked-library test", systemImage: "link")
                    }
                    Text("Resolves one symbol, writes a guest import pointer, protects its mapping, and calls the separately mapped guest function through the x86-64 interpreter.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let output {
                        Text(output)
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Emulation status") {
                    Label("Explicit guest import-table linking: prototype", systemImage: "checkmark.circle")
                    Label("ELF64 relative relocations: limited", systemImage: "puzzlepiece")
                    Label("Guest memory map/protect/unmap: whole regions", systemImage: "memorychip")
                    Label("PS4 PRX/NID loader: not implemented", systemImage: "xmark.circle")
                    Label("Real PS4 home menu: not bootable", systemImage: "xmark.circle")
                }
                Section("What this proves") {
                    Text("The test actually transfers control between separately mapped guest code modules and returns to the caller. It does not run PS4 firmware, game binaries, or Sony system libraries.")
                        .font(.footnote)
                }
            }
            .navigationTitle("Guest linker")
        }
    }

    private func runLinkedDemo() {
        do {
            let result = try GuestLibraryDemo.run()
            output = "Linked-library test: \(result.passed ? "PASS" : "FAIL")\nRAX=\(result.value)\nImports=\(result.linkedImports)\nInstructions=\(result.instructions)\nStack restored=\(result.stackRestored)"
        } catch {
            output = "Linked-library test failed: \(error.localizedDescription)"
        }
    }
}
