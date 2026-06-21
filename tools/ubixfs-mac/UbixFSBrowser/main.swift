//
// Copyright (c) 2002-2026 The UbixOS Project.
//
// UbixFS Browser — a macOS (AppKit) app to open a UbixFS pool image (raw or an
// MBR disk image), traverse its datasets/filesystems, and copy files in and out.
// The on-disk format is handled entirely by the portable lib/ubixfs_core via the
// UbixFSKit Obj-C bridge.  See docs/design/ubixfs-mac-browser-plan.md.
//
import AppKit

final class AppDelegate: NSObject, NSApplicationDelegate {
	private var window: NSWindow!
	private var controller: BrowserViewController!

	func applicationDidFinishLaunching(_ notification: Notification) {
		buildMenu()

		controller = BrowserViewController()
		window = NSWindow(contentViewController: controller)
		window.setContentSize(NSSize(width: 820, height: 520))
		window.minSize = NSSize(width: 640, height: 400)
		window.title = "UbixFS Browser"
		window.center()
		window.makeKeyAndOrderFront(nil)
		NSApp.activate(ignoringOtherApps: true)
	}

	func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }

	private func buildMenu() {
		let mainMenu = NSMenu()

		let appItem = NSMenuItem()
		mainMenu.addItem(appItem)
		let appMenu = NSMenu()
		appItem.submenu = appMenu
		appMenu.addItem(withTitle: "About UbixFS Browser", action: #selector(NSApplication.orderFrontStandardAboutPanel(_:)), keyEquivalent: "")
		appMenu.addItem(.separator())
		appMenu.addItem(withTitle: "Quit UbixFS Browser", action: #selector(NSApplication.terminate(_:)), keyEquivalent: "q")

		let fileItem = NSMenuItem()
		mainMenu.addItem(fileItem)
		let fileMenu = NSMenu(title: "File")
		fileItem.submenu = fileMenu
		fileMenu.addItem(withTitle: "Open Image…", action: #selector(BrowserViewController.openImageMenu), keyEquivalent: "o")
		fileMenu.addItem(withTitle: "Close Image", action: #selector(BrowserViewController.closeImage), keyEquivalent: "w")

		NSApp.mainMenu = mainMenu
	}
}

let app = NSApplication.shared
let delegate = AppDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
