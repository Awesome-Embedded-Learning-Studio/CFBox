#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_CHGRP

TEST(ChgrpTest, MissingOperand) {
    char a0[] = "chgrp";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(chgrp_main(1, argv), 2);
}

TEST(ChgrpTest, NonexistentFile) {
    char a0[] = "chgrp", a1[] = "root", a2[] = "/nonexistent_xyz_12345";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_NE(chgrp_main(3, argv), 0);
}

TEST(ChgrpTest, Help) {
    char a0[] = "chgrp", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return chgrp_main(2, argv); });
    EXPECT_NE(out.find("chgrp"), std::string::npos);
}

#endif // CFBOX_ENABLE_CHGRP
