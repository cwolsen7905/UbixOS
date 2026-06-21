//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// UbixFS Browser — a macOS app to open a UbixFS pool image (raw or an MBR disk
// image), traverse its datasets/filesystems, and copy files in and out.  The
// on-disk format is handled entirely by the portable lib/ubixfs_core via the
// UbixFSKit Obj-C bridge.  See docs/design/ubixfs-mac-browser-plan.md.
//
import SwiftUI

@main
struct UbixFSBrowserApp: App {
	@StateObject private var model = PoolModel()

	var body: some Scene {
		WindowGroup {
			ContentView()
				.environmentObject(model)
				.frame(minWidth: 720, minHeight: 460)
		}
		.commands {
			CommandGroup(replacing: .newItem) {
				Button("Open Image…") { openImage() }
					.keyboardShortcut("o")
				Button("Close Image") { model.close() }
					.keyboardShortcut("w")
					.disabled(!model.isOpen)
			}
		}
	}

	private func openImage() {
		let panel = NSOpenPanel()
		panel.allowsMultipleSelection = false
		panel.canChooseDirectories = false
		panel.message = "Choose a UbixFS pool image or disk image"
		if panel.runModal() == .OK, let url = panel.url {
			model.open(url: url, readOnly: true)
		}
	}
}
