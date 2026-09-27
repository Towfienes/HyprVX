#include "hyprvx/hyprland/Socket2Stream.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <cstdlib>
#include <cstring>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using namespace hyprvx::hyprland;

TEST(LineFramerTest, FramesOneCompleteLine) {
    LineFramer framer;
    EXPECT_EQ(framer.feed("activewindowv2>>0x1\n"),
              (std::vector<std::string>{"activewindowv2>>0x1"}));
}

TEST(LineFramerTest, FramesMultipleLinesInOneChunk) {
    LineFramer framer;
    EXPECT_EQ(framer.feed("one>>1\ntwo>>2\n"),
              (std::vector<std::string>{"one>>1", "two>>2"}));
}

TEST(LineFramerTest, HoldsPartialLineUntilNextChunk) {
    LineFramer framer;
    EXPECT_TRUE(framer.feed("activewindowv2>>0x").empty());
    EXPECT_EQ(framer.feed("1\n"), (std::vector<std::string>{"activewindowv2>>0x1"}));
}

TEST(LineFramerTest, FinishReturnsTrailingPartialFrameOnce) {
    LineFramer framer;
    EXPECT_TRUE(framer.feed("closewindow>>0x1").empty());
    const auto trailing = framer.finish();
    ASSERT_TRUE(trailing);
    EXPECT_EQ(*trailing, "closewindow>>0x1");
    EXPECT_FALSE(framer.finish());
}

TEST(LineFramerTest, StripsCarriageReturnFromCrLf) {
    LineFramer framer;
    EXPECT_EQ(framer.feed("workspacev2>>2,2\r\n"),
              (std::vector<std::string>{"workspacev2>>2,2"}));
}

TEST(LineFramerTest, DoesNotEmitDuplicateLines) {
    LineFramer framer;
    EXPECT_EQ(framer.feed("line>>1\nline>>2\n"),
              (std::vector<std::string>{"line>>1", "line>>2"}));
    EXPECT_TRUE(framer.feed("").empty());
    EXPECT_FALSE(framer.finish());
}

TEST(LineFramerTest, EnforcesMaximumBufferedLineSize) {
    LineFramer framer;
    const std::string maximum(LineFramer::MaximumLineBytes, 'x');
    EXPECT_TRUE(framer.feed(maximum).empty());
    EXPECT_THROW((void)framer.feed("x"), std::length_error);
}

TEST(LineFramerTest, AcceptsLineAtMaximumSizeWhenTerminated) {
    LineFramer framer;
    const std::string maximum(LineFramer::MaximumLineBytes, 'x');
    const auto lines = framer.feed(maximum + "\n");
    ASSERT_EQ(lines.size(), 1U);
    EXPECT_EQ(lines.front().size(), LineFramer::MaximumLineBytes);
}

namespace {

class TemporaryTree {
public:
    TemporaryTree() {
        static unsigned counter = 0;
        root_ = std::filesystem::temp_directory_path() /
            ("hyprvx-socket2-test-" + std::to_string(++counter));
        std::filesystem::create_directories(root_);
    }
    ~TemporaryTree() { std::filesystem::remove_all(root_); }
    const std::filesystem::path& root() const { return root_; }

    std::filesystem::path socketAt(
        const std::filesystem::path& base,
        const std::string& signature,
        bool runtimeLayout) {
        const auto directory = runtimeLayout ? base / "hypr" / signature : base / signature;
        std::filesystem::create_directories(directory);
        const auto path = directory / ".socket2.sock";
        std::ofstream(path).put('\0');
        return path;
    }

    std::filesystem::path runtimeSocketPath(const std::string& signature) {
        const auto directory = root_ / "hypr" / signature;
        std::filesystem::create_directories(directory);
        return directory / ".socket2.sock";
    }

private:
    std::filesystem::path root_;
};

} // namespace

