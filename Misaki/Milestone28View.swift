import SwiftUI
import Metal

struct Milestone28View: View {
    @State private var plan: NativeMetalResource28Plan?
    @State private var result: MetalResource28Execution?
    @State private var running = false

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 28 · Guest GPU memory resources") {
                    Button {
                        result = nil
                        running = false
                        let prepared = NativeMetalResource28API.makePlan()
                        plan = prepared
                        guard prepared.passed else { return }
                        running = true
                        let alu = prepared.alu.source
                        let resource = prepared.source
                        let bytes = prepared.inputBytes
                        let initial = prepared.outputInitial
                        let expectedRegisters = prepared.alu.expected
                        let expectedOutput = prepared.outputExpected
                        Task {
                            let completed = await Task.detached(priority: .userInitiated) {
                                MetalResource28Runner.execute(
                                    aluSource: alu, resourceSource: resource,
                                    inputBytes: bytes, initialOutput: initial,
                                    expectedRegisters: expectedRegisters,
                                    expectedOutput: expectedOutput)
                            }.value
                            result = completed
                            running = false
                        }
                    } label: {
                        Label("Run GPU resource-memory comparison", systemImage: "memorychip")
                    }
                    .disabled(running)
                    Text("First, the GPU runs the translated GCN ALU shader from Milestone 27. A second generated Metal kernel uses those GPU-computed registers to read a descriptor-backed 16-byte-stride resource and write one of 64 output records. Both kernels run in order on the device's GPU.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if let plan {
                        Text("Resource IR: \(plan.passed ? "PASS" : "FAIL")\nOperations=\(plan.operations)\nBuffer address=0x\(String(plan.descriptorBase, radix: 16))\nRecords / stride=\(plan.records) / \(plan.stride) bytes\nGPU read/write record=\(plan.readIndex)/\(plan.writeIndex)\nLoaded + scalar = \(plan.loadedWord) + 14 = \(plan.storedWord)\nProtected input / writable output=\(plan.guestInputReadOnly)/\(plan.guestOutputWritable)\nC++ status=\(plan.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if !plan.passed {
                            Text("Native resource validation failed; GPU execution skipped.")
                                .foregroundStyle(.red)
                        }
                    }
                    if running {
                        ProgressView("Compiling and executing two Metal compute passes…")
                    }
                    if let result {
                        Label("Metal resource comparison: \(result.didRun && result.matched ? "PASS" : "FAIL")",
                              systemImage: result.didRun && result.matched ? "checkmark.circle.fill" : "xmark.circle.fill")
                            .foregroundStyle(result.didRun && result.matched ? .green : .red)
                        Text(result.message).font(.footnote).textSelection(.enabled)
                        Text("GPU status=\(result.gpuStatus)")
                            .font(.system(.caption, design: .monospaced))
                    }
                }
                if let plan, plan.passed {
                    Section("CPU reference vs actual GPU memory") {
                        let idx = Int(plan.writeIndex)
                        Text("Destination record \(idx)")
                            .font(.headline)
                        Text("Initially: 0x\(String(plan.outputInitial[idx], radix: 16).uppercased())\nCPU expected: \(plan.outputExpected[idx])\nGPU actual: \(result?.output.count == 64 ? String(result!.output[idx]) : "Pending")")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        Text("All 64 output records and six GPU ALU registers must match. Unmodified records are compared too.")
                            .font(.footnote).foregroundStyle(.secondary)
                    }
                    Section("Generated Metal resource kernel") {
                        DisclosureGroup("Inspect Metal shader") {
                            Text(plan.source)
                                .font(.system(.caption2, design: .monospaced))
                                .textSelection(.enabled)
                        }
                    }
                }
                Section("Scope") {
                    Text("This is a controlled shader-resource experiment using a real-format AMD buffer descriptor, verified guest-memory bytes, and a synthetic three-operation resource IR. It does not yet decode or execute GCN MUBUF/MTBUF instructions, provide wavefront behavior, or run PS4 firmware or games.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GCN Resources")
        }
    }
}
