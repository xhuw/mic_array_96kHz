
#include <cstdint>
#include <iostream>
#include <mic_array.h>
#include <mic_array/etc/filters_default.h>
#include <platform.h>
#include <xs1.h>
#include <xcore/parallel.h>
#include <xcore/channel.h>
#include <print.h>
#include <xcore/hwtimer.h>
#include <xcore/port.h>

extern "C" {
#include <sw_pll.h>
}

// Change stage 2 decimation factor to change mic data rate.
#define MIC_ARRAY_CONFIG_STG2_DEC_FACTOR    1

/// TODO design a unity filter
#define MIC_ARRAY_STAGE_2_NUM_TAPS          96
#define MIC_ARRAY_CONFIG_STG2_RIGHT_SHIFT   2
#define STAGE_2_48K_COEFFS  \
{                           \
-0x2b915, -0x68daa, 0x12b1b, 0xe0dd5, 0x7aab9, -0x138439, -0x19f6aa, 0xe98af, 0x325327, 0x9d62c, -0x453461, -0x39da72, 0x3ff003, 0x79a63a, -0xf0b09, -0xb15cab, -0x56c4c6, 0xbb8595, 0xe472b5, -0x707afa, -0x16f40d3, -0x467c2b, 0x1b26780, 0x15a2769, -0x1613820, -0x28784d8, 0x45cf09, 0x35a69c3, 0x19be171, -0x345fdf3, -0x3eb6280, 0x1d11f71, 0x5f64572, 0x1337b74, -0x6c761d0, -0x57cf5b3, 0x5581126, 0xa2a12d7, -0xcc4e19, -0xdbd10c4, -0x761b6b2, 0xe0b081c, 0x13711ab2, -0x7854251, -0x23f260e4, -0xfcc09a7, 0x3a64a62b, 0x7fffffff, 0x7fffffff, 0x3a64a62b, -0xfcc09a7, -0x23f260e4, -0x7854251, 0x13711ab2, 0xe0b081c, -0x761b6b2, -0xdbd10c4, -0xcc4e19, 0xa2a12d7, 0x5581126, -0x57cf5b3, -0x6c761d0, 0x1337b74, 0x5f64572, 0x1d11f71, -0x3eb6280, -0x345fdf3, 0x19be171, 0x35a69c3, 0x45cf09, -0x28784d8, -0x1613820, 0x15a2769, 0x1b26780, -0x467c2b, -0x16f40d3, -0x707afa, 0xe472b5, 0xbb8595, -0x56c4c6, -0xb15cab, -0xf0b09, 0x79a63a, 0x3ff003, -0x39da72, -0x453461, 0x9d62c, 0x325327, 0xe98af, -0x19f6aa, -0x138439, 0x7aab9, 0xe0dd5, 0x12b1b, -0x68daa, -0x2b915 \
}

// Width of the PDM data port, TODO correct
constexpr auto PDM_DATA_PORT_WIDTH = 8;
// Number of mics connected to the port, TODO correct
constexpr auto MIC_COUNT = 4;
// order list of the PDM data port pins that should be used for microphones, TODO correct
unsigned channel_map[MIC_COUNT] = {4, 5, 6, 7};
constexpr auto MCLK_FREQ = 48000*512; // 24MHz
constexpr auto PDM_FREQ = 3072000;
constexpr auto SAMPLES_PER_FRAME = 1;
constexpr auto USE_DC_ELIMINATION = 1;
constexpr auto CLOCK_BLOCK_A = XS1_CLKBLK_1;
constexpr auto MCLK_DIVIDER = MCLK_FREQ / PDM_FREQ;

// the actual ports used for MCLK, PDM clock and data. TODO correct
pdm_rx_resources_t pdm_res = PDM_RX_RESOURCES_SDR(
        PORT_PDM_MCLK,
        PORT_PDM_CLK,
        PORT_PDM_DATA,
        CLOCK_BLOCK_A);

static const int32_t WORD_ALIGNED stage2_coef_custom[MIC_ARRAY_STAGE_2_NUM_TAPS] = STAGE_2_48K_COEFFS;
static constexpr right_shift_t stage2_shift_custom = MIC_ARRAY_CONFIG_STG2_RIGHT_SHIFT;

constexpr const uint32_t* stage_1_filter() {
    return &stage1_coef[0];
}

constexpr const int32_t* stage_2_filter() {
    return &stage2_coef_custom[0];
}
constexpr const right_shift_t* stage_2_shift() {
    return &stage2_shift_custom;
}

using TMicArray = mic_array::MicArray<MIC_COUNT,
                          mic_array::TwoStageDecimator<MIC_COUNT,
                                                       MIC_ARRAY_CONFIG_STG2_DEC_FACTOR,
                                                       MIC_ARRAY_STAGE_2_NUM_TAPS>,
                          mic_array::StandardPdmRxService<PDM_DATA_PORT_WIDTH,
                                                          MIC_COUNT,
                                                          MIC_ARRAY_CONFIG_STG2_DEC_FACTOR>,
                          // std::conditional uses USE_DCOE to determine which
                          // sample filter is used.
                          typename std::conditional<USE_DC_ELIMINATION,
                                              mic_array::DcoeSampleFilter<MIC_COUNT>,
                                              mic_array::NopSampleFilter<MIC_COUNT>>::type,
                          mic_array::FrameOutputHandler<MIC_COUNT,
                                                        SAMPLES_PER_FRAME,
                                                        mic_array::ChannelFrameTransmitter>>;

TMicArray mics;
channel_t mic_chan;

extern "C" {
    void mics_thread(void*);
    void mic_proc_thread(void*);
}

void mics_thread(void* a) {
    mics.PdmRx.InstallISR();
    mics.PdmRx.UnmaskISR();
    mics.ThreadEntry();
}

void pdmthread(void* a) {
    // mics.PdmRx.ThreadEntry();
}

void mic_proc_thread(void* a) {
    port_enable(XS1_PORT_4E);
    int32_t data[4];
    for(uint32_t i = 0; ; ++i) {
        port_out(XS1_PORT_4E, 0x0);
        ma_frame_rx(data, mic_chan.end_b, 4, 1);
        port_out(XS1_PORT_4E, 0xf);
    }
}


int main() {
    mic_chan = chan_alloc();


    sw_pll_fixed_clock(MCLK_FREQ);


    mics.Decimator.Init(stage_1_filter(), stage_2_filter(), *stage_2_shift());
    mics.PdmRx.Init(pdm_res.p_pdm_mics);
    mics.PdmRx.MapChannels(channel_map);
    mic_array_resources_configure(&pdm_res, MCLK_DIVIDER);
    mic_array_pdm_clock_start(&pdm_res);

    mics.OutputHandler.FrameTx.SetChannel(mic_chan.end_a);

    PAR_FUNCS(
            PFUNC(mics_thread, 0),
            PFUNC(mic_proc_thread, 0)
    );
}
