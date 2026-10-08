import Foundation

/// Experimental cooperative executor for independent guest *test* contexts.
/// Each CPU has its own memory, stack and toy syscall state. This does not
/// implement PS4 threading, shared memory, guest synchronization or host threads.
struct GuestThreadScheduler {
    private(set) var guests: [X86Interpreter]
    private(set) var totalInstructions = 0
    private(set) var dispatches = 0

    init(guests: [X86Interpreter]) {
        self.guests = guests
    }

    mutating func run(maxTotalSteps: Int = 10_000) throws {
        guard maxTotalSteps > 0 else { throw EmulatorError.stepLimit }
        while guests.contains(where: { !$0.isHalted }) {
            var progressed = false
            for index in guests.indices where !guests[index].isHalted {
                guard totalInstructions < maxTotalSteps else { throw EmulatorError.stepLimit }
                try guests[index].step()
                totalInstructions += 1
                dispatches += 1
                progressed = true
            }
            if !progressed { break }
        }
    }
}
