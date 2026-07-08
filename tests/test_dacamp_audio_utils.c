#include "unity.h"
#include "dacamp_audio_utils.h"

void setUp(void) {}
void tearDown(void) {}

void test_pack_stereo_sample(void)
{
    uint32_t packed = dacamp_pack_stereo_sample(0x1234, 0x5678);
    TEST_ASSERT_EQUAL_HEX32(0x56781234u, packed);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_pack_stereo_sample);
    return UNITY_END();
}
