import SwiftUI
import Metal

struct Milestone29View: View {
    @State private var plan: NativeMUBUF29Plan?
    @State private var result: MetalResource28Execution?
    @State private var running = false

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 29 · GCN MUBUF memory instructions") {
                    Button {
                        result = nil
                        let prepared = NativeMUBUF29API.makePlan()
                        plan = prepared
                        guard prepared.passed else { return }
                        running = true
                        Task {
                            let completed = await Task.detached(priority: .userInitiated) {
                                NativeMUBUF29API.execute(prepared)
                            }.value
                            result = completed
                            running = false
                        }
                    } label: {
                        Label("Decode GCN MUBUF and compare Metal", systemImage: "memorychip")
                    }
                    .disabled(running)
                    Text("Decodes genuine GCN1.1 64-bit BUFFER_LOAD_DWORD and BUFFER_STORE_DWORD encodings from read-only guest memory. The CPU evaluates their bounded single-lane memory semantics, and Metal runs a kernel generated from the decoded operand fields. The preceding ALU shader supplies the address index.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if let plan {
                        Text("GCN MUBUF decode: \(plan.passed ? "PASS" : "FAIL")\nInstructions=\(plan.instructionCount)\nOpcodes LOAD/STORE=\(plan.loadOpcode)/\(plan.storeOpcode)\nVADDR/VDATA=v\(plan.vaddr)/v\(plan.vdata)\nResource SGPRs=s\(plan.inputResourceSGPR), s\(plan.outputResourceSGPR)\nRecord index=\(plan.index)\nLoaded/Stored=\(plan.loaded)/\(plan.stored)\nGuest code/descriptor read-only=\(plan.codeReadOnly)/\(plan.descriptorsReadOnly)\nSource read-only / output writable=\(plan.inputReadOnly)/\(plan.outputWritable)\nC++ status=\(plan.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if !plan.passed {
                            Text("Native decoding or guest memory validation failed; Metal was not run.")
                                .foregroundStyle(.red)
                        }
                    }
                    if running {
                        ProgressView("Compiling and running two Metal kernels…")
                    }
                    if let result {
                        Label("Metal MUBUF comparison: \(result.didRun && result.matched ? "PASS" : "FAIL")",
                              systemImage: result.didRun && result.matched ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundStyle(result.didRun && result.matched ? .green : .red)
                        Text("GPU status=\(result.gpuStatus)\nOutput record 7=\(result.output.count == 64 ? String(result.output[7]) : "unavailable")\n\(result.message)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                Section("New GCN capability") {
                    Label("Documented GCN1.1 MUBUF 64-bit instruction layout", systemImage: "checkmark.circle")
                    Label("Real DWORD load and store opcode/operand decoding", systemImage: "checkmark.circle")
                    Label("Separate input/output resource descriptors", systemImage: "checkmark.circle")
                    Label("Guest-memory permission and bounds validation", systemImage: "checkmark.circle")
                    Label("GPU readback compared with native execution", systemImage: "checkmark.circle")
                }
                Section("Remaining limitations") {
                    Text("Only a specifically bounded two-instruction sequence and one staged GPU lane are executable. OFFEN, ADDR64, coherency modes, atomics, true SGPR/VGPR state, full wavefront scheduling, Sony GNM/GNMX and PS4 graphics are not implemented. The diagnostic operates on self-authored guest memory, not firmware.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GCN MUBUF")
        }
    }
}
