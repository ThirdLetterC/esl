const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    if (target.result.os.tag == .windows) {
        std.debug.panic("Windows is not a supported target.", .{});
    }

    const c_flags = &[_][]const u8{
        "-std=c23",
        "-D_POSIX_C_SOURCE=200112L",
        "-D_XOPEN_SOURCE=700",
    };

    const esl_sources = &[_][]const u8{
        "src/esl.c",
        "src/esl_buffer.c",
        "src/esl_config.c",
        "src/esl_event.c",
        "src/esl_json.c",
        "src/esl_threadmutex.c",
        "src/parson.c",
    };

    const esl_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    esl_module.addIncludePath(b.path("include"));
    esl_module.addCSourceFiles(.{
        .files = esl_sources,
        .flags = c_flags,
        .language = .c,
    });
    esl_module.link_libc = true;
    esl_module.linkSystemLibrary("pthread", .{});

    const esl = b.addLibrary(.{
        .name = "esl",
        .root_module = esl_module,
        .linkage = .static,
    });
    b.installArtifact(esl);

    const client_module = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });
    client_module.addIncludePath(b.path("include"));
    client_module.addCSourceFiles(.{
        .files = &.{"examples/testclient.c"},
        .flags = c_flags,
        .language = .c,
    });
    client_module.linkLibrary(esl);
    client_module.link_libc = true;
    client_module.linkSystemLibrary("pthread", .{});

    const testclient = b.addExecutable(.{
        .name = "testclient",
        .root_module = client_module,
    });
    b.installArtifact(testclient);

    const esl_step = b.step("esl", "Build the ESL static library");
    esl_step.dependOn(&esl.step);

    const client_step = b.step("testclient", "Build the sample test client");
    client_step.dependOn(&testclient.step);
}
