import SwiftUI

@main
struct OpenOpalApp: App {
    @State private var camera = CameraModel()

    var body: some Scene {
        Window("Open Opal", id: "main") {
            ContentView()
                .environment(camera)
                .frame(minWidth: 940, minHeight: 620)
                .task { await camera.start() }
                .onDisappear { camera.stop() }
        }
        .windowStyle(.hiddenTitleBar)
        .windowResizability(.contentMinSize)
        .commands {
            CommandGroup(replacing: .newItem) {}
            CommandMenu("Camera") {
                Button("Reconnect") {
                    Task { await camera.reconnect() }
                }
                .keyboardShortcut("r")

                Button("Trigger Autofocus") { camera.device.triggerAutofocus() }
                    .keyboardShortcut("f")
                    .disabled(!camera.device.state.isLive)

                Divider()

                Button("Reset All Settings") { camera.settings.reset(); camera.push() }

                Button("Toggle Advanced Settings") { camera.settings.showAdvanced.toggle() }
                    .keyboardShortcut("a", modifiers: [.command, .shift])

                Button(camera.previewFrozen ? "Unfreeze Preview" : "Freeze Preview") {
                    camera.previewFrozen.toggle()
                }
                .keyboardShortcut("f", modifiers: [.command, .shift])

                Divider()

                // Follow mode (T-003). A cold setting: toggling rebuilds the pipeline.
                Button(camera.settings.followEnabled ? "Turn Follow Mode Off" : "Turn Follow Mode On") {
                    camera.toggleFollow()
                }
                .keyboardShortcut("l", modifiers: [.command, .shift])

                // T-003 hand steering — replaced by T-010.
                Menu("Follow Test") {
                    Button("Pan Left")  { camera.nudgeFollow(dx: -0.05, dy: 0) }
                        .keyboardShortcut(.leftArrow, modifiers: [.command, .option])
                    Button("Pan Right") { camera.nudgeFollow(dx: 0.05, dy: 0) }
                        .keyboardShortcut(.rightArrow, modifiers: [.command, .option])
                    Button("Pan Up")    { camera.nudgeFollow(dx: 0, dy: -0.05) }
                        .keyboardShortcut(.upArrow, modifiers: [.command, .option])
                    Button("Pan Down")  { camera.nudgeFollow(dx: 0, dy: 0.05) }
                        .keyboardShortcut(.downArrow, modifiers: [.command, .option])
                    Button("Zoom In")   { camera.zoomFollow(by: 0.1) }
                        .keyboardShortcut("=", modifiers: [.command, .option])
                    Button("Zoom Out")  { camera.zoomFollow(by: -0.1) }
                        .keyboardShortcut("-", modifiers: [.command, .option])
                }
                .disabled(!camera.device.followOpen)
            }
        }
    }
}
