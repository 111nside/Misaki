import SwiftUI

struct Milestone22View: View {
    @State private var trace: NativePM4Snapshot?

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 22 · AMD GCN PM4 decoding") {
                    Button {
                        trace = NativePM4API.runDiagnostic()
                    } label: {
                        Label("Decode guest PM4 command stream", systemImage: "waveform.path.ecg")
                    }
                    Text("Parses documented PM4 Type-0, Type-2, and Type-3 packet headers from a read-only guest-memory buffer. Interprets register-write metadata and selected draw/event markers without sending commands to the iPhone GPU.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    if let trace {
                        Text("GCN PM4 decode: \(trace.passed ? "PASS" : "FAIL")\nPackets=\(trace.packets)\nType0/2/3=\(trace.type0Packets)/\(trace.type2Packets)/\(trace.type3Packets)\nRegister writes=\(trace.registerWrites)\nContext/Shader/Other=\(trace.contextWrites)/\(trace.shaderWrites)/\(trace.otherWrites)\nNOP/Draw/Event=\(trace.nops)/\(trace.draws)/\(trace.events)\nLast draw vertices=\(trace.lastVertexCount)\nGuest memory verified=\(trace.guestMemoryVerified)\nChecksum=0x\(String(trace.checksum, radix: 16))\nStatus=\(trace.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                    }
                }
                if let trace, trace.passed {
                    Section("Decoded register updates") {
                        ForEach(trace.registers) { register in
                            VStack(alignment: .leading, spacing: 3) {
                                Text("Register 0x\(String(register.address, radix: 16).uppercased())")
                                    .font(.system(.subheadline, design: .monospaced))
                                Text("Value 0x\(String(register.value, radix: 16).uppercased()) · Packet \(register.packetIndex)")
                                    .font(.system(.caption, design: .monospaced))
                                    .foregroundStyle(.secondary)
                            }
                        }
                    }
                }
                Section("Implemented") {
                    Label("Authentic AMD PM4 packet framing", systemImage: "checkmark.circle")
                    Label("SET_CONTEXT_REG / SET_SH_REG / SET_UCONFIG_REG", systemImage: "checkmark.circle")
                    Label("NOP, index state, DRAW_INDEX_AUTO and EVENT_WRITE metadata", systemImage: "checkmark.circle")
                    Label("Read-only guest memory and bounded parsing", systemImage: "checkmark.circle")
                    Label("Explicit unsupported-opcode and indirect-buffer rejection", systemImage: "checkmark.circle")
                }
                Section("Still missing") {
                    Text("This is a first PM4 decoder, not a graphics translator. It does not execute PS4 AMD GCN commands, translate shaders, access a real PS4 GPU, decode Sony GNM/GNMX libraries, or run PS4 firmware. The sample packets are self-authored, not extracted from a game or console.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GCN PM4")
        }
    }
}
