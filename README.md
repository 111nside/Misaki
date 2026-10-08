# Misaki

Experimental iOS PS4 emulator research project. **Not a playable PS4 emulator.**

## First milestone

SwiftUI app, file importer and minimal ELF64 header inspector. Does not execute PS4 code, load firmware or run games.

## Build

Install Xcode and [XcodeGen](https://github.com/yonaskolb/XcodeGen) on a Mac. Run `xcodegen generate`, then open `Misaki.xcodeproj` and choose your iPhone as target. Set your development signing team.

## Roadmap

1. ELF program headers, segmented memory mapping and error handling.
2. Restricted, test-driven x86-64 interpreter.
3. User-mode loader and high-level operating-system service stubs for homebrew programs.
4. GPU command decoding and Metal experiments.
5. Input/audio and debugging tools.

Only use executable files you have permission to analyze. No copyrighted firmware, keys, or games are included.
