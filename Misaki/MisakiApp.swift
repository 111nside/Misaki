import SwiftUI

@main
struct MisakiApp: App {
    var body: some Scene {
        WindowGroup {
            TabView {
                ContentView()
                    .tabItem { Label("Emulator", systemImage: "gamecontroller") }
                Milestone7View()
                    .tabItem { Label("Linker", systemImage: "link") }
                Milestone8View()
                    .tabItem { Label("Modules", systemImage: "shippingbox") }
                Milestone9View()
                    .tabItem { Label("Core", systemImage: "cpu") }
                Milestone17View()
                    .tabItem { Label("Lifecycle", systemImage: "square.stack.3d.up") }
            }
        }
    }
}
