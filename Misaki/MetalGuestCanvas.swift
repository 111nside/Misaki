import SwiftUI
import Metal
import MetalKit

/// Renders Misaki's validated 2D guest-command stream through Metal on iOS.
/// It is not a PlayStation GCN translator or a PS4 system-menu renderer.
struct MetalGuestCanvas: UIViewRepresentable {
    let scene: NativeGPUFrame

    func makeCoordinator() -> Coordinator { Coordinator(scene: scene) }

    func makeUIView(context: Context) -> MTKView {
        let view = MTKView(frame: .zero, device: MTLCreateSystemDefaultDevice())
        view.colorPixelFormat = .bgra8Unorm
        view.framebufferOnly = true
        view.isPaused = true
        view.enableSetNeedsDisplay = true
        view.delegate = context.coordinator
        context.coordinator.prepare(device: view.device, pixelFormat: view.colorPixelFormat)
        view.setNeedsDisplay()
        return view
    }

    func updateUIView(_ view: MTKView, context: Context) {
        context.coordinator.scene = scene
        view.setNeedsDisplay()
    }

    final class Coordinator: NSObject, MTKViewDelegate {
        var scene: NativeGPUFrame
        private var commandQueue: MTLCommandQueue?
        private var pipeline: MTLRenderPipelineState?

        init(scene: NativeGPUFrame) { self.scene = scene }

        func prepare(device: MTLDevice?, pixelFormat: MTLPixelFormat) {
            guard let device else { return }
            commandQueue = device.makeCommandQueue()
            // Tiny built-in shader; no bundled game shaders or external Metal files.
            let source = """
            #include <metal_stdlib>
            using namespace metal;
            vertex float4 misaki_vertex(const device float2 *positions [[buffer(0)]],
                                         uint index [[vertex_id]]) {
                return float4(positions[index], 0.0, 1.0);
            }
            fragment float4 misaki_fragment(constant float4 &color [[buffer(0)]]) {
                return color;
            }
            """
            guard let library = try? device.makeLibrary(source: source, options: nil),
                  let vertex = library.makeFunction(name: "misaki_vertex"),
                  let fragment = library.makeFunction(name: "misaki_fragment") else { return }
            let descriptor = MTLRenderPipelineDescriptor()
            descriptor.vertexFunction = vertex
            descriptor.fragmentFunction = fragment
            descriptor.colorAttachments[0].pixelFormat = pixelFormat
            pipeline = try? device.makeRenderPipelineState(descriptor: descriptor)
        }

        func mtkView(_ view: MTKView, drawableSizeWillChange size: CGSize) {}

        func draw(in view: MTKView) {
            guard scene.passed, let device = view.device,
                  let pipeline, let commandQueue,
                  let pass = view.currentRenderPassDescriptor,
                  let drawable = view.currentDrawable else { return }

            guard let clear = scene.commands.first, clear.kind == 1 else { return }
            let bg = rgba(clear.rgba)
            pass.colorAttachments[0].loadAction = .clear
            pass.colorAttachments[0].storeAction = .store
            pass.colorAttachments[0].clearColor = MTLClearColor(
                red: Double(bg.x), green: Double(bg.y),
                blue: Double(bg.z), alpha: Double(bg.w))
            guard let commandBuffer = commandQueue.makeCommandBuffer(),
                  let encoder = commandBuffer.makeRenderCommandEncoder(descriptor: pass) else { return }
            encoder.setRenderPipelineState(pipeline)
            for command in scene.commands where command.kind == 2 {
                // The guest protocol is in top-left-origin pixels. Metal uses NDC.
                let x0 = Float(command.x) / Float(scene.width) * 2 - 1
                let x1 = Float(command.x + command.width) / Float(scene.width) * 2 - 1
                let y0 = 1 - Float(command.y) / Float(scene.height) * 2
                let y1 = 1 - Float(command.y + command.height) / Float(scene.height) * 2
                let vertices: [SIMD2<Float>] = [
                    SIMD2(x0, y0), SIMD2(x0, y1), SIMD2(x1, y0),
                    SIMD2(x1, y0), SIMD2(x0, y1), SIMD2(x1, y1)
                ]
                let size = vertices.count * MemoryLayout<SIMD2<Float>>.stride
                guard let buffer = vertices.withUnsafeBufferPointer({ pointer in
                    device.makeBuffer(bytes: pointer.baseAddress!, length: size, options: .storageModeShared)
                }) else { continue }
                var color = rgba(command.rgba)
                encoder.setVertexBuffer(buffer, offset: 0, index: 0)
                withUnsafeBytes(of: &color) { raw in
                    guard let address = raw.baseAddress else { return }
                    encoder.setFragmentBytes(address, length: raw.count, index: 0)
                }
                encoder.drawPrimitives(type: .triangle, vertexStart: 0, vertexCount: 6)
            }
            encoder.endEncoding()
            commandBuffer.present(drawable)
            commandBuffer.commit()
        }

        private func rgba(_ packed: UInt32) -> SIMD4<Float> {
            SIMD4<Float>(Float((packed >> 24) & 255) / 255,
                         Float((packed >> 16) & 255) / 255,
                         Float((packed >> 8) & 255) / 255,
                         Float(packed & 255) / 255)
        }
    }
}
