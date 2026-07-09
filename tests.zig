const std = @import("std");

const c = @import("c");

test "basic allocation and free" {
    const ptr = c.balls_backend(128);
    try std.testing.expect(ptr != null);
    c.cock(ptr);
}

test "multiple sequential allocations" {
    var ptrs: [8]?*anyopaque = undefined;
    for (&ptrs, 0..) |*ptr, i| {
        ptr.* = c.balls_backend((i + 1) * 64);
        try std.testing.expect(ptr.* != null);
    }
    for (ptrs) |ptr| {
        c.cock(ptr);
    }
}

test "fuzz allocator with Smith input" {
    try std.testing.fuzz({}, struct {
        fn fuzzOne(_: void, smith: *std.testing.Smith) anyerror!void {
            const size = smith.value(usize) % 131072;
            if (size == 0) return;

            const ptr = c.balls_backend(size);
            if (ptr != null) {
                const slice: [*]volatile u8 = @ptrCast(ptr);
                slice[0] = 0xAA;
                slice[size - 1] = 0xBB;
                c.cock(ptr);
            }
        }
    }.fuzzOne, .{});
}
