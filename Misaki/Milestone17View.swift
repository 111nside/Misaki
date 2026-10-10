import SwiftUI

struct Milestone17View: View {
    @State private var output: String?

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 17 · Guest modules") {
                    Button {
                        let result = NativeCoreAPI.runModuleLifecycleDiagnostic()
                        output = "Guest module lifecycle: \(result.passed ? "PASS" : "FAIL")\nRAX=\(result.rax)\nModules=\(result.modules)\nImports=\(result.imports)\nInstructions=\(result.instructions)\nInitialized/Finalized=\(result.initializations)/\(result.finalizations)\nDependency order=\(result.dependencyOrderValid)\nUnloaded=\(result.unloaded)\nStack restored=\(result.stackRestored)\nStatus=\(result.status)"
                    } label: {
                        Label("Execute module lifecycle test", systemImage: "square.stack.3d.up")
                    }
                    if let output {
                        Text(output)
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("Verified behavior") {
                    Label("Load dependencies before dependents", systemImage: "checkmark.circle")
                    Label("Track shared module references", systemImage: "checkmark.circle")
                    Label("Patch guest import and run C++ CPU", systemImage: "checkmark.circle")
                    Label("Unload only after final release", systemImage: "checkmark.circle")
                    Label("Reject cycles, missing modules and overlapping memory", systemImage: "checkmark.circle")
                }
                Section("Limitations") {
                    Text("This is a deliberately bounded lifecycle manager for self-authored, immutable guest modules. Initialization and finalization are tracked as events; arbitrary ELF constructors/destructors are not run. It is not Sony's PRX/SCE module manager, and PS4 firmware is not yet bootable.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Modules lifecycle")
        }
    }
}
