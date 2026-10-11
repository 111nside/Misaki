import SwiftUI

struct Milestone26View: View {
    @State private var diagnostic: NativeShaderIRSnapshot?

    private func hex(_ value: UInt64) -> String {
        "0x" + String(value, radix: 16).uppercased()
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 26 · GCN shader IR") {
                    Button {
                        diagnostic = NativeShaderIRAPI.runDiagnostic()
                    } label: {
                        Label("Translate and evaluate guest shader", systemImage: "cpu")
                    }
                    Text("Decodes a self-authored GCN shader in protected guest memory, lowers its scalar/vector instructions into typed intermediate operations, then evaluates one safe software test lane. The output is not rendered by Metal.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let d = diagnostic {
                        Text("GCN shader IR: \(d.passed ? "PASS" : "FAIL")\nPM4 packets/draws=\(d.pm4Packets)/\(d.pm4Draws)\nIR instructions=\(d.irInstructions)\nScalar/Vector=\(d.scalarInstructions)/\(d.vectorInstructions)\nExecuted=\(d.executedInstructions)\nScalar/Vector writes=\(d.scalarWrites)/\(d.vectorWrites)\nS_ENDPGM=\(d.terminated)\nSource/Descriptor read-only=\(d.shaderReadOnly)/\(d.descriptorReadOnly)\nStatus=\(d.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                if let d = diagnostic, d.passed {
                    Section("Software execution results") {
                        LabeledContent("SGPR s1 / s2 / s3", value: "\(d.sgpr1) / \(d.sgpr2) / \(d.sgpr3)")
                        LabeledContent("VGPR v0", value: "\(Float(bitPattern: d.vgpr0Bits))")
                        LabeledContent("VGPR v1", value: "\(Float(bitPattern: d.vgpr1Bits))")
                        LabeledContent("VGPR v2", value: "\(Float(bitPattern: d.vgpr2Bits))")
                        LabeledContent("Guest shader address", value: hex(d.shaderAddress))
                        LabeledContent("Source checksum", value: hex(d.sourceChecksum))
                        LabeledContent("IR checksum", value: hex(d.irChecksum))
                        LabeledContent("Result checksum", value: hex(d.resultChecksum))
                    }
                    Section("Lowered IR program") {
                        ForEach(d.operations) { operation in
                            VStack(alignment: .leading, spacing: 4) {
                                Text("\(hex(operation.guestAddress)) · \(operation.mnemonic)")
                                    .font(.system(.subheadline, design: .monospaced))
                                    .textSelection(.enabled)
                                Text(operation.expression)
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundStyle(.secondary)
                                    .textSelection(.enabled)
                            }
                            .padding(.vertical, 2)
                        }
                    }
                }
                Section("Engine features") {
                    Label("GCN-to-typed-IR lowering for 11 opcodes", systemImage: "checkmark.circle")
                    Label("32-bit scalar and selected floating ALU operations", systemImage: "checkmark.circle")
                    Label("Single-lane register evaluator with input validation", systemImage: "checkmark.circle")
                    Label("Original PM4 draw-state and shader memory integration", systemImage: "checkmark.circle")
                }
                Section("Not yet PS4 shader execution") {
                    Text("This is a deliberately restricted diagnostic, not full GCN ISA support, wavefront scheduling, control flow, resource sampling, hardware rasterization, or shader-to-Metal generation. It cannot run PS4 firmware or the original home menu.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GCN Shader IR")
        }
    }
}
