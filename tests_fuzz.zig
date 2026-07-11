const std = @import("std");

const c = @import("c");

const Slot = struct {
    ptr: *anyopaque,
    size: usize,
};

fn fillEdges(ptr: *anyopaque, size: usize, tag: u8) void {
    const bytes: [*]volatile u8 = @ptrCast(ptr);
    bytes[0] = tag;
    bytes[size - 1] = tag ^ 0xA5;
    if (size > 2) {
        bytes[size / 2] = tag ^ 0x5A;
    }
}

fn sizeFromSmith(smith: *std.testing.Smith) usize {
    const bucket = smith.valueRangeAtMost(u8, 0, 9);
    return switch (bucket) {
        0 => 1 + @as(usize, smith.valueRangeAtMost(u16, 0, 15)),
        1 => 17 + @as(usize, smith.valueRangeAtMost(u16, 0, 15)),
        2 => 33 + @as(usize, smith.valueRangeAtMost(u16, 0, 31)),
        3 => 65 + @as(usize, smith.valueRangeAtMost(u16, 0, 63)),
        4 => 129 + @as(usize, smith.valueRangeAtMost(u16, 0, 127)),
        5 => 257 + @as(usize, smith.valueRangeAtMost(u16, 0, 255)),
        6 => 513 + @as(usize, smith.valueRangeAtMost(u16, 0, 511)),
        7 => 1025 + @as(usize, smith.valueRangeAtMost(u16, 0, 8191)),
        8 => 16 * 1024 + @as(usize, smith.valueRangeAtMost(u16, 0, 65535)),
        else => 4 * 1024 * 1024 + @as(usize, smith.valueRangeAtMost(u16, 0, 4095)),
    };
}

fn freeAll(slots: []?Slot) void {
    for (slots) |*slot| {
        if (slot.*) |s| {
            c.cock(s.ptr);
            slot.* = null;
        }
    }
}

test "fuzz: allocator single allocation sizes" {
    try std.testing.fuzz({}, struct {
        fn fuzzOne(_: void, smith: *std.testing.Smith) anyerror!void {
            const size = sizeFromSmith(smith);
            const ptr = c.balls_backend(size) orelse return;
            try std.testing.expectEqual(@as(usize, 0), @intFromPtr(ptr) % @alignOf(usize));
            fillEdges(ptr, size, smith.value(u8));
            c.cock(ptr);
        }
    }.fuzzOne, .{
        .corpus = &.{
            &.{ 0, 0, 0, 0, 0, 0, 0, 0 },
            &.{ 7, 0, 0, 0, 0, 0, 0, 0 },
            &.{ 9, 0, 0, 0, 0, 0, 0, 0 },
        },
    });
}

test "fuzz: allocator single threaded operation sequences" {
    try std.testing.fuzz({}, struct {
        fn fuzzOne(_: void, smith: *std.testing.Smith) anyerror!void {
            var slots: [32]?Slot = .{null} ** 32;
            defer freeAll(&slots);

            const op_count = smith.valueRangeAtMost(u16, 1, 512);
            for (0..op_count) |_| {
                const slot_index = smith.index(slots.len);
                const op = smith.valueRangeAtMost(u8, 0, 3);

                switch (op) {
                    0 => {
                        if (slots[slot_index]) |slot| {
                            c.cock(slot.ptr);
                            slots[slot_index] = null;
                        }
                    },
                    1 => {
                        if (slots[slot_index]) |slot| {
                            fillEdges(slot.ptr, slot.size, smith.value(u8));
                        }
                    },
                    else => {
                        if (slots[slot_index]) |slot| {
                            c.cock(slot.ptr);
                            slots[slot_index] = null;
                        }

                        const size = sizeFromSmith(smith);
                        if (c.balls_backend(size)) |ptr| {
                            try std.testing.expectEqual(@as(usize, 0), @intFromPtr(ptr) % @alignOf(usize));
                            fillEdges(ptr, size, smith.value(u8));
                            slots[slot_index] = .{ .ptr = ptr, .size = size };
                        }
                    },
                }
            }
        }
    }.fuzzOne, .{
        .corpus = &.{
            &.{},
            &.{ 2, 0, 0, 0, 0, 0, 0, 0 },
            &.{ 3, 0, 0, 0, 0, 0, 0, 0 },
        },
    });
}