namespace {

class ScopedEnvironment {
public:
    ScopedEnvironment(const char* name, const std::string& value) : name_(name) {
        if (const char* existing = std::getenv(name); existing != nullptr) {
            previous_ = existing;
        }
        setenv(name, value.c_str(), 1);
    }
    ~ScopedEnvironment() {
        if (previous_) {
            setenv(name_.c_str(), previous_->c_str(), 1);
        } else {
            unsetenv(name_.c_str());
        }
    }

private:
    std::string name_;
    std::optional<std::string> previous_;
};

} // namespace

TEST(Socket2StreamTest, ReadsUntilEofThenReturnsCleanlyWithoutSpinning) {
    TemporaryTree tree;
    const std::string signature = "phase2btest";
    const auto socketPath = tree.runtimeSocketPath(signature);
    ScopedEnvironment signatureEnv("HYPRLAND_INSTANCE_SIGNATURE", signature);
    ScopedEnvironment runtimeEnv("XDG_RUNTIME_DIR", tree.root().string());

    const int listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    const auto pathString = socketPath.string();
    ASSERT_LT(pathString.size(), sizeof(address.sun_path));
    std::copy(pathString.begin(), pathString.end(), address.sun_path);
    address.sun_path[pathString.size()] = '\0';
    ASSERT_EQ(::bind(listener, reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0);
    ASSERT_EQ(::listen(listener, 1), 0);

    std::thread server([listener] {
        const int peer = ::accept(listener, nullptr, nullptr);
        if (peer >= 0) {
            const std::string payload = "activewindowv2>>abc\nclosewindow>>partial";
            (void)::send(peer, payload.data(), payload.size(), 0);
            (void)::shutdown(peer, SHUT_WR);
            ::close(peer);
        }
        ::close(listener);
    });

    std::vector<std::string> lines;
    {
        Socket2Stream stream;
        while (auto line = stream.nextLine()) {
            lines.push_back(*line);
        }
        EXPECT_FALSE(stream.nextLine());
    }
    server.join();
    EXPECT_EQ(lines, (std::vector<std::string>{"activewindowv2>>abc", "closewindow>>partial"}));
}

TEST(SocketPathTest, PrefersExistingRuntimeSocket) {
    TemporaryTree tree;
    const auto runtime = tree.root() / "runtime";
    const auto temporary = tree.root() / "tmp-hypr";
    const auto runtimeSocket = tree.socketAt(runtime, "instance", true);
    tree.socketAt(temporary, "instance", false);
    EXPECT_EQ(resolveSocketPath("instance", runtime, temporary), runtimeSocket);
}

TEST(SocketPathTest, UsesTemporaryFallbackWhenRuntimeSocketIsAbsent) {
    TemporaryTree tree;
    const auto runtime = tree.root() / "runtime";
    const auto temporary = tree.root() / "tmp-hypr";
    const auto temporarySocket = tree.socketAt(temporary, "instance", false);
    EXPECT_EQ(resolveSocketPath("instance", runtime, temporary), temporarySocket);
}

TEST(SocketPathTest, UsesTemporarySocketWhenRuntimeDirectoryIsUnavailable) {
    TemporaryTree tree;
    const auto temporary = tree.root() / "tmp-hypr";
    const auto temporarySocket = tree.socketAt(temporary, "instance", false);
    EXPECT_EQ(resolveSocketPath("instance", std::nullopt, temporary), temporarySocket);
}

TEST(SocketPathTest, MissingSocketReportsCheckedPath) {
    TemporaryTree tree;
    const auto runtime = tree.root() / "runtime";
    const auto temporary = tree.root() / "tmp-hypr";
    try {
        (void)resolveSocketPath("instance", runtime, temporary);
        FAIL() << "Expected missing socket to fail";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("socket2.sock"), std::string::npos);
    }
}

TEST(SocketPathTest, RejectsEmptyInstanceSignature) {
    EXPECT_THROW((void)resolveSocketPath("", std::nullopt), std::runtime_error);
}
