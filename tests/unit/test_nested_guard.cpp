#include "hyprvx/platform/NestedGuard.hpp"

#include <gtest/gtest.h>

using namespace hyprvx::platform;

TEST(NestedGuardTest, RequiresAllIdentityEvidence) {
    NestedContract contract{"nested_sig", "host_sig", "wayland-2", "/tmp/nested/runtime", 321};
    NestedEvidence evidence{"1", "nested_sig", "wayland-2", "/tmp/nested/runtime", 321, 321};
    EXPECT_TRUE(nestedContractMatches(contract, evidence));
    evidence.socketPeerPid = 123;
    EXPECT_FALSE(nestedContractMatches(contract, evidence));
    evidence.socketPeerPid = 321;
    evidence.waylandPeerPid = 123;
    EXPECT_FALSE(nestedContractMatches(contract, evidence));
    evidence.waylandPeerPid = 321;
    evidence.signature = "host_sig";
    EXPECT_FALSE(nestedContractMatches(contract, evidence));
    evidence.signature = "nested_sig";
    evidence.markerValue = "0";
    EXPECT_FALSE(nestedContractMatches(contract, evidence));
}

TEST(NestedGuardTest, RejectsHostOrPathInjection) {
    NestedContract contract{"nested_sig", "nested_sig", "wayland-2", "/tmp/nested/runtime", 321};
    NestedEvidence evidence{"1", "nested_sig", "wayland-2", "/tmp/nested/runtime", 321, 321};
    EXPECT_FALSE(nestedContractMatches(contract, evidence));
    contract.hostSignature = "host_sig";
    contract.signature = "../host";
    evidence.signature = "../host";
    EXPECT_FALSE(nestedContractMatches(contract, evidence));
    contract.signature = "nested_sig";
    evidence.signature = "nested_sig";
    contract.waylandDisplay = "../../wayland-1";
    evidence.waylandDisplay = "../../wayland-1";
    EXPECT_FALSE(nestedContractMatches(contract, evidence));
}
