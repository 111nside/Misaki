import SwiftUI

struct Milestone25View: View {
    @State private var snapshot: NativeGCN25Snapshot?

    private func hex(_ word: UInt64) -> String {
        "0x" + String(word, radix: 16).uppercased()
    }

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 25 · AMD GCN shader decoding") {
                    Button {
                        snapshot = NativeGCN25API.runDiagnostic()
                    } label: {
                        Label("Decode guest GCN pixel shader", systemImage: "curlybraces.square")
                    }
                    Text("Reads an original, self-authored shader from the pixel-shader address identified by Milestone 24's AMD PM4 draw-state analysis. Decodes real-format GCN scalar/vector instruction headers and a 128-bit buffer descriptor; no shader instructions are executed.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let snapshot {
                        Text("GCN shader decode: \(snapshot.passed ? "PASS" : "FAIL")\nPM4 packets/draws=\(snapshot.pm4Packets)/\(snapshot.pm4Draws)\nShader instructions=\(snapshot.instructionCount)\nScalar/Vector=\(snapshot.scalarCount)/\(snapshot.vectorCount)\nLiteral DWORDs=\(snapshot.literalCount)\nCode DWORDs=\(snapshot.shaderWords)\nS_ENDPGM=\(snapshot.terminated)\nShader/Descriptor read-only=\(snapshot.shaderReadOnly)/\(snapshot.descriptorReadOnly)\nStatus=\(snapshot.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                if let snapshot, snapshot.passed {
                    Section("Shader and buffer descriptor") {
                        LabeledContent("PS address", value: hex(snapshot.shaderAddress))
                        LabeledContent("Buffer address", value: hex(snapshot.descriptorBase))
                        LabeledContent("Buffer stride", value: "\(snapshot.descriptorStride) bytes")
                        LabeledContent("Buffer records", value: "\(snapshot.descriptorRecords)")
                        LabeledContent("Data format (raw)", value: "\(snapshot.descriptorDataFormat)")
                        LabeledContent("Numeric format (raw)", value: "\(snapshot.descriptorNumericFormat)")
                        LabeledContent("Linear metadata", value: snapshot.descriptorSupported ? "Consistent" : "Unsupported")
                        LabeledContent("Shader checksum", value: hex(snapshot.checksum))
                    }
                    Section("Decoded GCN ISA · not executed") {
                        ForEach(snapshot.instructions) { instruction in
                            VStack(alignment: .leading, spacing: 4) {
                                Text("\(hex(instruction.guestAddress)) · \(instruction.mnemonic)")
                                    .font(.system(.subheadline, design: .monospaced))
                                    .textSelection(.enabled)
                                Text("\(instruction.family) · \(instruction.operandSummary)")
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundStyle(.secondary)
                                if instruction.hasLiteral {
                                    Text("Literal \(hex(UInt64(instruction.literal))) · 2 DWORDs")
                                        .font(.system(.caption, design: .monospaced))
                                        .foregroundStyle(.secondary)
                                }
                            }
                            .padding(.vertical, 2)
                        }
                    }
                }
                Section("What's implemented") {
                    Label("GCN scalar and vector instruction framing", systemImage: "checkmark.circle")
                    Label("Literal DWORD and instruction-length validation", systemImage: "checkmark.circle")
                    Label("Buffer-resource address, stride and format metadata", systemImage: "checkmark.circle")
                    Label("Bounded, read-only guest-memory shader inspection", systemImage: "checkmark.circle")
                    Label("Unsupported/unsafe instructions rejected", systemImage: "checkmark.circle")
                }
                Section("Not implemented") {
                    Text("This is a restricted GCN instruction decoder, not a shader interpreter or GCN-to-Metal translator. It does not execute wavefronts, handle full GCN ISA variants, validate actual resource mappings, render PS4 geometry, decrypt SELF/PRX files, or boot Sony firmware. Test instructions and buffers are original fixtures.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GCN Shader ISA")
        }
    }
}
