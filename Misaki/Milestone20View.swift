import SwiftUI
import Metal

struct Milestone20View: View {
    @State private var scene: NativeGraphics20Frame?
    @State private var animating = false

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 20 · Guest GPU queue") {
                    Button {
                        animating = false
                        scene = NativeGraphics20API.makeFrame()
                    } label: {
                        Label("Execute guest GPU command queue", systemImage: "square.stack.3d.forward.dottedline")
                    }
                    Text("An x86-64 guest writes drawing data into its isolated virtual memory, then submits seven commands and a fence through a simulated GPU service. Misaki copies, validates, and renders the queue. Metal displays the result.")
                        .font(.footnote).foregroundStyle(.secondary)
                    if let scene {
                        Text("Guest command queue: \(scene.passed ? "PASS" : "FAIL")\nCanvas=\(scene.width)x\(scene.height)\nCommands=\(scene.commands)\nRectangles=\(scene.rectangles)\nTextured sprites=\(scene.sprites)\nGuest instructions=\(scene.guestInstructions)\nGuest GPU calls=\(scene.guestServiceCalls)\nSprite X=\(scene.spriteX)\nSubmitted/Completed fence=\(scene.submittedFence)/\(scene.completedFence)\nQueue drained=\(scene.queueDrained)\nStatus=\(scene.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if scene.passed && MTLCreateSystemDefaultDevice() != nil {
                            Group {
                                if animating {
                                    TimelineView(.periodic(from: .now, by: 1.0 / 12.0)) { context in
                                        let index = UInt32(Int(context.date.timeIntervalSince1970 * 12) % 90)
                                        MetalGraphics20Canvas(frame: NativeGraphics20API.makeFrame(index: index))
                                    }
                                } else {
                                    MetalGraphics20Canvas(frame: scene)
                                }
                            }
                            .aspectRatio(CGFloat(scene.width) / CGFloat(scene.height), contentMode: .fit)
                            .clipShape(RoundedRectangle(cornerRadius: 12))
                            .accessibilityLabel("Metal guest graphics command queue diagnostic with moving textured sprite")
                            Button {
                                animating.toggle()
                            } label: {
                                Label(animating ? "Stop animation" : "Start animation",
                                      systemImage: animating ? "pause.fill" : "play.fill")
                            }
                        } else if MTLCreateSystemDefaultDevice() == nil {
                            Text("Metal is not available on this device.")
                        } else {
                            Text("Guest command queue was rejected; no frame rendered.")
                        }
                    }
                }
                Section("New engine behavior") {
                    Label("32-byte commands read from guest memory", systemImage: "checkmark.circle")
                    Label("Validated and copied into a bounded host queue", systemImage: "checkmark.circle")
                    Label("Monotonic submission and completion fences", systemImage: "checkmark.circle")
                    Label("Framebuffers displayed through two-pass Metal", systemImage: "checkmark.circle")
                    Label("Malformed/unreadable guest packets rejected", systemImage: "checkmark.circle")
                }
                Section("Limitations") {
                    Text("This is a synchronous software GPU queue for Misaki's original drawing format. It is not the PS4's GCN command processor, asynchronous GPU scheduling, shader ISA, or firmware. The actual PS4 home menu is not supported.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("GPU Queue")
        }
    }
}
