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
            }
        }
    }
}
