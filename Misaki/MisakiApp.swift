import SwiftUI

@main
struct MisakiApp: App {
    var body: some Scene {
        WindowGroup {
            TabView {
                ContentView()
                    .tabItem { Label("Emulator", systemImage: "gamecontroller") }
                Milestone9View()
                    .tabItem { Label("Core", systemImage: "cpu") }
                Milestone22View()
                    .tabItem { Label("PM4", systemImage: "waveform.path.ecg") }
                Milestone21View()
                    .tabItem { Label("Graphics", systemImage: "display") }
                Milestone20View()
                    .tabItem { Label("Queue", systemImage: "square.stack.3d.forward.dottedline") }
                Milestone19View()
                    .tabItem { Label("Textures", systemImage: "square.on.square") }
                Milestone18View()
                    .tabItem { Label("Basic GPU", systemImage: "rectangle.3.group") }
                Milestone7View()
                    .tabItem { Label("Linker", systemImage: "link") }
                Milestone8View()
                    .tabItem { Label("Modules", systemImage: "shippingbox") }
                Milestone17View()
                    .tabItem { Label("Lifecycle", systemImage: "square.stack.3d.up") }
            }
        }
    }
}
