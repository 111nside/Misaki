import SwiftUI
import Metal

struct Milestone19View: View {
    @State private var scene: NativeGraphics19Frame?
    @State private var animating = false

    var body: some View {
        NavigationStack {
            List {
                Section("Milestone 19 · Guest texture pipeline") {
                    Button {
                        animating = false
                        scene = NativeGraphics19API.makeFrame()
                    } label: {
                        Label("Render guest textured frame", systemImage: "square.on.square")
                    }
                    if let scene {
                        Text("Textured GPU: \(scene.passed ? "PASS" : "FAIL")\nCanvas=\(scene.width)x\(scene.height)\nCommands=\(scene.commands)\nRectangles=\(scene.rectangles)\nTextured sprites=\(scene.sprites)\nGuest instructions=\(scene.guestInstructions)\nGuest GPU calls=\(scene.guestServiceCalls)\nSprite X=\(scene.spriteX)\nFramebuffer checksum=0x\(String(scene.checksum, radix: 16))\nStatus=\(scene.status)")
                            .font(.system(.footnote, design: .monospaced))
                            .textSelection(.enabled)
                        if scene.passed && MTLCreateSystemDefaultDevice() != nil {
                            Group {
                                if animating {
                                    TimelineView(.periodic(from: .now, by: 1.0 / 12.0)) { context in
                                        let index = UInt32(Int(context.date.timeIntervalSince1970 * 12) % 90)
                                        MetalGraphics19Canvas(frame: NativeGraphics19API.makeFrame(index: index))
                                    }
                                } else {
                                    MetalGraphics19Canvas(frame: scene)
                                }
                            }
                            .aspectRatio(CGFloat(scene.width) / CGFloat(scene.height), contentMode: .fit)
                            .clipShape(RoundedRectangle(cornerRadius: 12))
                            .accessibilityLabel("Two-pass Metal-rendered guest frame with an animated textured sprite")
                            Button {
                                animating.toggle()
                            } label: {
                                Label(animating ? "Stop animation" : "Start animation",
                                      systemImage: animating ? "pause.fill" : "play.fill")
                            }
                        } else if MTLCreateSystemDefaultDevice() == nil {
                            Text("Metal is unavailable on this device.")
                        } else {
                            Text("The guest graphics frame was rejected.")
                        }
                    }
                    Text("The C++ guest CPU writes a sprite position into guest memory and submits a simulated GPU service. A bounded texture compositor creates RGBA pixels; two Metal shader passes display them.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
                Section("Implemented") {
                    Label("Synthetic x86-64 CPU → GPU service", systemImage: "checkmark.circle")
                    Label("16×16 RGBA8 texture and alpha-blended sprite", systemImage: "checkmark.circle")
                    Label("320×180 guest RGBA framebuffer", systemImage: "checkmark.circle")
                    Label("Offscreen Metal render target and shader sampling", systemImage: "checkmark.circle")
                    Label("90-frame animation cycle, approximately 12 FPS", systemImage: "checkmark.circle")
                }
                Section("Not yet implemented") {
                    Text("This is Misaki's own test graphics protocol. It does not decode PS4 GCN GPU commands, compile Sony shaders, load firmware, or display the genuine PS4 menu.")
                        .font(.footnote).foregroundStyle(.secondary)
                }
            }
            .navigationTitle("Textures")
        }
    }
}
